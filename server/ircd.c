/*************************************************************************
 ** ircd.c  Beta  v2.0    (22 Aug 1988)
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

char ircd_id[] = "ircd.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <signal.h>
#include "struct.h"

#define TRUE 1

extern int dummy();
extern struct Client *make_client();
struct Client me;                /* That's me */
struct Client *client = &me;    /* Pointer to beginning of Client list */
extern struct Confitem *conf;
extern struct Confitem *find_me();
char *uphost;
char **myargv;
int portnum;
int debuglevel;
int maxusersperchannel = MAXUSERSPERCHANNEL;

#ifndef TTYON
#define START_FD 0
#else
#define START_FD 3
#endif

restart()
{
  int i;
  debug(DEBUG_NOTICE,"Restarting server...");

  for (i=START_FD; i<MAXFD; i++) 
    close(i);
  execv(MYNAME, myargv);
  debug(DEBUG_FATAL,"Couldn't restart server !!!!!!!!");
  exit(-1);
}

main(argc, argv)
int argc;
char *argv[];
{
  struct Client *cptr;
  struct Confitem *aconf;
  int length;                      /* Length of message received from client */
  char buffer[BUFSIZE];
  int counter;
  long lasttime, lastconnecttry;
  myargv = argv;
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, restart);
  signal(SIGALRM, dummy);
  setuid(geteuid());
  if (getuid() == 0) {
    setgid(-2);
    setuid(-2);
  } 
  getmyname(myhostname,HOSTLEN-1);
  portnum = PORTNUM;
  debuglevel = -1;
  while (argc > 1 && argv[1][0] == '-') {
    switch (argv[1][1]) {
      case 'h':
        strncpy(myhostname, &argv[1][2], HOSTLEN-1);
        myhostname[HOSTLEN] = '\0';
        break;
      case 'p':
        if ((length = atoi(&argv[1][2])) > 0 )
          portnum = length;
        break;
      case 'x':
        debuglevel = atoi(&argv[1][2]);
        break;
/*      case 'c':
	maxusersperchannel = atoi(&argv[1][2]);
	break; */
      default:
	printf("Illegal option\n");
	break;
    }
    argv++; argc--;
  }
  if (argc > 1 && argv[1][0] > '9')
    uphost = argv[1];
  else
    uphost = NULL;
  init_sys();
  OpenLog();
  initconf();
  if (aconf = find_me())
    strcpy(me.server, aconf->name);
  else
    me.server[0] = '\0';
  me.next = NULL;
  strcpy(me.host,myhostname);
  me.fromhost[0] = me.nickname[0] = me.realname[0] = '\0';
  me.fd = open_port(portnum);
  me.status = STAT_ME;
  lastconnecttry = getlongtime();
  aconf = conf;
  while (aconf) {
    if (aconf->status == CONF_CONNECT_SERVER && aconf->port > 0) {
      if (connect_server(aconf) == 0)
	break;
    }
    aconf = aconf->next;
  }
  debug(DEBUG_DEBUG,"Server ready...");
  
  for (;;) {
    if ((length = read_msg(buffer, BUFSIZE, &cptr)) > 0) {
      cptr->lasttime = getlongtime();
      cptr->flags &= ~FLAGS_PINGSENT;
      dopacket(cptr, buffer, length);
    } 

    debug(DEBUG_DEBUG,"Got message");

    lasttime = getlongtime();
    if (lasttime - lastconnecttry > CONNECTFREQUENCY) {
      lastconnecttry = lasttime;
      aconf = conf;
      while (aconf) {
	if (aconf->status == CONF_CONNECT_SERVER) {
	  cptr = client;
	  while (cptr) {
	    if ((cptr->status == STAT_SERVER || cptr->status == STAT_ME) &&
		myncmp(cptr->host, aconf->name, HOSTLEN) == 0)
	      break;
	    cptr = cptr->next;
	  }
	  if (cptr == NULL && aconf->port > 0) 
	    if (connect_server(aconf) == 0) {
	      sendto_ops("Connection to %s established.", aconf->name);
	      break;
	    }
	}
	aconf = aconf->next;
      }
      lasttime = getlongtime();
    }

    cptr = client;
    while (cptr) {
      if (cptr->status != STAT_ME && cptr->fd >= 0 &&
	  (lasttime - cptr->lasttime) > PINGFREQUENCY) {
	if ((lasttime - cptr->lasttime) > 2 * PINGFREQUENCY) {
	  m_bye(cptr, cptr, myhostname);
	  cptr = client;
	}
	else if ((cptr->flags & FLAGS_PINGSENT) == 0) {
	  cptr->flags |= FLAGS_PINGSENT;
	  sendto_one(cptr, "PING %s", myhostname);
	}
      }
      cptr = cptr->next;
    }
  }
}

OpenLog()
{
  int fd;
#ifndef TTYON
  if (debuglevel >= 0) {
    if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
      if ((fd = open("/dev/null", O_WRONLY)) < 0)
        exit(-1);
    if (fd != 2) {
      dup2(fd, 2);
      close(fd); 
    }
  } else {
    if ((fd = open("/dev/null", O_WRONLY)) < 0) 
      exit(-1);
    if (fd != 2) {
      dup2(fd, 2);
      close(fd);
    }
  }
#endif
}

