/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "ovs_snf.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "stats_counters.h"

typedef struct t_snf
{
  char tap[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  char vhost[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  struct t_snf *prev;
  struct t_snf *next;
} t_snf;

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_snf[MAX_PATH_LEN];
static t_snf *g_head_snf;
static int g_nb_snf;

int get_glob_req_self_destruction(void);

/****************************************************************************/
int ovs_snf_get_qty(void)
{
  return (g_nb_snf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_snf *find_snf(char *tap)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if (!strcmp(cur->tap, tap))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_snf(t_snf *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_snf)
    g_head_snf = cur->next;
  g_nb_snf -= 1;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *tap)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_start(char *tap, char *mac, char *name_num)
{
  static char *argv[8];
  argv[0] = g_bin_snf;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = tap;
  argv[4] = mac;
  argv[5] = name_num;
  argv[6] = NULL;
  pid_clone_launch(utils_execve, process_demonized, NULL,
                   (void *) argv, NULL, NULL, tap, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_connect(char *socket, char *tap)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, tap);
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_snf, tap, 0))
      KERR(" ");
    llid_trace_alloc(llid, tap, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_snf, tap, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_snf *next, *cur = g_head_snf;
  int llid;
  char *msg = "cloonsnf_suidroot";
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    cur->watchdog_count += 1;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->tap);
      if (llid)
        {
        cur->llid = llid;
        cur->count = 0;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 50)
          {
          KERR("%s %s", cur->socket, cur->tap);
          free_snf(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      cur->count += 1;
      if (cur->count == 50)
        {
        KERR("%s %s", cur->socket, cur->tap);
        free_snf(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
      free_snf(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(cur->llid, type_hop_snf, cur->tap, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          free_snf(cur);
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_pid_resp(int llid, char *tap, int pid)
{
  t_snf *cur = find_snf(tap);
  if (!cur)
    KERR("%s %d", tap, pid);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      cur->pid = pid;
      }
    else if (cur->pid && (cur->pid != pid))
      {
      KERR("ERROR %s %d", tap, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_snf_diag_llid(int llid)
{
  t_snf *cur = g_head_snf;
  int result = 0;
  while(cur)
    {
    if (cur->llid == llid)
      {
      result = 1;
      break; 
      } 
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_snf_collect_snf(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_snf *cur = g_head_snf;
  while(cur)
    {
    strncpy(eventfull[result].name, cur->name, MAX_NAME_LEN-1);
    eventfull[result].type = endp_type_eths;
    eventfull[result].num  = cur->num;
    eventfull[result].ms   = cur->ms;
    eventfull[result].ptx  = cur->pkt_tx;
    eventfull[result].prx  = cur->pkt_rx;
    eventfull[result].btx  = cur->byte_tx;
    eventfull[result].brx  = cur->byte_rx;
    cur->pkt_tx  = 0;
    cur->pkt_rx  = 0;
    cur->byte_tx = 0;
    cur->byte_rx = 0;
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_poldiag_resp(int llid, int tid, char *line)
{
  t_snf *cur;
  char tap[MAX_NAME_LEN];
  int ms, ptx, btx, prx, brx;

  if (sscanf(line, "endp_eventfull_tx_rx %s %d %d %d %d %d",
             tap, &ms, &ptx, &btx, &prx, &brx) != 6)
    KERR("ERROR %s", line);
  else
    { 
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR %s", line);
    else
      {
      cur->ms      = ms;
      cur->pkt_tx  += ptx;
      cur->pkt_rx  += prx;
      cur->byte_tx += btx;
      cur->byte_rx += brx;
      stats_counters_update_endp_tx_rx(cur->name, cur->num,
                                       ms, ptx, btx, prx, brx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_sigdiag_resp(int llid, int tid, char *line)
{
  char tap[MAX_NAME_LEN];
  t_snf *cur;
  if (sscanf(line,
  "cloonsnf_suidroot_ko %s", tap) == 1)
    {
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR snf: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started snf: %s %s", g_cloonix_net, tap);
    }
  else if (sscanf(line,
  "cloonsnf_suidroot_ok %s", tap) == 1)
    {
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR snf: %s %s", g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }
  else
    KERR("ERROR snf: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_start_stop_process(char *name, int num, char *vhost,
                                char *tap, int on, char *mac)
{
  t_snf *cur = find_snf(tap);
  char *root = g_root_path;
  char name_num[MAX_NAME_LEN];
  if (on)
    {
    if (cur)
      KERR("ERROR %s %d %s %s", name, num, vhost, tap);
    else
      {
      memset(name_num, 0, MAX_NAME_LEN);
      snprintf(name_num, MAX_NAME_LEN-1, "%s_%d", name, num);
      cur = (t_snf *) malloc(sizeof(t_snf));
      memset(cur, 0, sizeof(t_snf));
      strncpy(cur->tap, tap, MAX_NAME_LEN-1);
      strncpy(cur->name, name, MAX_NAME_LEN-1);
      cur->num = num;
      strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
      snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s/c%s", root, SNF_DIR, tap);
      if (g_head_snf)
        g_head_snf->prev = cur;
      cur->next = g_head_snf;
      g_head_snf = cur; 
      snf_start(tap, mac, name_num);
      g_nb_snf += 1;
      }
    }
  else
    {
    if (!cur)
      KERR("ERROR %s %d %s %s", name, num, vhost, tap);
    else
      rpct_send_kil_req(cur->llid, type_hop_snf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_snf_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_snf *cur = g_head_snf;
  int i, result = 0;
  event_print("%s", __FUNCTION__);
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),5);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_snf;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->tap, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
        i++;
        }
      cur = cur->next;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_llid_closed(int llid)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if (llid && (cur->llid == llid))
      {
      hop_event_free(cur->llid);
      cur->closed_count = 2;
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num)
{
  char tap[MAX_NAME_LEN];
  uint8_t *mc;
  t_vm   *vm;
  int nb_eth;
  t_eth_table *eth_tab;
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  int found = 0;
  char mac_spyed_on[MAX_NAME_LEN];

  if (cnt_exists)
    {
    if ((num >=0 ) && (num < nb_eth))
      found = 1;
    }
  else
    {
    vm = cfg_get_vm(name);
    if (vm)
      { 
      found = 1;
      nb_eth = vm->kvm.nb_tot_eth;
      eth_tab = vm->kvm.eth_table; 
      if ((num >=0 ) && (num < nb_eth))
        found = 1;
      }
    }

  if (found == 0)
    KERR("ERROR %s %d %s", name, num, vhost);
  else if (is_ko)
    KERR("ERROR %s %d %s", name, num, vhost);
  else 
    {
    if (eth_tab[num].endp_type == endp_type_eths)
      {
      memset(mac_spyed_on, 0, MAX_NAME_LEN);
      mc = (uint8_t *) eth_tab[num].mac_addr;
      snprintf(mac_spyed_on, MAX_NAME_LEN-1,
               "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
      memset(tap, 0, MAX_NAME_LEN);
      snprintf(tap, MAX_NAME_LEN-1, "s%s", vhost);
      ovs_snf_start_stop_process(name, num, vhost, tap, 1, mac_spyed_on);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_resp_msg_add_lan(int is_ko, char *vhost, char *name, int num, char *lan)
{
  if (is_ko)
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_resp_msg_del_lan(int is_ko, char *vhost, char *name, int num, char *lan)
{
  if (is_ko)
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_snf = utils_get_ovs_snf_bin_dir();


  g_nb_snf = 0;
  g_head_snf = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_snf, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_snf, bin_snf, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
