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
int  circle_nb(int id);
int  circle_peek(int id, uint64_t *len, uint64_t *usec, struct rte_mbuf **mbuf);
int circle_swap(int id, int pcap_nok);
struct rte_mbuf *circle_get(int id);
void circle_put(int id, uint64_t len, uint64_t usec, struct rte_mbuf *mbuf);
void circle_flush(int id);
void circle_clean(int id);
void circle_cnf(int type, uint8_t *mac);
void circle_init(void);
/*--------------------------------------------------------------------------*/