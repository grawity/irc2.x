/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf.c
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

char conf_id[] = "conf.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <stdio.h>
#include "struct.h"
#include "sys.h"

aConfItem *conf = NULL;
extern int portnum;
extern char *configfile;

static MyFree(x)
char *x;
    {
	if (x != NULL)
		free(x);
    }

static char *MyMalloc(x)
int x;
    {
	char *ret = (char *) malloc(x);

	if (!ret)
	    {
		debug(DEBUG_FATAL, "Out of memory: restarting server...");
		restart();
	    }
	return ret;
    }

#define DupString(x,y) do { x = MyMalloc(strlen(y)+1); strcpy(x, y);}while (0)

static aConfItem *make_conf()
    {
	aConfItem *cptr =(struct Confitem *) MyMalloc(sizeof(struct Confitem));
	
	cptr->next = NULL;
	cptr->host = cptr->passwd = cptr->name = NULL;
	cptr->status = CONF_ILLEGAL;
	cptr->clients = 0;
	cptr->port = 0;
	cptr->hold = 0;
	return (cptr);
    }

static free_conf(aconf)
aConfItem *aconf;
    {
	MyFree(aconf->host);
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	free(aconf);
    }

/*
** detach_conf
**	Disassociate configuration from the client.
*/
detach_conf(cptr)
aClient *cptr;
    {
	aConfItem *aconf = cptr->conf;

	if (aconf != NULL &&
	    --aconf->clients == 0 &&
	    aconf->status == CONF_ILLEGAL)
		free_conf(aconf);
    }

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
attach_conf(aconf,cptr)
aConfItem *aconf;
aClient *cptr;
    {
	if (cptr->conf == aconf)
		return;
	detach_conf(cptr);
	aconf->clients += 1;
	cptr->conf = aconf;
    }

/*
**  Compare if a given string (name) matches the given
**  mask (which can contain wild cards: '*' match any
**  number of chars, '?'=match any single character.
**
**	return	1, if match
**		0, if no match
*/
int matches(mask, name)
char *mask, *name;
    {
	register char m, c;

	for (;; mask++, name++)
	    {
		m = isupper(*mask) ? tolower(*mask) : *mask;
		c = isupper(*name) ? tolower(*name) : *name;
		if (c == '\0')
			break;
		if (m != '?' && m != c)
			break;
	    }
	if (m == '*')
	    {
		for ( ; *mask == '*'; mask++);
		if (*mask == '\0')
			return(0);
		for (; *name && matches(mask, name); name++);
		return(*name ? 0 : 1);
	    }
	else
		return ((m == '\0' && c == '\0') ? 0 : 1);
    }

aConfItem *find_admin()
    {
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
    }

aConfItem *find_me()
    {
	aConfItem *aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;
	
	return (aconf);
    }

aConfItem *find_conf(host, aconf, name, statmask)
char *host, *name;
aConfItem *aconf;
int statmask;
    {
	aConfItem *tmp;
	int len = strlen(host);
	int namelen = name ? strlen(name) : 0;

	if (len > HOSTLEN || namelen > HOSTLEN)
		return aconf;
	for (tmp = conf; tmp; tmp = tmp->next)
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (matches(tmp->host, host) == 0 ||
		     matches(tmp->name, host) == 0))
			/*
			** In addition, if 'name' is defined,
			** require it matches the configuration
			** name field...
			*/
			if (name == NULL || matches(tmp->name, name) == 0)
				break;
	return((tmp) ? tmp : aconf);
    }

rehash()
    {
	aConfItem *tmp = conf, *tmp2;
	while (tmp)
	    {
		tmp2 = tmp->next;
		if (tmp->clients)
		    {
			/*
			** Configuration entry is still in use by some
			** local clients, cannot delete it--mark it so
			** that it will be deleted when the last client
			** exits...
			*/
			tmp->status = CONF_ILLEGAL;
			tmp->next = NULL;
		    }
		else
			free_conf(tmp);
		
		tmp = tmp2;
	    }
	conf = (aConfItem *) 0;
	initconf();
    }

extern char *getfield();

initconf()
    {
	FILE *fd;
	char line[256], *tmp;
	aConfItem *aconf;
	if (!(fd = fopen(configfile,"r")))
		return(-1);
	while (fgets(line,255,fd))
	    {
		if (line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		aconf = make_conf();
		switch (*getfield(line))
		    {
		    case 'C':   /* Server where I should try to connect */
		    case 'c':   /* in case of link failures             */
			aconf->status = CONF_CONNECT_SERVER;
			break;
		    case 'I':   /* Just plain normal irc client trying  */
		    case 'i':   /* to connect me */
			aconf->status = CONF_CLIENT;
			break;
		    case 'K':   /* Kill user line on irc.conf           */
		    case 'k':
			aconf->status = CONF_KILL;
			break;
		    case 'N':   /* Server where I should NOT try to     */
		    case 'n':   /* connect in case of link failures     */
			/* but which tries to connect ME        */
			aconf->status = CONF_NOCONNECT_SERVER;
			break;
		    case 'U':   /* Uphost, ie. host where client reading */
		    case 'u':   /* this should connect.                  */
			/* This is for client only, I must ignore this */
			/* ...U-line should be removed... --msa */
			break;    
		    case 'O':   /* Operator. Line should contain at least */
		    case 'o':   /* password and host where connection is  */
			aconf->status = CONF_OPERATOR;      /* allowed from */
			break;
		    case 'M':   /* Me. Host field is name used for this host */
		    case 'm':   /* and port number is the number of the port */
			aconf->status = CONF_ME;
			break;
		    case 'A':   /* Name, e-mail address of administrator */
		    case 'a':   /* of this server. */
			aconf->status = CONF_ADMIN;
			break;
		    default:
			debug(DEBUG_ERROR, "Error in config file: %s", line);
			break;
		    }
		if (aconf->status == CONF_ILLEGAL)
		    {
			free(aconf);
			continue;
		    }
		aconf->next = conf;
		conf = aconf;
		for (;;) /* Fake loop, that I can use break here --msa */
		    {
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host,tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd,tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name,tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if ((aconf->port = atoi(tmp)) == 0)
				debug(DEBUG_ERROR,
				  "Error in config file, illegal port field");
			break;
		    }
		/*
		** Own port and name cannot be changed after the startup.
		** (or could be allowed, but only if all links are closed
		** first).
		** Configuration info does not override the name and port
		** if previously defined. Note, that "info"-field can be
		** changed by "/rehash".
		*/
		if (aconf->status == CONF_ME)
		    {
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (me.name[0] == '\0' && aconf->host[0])
				strncpyzt(me.name, aconf->host,
					  sizeof(me.name));
			if (portnum <= 0 && aconf->port > 0)
				portnum = aconf->port;
		    }
		debug(DEBUG_NOTICE, "Read Init: (%d) (%s) (%s) (%s) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port);
	    }
    }
