/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas.c
 *   Copyright (C) 1990 Markku Savela
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

/* hpiirai 17 May 1990
 * Fixed m_whowas() to produce the correct time.
 */

/* -- Jto -- 12 May 1990
 * Added newline removal at the end of ctime().
 */

/* -- Jto -- 10 May 1990
 * Changed memcpy into bcopy
 * Added sys.h include to get bcopy work.
 */

/*
** This module contains utilites for keeping the
** history/database of recent users.
*/
#include "struct.h"
#include "common.h"
#include "sys.h"

#include "numeric.h"
#include "whowas.h"

typedef struct Home
    {
	int refcnt;
	char info[REALLEN+1];
	aClient *online; /* NULL, if user signed off */
	anUser *user;
    } aHome;
	
typedef struct Name
    {
	struct Name *next;
	struct Name *prev;
	struct Home *home;
	unsigned long time;
	char name[HOSTLEN+1];
    } aName;

static aName *NameHistory = NULL, *LastHistory = NULL;
static int HistoryLength = 0;
/*
** MaxHistoryLength
**	Max number of name changes remembered. This is defined as
**	a variable so that it will be easy to change it in runtime
**	if needed (should even work if set to 0).
*/
static int MaxHistoryLength = NICKNAMEHISTORYLENGTH;

add_history(cptr)
aClient *cptr;
    {
	Reg1 aName *new;

	if (cptr->name[0] == '\0')
		return 0; /* No old name defined, no history to save */
	if ((new = (aName *)malloc(sizeof(aName))) == NULL)
		return -1;
	if ((new->home = (aHome *)cptr->history) == NULL)
	    {
		/*
		** A fresh user. Allocate a Home block for the
		** fixed user specific information.
		*/
		if ((new->home = (aHome *)malloc(sizeof(aHome))) == NULL)
		    {
			free((char *)new);
			return -1;
		    }
		cptr->history = (char *)new->home;
		new->home->online = cptr;
		new->home->user = cptr->user;
		cptr->user->refcnt += 1;
		bcopy(cptr->info,new->home->info,sizeof(new->home->info));
		new->home->refcnt = 1;
	    }
	else
		/*
		** User changing nick. Just increment reference count
		*/
		new->home->refcnt += 1;
	strncpyzt(new->name,cptr->name,sizeof(new->name));
	new->time = time(NULL);
	new->prev = NULL;
	new->next = NameHistory;
	if (NameHistory != NULL)
		NameHistory->prev = new;
	NameHistory = new;
	if (LastHistory == NULL)
		LastHistory = new;
	HistoryLength += 1;
	/*
	** Release History blocks if max exeeded. (Note that this
	** is only after 'new' is added--easier this way, because we
	** don't need to worry about 'home' disappearing in case the
	** users previous history entry get's now deleted...)
	*/
	if (HistoryLength > MaxHistoryLength && (new = LastHistory))
	    {
		if (--(new->home->refcnt) == 0)
		    {
			free_user(new->home->user);
			if (new->home->online)
				new->home->online->history = NULL;
			free(new->home);
		    }
		LastHistory = new->prev;
		if (LastHistory != NULL)
			LastHistory->next = NULL;
		HistoryLength -= 1;
		free(new);
	    }
	return 0;
    }

/*
** OffHistory
**	This is called when client signs off the system.
*/
off_history(cptr)
aClient *cptr;
    {
	add_history(cptr);
	if (cptr->history)
		((aHome *)(cptr->history))->online = NULL;
    }

/*
** GetHistory
**	Return the current client that was using the given
**	nickname within the timelimit. Returns NULL, if no
**	one found...
*/
aClient *get_history(nick,timelimit)
char *nick;
long timelimit;
    {
	Reg1 aName *next;

	timelimit = time(NULL) - timelimit;
	/*
	** Note: history chain is in ordered by time
	*/
	for (next = NameHistory;
	     next != NULL && next->time > timelimit;
	     next = next->next)
		if (mycmp(nick,next->name) == 0)
			return next->home->online;
	return NULL;
    }

/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int m_whowas(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int parc;
char *parv[];
    {
	aName *next;

 	if (parc < 2)
	    {
		sendto_one(sptr, ":%s %d %s :No nickname specified",
			   me.name, ERR_NONICKNAMEGIVEN, sptr->name);
		return 0;
	    }
	for (next = NameHistory; next != NULL; next = next->next)
		if (mycmp(parv[1],next->name) == 0)
		    {
			sendto_one(sptr,":%s %d %s %s %s %s * :%s",
				   me.name, RPL_WHOWASUSER,
				   sptr->name, next->name,
				   next->home->user->username,
				   next->home->user->host,
				   next->home->info);
			sendto_one(sptr,":%s %d %s %s :%s",
				   me.name, 
				   RPL_WHOISSERVER,
				   sptr->name,
				   next->home->user->server,
				   myctime(next->time));
			if (next->home->user->away)
				sendto_one(sptr,":%s %d %s %s :%s",
					   me.name, RPL_AWAY,
					   sptr->name, next->name,
					   next->home->user->away);
			return 0;
		    }
	sendto_one(sptr, ":%s %d %s %s :There was no such nickname",
		   me.name, ERR_WASNOSUCHNICK, sptr->name, parv[1]);
	return 0;
    }

