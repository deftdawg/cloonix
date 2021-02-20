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
void move_manager_update_item(t_bank_item *bitem);
void move_manager_translate(t_bank_item *bitem, double x1, double y1);
void move_manager_rotate(t_bank_item *bitem, double x1, double y1);
void move_manager_single_step(void);
void move_init(void);
void move_single_step (t_bank_item *bitem);
void move_single_step_eth (t_bank_item *bitem);
void move_bitem_eth (t_bank_item *bitem, double acc_x, double acc_y);
void modif_position_generic(t_bank_item *bitem, double xi, double yi);
void modif_position_layout(t_bank_item *bitem, double xi, double yi);
void modif_position_eth_layout(t_bank_item *bitem, double xi, double yi);
void topo_repaint_request(void);








