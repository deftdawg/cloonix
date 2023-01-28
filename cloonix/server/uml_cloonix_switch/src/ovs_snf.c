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
#include "ovs_nat.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "ovs_snf.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "stats_counters.h"
#include "lan_to_name.h"

typedef struct t_snf
{
  char tap[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  int item_type;
  char vhost[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int send_mac_done;
  int watchdog_count;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  t_fct_cb callback_process_up;
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
static t_snf *find_snf_with_lan(char *lan)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_snf *find_snf_with_name_num(char *name, int num)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
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

/****************************************************************************/
static void update_snf_rx_mac(char *name, int llid)
{
  char msg[MAX_PATH_LEN];
  t_item_el *icur;
  t_item_mac *imac;
  char *mc;
  icur = lan_get_head_item(name);
  if (!icur)
    KERR("ERROR %s", name);
  else
    {
    snprintf(msg, MAX_PATH_LEN-1, "cloonsnf_mac_spyed_purge");
    rpct_send_sigdiag_msg(llid, type_hop_snf, msg);
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, msg);
    while(icur)
      {
      imac = icur->head_mac;
      while(imac)
        {
        mc = imac->mac;
        snprintf(msg, MAX_PATH_LEN-1, "cloonsnf_mac_spyed_rx_on=%s", mc);
        rpct_send_sigdiag_msg(llid, type_hop_snf, msg);
        hop_event_hook(llid, FLAG_HOP_SIGDIAG, msg);
        imac = imac->next;
        }
      icur = icur->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_mac_to_snf(t_snf *cur)
{
  uint8_t *mc;
  t_eth_table *eth_tab;
  t_vm *vm = cfg_get_vm(cur->name);
  int nb_eth;
  char mac_spyed_on[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ovs_phy *phy_exists = ovs_phy_exists(cur->name);
  t_ovs_tap *tap_exists = ovs_tap_exists(cur->name);
  t_ovs_nat *nat_exists = ovs_nat_exists(cur->name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(cur->name);
  t_ovs_c2c *c2c_exists = ovs_c2c_exists(cur->name);
  memset(msg, 0, MAX_PATH_LEN);
  if (nat_exists)
    {
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_CISCO);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_GW);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",NAT_MAC_DNS);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else if (tap_exists)
    {
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",tap_exists->mac);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else if (phy_exists)
    {
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",phy_exists->phy_mac);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else if (a2b_exists)
    {
    }
  else if (c2c_exists)
    {
    }
  else if (vm)
    {
    eth_tab = vm->kvm.eth_table;
    mc = (uint8_t *) eth_tab[cur->num].mac_addr;
    snprintf(mac_spyed_on, MAX_NAME_LEN-1,
             "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
             mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",mac_spyed_on);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else if (cnt_info(cur->name, &nb_eth, &eth_tab))
    {
    mc = (uint8_t *) eth_tab[cur->num].mac_addr;
    snprintf(mac_spyed_on, MAX_NAME_LEN-1,
             "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
             mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    snprintf(msg, MAX_PATH_LEN-1,"cloonsnf_mac_spyed_tx_on=%s",mac_spyed_on);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else
    KERR("ERROR %s", cur->name);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *tap)
{
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_kil_req(void *data)
{
  char *name = (char *) data;
  t_snf *cur = find_snf(name);
  if (!cur)
    KERR("ERROR %s", name); 
  else
    {
    rpct_send_kil_req(cur->llid, type_hop_snf);
    }
  clownix_free(data, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_start(char *tap, char *name_num)
{
  static char *argv[8];
  argv[0] = g_bin_snf;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = tap;
  argv[4] = name_num;
  argv[5] = NULL;
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
    else if (cur->send_mac_done == 0)
      {
      send_mac_to_snf(cur);
      cur->send_mac_done = 1;
      }
    else if (cur->watchdog_count >= 150)
      {
      KERR("ERROR %s %s %s", cur->name, cur->socket, cur->tap);
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
      if (cur->callback_process_up != NULL)
        cur->callback_process_up(cur->name, cur->num, cur->vhost);
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
void ovs_dyn_snf_stop_process(char *tap)
{
  t_snf *cur = find_snf(tap);
  char *copy_tap;
  if (!cur)
    KERR("ERROR %s", tap);
  else
    {
    copy_tap = (char *) clownix_malloc(MAX_NAME_LEN, 5);
    memset(copy_tap, 0, MAX_NAME_LEN);
    strncpy(copy_tap, tap, MAX_NAME_LEN-1);
    clownix_timeout_add(100, timer_kil_req, copy_tap, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_dyn_snf_start_process(char *name, int num, int item_type,
                               char *vhost, t_fct_cb cb)
{
  t_snf *cur;
  char *root = g_root_path;
  char name_num[MAX_NAME_LEN];
  char tap[MAX_NAME_LEN];
  char *tp = tap;

  memset(name_num, 0, MAX_NAME_LEN);
  snprintf(name_num, MAX_NAME_LEN-1, "%s_%d", name, num);
  memset(tap, 0, MAX_NAME_LEN);
  snprintf(tap, MAX_NAME_LEN-1, "s%s", vhost);
  cur = find_snf(tap);
  if (cur)
    {
    KERR("ERROR %s %d %s", name, num, vhost);
    ovs_dyn_snf_stop_process(tap);
    }
  else if ((item_type != item_type_keth) && (item_type != item_type_ceth) &&
           (item_type != item_type_tap) && (item_type != item_type_c2c) &&
           (item_type != item_type_nat) && (item_type != item_type_a2b) &&
           (item_type != item_type_phy))
    KERR("ERROR %s %d %s %d", name, num, vhost, item_type);
  else
    {
    cur = (t_snf *) malloc(sizeof(t_snf));
    memset(cur, 0, sizeof(t_snf));
    strncpy(cur->tap, tap, MAX_NAME_LEN-1);
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    cur->item_type = item_type;
    strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
    snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s/c%s", root, SNF_DIR, tp);
    cur->callback_process_up = cb;
    if (g_head_snf)
      g_head_snf->prev = cur;
    cur->next = g_head_snf;
    g_head_snf = cur;
    snf_start(tap, name_num);
    g_nb_snf += 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num)
{
  t_vm   *vm;
  int nb_eth, item_type;
  t_eth_table *eth_tab;
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  int found = 0;

  if (cnt_exists)
    {
    if ((num >=0 ) && (num < nb_eth))
      found = 1;
    item_type = item_type_ceth;
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
      item_type = item_type_keth;
      }
    }

  if (found == 0)
    KERR("ERROR %s %d %s", name, num, vhost);
  else if (is_ko)
    KERR("ERROR %s %d %s", name, num, vhost);
  else 
    {
    if (eth_tab[num].endp_type == endp_type_eths)
      ovs_dyn_snf_start_process(name, num, item_type, vhost, NULL);
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
void ovs_snf_c2c_update_mac(char *name)
{
  t_snf *cur = find_snf_with_name_num(name, 0);
  if (cur)
    update_snf_rx_mac(cur->lan, cur->llid);
  else
    KERR("WARNING NOT FOUND %s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_a2b_update_mac(char *name)
{
  t_snf *cur = find_snf_with_name_num(name, 0);
  if (cur)
    update_snf_rx_mac(cur->lan, cur->llid);
  else
    KERR("WARNING NOT FOUND %s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_snf_lan_mac_change(char *lan)
{
  t_snf *cur = find_snf_with_lan(lan);
  if (cur)
    {
    if ((cur->item_type==item_type_c2c)||(cur->item_type==item_type_a2b))
      {
      update_snf_rx_mac(lan, cur->llid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_snf_send_add_snf_lan(char *name, int num, char *vhost, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)  
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
  else
    {
    memset(cur->lan, 0, MAX_NAME_LEN);
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    }
  return(msg_send_add_snf_lan(name, num, vhost, lan));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_snf_send_del_snf_lan(char *name, int num, char *vhost, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)  
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
  else
    {
    memset(cur->lan, 0, MAX_NAME_LEN);
    }
  return(msg_send_del_snf_lan(name, num, vhost, lan));
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

