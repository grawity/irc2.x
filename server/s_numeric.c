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

extern struct Client *find_client();
extern struct Channel *find_channel();

static char buffer[1024];

do_numeric(numeric, cptr, sptr, sender, para, para2, para3, para4,
	   para5, para6)
int numeric;
struct Client *cptr, *sptr;
char *sender, *para, *para2, *para3, *para4, *para5, *para6;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
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
	sprintf(buffer,":%s %d %s ", sptr->nickname, numeric, nick);
	if (para3) {
	  strcat(buffer, para2);
	  strcat(buffer, " ");
	  if (para4) {
	    strcat(buffer, para3);
	    strcat(buffer, " ");
	    if (para5) {
	      strcat(buffer, para4);
	      strcat(buffer, " ");
	      if (para6) {
		strcat(buffer, para5);
		strcat(buffer, " ");
		strcat(buffer, ":");
		strcat(buffer, para6);
	      } else {
		strcat(buffer, ":");
		strcat(buffer, para5);
	      }
	    } else {
	      strcat(buffer, ":");
	      strcat(buffer, para4);
	    }
	  } else {
	    strcat(buffer, ":");
	    strcat(buffer, para3);
	  }
	} else if (para2) {
	  strcat(buffer, ":");
	  strcat(buffer, para2);
	}
	sendto_one(acptr, buffer);
      } else if (chptr = find_channel(nick, NULL)) {
	sprintf(buffer,":%s %d %d ", sptr->nickname, numeric, chptr->channo);
	if (para3) {
	  strcat(buffer, para2);
	  strcat(buffer, " ");
	  if (para4) {
	    strcat(buffer, para3);
	    strcat(buffer, " ");
	    if (para5) {
	      strcat(buffer, para4);
	      strcat(buffer, " ");
	      if (para6) {
		strcat(buffer, para5);
		strcat(buffer, " ");
		strcat(buffer, ":");
		strcat(buffer, para6);
	      } else {
		strcat(buffer, ":");
		strcat(buffer, para5);
	      }
	    } else {
	      strcat(buffer, ":");
	      strcat(buffer, para4);
	    }
	  } else {
	    strcat(buffer, ":");
	    strcat(buffer, para3);
	  }
	} else if (para2) {
	  strcat(buffer, ":");
	  strcat(buffer, para2);
	}
	sendto_channel_butone(cptr, chptr->channo, buffer);
      } else {
/*	sendto_one(sptr, ":%s %3d %s %s :No such nickname",
		   myhostname, ERR_NOSUCHNICK, sptr->nickname, nick); */
      }
    } while (tmp && *tmp);
  }
  return (0);
}
