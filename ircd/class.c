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

#include "struct.h"
#include "common.h"

#define BAD_CONF_CLASS		-1
#define BAD_PING		-2
#define BAD_CLIENT_CLASS	-3

aClass *classes;

int	GetConfClass(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfClass(aconf));

	debug(DEBUG_DEBUG,"No Class For %s",
	      (aconf) ? aconf->name : "*No Conf*");

	return (BAD_CONF_CLASS);

}

static	int	GetConfPing(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfPingFreq(aconf));

	debug(DEBUG_DEBUG,"No Ping For %s",
	      (aconf) ? aconf->name : "*No Conf*");

	return (BAD_PING);
}



int	GetClientClass(acptr)
aClient	*acptr;
{
	int i = 0, retc = BAD_CLIENT_CLASS;
	Link	*tmp;

	if (acptr && !IsMe(acptr)  && (acptr->confs))
		for (tmp = acptr->confs; tmp; tmp = tmp->next) {
			if (tmp->value == (char *)0)
				continue;
			i = GetConfClass((aConfItem *)tmp->value);
			if (i > retc)
				retc = i;
		}

	debug(DEBUG_DEBUG,"Returning Class %d For %s",retc,acptr->name);

	return (retc);
}

int	GetClientPing(acptr)
aClient	*acptr;
{
	int	ping = 0, ping2;
	aConfItem	*aconf;
	Link	*link;

	link = acptr->confs;

	if (link)
		while (link)
		    {
			aconf = (aConfItem *)link->value;
			ping2 = GetConfPing(aconf);
			if ((ping2 != BAD_PING) && ((ping > ping2) || !ping))
				ping = ping2;
			link = link->next;
		    }
	else {
		ping = PINGFREQUENCY;
		debug(DEBUG_DEBUG,"No Attached Confs");
	}
	debug(DEBUG_DEBUG,"Client %s Ping %d", acptr->name, ping);
	return (ping);
}

int	GetConFreq(clptr)
aClass	*clptr;
{
	if (clptr)
		return (ConFreq(clptr));
	else
		return (CONNECTFREQUENCY);
}

extern char *strerror();
extern int errno;

void AddClass(class, ping, confreq, maxli)
int class, ping, confreq, maxli;
{
  aClass *t, *p;

  t = find_class(class);
  p = (aClass *) malloc(sizeof(aClass));
  if (!p) {
    debug(DEBUG_FATAL, "malloc (addclass) %s", strerror(errno));
    restart();
  }

  NextClass(p) = NextClass(t);
  NextClass(t) = p;

  Class(p) = class;
  ConFreq(p) = confreq;
  PingFreq(p) = ping;
  MaxLinks(p) = maxli;
  Links(p) = 0;
}

aClass *find_class(cclass)
int cclass;
{
  aClass *cltmp;

  for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
    if (Class(cltmp) == cclass && (MaxLinks(cltmp) >= 0))
      return cltmp;
  return classes;
}

void check_class()
{
  aClass *cltmp, *cltmp2;

  debug(DEBUG_DEBUG, "Class check:");

  for (cltmp2 = cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp2)) {
    debug(DEBUG_DEBUG, "Class %d : CF: %d PF: %d ML: %d",
	  Class(cltmp), ConFreq(cltmp), PingFreq(cltmp),
	  MaxLinks(cltmp));
    if (MaxLinks(cltmp) < 0) {
      NextClass(cltmp2) = NextClass(cltmp);
      if (Links(cltmp) <= 0)
	free(cltmp);
    } else
      cltmp2 = cltmp;
  }
}

void initclass()
{
  classes = (aClass *) malloc(sizeof(aClass));

  if (!FirstClass()) {
    debug(DEBUG_FATAL, "malloc (initclass) %s", strerror(errno));
    restart();
  }
  Class(FirstClass()) = 0;
  ConFreq(FirstClass()) = CONNECTFREQUENCY;
  PingFreq(FirstClass()) = PINGFREQUENCY;
  MaxLinks(FirstClass()) = MAXIMUM_LINKS;
  Links(FirstClass()) = 0;
  NextClass(FirstClass()) = (aClass *) 0;
}
