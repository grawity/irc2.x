/*************************************************************************
 ** s_numeric.c  irc  v2.02    (16 Aug 1989)
 **
 ** This file is part of Internet Relay Chat v2.02
 **
 ** Author:           Jarkko Oikarinen 
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1989 Jarkko Oikarinen
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 ** 
 *************************************************************************/

char numeric_id[] = "numeric.c (c) 1989 Jarkko Oikarinen";

#include "sys.h" 
#include "struct.h"
#include "numeric.h"

#define NULL (char *) 0

extern aClient *find_client();
extern aChannel *find_channel();

do_numeric(numeric, cptr, sptr, sender, para, para1, para2, para3, para4,
	   para5, para6)
int numeric;
aClient *cptr, *sptr;
char *sender, *para, *para1, *para2, *para3, *para4, *para5, *para6;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;
  if (!para)  para = "";
  if (!para1) para1 = "";
  if (!para2) para2 = "";
  if (!para3) para3 = "";
  if (!para4) para4 = "";
  if (!para5) para5 = "";
  if (!para6) para6 = "";

  if (para == NULL || *para == '\0' || sptr->status < 0)
/*    sendto_one(sptr, ":%s %3d %s", myhostname, ERR_NOTEXTTOSEND,
	       sptr->nickname) */ ;
  else {
    tmp = para;
    do {
      nick = tmp;
      if (tmp = index(nick, ',')) {
	*(tmp++) = '\0';
      }
      acptr = find_client(nick, NULL);
      if (acptr) {
	sendto_one(acptr, ":%s %d %s %s %s %s %s %s",
		   sptr->nickname, numeric, nick, para2,
		   para3, para4, para5, para6);
      } else if (chptr = find_channel(nick, NULL)) {
	sendto_channel_butone(cptr, chptr->channo,
			      ":%s %d %d %s %s %s %s %s",
			      sptr->nickname, numeric, chptr->channo, para2,
			      para3, para4, para5, para6);
      } else {
/*	sendto_one(sptr, ":%s %3d %s %s :No such nickname",
		   myhostname, ERR_NOSUCHNICK, sptr->nickname, nick); */
      }
    } while (tmp && *tmp);
  }
  return (0);
}

