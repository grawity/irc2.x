/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_vms.c
 *   Copyright (C) 1990 University of Oulu, Computing Center
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

/* -- Jto -- 25 May 1990
 * VMS version changes from Ladybug (viljanen@cs.helsinki.fi)
 */

char c_vms_id[] = "c_vms.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"

#if VMS
#include <vms/inetiodef.h>
#include iodef
#include ssdef
#include descrip

typedef struct {
	short cond_value;
	short count;
	int info;
} io_statblk;

#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <file.h>
#include <curses.h>
#include "common.h"
#include "sys.h"
#include "sock.h"	/* If FD_ZERO isn't defined up to this point, */
			/* define it (BSD4.2 needs this) */

#define STDINBUFSIZE (0x80)

extern aClient me;
int goodbye;

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
/*	    if (fork() == 0) */  /* - server is SUID/SGID to ME so it's my UID */
	      {
		/* execl(MYNAME, "ircd", (char *)0); */
		printf("Sorry, I can't start a server up\n");
		exit(1);
	      } 
	    printf("Connection refused at your own host!\n");
	    printf("Rebooting IRCD Daemon and trying again....\n");
	    printf("Wait a moment...\n");
	    netclose(sock);
	    sleep(5);
	    tryagain = 2;
	  } else {
	    perror("irc");
	    exit(1);
	  }
      } else
	tryagain = 0;
    }
  return(sock);
}
/* End Fix */

client_loop(sock)
int sock;
{
  int i = 0, size, pos, x;
  char apubuf[STDINBUFSIZE+1], ch;
  struct timeval timeout = { 0, 0 };
  fd_set ready;
  short chan;
  int status;
  io_statblk status_block;

  struct {
	long part1;
	long part2;
  } terminator = { 0, 0};

  $DESCRIPTOR(terminal, "SYS$COMMAND");

  if (((status = SYS$ASSIGN(&terminal, &chan, 0, 0)) & 1 ) != 1)
        LIB$STOP(status);

  goodbye = 0;
  
  do {
    for (x=0; x <= 64; x++) ready.fds_bits[x]=0;
    ready.fds_bits[sock/32] |= 1 << (sock % 32);
    move(LINES-1,i); 
    if (goodbye == 1) return(-1);
    
    while (select(1, &ready, 0, 0, &timeout) > 0) {
      if ((ready.fds_bits[sock/32] & (1 << (sock % 32))) != 0) {
         if ((size = netread(sock, apubuf, STDINBUFSIZE)) < 0)
              perror("receiving stream packet");
         dopacket(&me, apubuf, size);
	 move(LINES-1, i);
         refresh();
      }
    }

    if (((status = SYS$QIOW(1, chan, IO$_READVBLK | IO$M_NOFILTR | IO$M_NOECHO
	| IO$M_TIMED, &status_block, 0, 0, &apubuf, 1, 1, &terminator, 0, 0))
	& 1 ) != 1)
          LIB$STOP(status);
/*
    if (((status = SYS$SYNCH( 1, &status_block)) & 1) != 1)
          LIB$STOP(status);
 */
    if ((status_block.cond_value != SS$_TIMEOUT) &&
    	(status_block.cond_value != SS$_NORMAL))
         LIB$STOP(status);

    if (status_block.count > 0 ) {
       i=do_char(apubuf[0]);
       move(LINES-1, i);
    }
/*
    apubuf[status_block.count] = '\0';
    
    if (status_block.count > 0 )
      for (pos = 0; pos < status_block.count; pos++) {
	if (apubuf[pos] == '\r') apubuf[pos] = '\n';
	i=do_char(apubuf[pos]);
	move(LINES-1, i);
      } 
*/
  } while (1);
}
