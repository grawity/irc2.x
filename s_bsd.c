/*************************************************************************
 ** s_bsd.c  Beta  v2.1    (22 Aug 1988)
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

char s_bsd_id[] = "s_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center";

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
#if !HPUX
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/errno.h>
#if BSD42 || ULTRIX || HPUX
#include "sock.h"
#endif
#include "sys.h"

extern aClient me;
extern aClient *client;
extern aConfItem *find_conf();
extern int portnum;
extern int dummy();

int
open_port(portnum)
int portnum;
{
  int sock, length;
  static struct sockaddr_in server;
  /* At first, open a new socket */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("opening stream socket");
    exit(-1);
  }

  /* Bind a port to listen for new connections */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(portnum);
  for (length = 0; length < 10; length++) {
    if (bind(sock, &server, sizeof(server))) {
      perror("binding stream socket");
      if (length >= 9)
	exit(-1);
      sleep(20);
    } else
      break;
  }
  listen(sock, 3);
  return(sock);
}

init_sys()
{
  int fd;
#ifdef NOTTY
#if !HPUX
  setlinebuf(stdout);
  setlinebuf(stderr);
#endif
  if (isatty(0)) {
    close (0); close(1); close(2);
    if (fork()) exit(0);
#ifdef TIOCNOTTY
    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
      ioctl(fd, TIOCNOTTY, (char *) 0);
      close(fd);
    }
#else
    setpgrp(0,getpid());
#endif
  } else {
    close (0); close(1); close(2);
  }
#endif
}

int
check_access(cptr, flags) /* Check whether any clients/daemons/operators */
aClient *cptr;   /* are allowed to enter from this host         */
int flags;
{
  struct sockaddr_in name;
  struct hostent *host;
  aConfItem *aconf;
  char *hname;
  int namecount = -1;
  int len = sizeof(name);
  if (getpeername(cptr->fd, &name, &len) == -1) {
    perror("check_access (getpeername)");
    return(-1);
  }
  if ((host = gethostbyaddr(&(name.sin_addr),
			    sizeof (struct in_addr), AF_INET)) == NULL) {
#if ULTRIX
    return(-1);
#endif
  }
#if !ULTRIX
  hname = inet_ntoa(name.sin_addr);
#else
  hname = host->h_name;
#endif
  strncpy(cptr->host, hname, HOSTLEN);
  cptr->host[HOSTLEN] = '\0';
  do {
    debug(DEBUG_DEBUG,"Checking access for (%s) host", hname);
    if (aconf = find_conf(hname, NULL, NULL, flags)) {
      debug(DEBUG_DEBUG,"Access ok, host (%s)", hname);
      break;
    }
    debug(DEBUG_DEBUG,"Access checked");
    if (hname) {
      if (namecount < 0) {
	if (host == 0)
	  return -1;
	hname = host->h_name;
	namecount = 0;
      }
      else {
	if (host == 0)
	  return -1;
	hname = host->h_aliases[namecount++];
      }
      if (hname == 0 || *hname == 0)
	hname = NULL;
    }
  } while (hname);
  if (!hname)
    return -1;
  if (index(aconf->host, '*') == (char *) 0 &&
      index(aconf->host, '?') == (char *) 0) {
    strncpy(cptr->host, aconf->name, HOSTLEN);
    cptr->host[HOSTLEN] = '\0';
  } else {
    if (host)
      strncpy(cptr->host, host->h_name, HOSTLEN);
    else
      cptr->host[0] = '\0';
  }
  strncpy(cptr->sockhost, hname, HOSTLEN);
  cptr->sockhost[HOSTLEN] = '\0';
}

read_msg(buffer, buflen, from)
char *buffer;
int buflen;
aClient **from;
{
  struct timeval wait;
  int nfds, res, length;
  fd_set read_set;                 
  aClient *cptr;
  aConfItem *aconf;
  res = 0;
  do {
    wait.tv_sec = TIMESEC;
    wait.tv_usec = 0;
    FD_ZERO(&read_set);
    cptr = client;
    while (cptr) {
      if (cptr->fd >= 0) FD_SET(cptr->fd, &read_set);
      cptr = cptr->next;
    }
    nfds = select(FD_SETSIZE, &read_set, (fd_set *) 0, (fd_set *) 0, &wait);
    if (nfds < 0) {
      perror("select");
      res++;
      if (res > 5) {
	restart();
      }
      sleep(10);
    }
  } while (nfds < 0);
  if (nfds == 0)
    return(nfds);
  else {
    if (FD_ISSET(me.fd, &read_set)) {
      nfds--;
      cptr = make_client();
      strcpy(cptr->server, myhostname);
      if ((cptr->fd = accept(me.fd, (struct sockaddr *) 0, (int *) 0)) < 0)
	{
	  perror("accept");
	  free(cptr);
	}
      else {
/*	res = fcntl(cptr->fd, F_GETFL, 0);
	fcntl(cptr->fd, F_SETFL, res | O_NDELAY); */
	if (check_access(cptr, CONF_CLIENT | CONF_NOCONNECT_SERVER
			 | CONF_UPHOST) >= 0)
	  client = cptr;
	else {
	  sendto_ops("Received unauthorized connection from %s.", cptr->host);
	  close(cptr->fd);
	  free(cptr);
	}
      }
    }
    cptr = client;
      while (cptr && nfds) {
	if (cptr->fd >= 0 && FD_ISSET(cptr->fd, &read_set) && 
	    cptr->status != STAT_ME) {
	  nfds--;
	  if ((length = read(cptr->fd, buffer, buflen)) <= 0) {
            perror("read");
	    debug(DEBUG_DEBUG,"READ ERROR (FATAL): fd = %d", cptr->fd);
	    aconf = (aConfItem *) 0;
	    if (cptr->status == STAT_SERVER) {
		aconf = find_conf(cptr->sockhost, NULL, cptr->host,
				   CONF_CONNECT_SERVER);
		sendto_ops("Lost server connection to %s.", cptr->host);
	      }
	    m_bye(cptr, cptr, myhostname);
	    if (aconf && aconf->port > 0) {
	      sleep(3);
	      if (connect_server(aconf) == 0)
		sendto_ops("Connection to %s established.", aconf->name);
	    }
	  }
	  *from = cptr;
	  return(length);
	}
	cptr = cptr->next;
      }
    }
  return(-1);
}    
  

connect_server(aconf)
aConfItem *aconf;
{
  struct sockaddr_in server;
  struct hostent *hp;
  aClient *cptr = make_client();
  int errtmp;
  char *tmp;
  extern int errno;

  cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (cptr->fd < 0) {
    perror("opening stream socket to server");
    free(cptr);
    return(-1);
  }
  server.sin_family = AF_INET;

#if !ULTRIX
  if (isdigit(*(aconf->host))) 
      server.sin_addr.s_addr = inet_addr(aconf->host);
  else 
  {
#endif
    tmp = aconf->host;
    hp = gethostbyname(tmp);
    if (hp == 0) {
      close(cptr->fd);
      free(cptr);
      debug(DEBUG_FATAL, "%s: unknown host", aconf->host);
      return(-2);
    }
   bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
#if !ULTRIX
  }
#endif
  server.sin_port = htons((aconf->port > 0) ? aconf->port : portnum);

/*  sleep(5); */


  signal(SIGALRM, dummy);
  alarm(4);
  if (connect(cptr->fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
    alarm(0);
    errtmp = errno; /* perror eats errno */
    perror("connect");
    close(cptr->fd);
    free(cptr);
    if (errtmp == EINTR)
      errtmp = ETIMEDOUT;
    return(errtmp);
  }
  alarm(0);
  CopyFixLen(cptr->fromhost, aconf->host, HOSTLEN+1);
  CopyFixLen(cptr->host, aconf->host, HOSTLEN+1);
  cptr->status = STAT_HANDSHAKE;
#ifdef never
   CopyFixLen(me.seraver, aconf->host, HOSTLEN+1);
#endif 
  
  if (aconf->passwd[0])
    sendto_one(cptr, "PASS %s",aconf->passwd);
  sendto_one(cptr, "SERVER %s %s", myhostname, me.server);
  if (check_access(cptr, CONF_NOCONNECT_SERVER | CONF_UPHOST) < 0) {
    close(cptr->fd);
    free(cptr);
    debug(DEBUG_DEBUG,"No such host in hosts database !");
    return(-1);
  }
  cptr->next = client;
  client = cptr;
  return(0);
#ifdef never
  {
   int res = fcntl(cptr->fd, F_GETFL, 0);
   fcntl(cptr->fd, F_SETFL, res | O_NDELAY);
  } 
#endif
}

int utmp_open()
{
  return (open(UTMP,O_RDONLY));
}

int utmp_read(fd, name, line, host)
int fd;
char *name, *line, *host;
{
  struct utmp ut;
  while (read(fd, &ut, sizeof (struct utmp)) == sizeof (struct utmp)) {
    strncpy(name,ut.ut_name,8);  name[8] = '\0';
    strncpy(line,ut.ut_line,8);  line[8] = '\0';
#ifdef USER_PROCESS
    strncpy(host, myhostname, 8); host[8] = '\0';
    if (ut.ut_type == USER_PROCESS)
      return(0);
#else
    strncpy(host,(ut.ut_host[0]) ? (ut.ut_host) : myhostname, 16);
    if (ut.ut_name[0])
      return(0);
#endif
  }
  return(-1);
}

int utmp_close(fd)
int fd;
{
  return(close(fd));
}

summon(who, namebuf, linebuf)
aClient *who;
char *namebuf, *linebuf;
{
  int fd;
  char line[120], *wrerr = "NOTICE %s :Write error. Couldn't summon.";
  if (strlen(linebuf) > 8) {
    sendto_one(who,"NOTICE %s :Serious fault in SUMMON.");
    sendto_one(who,"NOTICE %s :linebuf too long. Inform Administrator");
    return(-1);
  }
  /* Following line added to prevent cracking to e.g. /dev/kmem if */
  /* UTMP is for some silly reason writable to everyone... */
  if ((linebuf[0] != 't' || linebuf[1] != 't' || linebuf[2] != 'y') &&
      (linebuf[0] != 'c' || linebuf[1] != 'o' || linebuf[2] != 'n')) {
    sendto_one(who,"NOTICE %s :Looks like mere mortal souls are trying to");
    sendto_one(who,"NOTICE %s :enter the twilight zone... ");
    debug(0, "%s (%s@%s, nick %s, %s)",
	  "FATAL: major security hack. Notify Administrator !",
	  who->username, who->host, who->nickname, who->realname);
    return(-1);
  }
  strcpy(line,"/dev/");
  strcat(line,linebuf);
  alarm(5);
  if ((fd = open(line, O_WRONLY | O_NDELAY)) == -1) {
    alarm(0);
    sendto_one(who,"NOTICE %s :%s seems to have disabled summoning...",
	       who->nickname, namebuf);
    return(-1);
  }
  alarm(0);
  strcpy(line,
	 "\n\r\007ircd: You are being summoned to Internet Relay Chat by\n\r");
  alarm(5);
  if (write(fd, line, strlen(line)) != strlen(line)) {
    alarm(0);
    close(fd);
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  alarm(0);
  sprintf(line, "ircd: Channel %d: %s@%s (%s) %s\n\r", who->channel,
	  who->username, who->host, who->nickname, who->realname);
  alarm(5);
  if (write(fd, line, strlen(line)) != strlen(line)) {
    alarm(0);
    close(fd);
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  alarm(0);
  strcpy(line,"ircd: Respond with irc\n\r");
  alarm(5);
  if (write(fd, line, strlen(line)) != strlen(line)) {
    alarm(0);
    close(fd);
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  close(fd);
  alarm(0);
  sendto_one(who, "NOTICE %s :%s: Summoning user %s to irc",
	     who->nickname, myhostname, namebuf);
  return(0);
}
  
getmyname(name, len)
char *name;
int len;
{
  struct hostent *hp;

  if (gethostname(name,len) < 0)
    return(-1);
  name[len] = '\0';

  /* assume that a name containing '.' is a fully qualified domain name */
  if (index(name, '.') == (char *) 0) {
    if ((hp = gethostbyname(name)) != (struct hostent *) 0)
      strncpy(name, hp->h_name, len);
  }

  return (0);
}

long
getlongtime()
{
  return(time(0));
}
