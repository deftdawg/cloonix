/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "podman.h"
#include "podmanimages.h"
#include "crun_utils.h"


/*--------------------------------------------------------------------------*/
typedef struct t_image_info
{
  char brandtype[MAX_NAME_LEN];
  char image_name[MAX_NAME_LEN];
  char image_id[MAX_NAME_LEN];
} t_image_info;
/*--------------------------------------------------------------------------*/
typedef struct t_container_info
{
  char brandtype[MAX_NAME_LEN];
  char image_name[MAX_NAME_LEN];
  char id[MAX_NAME_LEN];
  int pid;
} t_container_info;
/*--------------------------------------------------------------------------*/
static t_image_info g_image[MAX_IMAGE_TAB];
static int g_image_nb;
static t_container_info g_container[MAX_CONTAINER_TAB];
static int g_container_nb;
static int g_podman_ok;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void podman_podman_images(char *cmd, char *brandtype)
{
  int nb, child_pid, wstatus, result = 0;
  char resp[MAX_PATH_LEN];
  char it1[MAX_NAME_LEN];
  char it2[MAX_NAME_LEN];
  char it3[MAX_NAME_LEN];
  FILE *fp;
  char *ptr;
  fp = cmd_lio_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    while (result == 0)
      {
      if (!fgets(resp, MAX_PATH_LEN-1, fp))
        {
        result = -1;
        }
      else if (strlen(resp) == 0)
        {
        result = -1;
        KERR("%s", cmd);
        }
      else
        {
        ptr = strchr(resp, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(resp, '\n');
        if (ptr)
          *ptr = 0;
        if (strncmp(resp, "REPOSITORY", strlen("REPOSITORY")))
          {
          if (sscanf(resp, "%s %s %s", it1, it2, it3) != 3)
            KERR("ERROR %s", resp);
          else
            {
            nb = g_image_nb;
            g_image_nb += 1;
            strncpy(g_image[nb].brandtype, brandtype, MAX_NAME_LEN-1);
            strncpy(g_image[nb].image_name, it1, MAX_NAME_LEN-1);
            strncpy(g_image[nb].image_id, it3, MAX_NAME_LEN-1);
            }
          }
        }
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_container_pid(char *brandtype, char *id)
{
  int child_pid, wstatus, pid = 0;
  char cmd[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  FILE *fp;
  memset(cmd, 0, MAX_PATH_LEN);
  if (g_podman_ok)
    {
    snprintf(cmd, MAX_PATH_LEN-1,
             "%s inspect -f \'{{.State.Pid}}\' %s",
             PODMAN_BIN, id);
    fp = cmd_lio_popen(cmd, &child_pid);
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      if ((!fgets(resp, MAX_PATH_LEN-1, fp)) ||
          (strlen(resp) == 0))
        {
        KERR("%s", cmd);
        }
      else
        {
        if (sscanf(resp, "%d", &pid) != 1)
          KERR("ERROR %s", resp);
        }
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      }
    }
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void container_ls(char *cmd, char *brandtype)
{
  int nb, child_pid, wstatus, result = 0;
  char resp[MAX_PATH_LEN];
  char it1[MAX_NAME_LEN];
  char it2[MAX_NAME_LEN];
  FILE *fp;
  char *ptr;
  fp = cmd_lio_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    while (result == 0)
      {
      if (!fgets(resp, MAX_PATH_LEN-1, fp))
        {
        result = -1;
        }
      else if (strlen(resp) == 0)
        {
        result = -1;
        KERR("%s", cmd);
        }
      else
        {
        ptr = strchr(resp, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(resp, '\n');
        if (ptr)
          *ptr = 0;
        if (strncmp(resp, "CONTAINER", strlen("CONTAINER")))
          {
          if (sscanf(resp, "%s %s", it1, it2) != 2)
            KERR("ERROR %s", resp);
          else
            {
            ptr = strrchr(it2, ':');
            if (ptr)
              *ptr = 0;
            if (strlen(it2) == 0)
              KERR("ERROR %s", resp);
            else
              {
              nb = g_container_nb;
              g_container_nb += 1;
              strncpy(g_container[nb].brandtype, brandtype, MAX_NAME_LEN-1);
              strncpy(g_container[nb].id, it1, MAX_NAME_LEN-1);
              strncpy(g_container[nb].image_name,it2,MAX_NAME_LEN-1);
              g_container[nb].pid = get_container_pid(brandtype, it1);
              }
            }
          }
        }
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void dump_podman_images(void)
{
  char cmd[MAX_PATH_LEN];
  if (g_podman_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s images", PODMAN_BIN);
    podman_podman_images(cmd, "podman");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dump_container_ls(void)
{
  char cmd[MAX_PATH_LEN];
  if (g_podman_ok)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s ps", PODMAN_BIN);
    container_ls(cmd, "podman");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int podmanimages_container_name_already_used(char *brandtype, char *name)
{
  int child_pid, wstatus, result = 0;
  char cmd[MAX_PATH_LEN];
  char resp[MAX_PATH_LEN];
  char *ptr;
  FILE *fp;
  memset(cmd, 0, MAX_PATH_LEN);
  if (g_podman_ok)
    {
    snprintf(cmd, MAX_PATH_LEN-1, "%s ps | %s '{print $NF}'",
             PODMAN_BIN, AWK_BIN);
    fp = cmd_lio_popen(cmd, &child_pid);
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      while(fgets(resp, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(resp, '\r');
        if (ptr)
          *ptr = 0;
        ptr = strchr(resp, '\n');
        if (ptr)
          *ptr = 0;
        if (!strcmp(name, resp))
          {
          KERR("ERROR NAME USED %s", name);
          result = 1;
          }
        }
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR %s", name);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podmanimages_rebuild(void)
{
  g_image_nb = 0;
  g_container_nb = 0;
  memset(g_image, 0, MAX_IMAGE_TAB * sizeof(t_image_info));
  memset(g_container, 0, MAX_CONTAINER_TAB * sizeof(t_container_info));
  dump_podman_images();
  dump_container_ls();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podmanimages_update(int llid)
{
  int i;
  char resp[MAX_PATH_LEN];
  podmanimages_rebuild();
  snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_podman_resp_image_begin");
  rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
  for (i=0; i<g_image_nb; i++)
    {
    memset(resp, 0, MAX_PATH_LEN);
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_podman_resp_image_name brandtype %s name %s",
             g_image[i].brandtype, g_image[i].image_name);
    rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
    }
  snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_podman_resp_image_end");
  rpct_send_poldiag_msg(llid, type_hop_suid_power, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int podmanimages_get_pid(char *brandtype, char *image_name, char *id)
{
  int i, pid = 0;
  for (i=0; i<g_container_nb; i++)
    {
    if ((!strcmp(g_container[i].brandtype, brandtype)) &&
        (!strcmp(g_container[i].image_name, image_name)) &&
        (!strcmp(g_container[i].id, id)))
      pid = g_container[i].pid;
    }
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int podmanimages_exits(char *brandtype, char *name, char *image_id)
{
  int i, result = 0;
  for (i=0; i<g_image_nb; i++)
    {
    if ((!strcmp(g_image[i].brandtype, brandtype)) &&
        ((!strcmp(g_image[i].image_name, name)) ||
         (!strcmp(g_image[i].image_id, name))))
      {
      memset(image_id, 0, MAX_NAME_LEN);
      strncpy(image_id, g_image[i].image_id, MAX_NAME_LEN-1);
      result = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void podmanimages_init(void)
{
  g_podman_ok = 0;
  g_image_nb = 0;
  g_container_nb = 0;
  memset(g_image, 0, MAX_IMAGE_TAB * sizeof(t_image_info));
  memset(g_container, 0, MAX_CONTAINER_TAB * sizeof(t_container_info));
  if (!access(PODMAN_BIN, F_OK))
    g_podman_ok = 1;
}
/*--------------------------------------------------------------------------*/
