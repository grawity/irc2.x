/************************************************************************
 *   IRC - Internet Relay Chat, lib/ircd/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

char send_id[] = "send.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include <stdio.h>


#define NEWLINE  "\n"

static char sendbuf[2048];

#ifndef CLIENT_COMPILE
extern aClient *client, *local[];
extern aClient me;
extern int highest_fd;
static int sentalong[MAXCONNECTIONS], i;
#endif
extern aChannel *channel;

/*
** dead_link
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
static	int dead_link(to,notice)
aClient *to;
char	*notice;
    {
	to->flags |= FLAGS_DEADSOCKET;
#ifndef CLIENT_COMPILE
	if (notice != (char *)NULL && !IsPerson(to) && !IsUnknown(to) &&
	    !(to->flags & FLAGS_CLOSING))
		sendto_ops(notice, get_client_name(to, FALSE));
#endif
	return 0;
    }

#ifndef CLIENT_COMPILE
/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we cant send it, it goes into the sendQ
**	-avalon
*/
void	flush_connections(fd)
int	fd;
{
#ifdef SENDQ_ALWAYS
	Reg1	int	i;
	Reg2	aClient *cptr;
	if (fd == me.fd)
	    {
		for (i = 0; i <= highest_fd; i++)
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				send_queued(cptr);
	    }
	else if (fd >= 0 && local[fd])
		send_queued(local[fd]);
#endif
}
#endif

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
*/
static	int	send_message(to,msg,len)
aClient	*to;
char	*msg;	/* if msg is a null pointer, we are flushing connection */
int	len;
    {
#ifdef SENDQ_ALWAYS
	if (to->flags & FLAGS_DEADSOCKET)
		return 0; /* This socket has already been marked as dead */
	if (DBufLength(&to->sendQ) > MAXSENDQLENGTH)
		dead_link(to,"Max buffering limit exceed for %s");
	else if (dbuf_put(&to->sendQ, msg, len) < 0)
		dead_link(to, "Buffer allocation error for %s");
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	/*
	** This little bit is to stop the sendQ from growing too large when
	** there is no need for it to. Thus we call send_queued() every time
	** 2k has been added to the queue since the last non-fatal write.
	** Also stops us from deliberately building a large sendQ and then
	** trying to flood that link with data (possible during the net
	** relinking done by servers with a large load).
	*/
	if (DBufLength(&to->sendQ)/2048 > to->lastsq)
		send_queued(to);
	return 0;
    }
#else
	int	rlen = 0;

	if (to->flags & FLAGS_DEADSOCKET)
		return 0; /* This socket has already been marked as dead */

	/*
	** DeliverIt can be called only if SendQ is empty...
	*/
	if ((DBufLength(&to->sendQ) == 0) &&
	    (rlen = deliver_it(to, msg, len)) < 0)
		dead_link(to,"Write error to %s, closing link");
	else if (rlen < len)
	    {
		/*
		** Was unable to transfer all of the requested data. Queue
		** up the remainder for some later time...
		*/
		if (DBufLength(&to->sendQ) > MAXSENDQLENGTH)
			dead_link(to,"Max buffering limit exceed for %s");
		else if (dbuf_put(&to->sendQ,msg+rlen,len-rlen) < 0)
			dead_link(to,"Buffer allocation error for %s");
	    }
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	to->sendB += rlen;
	me.sendB += rlen;
	if (to->acpt != &me)
		to->acpt->sendB += 1;
	return 0;
    }
#endif

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int	send_queued(to)
aClient *to;
    {
	char	*msg;
	int	len, rlen;

	/*
	** Once socket is marked dead, we cannot start writing to it,
	** even if the error is removed...
	*/
	if (to->flags & FLAGS_DEADSOCKET)
	    {
		/*
		** Actually, we should *NEVER* get here--something is
		** not working correct if send_queued is called for a
		** dead socket... --msa
		*/
#ifndef SENDQ_ALWAYS
		dead_link(to, "send_queued called for a DEADSOCKET %s :-(");
#endif
		dbuf_delete(&to->sendQ, DBufLength(&to->sendQ));
		return -1;
	    }
	while (DBufLength(&to->sendQ) > 0)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
		    {
			dead_link(to,"Write error to %s, closing link");
			break;
		    }
		dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/2048;
		to->sendB += rlen;
		me.sendB += rlen;
		if (to->acpt != &me)
			to->acpt->sendB += rlen;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;
	    }

	return (to->flags & FLAGS_DEADSOCKET) ? -1 : 0;
    }

/*
** send message to single client
*/
int	sendto_one(to, pattern, par1, par2, par3, par4, par5,
		   par6, par7, par8, par9, par10, par11)
aClient *to;
char	*pattern, *par1, *par2, *par3, *par4, *par5;
char	*par6, *par7, *par8, *par9, *par10, *par11;
{
#ifdef VMS
	extern int goodbye;
	
	if (StrEq("QUIT", pattern)) 
		goodbye = 1;
#endif

	sprintf(sendbuf, pattern,
		par1, par2, par3, par4, par5, par6,
		par7, par8, par9, par10, par11);
	debug(DEBUG_SEND,"Sending [%s] to %s", sendbuf,to->name);

	if (to->from)
		to = to->from;
	if (to->fd < 0)
		debug(DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name);
	else if (IsMe(to))
#ifndef	CLIENT_COMPILE
		sendto_ops("Trying to send [%s] to myself!", sendbuf);
#else
		;
#endif
	else
	    {
		strcat(sendbuf, NEWLINE);
		sendbuf[510] = '\n';
		sendbuf[511] = '\0';
		send_message(to, sendbuf, strlen(sendbuf));
	    }

	return 0;
}

#ifndef CLIENT_COMPILE
int	sendto_channel_butone(one, from, channel, pattern,
			      par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one, *from;
aChannel *channel;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	Reg1 Link *link;
	Reg2 aClient *acptr;

	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (link = channel->members; link; link = link->next)
	    {
		acptr = link->value.cptr;
		if (acptr->from == one)
			continue;	/* ...was the one I should skip */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
			sendto_prefix_one(acptr, from, pattern,
					  par1, par2, par3, par4,
					  par5, par6, par7, par8);
			sentalong[i] = 1;
		    }
		else
		    {
		/* Now check whether a message has been sent to this
		 * remote link already */
			if (sentalong[i] == 0)
			    {
	  			sendto_prefix_one(acptr, from, pattern,
						  par1, par2, par3, par4,
						  par5, par6, par7, par8);
				sentalong[i] = 1;
			    }
		    }
	    }
	return 0;
    }

int	sendto_serv_butone(one, pattern, par1, par2, par3, par4, par5, par6,
			   par7, par8)
aClient *one;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	Reg1	int i;
	Reg2	aClient *cptr;

#ifdef NPATH
        check_command(one, pattern, par1, par2, par3);
#endif
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		if (IsServer(cptr))
			sendto_one(cptr, pattern,
				   par1, par2, par3, par4,
				   par5, par6, par7, par8);
	    }
	return 0;
}

/* sendto_common_channels()
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
int	sendto_common_channels(user, pattern,
				par1, par2, par3, par4,
				par5, par6, par7, par8)
aClient *user;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	Reg1 int i;
	Reg2 aClient *cptr;
	Reg3 Link *link;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || IsServer(cptr) ||
		    user == cptr || !user->user)
			continue;
		for (link = user->user->channel; link; link = link->next)
			if (IsMember(user, link->value.chptr) &&
			    IsMember(cptr, link->value.chptr))
			    {
				sendto_prefix_one(cptr, user, pattern,
						  par1, par2, par3, par4,
						  par5, par6, par7, par8);
					break;
			    }
	    }
	if (MyConnect(user))
		sendto_prefix_one(user, user, pattern, par1, par2, par3, par4,
					par5, par6, par7, par8);
	return 0;
}
#endif


int sendto_channel_butserv(channel, from, pattern,
			   par1, par2, par3, par4, par5, par6, par7, par8)
aChannel *channel;
aClient *from;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	Reg1	Link *link;

	for (link = channel->members; link; link = link->next)
		if (MyConnect(link->value.cptr))
			sendto_prefix_one(link->value.cptr, from, pattern,
					  par1, par2, par3, par4,
					  par5, par6, par7, par8);

	return 0;
}

/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static	int	match_it(one, mask, what)
aClient *one;
char	*mask;
int	what;
{
	switch (what)
	{
	case MATCH_HOST:
		return (matches(mask, one->user->host)==0);
	case MATCH_SERVER:
	default:
		return (matches(mask, one->user->server)==0);
	}
}

#ifndef CLIENT_COMPILE
int	sendto_match_butone(one, from, mask, what, pattern,
			    par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one, *from;
int	what;
char	*mask, *pattern, *par1, *par2, *par3, *par4, *par5, *par6;
char	*par7, *par8;
{
	Reg1	int	i;
	Reg2	aClient *cptr, *acptr;
  
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr))
		    {
			for (acptr=client; acptr; acptr=acptr->next)
				if (IsRegisteredUser(acptr)
				    && match_it(acptr, mask, what)
				    && acptr->from == cptr)
					break;
			/* a person on that server matches the mask, so we
			** send *one* msg to that server ...
			*/
			if (acptr == NULL)
				continue;
			/* ... but only if there *IS* a matching person */
		    }
		/* my client, does he match ? */
		else if (!(IsRegisteredUser(cptr) &&
			 match_it(cptr, mask, what)))
			continue;
		sendto_prefix_one(cptr, from, pattern,
				  par1, par2, par3, par4,
				  par5, par6, par7, par8);
	    }
	return 0;
}

/*
 * sendto_all_butone.
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */
int	sendto_all_butone(one, from, pattern, par1, par2, par3, par4, par5,
			  par6, par7, par8)
aClient *one, *from;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	Reg1	int	i;
	Reg2	aClient *cptr;

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			sendto_prefix_one(cptr, from, pattern,
					  par1, par2, par3, par4,
					  par5, par6, par7, par8);

	return 0;
}

/*
** sendto_ops
**	Send to *local* ops only.
*/
int	sendto_ops(pattern, par1, par2, par3, par4, par5, par6, par7, par8)
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	Reg1	aClient *cptr;
	Reg2	int	i;
	char	nbuf[1024];

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && SendServNotice(cptr))
		    {
			sprintf(nbuf, "NOTICE %s :*** Notice -- ", cptr->name);
			strncat(nbuf, pattern, sizeof(nbuf) - strlen(nbuf));
			sendto_one(cptr, nbuf,
				   par1, par2, par3, par4,
				   par5, par6, par7, par8);
		    }
	return 0;
}

/*
** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
int	sendto_ops_butone(one, from, pattern,
			  par1, par2, par3, par4, par5, par6, par7, par8)
aClient *one, *from;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
    {
	Reg1	int	i;
	Reg2	aClient *cptr;

	if (IsPerson(from))
		return 0;
	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
      		sendto_prefix_one(cptr->from, from, pattern,
				  par1, par2, par3, par4,
				  par5, par6, par7, par8);
	    }
	return 0;
    }
#endif

/*
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */
int	sendto_prefix_one(to, from, pattern,
			  par1, par2, par3, par4, par5, par6, par7, par8)
aClient *to;
Reg1	aClient *from;
char	*pattern, *par1, *par2, *par3, *par4, *par5, *par6, *par7, *par8;
{
	char	sender[HOSTLEN+NICKLEN+USERLEN+5];
	anUser	*user;
	int	flag = 0;

	if (to && from && MyConnect(to) && IsPerson(from) && IsPerson(to) &&
	    !mycmp(par1,from->name))
	    {
		user = from->user;
		bzero(sender, sizeof(sender));
		strcpy(sender, from->name);
		if (user)
		    {
			if (user->username && *user->username)
			    {
				strcat(sender, "!");
				strcat(sender, user->username);
			    }
			if (user->host && *user->host && !MyConnect(from))
			    {
				strcat(sender, "@");
				strcat(sender, user->host);
				flag = 1;
			    }
		    }
		/*
		** flag is used instead of index(sender, '@') for speed and
		** also since username/nick may have had a '@' in them. -avalon
		*/
		if (!flag && MyConnect(from))
		    {
			strcat(sender, "@");
			if (IsUnixSocket(from))
				strcat(sender, user->host);
			else
				strcat(sender, from->sockhost);
		    }
		par1 = sender;
	    }
	return sendto_one(to, pattern, par1, par2, par3, par4,
			  par5, par6, par7, par8);
}
