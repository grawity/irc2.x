/************************************************************************
 *   IRC - Internet Relay Chat, ircd/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 03 Jun 1990
 * Added chname initialization...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() to channel.c
 */

/* -- Jto -- 10 May 1990
 * Added #include <sys.h>
 * Changed memset(xx,0,yy) into bzero(xx,yy)
 */

char list_id[] = "list.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"

extern aClient *client;
extern aConfItem *conf;

static outofmemory()
    {
	debug(DEBUG_FATAL, "Out of memory: restarting server...");
	restart();
	return -1;
    }

	
/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from != NULL,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient *make_client(from)
aClient *from;
    {
	aClient *cptr;
	int size = (from == NULL) ? CLIENT_LOCAL_SIZE : CLIENT_REMOTE_SIZE;

	if ((cptr = (aClient *) calloc(1, size)) == NULL)
		outofmemory();

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->user = NULL;
	cptr->history = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	cptr->hopcount = 0;
	if (size == CLIENT_LOCAL_SIZE) {
	  cptr->since = cptr->lasttime = cptr->firsttime = time(NULL);
	  cptr->confs = (Link *) 0;
	  cptr->sockhost[0] = '\0';
#ifdef DOUBLE_BUFFER
	  cptr->ocount = 0;
	  cptr->obuffer[0] = '\0';
#endif
	  cptr->count = 0;
	  cptr->buffer[0] = '\0';
	}
	return (cptr);
    }

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
make_user(sptr)
aClient *sptr;
    {
	if (sptr->user != NULL)
		return -1;
	sptr->user = (anUser *)MyMalloc(sizeof(anUser));
	bzero((char *)sptr->user,sizeof(anUser));
	sptr->user->away = NULL;
	sptr->user->refcnt = 1;
	sptr->user->channel = NULL;
	sptr->user->invited = NULL;
	return 0;
    }

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
free_user(user)
anUser *user;
    {
	if (--user->refcnt == 0)
	    {
		if (user->away)
			free(user->away);
		free(user);
	    }
    }

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
remove_client_from_list(cptr)
aClient	*cptr;
{
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else {
		client = cptr->next;
		client->prev = NULL;
	}
	if (cptr->next)
		cptr->next->prev = cptr->prev;
	if (cptr->user) {
		off_history(cptr);
		free_user(cptr->user);
	}
	free(cptr);
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
add_client_to_list(cptr)
aClient	*cptr;
{
	/*
	 * since we always insert new clients to the top of the list,
	 * this should mean the "me" is the bottom most item in the list.
	 */
	cptr->next = client;
	client = cptr;
	if (cptr->next)
		cptr->next->prev = cptr;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link	*find_user_link(link, ptr)
Reg1	Link	*link;
Reg2	aClient *ptr;
{
	while (link && ptr)
	   {
		if (link->value.cptr == ptr)
			return (link);
		link = link->next;
	    }
	return (Link *)NULL;
}
