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

char list_id[] = "list.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#if HAS_ANSI_INCLUDES
#	include <stdlib.h>
#	include <string.h>
#else
#if HAS_SYSV_INCLUDES
#	include <memory.h>
#	include <malloc.h>
#else
char *malloc();
free();
char *memset();
#endif
#endif
#include "struct.h"

#ifndef NULL
#define NULL 0
#endif

extern aClient *client;
extern aConfItem *conf;
extern int maxusersperchannel;

static outofmemory()
    {
	perror("malloc");
	debug(DEBUG_FATAL, "Out of memory: restarting server...");
	restart();
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

	if ((cptr = (aClient *) malloc(size)) == NULL)
		outofmemory();
	/*
	** Zero out the block and fill in the non-zero fields
	**
	** (hmm, perhaps this is not the most efficient way,
	**  if Client has long fields like buffer... --msa)
	*/
	memset((char *)cptr,0,size);
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->user = NULL;
	cptr->history = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	cptr->since = cptr->lasttime = getlongtime();
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
		return;
	if ((sptr->user = (anUser *)malloc(sizeof(anUser))) == NULL)
		outofmemory();
	memset((char *)sptr->user,0,sizeof(anUser));
	sptr->user->away = NULL;
	sptr->user->refcnt = 1;
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

int is_full(channel, users)
int channel, users;
    {
	if (channel > 0 && channel < 10)
		return(0);
	if (users >= maxusersperchannel)
		return(1);
	return(0);
    }

