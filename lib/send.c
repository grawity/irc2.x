/*************************************************************************
 ** send.c  Beta  v2.0    (22 Aug 1988)
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

char send_id[] = "send.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"

#define NULL ((char *) 0)
#define NEWLINE  "\n"

char sendbuf[1024];

extern struct Client *client;
extern struct Client *find_server();

sendto_one(to, pattern, para1, para2, para3, para4, para5, para6, para7, para8)
struct Client *to;
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
{
  struct Client *cptr;
#if VMS
  extern int goodbye;

  if (strcmp("QUIT", pattern) == 0) goodbye = 1;
#endif
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  if (to->status != STAT_ME)
    debug(DEBUG_NOTICE,"%s", sendbuf);
  if (to->fd < 0) {
    if (to->fromhost[0] == '\0' && to->status != STAT_ME) {
      debug(DEBUG_ERROR,"Client %s with negative fd and no fromhost... AARGH!",
	    to->nickname);
      if (to->fd != -20) m_bye(to, to, myhostname);
    } else if (to->status != STAT_ME){
      if ((cptr = find_server(to->fromhost, NULL)) == (struct Client *) 0 ||
	  cptr->fd < 0)
	return(-1);
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
/*	m_bye(cptr, cptr) /* */ ;
    }
  } else {
    if (deliver_it(to->fd, sendbuf, strlen(sendbuf)) == -1)
/*      m_bye(to, to)  /* */ ;
  }
}

sendto_channel_butone(one, channel, pattern,
		      para1, para2, para3, para4, para5, para6, para7, para8)
struct Client *one;
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
int channel;
{
  struct Client *cptr=client, *acptr;
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  while (cptr) {
    if (cptr != one && cptr->fd >= 0 && 
	(cptr->status == STAT_SERVER || 
	 (cptr->channel == channel && 
	  (cptr->status == STAT_OPER || cptr->status == STAT_CLIENT)))) {
      acptr = client;
      if (cptr->status == STAT_SERVER) {
	while (acptr) {
	  if (acptr->channel == channel && 
	      (acptr->status == STAT_CLIENT || acptr->status == STAT_OPER) &&
	      strcmp(acptr->fromhost, cptr->host) == 0 && cptr->host[0])
	    break;
	  acptr = acptr->next;
	}
      }
      if (acptr) {
	if (cptr->fd >= 0 &&
	    deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
/*	  m_bye(cptr, cptr) /* */ ;
      }
    }
    cptr = cptr->next;
  }
}

sendto_serv_butone(one, pattern, para1, para2, para3, para4, para5, para6,
		   para7, para8)
struct Client *one;
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
{
  struct Client *cptr = client;
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  while(cptr) {
    if (cptr->status == STAT_SERVER && cptr != one && cptr->fd >= 0) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
/*	m_bye(cptr, cptr) /* */ ;
    }
    cptr = cptr->next;
  }
}

sendto_channel_butserv(channel, pattern,
		       para1, para2, para3, para4, para5, para6, para7, para8)
int channel;
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
{
  struct Client *cptr = client;
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  while(cptr) {
    if (cptr->status != STAT_SERVER && cptr->fd >= 0 &&
	(cptr->channel == channel || channel == 0)) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
/*	m_bye(cptr, cptr) /* */ ;
    }
    cptr = cptr->next;
  }
}
sendto_all_butone(one, pattern, para1, para2, para3, para4, para5,
                  para6, para7, para8)
struct Client *one;
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
{
  struct Client *cptr = client;
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  while(cptr) {
    if (one != cptr && cptr->fd >= 0) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
/*        	m_bye(cptr, cptr) /* */ ;
    }
    cptr = cptr->next;
  }
}

sendto_ops(pattern, para1, para2, para3, para4, para5, para6, para7, para8)
char *pattern, *para1, *para2, *para3, *para4, *para5, *para6, *para7, *para8;
{
  char buf[1024];
  struct Client *cptr = client;
  sprintf(sendbuf, pattern, para1, para2, para3, para4, para5, para6, para7,
	  para8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE, "%s", sendbuf);
  while (cptr) {
    if (cptr->fd >= 0 && cptr->status == STAT_OPER) {
      sprintf(buf, "NOTICE %s :*** Notice: %s", cptr->nickname, sendbuf);
      if (deliver_it(cptr->fd, buf, strlen(buf)) == -1)
	/*            m_bye(cptr, cptr) /* */ ;
    }
    cptr = cptr->next;
  }
}
