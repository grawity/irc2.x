/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_sysv.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

char c_sysv_id[]="c_sysv.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <curses.h>
#include "struct.h"
#ifdef BSD42
#include "sock.h"
#endif

extern aClient me;
int
client_init(host, portnum)
char *host;
int portnum;
{
  int sock;
  static struct hostent *hp;
  static struct sockaddr_in server;
  sock = msgget(portnum, 0);
  if (sock < 0) {
    perror("opening stream socket");
    exit(1);
  }
  server.sin_family = AF_INET;
  gethostname(me.host,HOSTLEN);
  hp = gethostbyname(host);
  if (hp == 0) {
    fprintf(stderr, "%s: unknown host", host);
    exit(2);
  }
  bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
  server.sin_port = htons(portnum);
  if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("connect");
    exit(3);
  }
  return(sock);
}

client_loop(sock)
int sock;
{
  int i = 0, size;
  char apubuf[101], ch;
  fd_set ready;
  do {
    ready.fds_bits[0] = (1 << sock) | 1;
    move(LINES-1,i); refresh();
    if (select(sock+1, &ready, 0, 0, NULL) < 0) {
      perror("select");
      continue;
    }
    if (ready.fds_bits[0] & (1 << sock)) {
      if ((size = read(sock, apubuf, 100)) < 0)
	perror("receiving stream packet");
      if (size == 0) return(-1);
      dopacket(&me, apubuf, size);
    }
    if (ready.fds_bits[0] & 1) {
      if ((ch = getchar()) == -1) {
	move(0,0);
	addstr("\rFATAL ERROR: End of stdin file !\n\r");
	refresh();
	return;
      }
      i=do_char(ch);
    }
  } while (1);
}
