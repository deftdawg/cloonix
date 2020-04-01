/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h"
#include "bank_item.h"
#include "external_bank.h"

/****************************************************************************/
static t_bank_item *edge_does_exist(t_bank_item *bitem, t_bank_item *lan)
{
  t_bank_item *result = NULL;
  t_list_bank_item *cur = bitem->head_edge_list;
  while (cur)
    {
    if (cur->bitem->att_lan == lan )
      {
      if (result == NULL)
        result = cur->bitem;
      else
        KOUT(" ");
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_edge_create(char *name, int num, char *lan)
{
  t_bank_item *intf, *blan, *edge_item;
  t_bank_item *sat;
  intf = look_for_eth_with_id(name, num);
  sat = look_for_sat_with_id(name);
  blan = look_for_lan_with_id(lan);
  if (blan)
    {
    if (intf)
      {
      edge_item = edge_does_exist(intf, blan);
      if (!edge_item)
        add_new_edge(intf, blan, eorig_modif);
      }
    else if (sat)
      {
      edge_item = edge_does_exist(sat, blan);
      if (!edge_item)
        add_new_edge(sat, blan, eorig_modif);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_node_create(char *name, char *kernel, char *rootfs_used,
                      char *rootfs_backing, char *install_cdrom,
                      char *added_cdrom, char *added_disk,
                      int nb_dpdk,  int nb_eth, int nb_wlan,
                      int color_choice, int vm_id, int vm_config_flags,
                      double x, double y, int hidden_on_graph,
                      double *tx, double *ty, int32_t *thidden_on_graph)
{
  int i;
  add_new_node(name, kernel, rootfs_used, rootfs_backing, install_cdrom,
               added_cdrom, added_disk, x, y, hidden_on_graph, color_choice,
               vm_id, vm_config_flags, nb_dpdk, nb_eth);

  for (i=0; i<nb_dpdk; i++)
    add_new_eth(name,i,endp_type_kvm_dpdk,tx[i],ty[i], thidden_on_graph[i]);
  for (i=nb_dpdk; i<nb_dpdk+nb_eth; i++)
    add_new_eth(name,i,endp_type_kvm_eth, tx[i],ty[i], thidden_on_graph[i]);
  for (i=nb_dpdk+nb_eth; i<nb_dpdk+nb_eth+nb_wlan; i++)
    add_new_eth(name,i,endp_type_kvm_wlan,tx[i],ty[i], thidden_on_graph[i]);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void bank_sat_create(char *name, int mutype,
                     t_topo_c2c *c2c, t_topo_snf *snf,
                     double x, double y,
                     double xa, double ya,
                     double xb, double yb,
                     int hidden)
{
  t_bank_item *sat;
  sat = look_for_sat_with_id(name);
  if (sat)
    KERR("%s", name);
  else
    {
    add_new_sat(name, mutype, c2c, snf, x, y, hidden);
    if (mutype == endp_type_a2b)
      {
      add_new_eth(name, 0, endp_type_kvm_eth, xa, ya, hidden);
      add_new_eth(name, 1, endp_type_kvm_eth, xb, yb, hidden);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_lan_create(char *lan,  double x, double y, int hidden_on_graph)
{
  add_new_lan(lan, x, y, hidden_on_graph);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_edge_delete(char *name, int num, char *lan)
{
  t_bank_item *edge_item;
  t_bank_item *intf, *blan;
  t_bank_item *sat;
  intf = look_for_eth_with_id(name, num);
  sat = look_for_sat_with_id(name);
  blan  = look_for_lan_with_id(lan);
  if (blan)
    {
    if (intf)
      {
      edge_item = edge_does_exist(intf, blan);
      if (edge_item)
        {
        if (edge_item->att_eth != intf)
          KOUT(" ");
        if (edge_item->att_lan != blan)
          KOUT(" ");
        delete_bitem(edge_item);
        }
      }
    else if (sat)
      {
      edge_item = edge_does_exist(sat, blan);
      if (edge_item)
        {
        if (edge_item->att_lan != blan)
          KOUT(" ");
        delete_bitem(edge_item);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_node_delete(char *name)
{
  t_bank_item *node;
  node = look_for_node_with_id(name);
  if (node)
    {
    delete_bitem(node);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_sat_delete(char *name)
{
  t_bank_item *sat;
  sat = look_for_sat_with_id(name);
  if (sat)
    delete_bitem(sat);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_lan_delete(char *lan)
{
  t_bank_item *blan;
  blan = look_for_lan_with_id(lan);
  if (blan)
    {
    delete_bitem(blan);
    }
}
/*--------------------------------------------------------------------------*/
