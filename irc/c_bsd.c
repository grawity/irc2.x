/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_bsd.c
 *   Copyright (C) 1990, Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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

char c_bsd_id[] = "c_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#ifndef AUTOMATON
#include <curses.h>
#endif
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "sock.h"	/* If FD_ZERO isn't defined up to this point, */
			/* define it (BSD4.2 needs this) */

#ifdef AUTOMATON
#ifdef DOCURSES
#undef DOCURSES
#endif
#ifdef DOTERMCAP
#undef DOTERMCAP
#endif
#endif /* AUTOMATON */

#define STDINBUFSIZE (0x80)

extern aClient me;
extern int termtype;
extern int QuitFlag;

int
client_init(host, portnum)
char *host;
int portnum;
{
  int sock, tryagain = 1;
  static struct hostent *hp;
  static struct sockaddr_in server;

  gethostname(me.sockhost,HOSTLEN);

  /* FIX:  jtrim@duorion.cair.du.edu -- 3/4/89 
     and jto@tolsun.oulu.fi -- 3/7/89 */

  while (tryagain) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      perror("opening stream socket");
      exit(1);
    }
    server.sin_family = AF_INET;
    
    /* MY FIX -- jtrim@duorion.cair.du.edu   (2/10/89) */
    if ( isdigit(*host))
      {
	server.sin_addr.s_addr = inet_addr(host);
      }
    else
      { 
	hp = gethostbyname(host);
	if (hp == 0) {
	  fprintf(stderr, "%s: unknown host\n", host);
	  exit(2);
	}
	bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
      }
    server.sin_port = htons(portnum);
    /* End Fix */
    
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
      if (StrEq(host, me.sockhost) && tryagain == 1)  /* Is this MY host? */
	{
	  if (fork() == 0)   /* - server is SUID/SGID to ME so it's my UID */
	    {
	      execl(MYNAME, "ircd", (char *)0);
	      exit(1);
	    }
	  printf("Connection refused at your own host!\n");
	  printf("Rebooting IRCD Daemon and trying again....\n");
	  printf("Wait a moment...\n");
	  close(sock);
	  sock = -1;
	  sleep(5);
	  tryagain = 2;
	} else {
	  perror("irc");
	  exit(1);
	}
    } else {
      tryagain = 0;
    }
  }
  return(sock);
}
/* End Fix */

client_loop(sock)
int sock;
{
  int i = 0, size, pos;
  char apubuf[STDINBUFSIZE+1], ch;
  fd_set ready;
  do {
    if (sock < 0 || QuitFlag)
      return(-1);
    ready.fds_bits[0] = (1 << sock) | 1;
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      move(LINES-1,i); refresh();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM)
      move (-1, i);
#endif
    if (select(sock+1, &ready, 0, 0, NULL) < 0) {
/*      perror("select"); */
      continue;
    }
    if (ready.fds_bits[0] & (1 << sock)) {
      if ((size = read(sock, apubuf, STDINBUFSIZE)) < 0)
	perror("receiving stream packet");
      if (size <= 0) {
	close(sock);
	return(-1);
      }
      dopacket(&me, apubuf, size);
    }
#ifndef AUTOMATON
    if (ready.fds_bits[0] & 1) {
      if ((size = read(0, apubuf, STDINBUFSIZE)) < 0) {
	putline("FATAL ERROR: End of stdin file !");
	return -1;
      }
      for (pos = 0; pos < size; pos++) {
	i=do_char(apubuf[pos]);
#ifdef DOCURSES
	if (termtype == CURSES_TERM)
	  move(LINES-1, i);
#endif
#ifdef DOTERMCAP
	if (termtype == CURSES_TERM)
	  move(-1, i);
#endif
      }
    }
#endif /* AUTOMATON */
  } while (1);
}
