/*************************************************************************
 ** date.c  Beta v2.0    (23 Mar 1989)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 **
 *************************************************************************/

char date_id[]="date.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include <sys/time.h>

static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static char *weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

char *date() {
	long clock;
	struct tm *ltbuf;
	static char buf[80];
	struct timeval tp;
	struct timezone tzp;

	time(&clock);
	ltbuf = localtime(&clock);
	gettimeofday(&tp, &tzp);
	sprintf(buf, "%s %s %d 19%02d -- %02d:%02d %s",
		weekdays[ltbuf->tm_wday], months[ltbuf->tm_mon],
		ltbuf->tm_mday, ltbuf->tm_year, ltbuf->tm_hour,
		ltbuf->tm_min, timezone(tzp.tz_minuteswest, ltbuf->tm_isdst));
	return buf;
}
