/************************************************************************
 *   IRC - Internet Relay Chat, ircd/date.c
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

/*
 * $Id: date.c,v 6.1 1991/07/04 21:05:15 gruner stable gruner $
 *
 * $Log: date.c,v $
 * Revision 6.1  1991/07/04  21:05:15  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:37  gruner
 * frozen beta revision 2.6.1
 *
 */

char date_id[]="date.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "config.h"
#include "common.h"
#include "sys.h"

static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static char *weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};


char *date(clock) 
long clock;
{
  struct tm *ltbuf;
  static char buf[80];
  struct timeval tp;
  struct timezone tzp;
  char *timezonename;
#if !(HPUX || AIX)
  extern char *timezone();
#endif

  if (!clock) 
    time(&clock);
  ltbuf = localtime(&clock);
  gettimeofday(&tp, &tzp);

#if HPUX || AIX
  tzset();
  timezonename = tzname[ltbuf->tm_isdst];
#else
  timezonename = timezone(tzp.tz_minuteswest, ltbuf->tm_isdst);
#endif

  sprintf(buf, "%s %s %d 19%02d -- %02d:%02d %s",
	  weekdays[ltbuf->tm_wday], months[ltbuf->tm_mon],ltbuf->tm_mday,
	  ltbuf->tm_year, ltbuf->tm_hour,	ltbuf->tm_min, timezonename);
  return buf;
}

/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time valie as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there ane never 'refreshes' it--zapping that
 **   might break things in other places..
 **
 **/

char *myctime(value)
long value;
{
  char *p;
  static char buf[28];

  strcpy(buf, ctime(&value));
  if ((p = index(buf, '\n')) != NULL)
    *p = '\0';
  return buf;
}
