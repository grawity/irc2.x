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

char send_id[] = "send.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"

#ifndef NULL
#define NULL 0
#endif

#define NEWLINE  "\n"

static char sendbuf[1024];

extern aClient *client;

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
		msg = dbuf_map(&to->sendQ, &len); /* Returns always len > 0 */
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
int channel;
    {
	aClient *cptr, *acptr;
	sprintf(sendbuf, pattern, par1, par2, par3, par4, par5,
		par6, par7,par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE,"%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (cptr == one)
			continue;	/* ...was the one I should skip */
		if (!MyConnect(cptr))
			continue;	/* ...no connection on this */
		if (IsServer(cptr))
		    {
			/*
			** Send message to server links only, if
			** there is a user behind, on the same
			** channel.
			*/
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsPerson(acptr) &&
				    acptr->from == cptr &&
				    acptr->user->channel == channel)
					break;
			if (acptr == NULL)
				continue;	/* ...no users there! */
		    }
		else if (!IsPerson(cptr) || cptr->user->channel != channel)
			continue;
		SendMessage(cptr, sendbuf, strlen(sendbuf));
	    }
    }

sendto_serv_butone(one, pattern, par1, par2, par3, par4, par5, par6,
		   par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	aClient *cptr;
	sprintf(sendbuf, pattern,
		par1, par2, par3, par4, par5, par6, par7, par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE,"%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next) 
		if (MyConnect(cptr) && IsServer(cptr) && cptr != one)
			SendMessage(cptr, sendbuf, strlen(sendbuf));
    }

sendto_channel_butserv(channel, pattern,
		       par1, par2, par3, par4, par5, par6, par7, par8)
int channel;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	aClient *cptr;
	sprintf(sendbuf, pattern,
		par1, par2, par3, par4, par5, par6, par7, par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE,"%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next) 
		if (IsPerson(cptr) && MyConnect(cptr) &&
		    (cptr->user->channel == channel || channel == 0))
			SendMessage(cptr, sendbuf, strlen(sendbuf));
    }

sendto_all_butone(one, pattern, par1, par2, par3, par4, par5,
                  par6, par7, par8)
aClient *one;
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	aClient *cptr;
	sprintf(sendbuf, pattern, par1, par2, par3, par4, par5, par6, par7,
		par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE,"%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next) 
		if (MyConnect(cptr) && !IsMe(cptr) && one != cptr)
			SendMessage(cptr, sendbuf, strlen(sendbuf));
    }

/*
** sendto_ops
**	Send to *local* ops only.
*/
sendto_ops(pattern, par1, par2, par3, par4, par5, par6, par7, par8)
char *pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	char buf[1024];
	aClient *cptr;

	sprintf(sendbuf, pattern,
		par1, par2, par3, par4, par5, par6, par7, par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE, "%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next) 
		if (MyConnect(cptr) && IsOper(cptr))
		    {
			sprintf(buf, "NOTICE %s :*** Notice: %s",
				cptr->name, sendbuf);
			SendMessage(cptr, buf, strlen(buf));
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
	sprintf(sendbuf, pattern, par1, par2, par3, par4, par5,
		par6, par7,par8);
	strcat(sendbuf, NEWLINE);
	debug(DEBUG_NOTICE,"%s", sendbuf);
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (cptr == one)
			continue;	/* ...was the one I should skip */
		if (!MyConnect(cptr))
			continue;	/* ...no connection on this */
		if (IsServer(cptr))
		    {
			/*
			** Send message to server links only, if
			** there are opers behind that link.
			*/
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsOper(acptr) && acptr->from == cptr)
					break;
			if (acptr == NULL)
				continue;	/* ...no opers there! */
		    }
		else if (!IsOper(cptr))
			continue;
		SendMessage(cptr, sendbuf, strlen(sendbuf));
	    }
    }

