/************************************************************************
 *   IRC - Internet Relay Chat, lib/ircd/bsd.c
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

char bsd_id[] = "bsd.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <signal.h>
#include <sys/errno.h>
#include "config.h"
#include "common.h"
#include "sys.h"

extern int errno; /* ...seems that errno.h doesn't define this everywhere */

VOIDSIG dummy()
    {
#if !HAVE_RELIABLE_SIGNALS
	signal(SIGALRM, dummy);
#endif
    }

/*
** DeliverIt
**	Attempt to send a sequence of bytes to the connection.
**	Returns
**
**	< 0	Some fatal error occurred, (but not EWOULDBLOCK).
**		This return is a request to close the socket and
**		clean up the link.
**	
**	>= 0	No real error occurred, returns the number of
**		bytes actually transferred. EWOULDBLOCK and other
**		possibly similar conditions should be mapped to
**		zero return. Upper level routine will have to
**		decide what to do with those unwritten bytes...
**
**	*NOTE*	alarm calls have been preserved, so this should
**		work equally well whether blocking or non-blocking
**		mode is used...
*/
int DeliverIt(fd, str, len)
int fd, len;
char *str;
    {
	int retval;

	alarm(WRITEWAITDELAY);
#if VMS
	retval = netwrite(fd, str, len);
#else
	retval = write(fd, str, len);
	/*
	** Convert WOULDBLOCK to a return of "0 bytes moved". This
	** should occur only if socket was non-blocking. Note, that
	** all is Ok, if the 'write' just returns '0' instead of an
	** error and errno=EWOULDBLOCK.
	**
	** ...now, would this work on VMS too? --msa
	*/
	if (retval < 0 && errno == EWOULDBLOCK)
		retval = 0;
#endif
	alarm(0);
	return(retval);
    }

#if NEED_STRTOK
/*
** 	strtok.c --  	walk through a string of tokens, using a set
**			of separators
**			ag 9/90
**
**	$Id: strtok.c,v 1.1 90/09/15 12:59:51 ag Exp $
*/

char * strtok(str, fs)
char *str, *fs;
{
	static char *pos = NULL;	/* keep last position across calls */
	Reg1 char *tmp;

	if (str)
		pos = str;		/* new string scan */

	while (pos && *pos && index(fs, *pos) != NULL)
		*pos++ = NULL; 		/* delete & skip leading separators */

	if (!pos || !*pos)
		return (pos = NULL); 	/* string contains only sep's */

	tmp = pos; 			/* now, keep position of the token */

	while (*pos && index(fs, *pos) == NULL)
		pos++; 			/* skip content of the token */

	if (*pos)
		*pos++ = NULL;		/* remove first sep after the token */
	else	pos = NULL;		/* end of string */

	return(tmp);
}

#endif /* NEED_STRTOK */

#if NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   ag 11/90
**	$Id$
*/

char *strerror(err_no)
int err_no;
{
	extern char *sys_errlist[];	 /* Sigh... hopefully on all systems */
	extern int sys_nerr;

	return(err_no > sys_nerr ? 
		"Undefined system error" : sys_errlist[err_no]);
}

#endif /* NEED_STRERROR */

#if NEED_INET_NTOA
/*
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX doesn't have this)
**			ag 11/90
**	$Id$
*/

char *inet_ntoa(in)
struct in_addr in;
{
	static char buf[16];

	(void) sprintf(buf, "%d.%d.%d.%d",
			    (int)  in.s_net,
			    (int)  in.s_host,
			    (int)  in.s_lh,
			    (int)  in.s_impno);

	return buf;
}
#endif /* NEED_INET_NTOA */

#if NEED_INET_ADDR
/*
**	inet_addr --	convert a character string
**			to the internet number
**		   	ag 11/90
**	$Id$
**
*/

/* netinet/in.h and sys/types.h already included from common.h */

unsigned long inet_addr(host)
char *host;
{
	char hosttmp[16];
	struct in_addr addr;
	Reg1 char *tmp;
	Reg2 int i = 0;
	extern char *strtok();
	extern int atoi();

	if (host == NULL)
		return -1;

	bzero((char *)&addr, sizeof(addr));
	strncpy(hosttmp, host, sizeof(hosttmp));
	host = hosttmp;

	for (; tmp = strtok(host, "."); host = NULL) 
		switch(i++)
		{
		case 0:	addr.s_net   = (unsigned char) atoi(tmp); break;
		case 1:	addr.s_host  = (unsigned char) atoi(tmp); break;
		case 2:	addr.s_lh    = (unsigned char) atoi(tmp); break;
		case 3:	addr.s_impno = (unsigned char) atoi(tmp); break;
		}

	return(addr.s_addr ? addr.s_addr : -1);
}
#endif /* NEED_INET_ADDR */

#if NEED_INET_NETOF
/*
**	inet_netof --	return the appropriate part of the internet number
**			to reflect net-class.
**			ag 11/90
**	$Id$
**
*/

int inet_netof(in)
struct in_addr in;
{
	int addr = in.s_net;

   	if (addr & 0x80 == 0)
		return ((int) in.s_net);

	if (addr & 0x40 == 0)
		return ((int) in.s_net * 256 + in.s_host);

	return ((int) in.s_net * 256 + in.s_host * 256 + in.s_lh);
}
#endif /* NEED_INET_NETOF */
