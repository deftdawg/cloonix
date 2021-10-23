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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "cfg_store.h"
#include "lan_to_name.h"
#include "layout_topo.h"
#include "dpdk_xyx.h"
#include "dpdk_a2b.h"
#include "dpdk_d2d.h"
#include "dpdk_ovs.h"



/*****************************************************************************/
static int can_increment_index(int val)
{
  int result = 1;
  if (val >= MAX_LIST_COMMANDS_QTY)
    {
    KERR("TOO MANY COMMANDS IN LIST");
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/***************************************************************************/
static void eth_tab_to_str(char *out, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  memset(out, 0, MAX_NAME_LEN);
  for (i=0 ; i < nb_tot_eth; i++)
    {
    if (i > MAX_DPDK_VM)
      KOUT("%d", nb_tot_eth); 
    if(eth_tab[i].eth_type == endp_type_ethd)
      out[i] = 'd';
    else if(eth_tab[i].eth_type == endp_type_eths)
      out[i] = 's';
    else
      KOUT("%d %d", i, nb_tot_eth); 
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_vm_cmd(int offset, t_list_commands *hlist, 
                            t_topo_kvm *kvm)
{
  int i, len = 0;
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  char eth_desc[MAX_NAME_LEN];
  char *mc;
  if (can_increment_index(result))
    {
    eth_tab_to_str(eth_desc, kvm->nb_tot_eth, kvm->eth_table);
    len += sprintf(list->cmd + len, 
           "cloonix_cli %s add kvm %s ram=%d cpu=%d eth=%s",
            cfg_get_cloonix_name(), kvm->name, kvm->mem, kvm->cpu, eth_desc);
    len += sprintf(list->cmd + len, " %s", kvm->rootfs_input);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      len += sprintf(list->cmd + len, " --persistent");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(list->cmd + len, " --fullvirt");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
      len += sprintf(list->cmd + len, " --no_reboot");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_WITH_PXE)
      len += sprintf(list->cmd + len, " --with_pxe");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_BALLOONING)
      len += sprintf(list->cmd + len, " --balloon");
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_9P_SHARED)
      len += sprintf(list->cmd + len, " --9p_share=%s", kvm->p9_host_share);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
      len += sprintf(list->cmd + len, " --install_cdrom=%s",
                                         kvm->install_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
      len += sprintf(list->cmd + len, " --added_cdrom=%s",
                                         kvm->added_cdrom);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      len += sprintf(list->cmd + len, " --added_disk=%s",
                                         kvm->added_disk);
    for (i=0; i < kvm->nb_tot_eth; i++)
      {
      if (kvm->eth_table[i].randmac == 0)
        {
        mc = kvm->eth_table[i].mac_addr;
	len += sprintf(list->cmd + len,
        " --mac_addr=eth%d:%02x:%02x:%02x:%02x:%02x:%02x", i,
                           mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                           mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
        }
      }
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_a2b_cmd(int offset, t_list_commands *hlist, char *name)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add a2b %s",
            cfg_get_cloonix_name(), name);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_add_d2d_cmd(int offset, t_list_commands *hlist,
                             char *name, char *dist_cloonix)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add d2d %s %s",
            cfg_get_cloonix_name(), name, dist_cloonix);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int build_add_sat_lan_cmd(int offset, t_list_commands *hlist, 
                                 char *name, int num, char *lan)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s add lan %s %d %s", 
                        cfg_get_cloonix_name(), name, num, lan);
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_stop_go_cmd(int offset, t_list_commands *hlist, int go)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (can_increment_index(result))
    {
    if (go)
      sprintf(list->cmd, "cloonix_cli %s cnf lay go", cfg_get_cloonix_name());
    else
      sprintf(list->cmd, "cloonix_cli %s cnf lay stop", cfg_get_cloonix_name());
    result += 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_width_height_cmd(int offset, t_list_commands *hlist, 
                                  int width, int height)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (width && height)
    {
    if (can_increment_index(result))
      {
      sprintf(list->cmd, "cloonix_cli %s cnf lay width_height %d %d", 
                         cfg_get_cloonix_name(), width, height);
      result += 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_center_scale_cmd(int offset, t_list_commands *hlist,
                                  int cx, int cy, int cw, int ch)
{
  int result = offset;
  t_list_commands *list = &(hlist[offset]);
  if (cw && ch)
    {
    if (can_increment_index(result))
      {
      sprintf(list->cmd, "cloonix_cli %s cnf lay scale %d %d %d %d", 
                         cfg_get_cloonix_name(), cx, cy, cw, ch);
      result += 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_eth(int offset, t_list_commands *hlist, 
                            char *name, int num, t_layout_eth_wlan *eth_wlan)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_eth %s %d %d",
                        cfg_get_cloonix_name(), name, num, 
                        layout_node_solve(eth_wlan->x, eth_wlan->y));
    result += 1;
    if (eth_wlan->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_eth %s %d 1", 
                           cfg_get_cloonix_name(), name, num);
        result += 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_node(int offset, t_list_commands *hlist, 
                             t_layout_node *node)
{
  int i, result = offset;
  t_list_commands *list = &(hlist[result]);

  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_kvm %s %d %d",
                       cfg_get_cloonix_name(), node->name, 
                       (int) node->x, (int) node->y);
    result += 1;
    if (node->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_kvm %s 1", 
                           cfg_get_cloonix_name(), node->name);
        result += 1;
        }
      }
    }
  for (i=0; i < node->nb_eth_wlan; i++)
    result = build_layout_eth(result, hlist, node->name, i, &(node->eth_wlan[i])); 
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_sat(int offset, t_list_commands *hlist, 
                            t_layout_sat *sat)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_sat %s %d %d",
                       cfg_get_cloonix_name(), sat->name, 
                       (int) sat->x, (int) sat->y);
    result += 1;
    if (sat->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_sat %s 1", 
                           cfg_get_cloonix_name(), sat->name);
        result += 1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int build_layout_lan(int offset, t_list_commands *hlist, 
                            t_layout_lan *lan)
{
  int result = offset;
  t_list_commands *list = &(hlist[result]);
  if (can_increment_index(result))
    {
    sprintf(list->cmd, "cloonix_cli %s cnf lay abs_xy_lan %s %d %d",
                       cfg_get_cloonix_name(), lan->name, 
                       (int) lan->x, (int) lan->y);
    result += 1;
    if (lan->hidden_on_graph)
      {
      if (can_increment_index(result))
        {
        list = &(hlist[result]);
        sprintf(list->cmd, "cloonix_cli %s cnf lay hide_lan %s 1", 
                           cfg_get_cloonix_name(), lan->name);
        result += 1;
        }
      }
   }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_vm_cmd(int offset, t_list_commands *hlist,
                               int nb_vm, t_vm *vm) 
{
  int i, result = offset;
  for (i=0; i<nb_vm; i++)
    {
    result = build_add_vm_cmd(result, hlist, &(vm->kvm));
    vm = vm->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_ovs_lan_cmd(int offset, t_list_commands *hlist)
{
  int nb_endp_a2b, nb_endp_d2d, nb_endp_xyx;
  int  i, j, result=offset;
  t_topo_endp *xyx_endp = translate_topo_endp_xyx(&nb_endp_xyx);
  t_topo_endp *a2b_endp = translate_topo_endp_a2b(&nb_endp_a2b);
  t_topo_endp *d2d_endp = translate_topo_endp_d2d(&nb_endp_d2d);

  for (i=0; i<nb_endp_xyx; i++)
    {
    for (j=0; j<xyx_endp[i].lan.nb_lan; j++)
      {
      result = build_add_sat_lan_cmd(result, hlist,
                                     xyx_endp[i].name,
                                     xyx_endp[i].num,
                                     xyx_endp[i].lan.lan[j].lan);
      }
    }

  for (i=0; i<nb_endp_a2b; i++)
    {
    for (j=0; j<a2b_endp[i].lan.nb_lan; j++)
      {
      result = build_add_sat_lan_cmd(result, hlist,
                                     a2b_endp[i].name,
                                     a2b_endp[i].num,
                                     a2b_endp[i].lan.lan[j].lan);
      }
    }
  for (i=0; i<nb_endp_d2d; i++)
    {
    for (j=0; j<d2d_endp[i].lan.nb_lan; j++)
      {
      result = build_add_sat_lan_cmd(result, hlist,
                                     d2d_endp[i].name,
                                     d2d_endp[i].num,
                                     d2d_endp[i].lan.lan[j].lan);
      }
    }
  clownix_free(xyx_endp, __FUNCTION__);
  clownix_free(a2b_endp, __FUNCTION__);
  clownix_free(d2d_endp, __FUNCTION__);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_canvas_layout_cmd(int offset, t_list_commands *hlist, 
                                          int go, int width, int height, 
                                          int cx, int cy, int cw, int ch)
{
  int result = offset;
  result = build_stop_go_cmd(result, hlist, go);
  result = build_width_height_cmd(result, hlist, width, height);
  result = build_center_scale_cmd(result, hlist, cx, cy, cw, ch);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_node_cmd(int offset, t_list_commands *hlist,
                                        t_layout_node_xml *node_xml)
{
  int result = offset;
  t_layout_node_xml *cur = node_xml;
  while(cur)
    {
    result = build_layout_node(result, hlist, &(cur->node));
    cur = cur->next;
    }
  return result;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_sat_cmd(int offset, t_list_commands *hlist,
                                       t_layout_sat_xml *sat_xml)    
{
  int result = offset;
  t_layout_sat_xml *cur = sat_xml;
  while(cur)
    {
    result = build_layout_sat(result, hlist, &(cur->sat));
    cur = cur->next;
    }
  return result;

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_layout_lan_cmd(int offset, t_list_commands *hlist,
                                       t_layout_lan_xml *lan_xml)    
{
  int result = offset;
  t_layout_lan_xml *cur = lan_xml;
  while(cur)
    {
    result = build_layout_lan(result, hlist, &(cur->lan));
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int produce_list_sat_cmd(int offset, t_list_commands *hlist)
{
  int i, result = offset;
  int nb_d2d, nb_a2b;
  t_d2d_cnx *d2d = dpdk_d2d_get_first(&nb_d2d);
  t_a2b_cnx *a2b = dpdk_a2b_get_first(&nb_a2b);

  for (i=0; i<nb_a2b; i++)
    {
    result = build_add_a2b_cmd(result, hlist, a2b->name);
    a2b = a2b->next;
    }

  for (i=0; i<nb_d2d; i++)
    {
    if (d2d->local_is_master)
      result = build_add_d2d_cmd(result, hlist, d2d->name, d2d->dist_cloonix);
    d2d = d2d->next;
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int produce_list_commands(t_list_commands *hlist, int is_layout)
{
  int nb_vm, result = 0;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  int go, width, height, cx, cy, cw, ch;
  t_layout_xml *layout_xml;

  if (is_layout == 0)
    {
    result = produce_list_vm_cmd(result, hlist, nb_vm, vm); 
    result = produce_list_sat_cmd(result, hlist);
    result = produce_list_ovs_lan_cmd(result, hlist);
    }
  else
    {
    get_layout_main_params(&go, &width, &height, &cx, &cy, &cw, &ch);
    layout_xml = get_layout_xml_chain();
    result = produce_list_layout_node_cmd(result, hlist, layout_xml->node_xml);
    result = produce_list_layout_sat_cmd(result, hlist, layout_xml->sat_xml);
    result = produce_list_layout_lan_cmd(result, hlist, layout_xml->lan_xml);
    result = produce_list_canvas_layout_cmd(result, hlist, go, width, height, 
                                                              cx, cy, cw, ch);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


