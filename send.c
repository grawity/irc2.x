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

extern aClient *client;
extern aClient *find_server();

#ifdef BYE
#define DoBye(x,y) m_bye((x),(y))
#else
#define DoBye(x,y)
#endif

sendto_one(to, pattern, par1, par2, par3, par4, par5, par6, par7, par8)
aClient *to;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr;
#if VMS
  extern int goodbye;

  if (Equal("QUIT", pattern)) goodbye = 1;
#endif

  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7,
	  par8);
  strcat(sendbuf, NEWLINE);
  if (!ISME(to))
    debug(DEBUG_NOTICE,"%s", sendbuf);

  if (to->fd < 0) {
    if (to->fromhost[0] == '\0' && !ISME(to)) {
      debug(DEBUG_ERROR,"Client %s with negative fd and no fromhost... AARGH!",
	    to->nickname);
      if (to->fd != -20) m_bye(to, to, myhostname);
    } else if (!ISME(to)){
      if (!(cptr = find_server(to->fromhost, NULL)) || cptr->fd < 0)
	return(-1);
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
	DoBye(cptr, cptr)   ;
    }
  } else {
    if (deliver_it(to->fd, sendbuf, strlen(sendbuf)) == -1)
      DoBye(to, to)   ;
  }
}

sendto_channel_butone(one, channel, pattern,
		      par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
int channel;
{
  aClient *cptr=client, *acptr;
  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7,
	  par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  for (; cptr; cptr = cptr->next) 
    if (cptr != one && cptr->fd >= 0 && 
	(ISSERVER(cptr) || (cptr->channel == channel && 
	  (ISOPER(cptr) || ISCLIENT(cptr))))) {
      acptr = client;
      if (ISSERVER(cptr)) {
	for (; acptr; acptr = acptr->next) 
	  if (acptr->channel == channel && 
	      (ISCLIENT(acptr) || ISOPER(acptr)) &&
	      Equal(acptr->fromhost, cptr->host) && cptr->host[0])
	    break;
	
      }
      if (acptr) {
	if (cptr->fd >= 0 &&
	    deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
	  DoBye(cptr, cptr)   ;
      }
    }
  
}

sendto_serv_butone(one, pattern, par1, par2, par3, par4, par5, par6,
		   par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr = client;
  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7, par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  for ( ; cptr ; cptr = cptr->next ) 
    if (ISSERVER(cptr) && cptr != one && cptr->fd >= 0) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
	DoBye(cptr, cptr)   ;
    }
}

sendto_channel_butserv(channel, pattern,
		       par1, par2, par3, par4, par5, par6, par7, par8)
int channel;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr = client;
  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7,
	  par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  for( ; cptr; cptr = cptr->next) 
    if (!ISSERVER(cptr) && cptr->fd >= 0 &&
	(cptr->channel == channel || channel == 0)) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
	DoBye(cptr, cptr)  ;
    }
  
}
sendto_all_butone(one, pattern, par1, par2, par3, par4, par5,
                  par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr = client;
  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7, par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  for ( ; cptr; cptr = cptr->next) 
    if (one != cptr && cptr->fd >= 0) {
      if (deliver_it(cptr->fd, sendbuf, strlen(sendbuf)) == -1)
        	DoBye(cptr, cptr)  ;
    }
}

sendto_ops(pattern, par1, par2, par3, par4, par5, par6, par7, par8)
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  char buf[1024];
  aClient *cptr = client;
  sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7, par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE, "%s", sendbuf);
  for (; cptr; cptr = cptr->next) 
    if (cptr->fd >= 0 && ISOPER(cptr)) {
      sprintf(buf, "NOTICE %s :*** Notice: %s", cptr->nickname, sendbuf);
      if (deliver_it(cptr->fd, buf, strlen(buf)) == -1)
	            DoBye(cptr, cptr)  ;
    }
}
