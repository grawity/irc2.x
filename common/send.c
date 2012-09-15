/************************************************************************
 *   IRC - Internet Relay Chat, lib/ircd/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

char send_id[] = "send.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"

#define NEWLINE  "\n"

static char sendbuf[1024];

extern aClient *client;
extern aChannel *channel;

/*
** DeadLink
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	If 'notice' is not NULL, it is assumed to be a format
**	for a message to local opers. I can contain only one
**	'%s', which will be replaced by the sockhost field of
**	the failing link.
**
**	Also, the notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static int DeadLink(to,notice)
aClient *to;
char *notice;
    {
	to->flags |= FLAGS_DEADSOCKET;
	if (notice != NULL && !IsPerson(to) && !IsUnknown(to))
		sendto_ops(notice,to->sockhost);
	return 0;
    }

/*
** SendMessage
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
*/
static SendMessage(to,msg,len)
aClient *to;
char *msg;
    {
	int rlen = 0;

	if (to->flags & FLAGS_DEADSOCKET)
		return 0; /* This socket has already been marked as dead */
	/*
	** DeliverIt can be called only if SendQ is empty...
	*/
	if ((dbuf_length(&to->sendQ) == 0) &&
	    (rlen = DeliverIt(to->fd, msg, len)) < 0)
		DeadLink(to,"Write error to %s, closing link");
	else if (rlen < len)
	    {
		/*
		** Was unable to transfer all of the requested data. Queue
		** up the remainder for some later time...
		*/
		if (dbuf_length(&to->sendQ) > MAXSENDQLENGTH)
			DeadLink(to,"Max buffering limit exceed for %s");
		else if (dbuf_put(&to->sendQ,msg+rlen,len-rlen) < 0)
			DeadLink(to,"Buffer allocation error for %s");
	    }

	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	to->sendB += rlen;
	me.sendB += rlen;
	return 0;
    }

/*
** SendQueued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
SendQueued(to)
aClient *to;
    {
	char *msg;
	int len, rlen;

	/*
	** Once socket is marked dead, we cannot start writing to it,
	** even if the error is removed...
	*/
	if (to->flags & FLAGS_DEADSOCKET)
	    {
		/* Actually, we should *NEVER* get here--something is
		   not working correct if SendQueued is called for a
		   dead socket... --msa */
		DeadLink(to,"SendQueued called for a DEADSOCKET %s :-(");
		dbuf_delete(&to->sendQ, dbuf_length(&to->sendQ));
		return 0;
	    }
	while (dbuf_length(&to->sendQ) > 0)
	    {
		msg = dbuf_map(&to->sendQ, (long *)&len);
					/* Returns always len > 0 */
		if ((rlen = DeliverIt(to->fd, msg, len)) < 0)
		    {
			DeadLink(to,"Write error to %s, closing link");
			break;
		    }
		dbuf_delete(&to->sendQ, rlen);
		to->sendB += rlen;
		me.sendB += rlen;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;
	    }
    }

/*
** send message to single client
*/
sendto_one(to, pattern, par1, par2, par3, par4, par5, par6, par7, par8)
aClient *to;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
#if VMS
  extern int goodbye;
	
  if (StrEq("QUIT", pattern)) 
    goodbye = 1;
#endif
  sprintf(sendbuf, pattern,
	  par1, par2, par3, par4, par5, par6, par7, par8);
  strcat(sendbuf, NEWLINE);
  debug(DEBUG_NOTICE,"%s", sendbuf);
  to = to->from;
  if (to->fd < 0)
    debug(DEBUG_ERROR,
	  "Local socket %s with negative fd... AARGH!",
	  to->name);
  else
    SendMessage(to, sendbuf, strlen(sendbuf));
}

sendto_channel_butone(one, channel, pattern,
		      par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
aChannel *channel;
{
  Link *link, *tmplink;
  for (link = channel->members; link; link = link->next)
    {
      if (((aClient *) link->value) == one)
	continue;	/* ...was the one I should skip */
      if (MyConnect(((aClient *)link->value)) &&
	  IsRegisteredUser((aClient *)link->value)) {
	sendto_one(link->value, pattern,
		   par1, par2, par3, par4, par5, par6, par7, par8);
      } else {  /* Now check whether a message has been sent to this
		   remote link already */
	for (tmplink = channel->members; tmplink != link;
	     tmplink = tmplink->next) {
	  if (((aClient *) tmplink->value)->from ==
	      ((aClient *) link->value)->from) {
	    break;
	  }
	}
	if (tmplink == link) {
	  sendto_one(link->value, pattern,
		     par1, par2, par3, par4, par5, par6, par7, par8);
	}
      }
    }
}

sendto_serv_butone(one, pattern, par1, par2, par3, par4, par5, par6,
		   par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr;
  for (cptr = client; cptr; cptr = cptr->next) 
    if (MyConnect(cptr) && IsServer(cptr) && cptr != one)
      sendto_one(cptr, pattern,
		 par1, par2, par3, par4, par5, par6, par7, par8);
}

/* sendto_common_channels()
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
sendto_common_channels(user, pattern,
		       par1, par2, par3, par4, par5, par6, par7, par8)
aClient *user;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr;
  aChannel *chptr;
  for (cptr = client; cptr; cptr = cptr->next) {
    if (!MyConnect(cptr) || IsServer(cptr) || user == cptr)
      continue;
    for (chptr = channel; chptr; chptr = chptr->nextch) {
      if (IsMember(user, chptr) && IsMember(cptr, chptr)) {
	sendto_one(cptr, pattern, par1, par2, par3, par4,
		   par5, par6, par7, par8);
	break;
      }
    }
  }
  if (MyConnect(user))
    sendto_one(user, pattern, par1, par2, par3, par4,
	       par5, par6, par7, par8);
}

sendto_channel_butserv(channel, pattern,
		       par1, par2, par3, par4, par5, par6, par7, par8)
aChannel *channel;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  Link *link;
  for (link = channel->members; link; link = link->next) {
    if (MyConnect(((aClient *) link->value)))
      sendto_one(link->value, pattern,
		 par1, par2, par3, par4, par5, par6, par7, par8);
  }
}

/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static int match_it(one, mask, what)
     aClient *one;
     char *mask;
     int what;
{
  switch (what)
    {
    case MATCH_HOST:
      return(matches(mask, one->user->host)==0);
    case MATCH_SERVER:
    default:
      return(matches(mask, one->user->server)==0);
    }
}

sendto_match_butone(one, mask, what, pattern, par1, par2, par3, par4, par5,
		    par6, par7, par8)
     aClient *one;
     int what;
     char *mask, *pattern, *par1, *par2, *par3, *par4, *par5, *par6;
     char *par7, *par8;
{
  aClient *cptr, *acptr;
  
  for (cptr = client; cptr; cptr = cptr->next)
    {
      if (cptr == one)        /* must skip the origin !! */
	continue;
      if (!MyConnect(cptr))
	continue;       /* that clients are not mine */
      if (IsServer(cptr))
	{
	  for (acptr=client; acptr; acptr=acptr->next)
	    if (IsRegisteredUser(acptr)
		&& match_it(acptr, mask, what)
		&& acptr->from == cptr)
	      break;
	  /* a person on that server
	   ** matches the mask, so we
	   ** send *one* msg to that
	   ** server ...
	   */
	  if (acptr == NULL)
	    continue;
	  /*
	   ** ... but only if there *IS*
	   **     a matching person
	   */
	}
      /* my client, does he match ? */
      else if (!(IsRegisteredUser(cptr) && match_it(cptr, mask, what)))
	continue;
      sendto_one(cptr, pattern,
			   par1, par2, par3, par4, par5, par6, par7, par8);
    }
}

sendto_all_butone(one, pattern, par1, par2, par3, par4, par5,
                  par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr;
  for (cptr = client; cptr; cptr = cptr->next) 
    if (MyConnect(cptr) && !IsMe(cptr) && one != cptr)
      sendto_one(cptr, pattern,
		 par1, par2, par3, par4, par5, par6, par7, par8);
}

/*
** sendto_ops
**	Send to *local* ops only.
*/
sendto_ops(pattern, par1, par2, par3, par4, par5, par6, par7, par8)
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
  aClient *cptr;
  char buf[512];

  for (cptr = client; cptr; cptr = cptr->next) 
    if (MyConnect(cptr) && IsAnOper(cptr))
      {
	sprintf(buf, "NOTICE %s :*** Notice -- ", cptr->name);
	strncat(buf, pattern, sizeof(buf) - strlen(buf));
	sendto_one(cptr, buf,
		   par1, par2, par3, par4, par5, par6, par7, par8);
      }
}

/*
** sendto_ops_butone
**	Send message to all operators.
*/
sendto_ops_butone(one, pattern,
		      par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	aClient *cptr, *acptr;
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (cptr == one)
			continue;	/* ...was the one I should skip */
		if (!MyConnect(cptr))
			continue;	/* ...no connection on this */
		if (IsServer(cptr))
		    {  /* Now wallops get sent to all servers... -SRB */
		    }
		else if (!IsOper(cptr))
			continue;
      		sendto_one(cptr, pattern,
			   par1, par2, par3, par4, par5, par6, par7, par8);
	    }
    }

