/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "vhost_client.h"
#include "circle.h"




typedef struct t_wrk
{
  int created;
  int rx_enable;
  int tx_enable;
  char socket[MAX_PATH_LEN];
  struct vhost_device_ops virtio_net_ops;
} t_wrk;

static int g_rxtx_worker;
static int g_rxtx_worker_active;
static uint32_t volatile g_lock;
static int g_running_lcore;
static t_wrk g_wrk[2];
void end_clean_unlink(void);
static char g_name[MAX_NAME_LEN];
static char g_memid[MAX_NAME_LEN];

/****************************************************************************/
static void vhost_lock_acquire(void)
{
  while (__sync_lock_test_and_set(&(g_lock), 1));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vhost_lock_release(void)
{
  __sync_lock_release(&(g_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static uint64_t get_usec(void)
{
  uint64_t usec;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  usec = (ts.tv_sec * 1000000);
  usec += (ts.tv_nsec / 1000);
  return usec;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_new_device(int vid)
{
  if ((vid != 0) && (vid != 1))
    KOUT("%d", vid);
  vhost_lock_acquire();
  rte_vhost_enable_guest_notification(vid, 0, 0);
  rte_vhost_enable_guest_notification(vid, 1, 0);
  g_wrk[vid].created = 1;
  vhost_lock_release();
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_features_changed(int vid, uint64_t features)
{
  return features; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void virtio_destroy_device(int vid)
{
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_vring_state_changed(int vid, uint16_t queue_id, int enable)
{
  vhost_lock_acquire();
  if (queue_id==0)
    {
    if (enable)
      g_wrk[vid].rx_enable = 1;
    else
      g_wrk[vid].rx_enable = 0;
    }
  else
    {
    if (enable)
      g_wrk[vid].tx_enable = 1;
    else
      g_wrk[vid].tx_enable = 0;
    }
  vhost_lock_release();
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void enqueue(int id, uint64_t usec, int nb, struct rte_mbuf **pkts)
{
  int i;
  for (i=0; i<nb; i++)
    { 
    circle_put(id, pkts[i]->pkt_len, usec, pkts[i]);
    }
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flush_tx_circle(int id)
{
  struct rte_mbuf *pkts_tx[MAX_PKT_BURST];
  int nb_tx_todo, result = 0;
  int peer_id;
  if (id == 0)
    peer_id = 1;
  else
    peer_id = 0;
  while (result == 0)
    {
    result = -1;
    nb_tx_todo = 0;
    pkts_tx[nb_tx_todo] = circle_get(peer_id);
    while(pkts_tx[nb_tx_todo])
      {
      nb_tx_todo += 1;
      if (nb_tx_todo == MAX_PKT_BURST)
        {
        result = 0;
        break;
        }
      pkts_tx[nb_tx_todo] = circle_get(peer_id);
      }
    rte_vhost_enqueue_burst(id, 0, pkts_tx, nb_tx_todo);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int store_rx_circle(int id, uint64_t usec, struct rte_mempool *mpool)
{
  struct rte_mbuf *pkts_rx[MAX_PKT_BURST];
  int nb_rx, result = -1;
  nb_rx = rte_vhost_dequeue_burst(id, 1, mpool, pkts_rx, MAX_PKT_BURST);
  if (nb_rx)
    {
    enqueue(id, usec, nb_rx, pkts_rx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void sched_mngt(int id)
{
  uint64_t len;
  uint64_t usec;
  struct rte_mbuf *mbuf;
  int result = -1, pcap_nok = 0;
  if (!circle_peek(id, &len, &usec, &mbuf))
    result = 0;
  while(result == 0)
    {
    pcap_nok = circle_swap(id, pcap_nok);
    result = -1;
    if (!circle_peek(id, &len, &usec, &mbuf))
      result = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rxtx_worker(void *data)
{
  int id, sid;
  uint32_t mcache = MBUF_MCACHE;
  uint32_t mbufs = MBUF_MAX;
  uint32_t msize = MBUF_SIZE;
  struct rte_mempool *mpool;

  if (rte_lcore_id() != g_running_lcore) 
    KOUT("%s", g_name);
  sid = rte_lcore_to_socket_id(rte_lcore_id());
  mpool = rte_pktmbuf_pool_create(g_memid, mbufs, mcache, 0, msize, sid);
  if (!mpool)
    KOUT("%s", g_name);

  g_rxtx_worker_active = 1;

  while(g_rxtx_worker)
    {
    if (!(g_wrk[0].rx_enable && g_wrk[1].rx_enable && g_wrk[0].created &&
          g_wrk[0].tx_enable && g_wrk[1].tx_enable && g_wrk[1].created))
      {
      rte_pause();
      usleep(500);
      }
    else
      break;
    }


  while(g_rxtx_worker)
    {
    rte_pause();
    usleep(80);
    if (g_rxtx_worker)
      {
      vhost_lock_acquire();
      if (g_rxtx_worker &&
          g_wrk[0].created && g_wrk[1].created &&
          g_wrk[0].rx_enable && g_wrk[1].rx_enable &&
          g_wrk[0].tx_enable && g_wrk[1].tx_enable)
        {
        for (id=0; id<2; id++)
          {
          store_rx_circle(id, get_usec(), mpool);
          sched_mngt(id);
          flush_tx_circle(id);
          circle_clean(id);
          }
        }
      vhost_lock_release();
      }
    }
  for (id=0; id<2; id++)
    {
    circle_flush(id);
    rte_vhost_driver_unregister(g_wrk[id].socket);
    }
  rte_mempool_free(mpool);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_end_and_exit(void)
{
  g_rxtx_worker = 0;
  if (g_running_lcore != -1)
    {
    rte_eal_wait_lcore(g_running_lcore);
    g_running_lcore = -1;
    }
  end_clean_unlink();
  rte_exit(EXIT_SUCCESS, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start_id(int id, int sid, char *path)
{
  uint64_t flags = 0;
  int err;
  strncpy(g_wrk[id].socket, path, MAX_PATH_LEN-1);
  g_wrk[id].virtio_net_ops.new_device          = virtio_new_device;
  g_wrk[id].virtio_net_ops.destroy_device      = virtio_destroy_device;
  g_wrk[id].virtio_net_ops.vring_state_changed = virtio_vring_state_changed;
  g_wrk[id].virtio_net_ops.features_changed    = virtio_features_changed;
  err = rte_vhost_driver_register(path, flags);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_callback_register(path, &g_wrk[id].virtio_net_ops);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_start(path);
  if (err)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start(char *memid, char *path0, char *path1)
{
  int sid;
  g_running_lcore = rte_get_next_lcore(-1, 1, 0);
    if (g_running_lcore >= RTE_MAX_LCORE)
      KOUT("%d", g_running_lcore);
  sid = rte_lcore_to_socket_id(rte_lcore_id());
  if (sid < 0)
    KOUT(" ");
  memset(g_memid, 0, MAX_NAME_LEN);
  strncpy(g_memid, memid, MAX_NAME_LEN-1);
  vhost_client_start_id(0, sid, path0);
  vhost_client_start_id(1, sid, path1);
  g_rxtx_worker = 1;
  rte_eal_remote_launch(rxtx_worker, NULL, g_running_lcore);
  while(g_rxtx_worker_active == 0)
    {
    usleep(10000);
    rte_pause();
    }
  usleep(100000);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_init(char *name)
{
  memset(g_name, 0, MAX_NAME_LEN);
  memset(g_wrk, 0, 2*sizeof(t_wrk));
  strncpy(g_name,  name, MAX_NAME_LEN-1);
  g_running_lcore = -1;
  g_rxtx_worker_active = 0;
}
/*--------------------------------------------------------------------------*/
