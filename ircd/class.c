/************************************************************************
 *   IRC - Internet Relay Chat, ircd/class.c
 *   Copyright (C) 1990 Darren Reed
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

#ifndef lint
char class_id[] = "class.c v1.4 (c) 1991 Darren Reed";
#endif

#include "struct.h"
#include "common.h"
#include "numeric.h"

extern char *strerror();
extern int errno;
extern void restart();

#define BAD_CONF_CLASS		-1
#define BAD_PING		-2
#define BAD_CLIENT_CLASS	-3

aClass *classes;

int	get_conf_class(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfClass(aconf));

	debug(DEBUG_DEBUG,"No Class For %s",
	      (aconf) ? aconf->name : "*No Conf*");

	return (BAD_CONF_CLASS);

}

static	int	get_conf_ping(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfPingFreq(aconf));

	debug(DEBUG_DEBUG,"No Ping For %s",
	      (aconf) ? aconf->name : "*No Conf*");

	return (BAD_PING);
}

/*
 * Return the highest class for a client by virtue of its associated conf
 * lines or BAD_CLIENT_CLASS if there are no attached conf lines.
 */
int	get_client_class(acptr)
aClient	*acptr;
{
	int i, retc = BAD_CLIENT_CLASS;
	Link	*tmp;

	if (acptr && !IsMe(acptr)  && (acptr->confs))
		for (tmp = acptr->confs; tmp; tmp = tmp->next) {
			if (!tmp->value.aconf)
				continue;
			i = get_conf_class(tmp->value.aconf);
			if (i > retc)
				retc = i;
		}

	debug(DEBUG_DEBUG,"Returning Class %d For %s",retc,acptr->name);

	return (retc);
}

/*
 * Find the lowest ping value for a client from all its associated conf lines.
 * In an ideal situation, there should be only one, but in can of others, we
 * check just to be sure. If there are no conf lines attached, the default
 * ping value is used.
 */
int	get_client_ping(acptr)
aClient	*acptr;
{
	int	ping = 0, ping2;
	aConfItem	*aconf;
	Link	*link;

	link = acptr->confs;

	if (link)
		while (link)
		    {
			aconf = link->value.aconf;
			if (aconf->status & (CONF_CLIENT|CONF_CONNECT_SERVER|
					     CONF_NOCONNECT_SERVER))
			    {
				ping2 = get_conf_ping(aconf);
				if ((ping2 != BAD_PING) && ((ping > ping2) ||
				    !ping))
					ping = ping2;
			     }
			link = link->next;
		    }
	else {
		ping = PINGFREQUENCY;
		debug(DEBUG_DEBUG,"No Attached Confs");
	}
	if (ping <= 0)
		ping = PINGFREQUENCY;
	debug(DEBUG_DEBUG,"Client %s Ping %d", acptr->name, ping);
	return (ping);
}

int	get_con_freq(clptr)
aClass	*clptr;
{
	if (clptr)
		return (ConFreq(clptr));
	else
		return (CONNECTFREQUENCY);
}

/*
 * When adding a class, check to see if it is already present first.
 * if so, then update the information for that class, rather than create
 * a new entry for it and later delete the old entry.
 * if no present entry is found, then create a new one and add it in
 * immeadiately after the first one (class 0).
 */
void add_class(class, ping, confreq, maxli)
int class, ping, confreq, maxli;
{
	aClass *t, *p;

	t = find_class(class);
	if ((t == classes) && (class != 0))
	    {
		p = (aClass *)MyMalloc(sizeof(aClass));
		NextClass(p) = NextClass(t);
		NextClass(t) = p;
	    }
	else
		p = t;

	debug(DEBUG_DEBUG,"Add Class %d: p %x t %x - cf: %d pf: %d ml: %d",
		class, p, t, confreq, ping, maxli);

	Class(p) = class;
	ConFreq(p) = confreq;
	PingFreq(p) = ping;
	MaxLinks(p) = maxli;
	if (p != t)
		Links(p) = 0;
}

/*
 * Search for a class in those already present by the class number.
 * If it isnt found, the default class (0) is returned.
 */
aClass *find_class(cclass)
int cclass;
{
	aClass *cltmp;

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
		if (Class(cltmp) == cclass)
			return cltmp;
	return classes;
}

/*
 * Check the class list for entries which are void and marked for deletion.
 */
void check_class()
{
	Reg1 aClass *cltmp, *cltmp2;

	debug(DEBUG_DEBUG, "Class check:");

	for (cltmp2 = cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp2))
	    {
		debug(DEBUG_DEBUG, "Class %d : CF: %d PF: %d ML: %d LI: %d",
			Class(cltmp), ConFreq(cltmp), PingFreq(cltmp),
			MaxLinks(cltmp), Links(cltmp));

		if (MaxLinks(cltmp) < 0)
		    {
			NextClass(cltmp2) = NextClass(cltmp);
			if (Links(cltmp) <= 0)
				free(cltmp);
		    }
		else
			cltmp2 = cltmp;
	    }
}

/*
 * Build initial class (0) from defined defaults in config.h
 */
void initclass()
{
	classes = (aClass *)MyMalloc(sizeof(aClass));

	Class(FirstClass()) = 0;
	ConFreq(FirstClass()) = CONNECTFREQUENCY;
	PingFreq(FirstClass()) = PINGFREQUENCY;
	MaxLinks(FirstClass()) = MAXIMUM_LINKS;
	Links(FirstClass()) = 0;
	NextClass(FirstClass()) = (aClass *) NULL;
}

/*
 * Function to send reponses to the "STATS Y" command.
 */
int	report_classes(sptr)
aClient *sptr;
{
	Reg1 aClass *cltmp;

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
		sendto_one(sptr,":%s %d %s Y %d %d %d %d",
			   me.name, RPL_STATSYLINE, sptr->name,
			   Class(cltmp), PingFreq(cltmp), ConFreq(cltmp),
			   MaxLinks(cltmp));
	return 0;
}
