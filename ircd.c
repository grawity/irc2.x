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

char *version = "2.1.0";
char *info1 = "Programmed by Jarkko Oikarinen";
char *info2 = "(c)  1988,1989   University of Oulu,   Computing Center";
char *info3 = "INTERNET: jto@tolsun.oulu.fi    BITNET: toljto at finou";
char myhostname[HOSTLEN+1];

char *welcome1 = "Welcome to Internet Relay Server v";


#define TRUE 1

extern int dummy();
extern aClient *make_client();
aClient me;                /* That's me */
aClient *client = &me;    /* Pointer to beginning of Client list */
extern aConfItem *conf;
extern aConfItem *find_me();
char *uphost;
char **myargv;
int portnum;
int debuglevel;
int maxusersperchannel = MAXUSERSPERCHANNEL;

restart()
{
  int i;
  debug(DEBUG_NOTICE,"Restarting server...");
#ifdef NOTTY
  for (i=0; i<MAXFD; i++) 
#else
  for (i=3; i<MAXFD; i++)
#endif
    close(i);
  execv(MYNAME, myargv);
  debug(DEBUG_FATAL,"Couldn't restart server !!!!!!!!");
  exit(-1);
}

main(argc, argv)
int argc;
char *argv[];
{
  aClient *cptr;
  aConfItem *aconf;
  int length;                      /* Length of message received from client */
  char buffer[BUFSIZE];
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
#ifdef never
        case 'c':
	maxusersperchannel = atoi(&argv[1][2]);
	break;
#endif 
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
  for (aconf = conf ; aconf; aconf=aconf->next ) 
    if (aconf->status == CONF_CONNECT_SERVER && aconf->port > 0) {
      if (!connect_server(aconf))
	break;
    }
  
  debug(DEBUG_DEBUG,"Server ready...");
  
  for (;;) {
    if ((length = read_msg(buffer, BUFSIZE, &cptr)) > 0) {
      cptr->lasttime = getlongtime();
      CLRPINGSENT(cptr);
      dopacket(cptr, buffer, length);
    } 

    debug(DEBUG_DEBUG,"Got message");

    lasttime = getlongtime();
    if (lasttime - lastconnecttry > CONNECTFREQUENCY) {
      lastconnecttry = lasttime;
      for (aconf = conf; aconf; aconf=aconf->next) {
	if (aconf->status != CONF_CONNECT_SERVER)
          continue; 
	cptr = client;
	for (cptr = client; cptr; cptr = cptr ->next) 
	  if ((ISSERVER(cptr) || ISME(cptr)) &&
	    MyEqual(cptr->host, aconf->name, HOSTLEN))
	    break;
	  
	  if (!cptr  && aconf->port > 0) 
	    if (!connect_server(aconf)) {
	      sendto_ops("Connection to %s established.", aconf->name);
	      break;
	    }
	
      }
      lasttime = getlongtime();
    }

    for (cptr = client; cptr; cptr = cptr->next) 
      if (!ISME(cptr) && cptr->fd >= 0 &&
	  (lasttime - cptr->lasttime) > PINGFREQUENCY) {
	if ((lasttime - cptr->lasttime) > 2 * PINGFREQUENCY) {
	  m_bye(cptr, cptr, myhostname);
	  cptr = client;
	}
	else if (!ISPINGSENT(cptr)) {
	  SETPINGSENT(cptr);
	  sendto_one(cptr, "PING %s", myhostname);
	}
      }
  }
}
