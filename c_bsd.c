/*************************************************************************
 ** c_bsd.c  Beta  v2.0    (22 Aug 1988)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen 
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 ** 
 *************************************************************************/

char c_bsd_id[] = "c_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#if HPUX
#include <time.h>
#else
#include <sys/time.h>
#endif
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
#if BSD42 || ULTRIX || HPUX
#include "sock.h"
#endif
#include "sys.h"

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

#define WAITMESSAGE() { \
	    printf("Connection refused at your own host!\n"); \
	    printf("Rebooting IRCD Daemon and trying again....\n"); \
	    printf("Wait a moment...\n");\
} 


int
client_init(host, portnum)
char *host;
int portnum;
{
  int sock, tryagain = 1;
  static struct hostent *hp;
  static struct sockaddr_in server;

  gethostname(me.host,HOSTLEN);

  for (;;) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
      ErrorExit("opening stream socket",1);
    
    server.sin_family = AF_INET;
    
    if (isdigit(*host))
	server.sin_addr.s_addr = inet_addr(host);
    else { 
      if (!(hp = gethostbyname(host))) {
	fprintf(stderr, "%s: unknown host", host);
	exit(2);
      }
      bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
    }
    server.sin_port = htons(portnum);
    
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) >= 0)
      break; 

    if (!Equal(host, me.host) || tryagain != 1)   /* Is this MY host? */
       ErrorExit("irc",1);
    
    if (!fork()) {   /* - server is SUID/SGID to ME so it's my UID */
      execl(MYNAME, "ircd", (char *)0);
      exit(1);
    }
    WAITMESSAGE();
    close(sock);
    sleep(5);
    tryagain = 2;
  }
  return(sock);
}

client_loop(sock)
int sock;
{
  int i = 0, size, pos;
  char apubuf[STDINBUFSIZE+1];
  fd_set ready;
  do {
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
      if (size <= 0) return(-1);
      dopacket(&me, apubuf, size);
    }
#ifndef AUTOMATON
    if (ready.fds_bits[0] & 1) {
      if ((size = read(0, apubuf, STDINBUFSIZE)) < 0) {
	putline("FATAL ERROR: End of stdin file !");
	return;
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
