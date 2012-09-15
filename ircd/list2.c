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

#ifndef lint
static  char sccsid[] = "@(#)list.c	2.17 3/30/93 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#ifdef	DBMALLOC
#include "malloc.h"
#endif
void	free_link PROTO((Link *));
Link	*make_link PROTO(());

static	struct	liststats {
	int	inuse;
	int	free;
} listc[8];

#define	LC_CLOC	0
#define	LC_CREM 1
#define	LC_SERV	2
#define	LC_LINK	3
#define	LC_USER	4
#define	LC_CONF	5
#define	LC_CLAS	6
#define	LC_DBUF	7

void	outofmemory();

static	aClient	*clofree = NULL;
static	aClient	*crefree = NULL;
static	aClass	*clfree = NULL;
static	aConfItem *cofree = NULL;
static	anUser	*ufree = NULL;
static	Link	*lfree = NULL;
static	aServer	*sfree = NULL;

int	numclients = 0;

void	initlists()
{
	bzero(listc, sizeof(struct liststats)* 7);
}

void	outofmemory()
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	restart("Out of Memory");
}

	
/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient	*make_client(from)
aClient	*from;
{
	Reg1	aClient *cptr = NULL;
	Reg2	unsigned size = CLIENT_REMOTE_SIZE;

	/*
	 * Check freelists first to see if we can grab a client without
	 * having to call malloc.
	 */
	if (!from)
	    {
		size = CLIENT_LOCAL_SIZE;
		if (cptr = clofree)
		    {
			clofree = cptr->next;
			listc[LC_CLOC].free--;
		    }
	    }
	else if (cptr = crefree)
	    {
			crefree = cptr->next;
			listc[LC_CREM].free--;
	    }

	if (!cptr)
	    {
		if (!(cptr = (aClient *)malloc(size)))
			outofmemory();
		else
		    {
			if (size == CLIENT_LOCAL_SIZE)
				listc[LC_CLOC].inuse++;
			else
				listc[LC_CREM].inuse++;
		    }
	    }

	bzero((char *)cptr, (int)size);

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->hnext = NULL;
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	    {
		cptr->since = cptr->lasttime = cptr->firsttime = time(NULL);
		cptr->confs = NULL;
		cptr->sockhost[0] = '\0';
		cptr->buffer[0] = '\0';
		cptr->authfd = -1;
	    }
	return (cptr);
}

void	free_client(cptr)
aClient	*cptr;
{
	if (cptr->fd != -1)
	    {
		bzero((char *)cptr, CLIENT_LOCAL_SIZE);
		listc[LC_CLOC].free++;
		cptr->next = clofree;
		clofree = cptr;
	    }
	else
	    {
		bzero((char *)cptr, CLIENT_REMOTE_SIZE);
		listc[LC_CREM].free++;
		cptr->next = crefree;
		crefree = cptr;
	    }
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
anUser	*make_user(cptr)
aClient *cptr;
{
	Reg1	anUser	*user;

	user = cptr->user;
	if (!user)
		if (user = ufree)
		    {
			ufree = user->nextu;
			listc[LC_USER].free--;
			cptr->user = user;
		    }
	if (!user)
	    {
		user = (anUser *)MyMalloc(sizeof(anUser));
		listc[LC_USER].inuse++;
		user->away = NULL;
		user->refcnt = 1;
		user->channel = NULL;
		user->invited = NULL;
		cptr->user = user;
	    }
	return user;
}

aServer	*make_server(cptr)
aClient	*cptr;
{
	Reg1	aServer	*serv = cptr->serv;

	if (!serv)
		if (serv = sfree)
		    {
			cptr->serv = serv;
			sfree = serv->nexts;
			listc[LC_SERV].free--;
		    }
	if (!serv)
	    {
		serv = (aServer *)MyMalloc(sizeof(aServer));
		listc[LC_SERV].inuse++;
		serv->user = NULL;
		serv->nexts = NULL;
		*serv->by = '\0';
		*serv->up = '\0';
		cptr->serv = serv;
	    }
	return cptr->serv;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void	free_user(user)
Reg1	anUser	*user;
{
	if (--user->refcnt <= 0)
	    {
		if (user->away)
			(void)free((char *)user->away);
		bzero((char *)user, sizeof(*user));
		user->nextu = ufree;
		ufree = user;
		listc[LC_USER].free++;
	    }
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void	remove_client_from_list(cptr)
Reg1	aClient	*cptr;
{
	checklist();
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else
	    {
		client = cptr->next;
		client->prev = NULL;
	    }
	if (cptr->next)
		cptr->next->prev = cptr->prev;
	if (cptr->user)
	    {
		add_history(cptr);
		off_history(cptr);
		(void)free_user(cptr->user);
	    }
	if (cptr->serv)
	    {
		if (cptr->serv->user)
			free_user(cptr->serv->user);
		listc[LC_SERV].free++;
		cptr->serv->nexts = sfree;
		sfree = cptr->serv;
	    }
	free_client(cptr);
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void	add_client_to_list(cptr)
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
	return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link	*find_user_link(lp, ptr)
Reg1	Link	*lp;
Reg2	aClient *ptr;
{
	while (lp && ptr)
	   {
		if (lp->value.cptr == ptr)
			return (lp);
		lp = lp->next;
	    }
	return NULL;
}

Link	*make_link()
{
	Reg1	Link	*lp;

	if (lp = lfree)
	    {
		lfree = lp->next;
		listc[LC_LINK].free--;
	    }
	else
	    {
		lp = (Link *)MyMalloc(sizeof(Link)*3);
		lp->next = lp+1;
		lp->next->next = lp+2;
		lp->next->next->next = lfree;
		lfree = lp->next;
		listc[LC_LINK].inuse += 3;
		listc[LC_LINK].free += 2;
	    }
	return lp;
}

void	free_link(lp)
Reg1	Link	*lp;
{
	bzero((char *)lp, sizeof(*lp));
	lp->next = lfree;
	lfree = lp;
	listc[LC_LINK].free++;
}


aClass	*make_class()
{
	Reg1	aClass	*tmp;

	if (tmp = clfree)
	    {
		listc[LC_CLAS].free--;
		clfree = tmp->next;
	    }
	else
	    {
		tmp = (aClass *)MyMalloc(sizeof(aClass));
		listc[LC_CLAS].inuse++;
	    }
	return tmp;
}

void	free_class(tmp)
Reg1	aClass	*tmp;
{
	bzero((char *)tmp, sizeof(*tmp));
	tmp->next = clfree;
	clfree = tmp;
	listc[LC_CLAS].free++;
}

aConfItem	*make_conf()
{
	Reg1	aConfItem *aconf;

	if (aconf = cofree)
	    {
		cofree = aconf->next;
		listc[LC_CONF].free--;
	    }
	else
	    {
		aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));
		listc[LC_CONF].inuse++;
		bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
		aconf->next = NULL;
		aconf->host = aconf->passwd = aconf->name = NULL;
		aconf->status = CONF_ILLEGAL;
		Class(aconf) = 0;
	    }
	return (aconf);
}

void	free_conf(aconf)
aConfItem *aconf;
{
	MyFree(aconf->host);
	if (aconf->passwd)
		bzero(aconf->passwd, strlen(aconf->passwd));
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	bzero((char *)aconf, sizeof(*aconf));
	aconf->next = cofree;
	cofree = aconf;
	listc[LC_CONF].free++;
	return;
}

void	send_listinfo(cptr, name)
aClient	*cptr;
char	*name;
{
	static	char	*labels[] = { "Local", "Remote", "Servs", "Links",
				      "Users", "Confs", "Classes", "dbufs" };
	static	int	sizes[] = { CLIENT_LOCAL_SIZE, CLIENT_REMOTE_SIZE,
				    sizeof(aServer), sizeof(Link),
				    sizeof(anUser), sizeof(aConfItem),
				    sizeof(aClass), sizeof(dbufbuf)};

	struct	liststats *ls = listc;
	int	inuse = 0, mem = 0, tmp = 0, i;

	listc[LC_DBUF].inuse = dbufblocks;
	listc[LC_DBUF].free = dbufblocks - dbufalloc;
	for (i = 0; i < 8; i++, ls++)
	    {
		tmp = sizes[i] * ls->inuse;
		sendto_one(cptr, ":%s NOTICE %s :%s: inuse: %d(%d) free: %d",
			   me.name, cptr->name,
			   labels[i], ls->inuse, tmp, ls->free);
		inuse += ls->inuse;
		mem += tmp;
	    }

	sendto_one(cptr, ":%s NOTICE %s :Totals: inuse %d %d",
		   me.name, name, inuse, mem);
}
