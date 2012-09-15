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
#include "struct.h"

extern int errno; /* ...seems that errno.h doesn't define this everywhere */

dummy()
    {
#if !HAVE_RELIABLE_SIGNALS
	signal(SIGALRM, dummy);
#endif
	return(0);
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
	int retval, errtmp;

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

