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
#define _GNU_SOURCE  
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <pty.h>
#include <pthread.h>
#include <dirent.h>


#include "mdl.h"
#include "wrap.h"
#include "debug.h"
#include "fd_spy.h"

static pthread_mutex_t g_mutex;
#define PROTECT_ZONE 20
/*****************************************************************************/
void *wrap_malloc(size_t size)
{
 // return malloc(size);
  int i;
  char *tmp;
  tmp = (char *) malloc(size + 3*PROTECT_ZONE + 4);
  if (!tmp)
    XOUT("%d", size);
  tmp[PROTECT_ZONE + 0] = ((size & 0xFF000000) >> 24) & 0xFF;   
  tmp[PROTECT_ZONE + 1] = ((size & 0x00FF0000) >> 16) & 0xFF;   
  tmp[PROTECT_ZONE + 2] = ((size & 0x0000FF00) >> 8) & 0xFF;   
  tmp[PROTECT_ZONE + 3] = ((size & 0x000000FF) >> 0) & 0xFF;   

  for (i=0; i<PROTECT_ZONE; i++)
    {
    tmp[i] = 0xAA;
    tmp[PROTECT_ZONE + 4 + i] = 0xAA;
    tmp[2*PROTECT_ZONE + 4 + size + i] = 0xAA;
    }

  return ((void *)(&(tmp[2*PROTECT_ZONE + 4])));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wrap_free(void *ptr, int line)
{
//  free(ptr);
  char *tmp;
  int i, j, size, idx;
  if (ptr)
    {
    tmp = ptr - 2*PROTECT_ZONE - 4;
    size = tmp[PROTECT_ZONE + 3] & 0xFF; 
    size += ((tmp[PROTECT_ZONE + 2] & 0xFF) << 8); 
    size += ((tmp[PROTECT_ZONE + 1] & 0xFF) << 16); 
    size += ((tmp[PROTECT_ZONE + 0] & 0xFF) << 24); 

    for (i=0; i<PROTECT_ZONE; i++)
      {
      idx = i;
      if ((tmp[idx] & 0xFF) != 0xAA)
        {
        XERR("%d", line);
        for (j=0; j < 8; j++)
          XERR("%d %X", idx+j, tmp[idx+j]&0xFF);
        XOUT("%d %d %X", size, idx, tmp[idx]&0xFF);
        }
      idx = PROTECT_ZONE + 4 + i;
      if ((tmp[idx] & 0xFF) != 0xAA)
        {
        XERR("%d", line);
        for (j=0; j < 8; j++)
          XERR("%d %X", idx+j, tmp[idx+j]&0xFF);
        XOUT("%d %d %X", size, idx, tmp[idx]&0xFF);
        }
      idx = 2*PROTECT_ZONE + 4 + size + i;
      if ((tmp[idx] & 0xFF) != 0xAA)
        {
        XERR("%d", line);
        for (j=0; j < 8; j++)
          XERR("%d %X", idx+j, tmp[idx+j]&0xFF);
        XOUT("%d %d %X", size, idx, tmp[idx]&0xFF);
        }
      }

    free(tmp);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_pty(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_pty, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_scp(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_scp, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_srv(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen;
  wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_srv, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_cli(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen;
  wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_cli, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_one(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_one, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static ssize_t write_wrwait(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen=0;
  int len_done, offst=0;

  if (len == 0)
    return 0; 
  do
    {
    len_done = write(s, buf+offst, len-offst);
    if (len_done < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        {
        XERR("%d %s", s, strerror(errno));
        wlen = -1;
        break;
        }
      else
        len_done = 0;
      }
    if (len_done == 0)
      usleep(5000);
    else
      offst += len_done;
    } while(offst != len);

  if (wlen == 0)
    wlen = offst;

  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_x11_wrwait_x11(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write_wrwait(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_x11_wr_x11, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_x11_wrwait_soc(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write_wrwait(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_x11_wr_soc, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_x11_x11(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_x11_wr_x11, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_x11_soc(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_x11_wr_soc, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_dialog_thread(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_cloon(int s, const void *buf, long unsigned int len)
{
  ssize_t wlen = write(s, buf, len);
  DEBUG_WRAP_READ_WRITE(0, s, wlen, fd_type_cloon, buf);
  return wlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_write_kout(int s, const void *buf, long unsigned int len)
{
  XOUT("%d", s);
  return 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
ssize_t wrap_read_cli(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_cli, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_srv(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_srv, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_x11_rd_x11(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_x11_rd_x11, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_x11_rd_soc(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_x11_rd_soc, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
ssize_t wrap_read_pty(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_pty, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_scp(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_scp, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_zero(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_zero, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_sig(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_sig, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_dialog_thread(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_cloon(int s, void *buf, size_t len)
{
  ssize_t rlen = read(s, buf, len);
  DEBUG_WRAP_READ_WRITE(1, s, rlen, fd_type_cloon, buf);
  return rlen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
ssize_t wrap_read_kout(int s, void *buf, size_t len)
{
  XOUT("%d %s %d", s, (char *)buf, len);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void wrap_sockoptset(int fd, const char *fct1, const char *fct2)
{
  int optval = 0, on=1;
  struct linger linger;
  socklen_t optlen = sizeof(optval);
  linger.l_onoff = 0;
  linger.l_linger = 0;
  if (setsockopt(fd,SOL_SOCKET,SO_LINGER,(char *)&linger,sizeof linger)<0)
    XOUT(" ");
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
    XOUT("setsockopt SO_REUSEADDR fd %d: %s", fd, strerror(errno));
  wrap_nonblock(fd);
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1)
    XOUT("getsockopt: %s", strerror(errno));
  if (optval != 0)
    XOUT("getsockopt: SO_ERROR %d", optval);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_accept(int fd_listen, int fd_type, int is_tcp, const char *fct)
{
  int fd, opt=1;

  fd = accept4(fd_listen, NULL, NULL, SOCK_CLOEXEC|SOCK_NONBLOCK);
  if (fd >= 0)
    {
    if (is_tcp)
      {
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
        XOUT(" ");
      }
    wrap_sockoptset(fd, __FUNCTION__, fct);
    if (fd_spy_add(fd, fd_type, __LINE__))
      {
      close(fd);
      fd = -1;
      }
    }
  return fd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_socketpair(int sockfd[2], int fd_type, const char *fct)
{
  int result = -1;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) == 0)
    {
    if((fd_spy_add(sockfd[0], fd_type, __LINE__)) ||
       (fd_spy_add(sockfd[1], fd_type, __LINE__)))
      {
      XERR("socket fd too big %s", fct);
      close(sockfd[0]);
      close(sockfd[1]);
      sockfd[0] = -1;
      sockfd[1] = -1;
      result = -1;
      }
    else
      {
      wrap_nonblock(sockfd[0]);
      fcntl(sockfd[0], F_SETFD, FD_CLOEXEC);
      wrap_nonblock(sockfd[1]);
      fcntl(sockfd[1], F_SETFD, FD_CLOEXEC);
      result = 0;
      }
    }
  else
    XOUT("socketpair error: %s %s", strerror(errno), fct);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_pipe(int pipefd[2], int fd_type, const char *fct)
{
  int result = -1;
  if (pipe(pipefd) == 0)
    { 
    if((fd_spy_add(pipefd[0], fd_type, __LINE__)) ||
       (fd_spy_add(pipefd[1], fd_type, __LINE__)))
      {
      XERR("pipe too big %s", fct);
      close(pipefd[0]);
      close(pipefd[1]);
      pipefd[0] = -1;
      pipefd[1] = -1;
      result = -1;
      }
    else
      {
      wrap_nonblock(pipefd[0]);
      fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
      wrap_nonblock(pipefd[1]);
      fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
      result = 0;
      }
    }
  else
    XOUT("pipe error: %s %s", strerror(errno), fct);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void wrap_pty_make_controlling_tty(int ttyfd, char *name)
{
  int fd;
  fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (fd >= 0)
     {
     (void) ioctl(fd, TIOCNOTTY, NULL);
     close(fd);
      }
  fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (fd >= 0)
    {
    XERR("Failed to disconnect from controlling tty.");
    close(fd);
    }
  if (ioctl(ttyfd, TIOCSCTTY, NULL) < 0)
    XERR("ioctl(TIOCSCTTY): %.100s", strerror(errno));
  fd = open(name, O_RDWR);
  if (fd < 0)
    XERR("%.100s: %.100s", name, strerror(errno));
  else
    close(fd);
  fd = open("/dev/tty", O_WRONLY);
  if (fd < 0)
    XERR("could not set controlling tty: %.100s", strerror(errno));
  else
   close(fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int wrap_openpty(int *amaster, int *aslave, char *name, int fd_type)
{
  int result;
  char *nm;
  result = openpty(amaster, aslave, name, NULL, NULL);
  if (result == -1)
    XERR("openpty error: %s", strerror(errno));
  else if (result == 0)
    {
    nm = ttyname(*aslave);
    if (!nm)
      {
      XERR("openpty returns device for which ttyname fails.");
      close(*amaster);
      close(*aslave);
      *amaster = -1;
      *aslave = -1;
      result == -1;
      }
    else 
      {
      memset(name, 0, MAX_TXT_LEN);
      strncpy(name, nm, MAX_TXT_LEN-1);
      if (fd_spy_add(*amaster, fd_type, __LINE__))
        XOUT(" ");
      if (fd_spy_add(*aslave, fd_type, __LINE__))
        XOUT(" ");
      } 
    } 
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_close(int fd, const char *fct)
{
  int result, srv_idx, cli_idx;
  int fd_type = fd_spy_get_type(fd, &srv_idx, &cli_idx);
  result = close(fd);
  if (result == 0)
    {
    if (fd_type < 0)
      XERR("Wrong: %d  %d srv_idx:%d cli_idx:%d  %s", 
           fd, fd_type, srv_idx, cli_idx, fct);
    fd_spy_del(fd);
    }
  else
    {
    if (fd_type > 0)
      XERR("Wrong: %d  %d srv_idx:%d cli_idx:%d  %s", 
            fd, fd_type, srv_idx, cli_idx, fct);
    XERR("close error fd:%d  %s %s srv_idx:%d cli_idx:%d",
          fd, strerror(errno), fct, srv_idx, cli_idx);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_socket_listen_inet(unsigned long ip, int port,
                            int fd_type, const char *fct)
{
  int s, opt=1;
  struct sockaddr_in isock;
  memset(&isock, 0, sizeof(isock));
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    XOUT("%s", strerror(errno));
  isock.sin_family = AF_INET;
  isock.sin_addr.s_addr = htonl(ip);
  isock.sin_port = htons(port);
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    XOUT(" ");
  if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
    XOUT(" ");
  if (bind(s, (struct sockaddr*)&isock, sizeof(isock)) < 0)
    XOUT("bind error: %s", strerror(errno));
  if (listen(s, 5) < 0)
    XOUT("listen error: %s", strerror(errno));
  if (fd_spy_add(s, fd_type, __LINE__))
    {
    XERR("too big %s", fct);
    close(s);
    s = -1;
    }
  else
    {
    wrap_nonblock(s);
    fcntl(s, F_SETFD, FD_CLOEXEC);
    }
  return s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_chmod_666(char *path)
{
  int result = chmod(path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  if (result)
    XERR("chmod fail %s", path);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_socket_listen_unix(char *path, int fd_type, const char *fct)
{
  int s;
  struct sockaddr_un sockun;
  unlink(path);
  s = socket(PF_UNIX, SOCK_STREAM, 0);
  if (s < 0)
    XOUT("%s", strerror(errno));
  sockun.sun_family = AF_UNIX;
  strcpy(sockun.sun_path, path);
  if (bind(s, (struct sockaddr*)&sockun, sizeof(sockun)) < 0)
    XOUT("bind error: %s", strerror(errno));
  if (listen(s, 5) < 0)
    XOUT("listen error: %s", strerror(errno));
  if (chmod(path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
    {
    XERR("chmod fail %s", fct);
    close(s);
    s = -1;
    }
  else if (fd_spy_add(s, fd_type, __LINE__))
    {
    XERR("too big %s", fct);
    close(s);
    s = -1;
    }
  else
    {
    wrap_nonblock(s);
    fcntl(s, F_SETFD, FD_CLOEXEC);
    }

  return s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_socket_connect_inet(unsigned long ip, int port,
                             int fd_type, const char *fct)
{
  int s, opt = 1, result = -1;
  struct sockaddr_in sockin;
  memset(&sockin, 0, sizeof(sockin));
  s = socket (AF_INET,SOCK_STREAM,0);
  if (s < 0)
    XOUT(" ");
  sockin.sin_family = AF_INET;
  sockin.sin_port = htons(port);
  sockin.sin_addr.s_addr = htonl(ip);
  if (connect(s, (struct sockaddr*)&sockin, sizeof(sockin)) == 0)
    {
    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0)
      XOUT(" ");
    wrap_sockoptset(s, __FUNCTION__, fct);
    if (fd_spy_add(s, fd_type, __LINE__))
      {
      XERR("too big %s", fct);
      close(s);
      }
    else
      {
      result = s;
      }
    }
  else
    XERR("Error connect %s\n", strerror(errno), fct);
  return result;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
int wrap_socket_connect_unix(char *path, int fd_type, const char *fct)
{
  int s, result = -1;
  struct sockaddr_un sockun;
  memset(&sockun, 0, sizeof(sockun));
  s = socket(PF_UNIX, SOCK_STREAM, 0);
  if (s < 0)
    XOUT(" ");
  sockun.sun_family = AF_UNIX;
  strcpy(sockun.sun_path, path);
  if (connect(s, (struct sockaddr*)&sockun, sizeof(sockun)) == 0)
    {
    wrap_sockoptset(s, __FUNCTION__, fct);
    if (fd_spy_add(s, fd_type, __LINE__))
      {
      XERR("too big %s", fct);
      close(s);
      }
    else
      {
      result = s;
      }
    }
  else
    XERR("Error connect unix %s %s %s", strerror(errno), path, fct);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_epoll_create(int fd_type, const char *fct)
{
  int result = epoll_create1(EPOLL_CLOEXEC);
  if (result < 0)
    XOUT(" ");
  if (fd_spy_add(result, fd_type, __LINE__))
    {
    XERR("too big %s", fct);
    close(result);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
struct epoll_event *wrap_epoll_event_alloc(int epfd, int fd, int id)
{
  struct epoll_event *epev;
  int len = sizeof(struct epoll_event);
  epev = (struct epoll_event *) wrap_malloc(len);
  memset(epev, 0, sizeof (struct epoll_event));
  epev->events = 0;
  epev->data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, epev))
    XOUT("%d", id);
  return(epev);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_epoll_event_free(int epfd, struct epoll_event *epev)
{
  epoll_ctl(epfd, EPOLL_CTL_DEL, epev->data.fd, NULL);
  wrap_free(epev, __LINE__);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void wrap_nonblock(int fd)
{
  int flags;
  flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    XOUT(" ");
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    XOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void wrap_nonnonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    XOUT(" ");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_mkdir(char *dst_dir)
{
  struct stat stat_file;
  if (mkdir(dst_dir, 0700))
    {
    if (errno != EEXIST)
      XOUT("%s, %s", dst_dir, strerror(errno));
    else
      {
      if (stat(dst_dir, &stat_file))
        XOUT("%s, %s", dst_dir, strerror(errno));
      if (!S_ISDIR(stat_file.st_mode))
        {
        XERR("%s", dst_dir);
        unlink(dst_dir);
        if (mkdir(dst_dir, 0700))
          XOUT("%s, %s", dst_dir, strerror(errno));
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_touch(char *path)
{
  FILE *fp;
  int result = -1;
  fp = fopen(path, "a");
  if (fp == NULL)
    XERR("%s", path);
  else
    {
    fclose(fp);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int wrap_file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void wrap_mutex_lock(void)
{
  pthread_mutex_lock(&g_mutex);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_mutex_unlock(void)
{
  pthread_mutex_unlock(&g_mutex);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void wrap_init(void)
{
  pthread_mutex_init(&g_mutex, NULL);
}
/*---------------------------------------------------------------------------*/
