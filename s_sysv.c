/*************************************************************************
 ** s_sysv.c  Beta  v2.0    (22 Aug 1988)
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

char s_sysv_id[]="s_sysv.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include "struct.h"

extern aClient me;
extern aClient *client;

doalrm()
{
  return(0);
}

int
open_port(portnum)
int portnum;
{
  int key, length;
  if ((me.fd = msgget((key_t)portnum, IPC_CREAT | 0622)) == -1)
    perror("Getting SysV messageport");
  return(me.fd);
}

init_sys()
{
  int fd;
  setlinebuf(stdout);
  setlinebuf(stderr);
#ifdef NOTTY
  close (0); close(1); close(2);
  if (fork()) exit(0);
  if (setpgrp(0,0) == -1)
    perror("setpgrp");
#endif
}

read_msg(buffer, buflen, from)
char *buffer;
int buflen;
aClient **from;
{
  static char buf[BUFSIZE], *ptr;
  static struct msgbuf *ipcbuf = (struct msgbuf *) buf;
  int length;
  aClient *cptr;
  signal(SIGALRM, doalrm);
  length = msgrcv(me.fd, ipcbuf, sizeof (buf), 0, 0);
  if (length < 0)
    return(-1);

    cptr = client;
    for ( ; cptr; cptr=cptr->next) 
      if (cptr->fd == ipcbuf->mtype)
	break;
    
    if (!cptr) {
      cptr = make_client();
      strcpy(cptr->server, myhostname);
      cptr->fd = ipcbuf->mtype;
    }
    *from = cptr;
    length -= sizeof(ipcbuf->mtype);
    buflen = length;
    ptr = ipcbuf->mtext; 
    for (; length; length--)
      *buffer++ = *ptr++;
    return(length);
  
}    
  

connect_server(host, port)
char *host;
int port;
{
/*  struct sockaddr_in server;
  struct hostent *hp;
  aClient *acptr;
  aClient *cptr = make_client();
  int res;
  cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (cptr->fd < 0) {
    perror("opening stream socket to server");
    free(cptr);
    return(-1);
  }
  server.sin_family = AF_INET;
  if (!(hp = gethostbyname(host))) {
    close(cptr->fd);
    free(cptr);
    debug(DEBUG_FATAL, "%s: unknown host", host);
    return(-1);
  }
  bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
  server.sin_port = htons(port);

  if (connect(cptr->fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
    close(cptr->fd);
    free(cptr);
    perror("connect");
    return(-1);
  }
  CopyFixLen(cptr->fromhost, host, HOSTLEN+1);
  CopyFixLen(cptr->host, host, HOSTLEN+1);
  cptr->status = STAT_UNKNOWN;
  CopyFixLen(me.server, host, HOSTLEN+1);
  sendto_one(cptr, "SERVER %s",myhostname);
  cptr->next = client;
  client = cptr;
  res = fcntl(cptr->fd, F_GETFL, 0);
  fcntl(cptr->fd, F_SETFL, res | O_NDELAY); */
}

int utmp_open()
{
  int fd;
  return (open(UTMP,O_RDONLY));
}

int utmp_read(fd, name, line, host)
int fd;
char *name, *line, *host;
{
  struct utmp ut;
  while (read(fd, &ut, sizeof (struct utmp)) == sizeof (struct utmp)) {
    CopyFixLen(name,ut.ut_name,9); 
    CopyFixLen(line,ut.ut_line,9); 
    strncpy(host,(ut.ut_host[0]) ? (ut.ut_host) : myhostname, 16);
    if (ut.ut_name[0])
      return(0);
  }
  return(-1);
}

int utmp_close(fd)
int fd;
{
  return(close(fd));
}

#define CheckTTY(x) ((x)[0] == 't' && (x)[1] == 't' && (x)[2] == 'y')
#define CheckCON(x) ((x)[0] == 'c' && (x)[1] == 'o' && (x)[2] == 'n')

summon(who, namebuf, linebuf)
aClient *who;
char *namebuf, *linebuf;
{
  int fd;
  char line[120], *wrerr = "PRIVMSG %s :Write error. Couldn't summon.";
  if (strlen(linebuf) > 8) {
    sendto_one(who,"PRIVMSG %s :Serious fault in SUMMON.");
    sendto_one(who,"PRIVMSG %s :linebuf too long. Inform Administrator");
    return(-1);
  }
  /* Following line added to prevent cracking to e.g. /dev/kmem if */
  /* UTMP is for some silly reason writable to everyone... */
  if (CheckTTY(linebuf) || CheckCON(linebuf)) {
    sendto_one(who,"PRIVMSG %s :Looks like mere mortal souls are trying to");
    sendto_one(who,"PRIVMSG %s :enter the twilight zone... ");
    debug(0, "%s (%s@%s, nick %s, %s)",
	  "FATAL: major security hack. Notify Administrator !",
	  who->username, who->host, who->nickname, who->realname);
    return(-1);
  }
  strcpy(line,"/dev/");
  strcat(line,linebuf);
  if ((fd = open(line, O_WRONLY)) == -1) {
    sendto_one(who,"PRIVMSG %s :%s seems to have disabled summoning...");
    return(-1);
  }
  strcpy(line,"\007\007ircd: *You* are being summoned to irc by\n");
  if (write(fd, line, strlen(line)) != strlen(line)) {
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  sprintf(line, "ircd: Channel %d: %s@%s (%s) %s\n", who->channel,
	  who->username, who->host, who->nickname, who->realname);
  if (write(fd, line, strlen(line)) != strlen(line)) {
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  strcpy(line,"ircd: Respond with irc\n");
  if (write(fd, line, strlen(line)) != strlen(line)) {
    sendto_one(who,wrerr,who->nickname);
    return(-1);
  }
  sendto_one(who, "PRIVMSG %s :%s: Summoning user %s to irc",
	     who->nickname, myhostname, namebuf);
  return(0);
}
  
