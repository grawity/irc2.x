/*************************************************************************
 ** r_bsd.c  Beta  v2.0    (22 Aug 1988)
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

char r_bsd_id[] = "r_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "struct.h"
#if BSD42 || ULTRIX
#include "sock.h"
#endif
#include "sys.h"

extern aClient me;
int
client_init(host, portnum)
char *host;
int portnum;
{
  int sock, tryagain = 1;
  static struct hostent *hp;
  static struct sockaddr_in server;

  gethostname(me.host,HOSTLEN);

  /* FIX:  jtrim@duorion.cair.du.edu -- 3/4/89 
     and jto@tolsun.oulu.fi -- 3/7/89 */

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
	  fprintf(stderr, "%s: unknown host", host);
	  exit(2);
	}
	bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
      }
    server.sin_port = htons(portnum);
    /* End Fix */
    
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
      perror("irc");
      exit(1);
    }
  return(sock);
}
/* End Fix */

client_loop(sock)
int sock;
{
  int i = 0, size;
  char apubuf[101], ch;
  fd_set ready;
  do {
    ready.fds_bits[0] = (1 << sock);
    if (select(sock+1, &ready, 0, 0, NULL) < 0) {
/*      perror("select"); */
      continue;
    }
    if (ready.fds_bits[0] & (1 << sock)) {
      if ((size = read(sock, apubuf, 100)) < 0)
	perror("receiving stream packet");
      if (size == 0) return(-1);
      dopacket(&me, apubuf, size);
    }
/*    if (ready.fds_bits[0] & 1) {
      if ((ch = getchar()) == -1) {
	move(0,0);
	addstr("\rFATAL ERROR: End of stdin file !\n\r");
	refresh();
	return;
      }
      i=do_char(ch);
    } */
  } while (1);
}
