/*************************************************************************
 ** list.c  Beta  v2.0    (22 Aug 1988)
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

char list_id[] = "list.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#define NULL ((char *) 0)

extern aClient *client;
extern aConfItem *conf;
extern int maxusersperchannel;

aClient *
make_client()
{
  aClient *cptr = (aClient *) MyMalloc(sizeof(aClient));
  if (!cptr)
    return 0;
  cptr->next = client;
  cptr->away = NULL;
  cptr->host[0] = cptr->nickname[0] = cptr->username[0] = '\0';
  cptr->realname[0] = cptr->server[0] = cptr->buffer[0] = '\0';
  cptr->passwd[0] = cptr->sockhost[0] = '\0';
  cptr->status = STAT_UNKNOWN; cptr->channel = 0; cptr->fd = -1;
  cptr->lasttime = getlongtime(); cptr->flags = 0;
 
  return (cptr);
}

