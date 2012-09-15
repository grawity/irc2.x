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

/*
 * $Id: list.c,v 6.1 1991/07/04 21:05:19 gruner stable gruner $
 *
 * $Log: list.c,v $
 * Revision 6.1  1991/07/04  21:05:19  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:39  gruner
 * frozen beta revision 2.6.1
 *
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

char list_id[] = "list.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

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
	cptr->user = NULL;
	cptr->history = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	cptr->since = cptr->lasttime = cptr->firsttime = time(NULL);
	if (size == CLIENT_LOCAL_SIZE) {
	  cptr->confs = (Link *) 0;
	  cptr->sockhost[0] = '\0';
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
	if ((sptr->user = (anUser *)malloc(sizeof(anUser))) == NULL)
		outofmemory();
	bzero((char *)sptr->user,sizeof(anUser));
	sptr->user->away = NULL;
	sptr->user->refcnt = 1;
	sptr->user->channel = (aChannel *) 0;
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

