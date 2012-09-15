/************************************************************************
 *   IRC - Internet Relay Chat, common/bsd.c
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

char bsd_id[] = "bsd.c v2.0 (c) 1988 University of Oulu, Computing Center and\
 Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include <signal.h>
#include <sys/errno.h>

extern	int errno; /* ...seems that errno.h doesn't define this everywhere */
extern	char	*sys_errlist[];

#ifdef DEBUGMODE
int	writecalls = 0, writeb[10];
#endif
VOIDSIG dummy()
    {
#ifndef HAVE_RELIABLE_SIGNALS
	signal(SIGALRM, dummy);
#else
# ifdef POSIX_SIGNALS
	struct  sigaction       act;

	act.sa_handler = dummy;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	sigaction(SIGALRM, &act, (struct sigaction *)NULL);
# endif
#endif
    }


/*
** deliver_it
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
int	deliver_it(cptr, str, len)
aClient *cptr;
int	len;
char	*str;
    {
	int	retval;

#ifdef DEBUGMODE
	if (writecalls == 0)
		for (retval = 0; retval < 10; retval++)
			writeb[retval] = 0;
	writecalls++;
#endif
	(void) alarm(WRITEWAITDELAY);
#ifdef VMS
	retval = netwrite(cptr->fd, str, len);
#else
	retval = write(cptr->fd, str, len);
	/*
	** Convert WOULDBLOCK to a return of "0 bytes moved". This
	** should occur only if socket was non-blocking. Note, that
	** all is Ok, if the 'write' just returns '0' instead of an
	** error and errno=EWOULDBLOCK.
	**
	** ...now, would this work on VMS too? --msa
	*/
	if (retval < 0 && errno == EWOULDBLOCK)
	    {
		retval = 0;
		cptr->flags |= FLAGS_BLOCKED;
	    }
	else if (retval > 0)
		cptr->flags &= ~FLAGS_BLOCKED;

#endif
	(void )alarm(0);
#ifdef DEBUGMODE
	if (retval < 0) {
		writeb[0]++;
		debug(DEBUG_ERROR,"write error (%s) to %s",
			sys_errlist[errno], cptr->name);
	} else if (retval == 0)
		writeb[1]++;
	else if (retval < 16)
		writeb[2]++;
	else if (retval < 32)
		writeb[3]++;
	else if (retval < 64)
		writeb[4]++;
	else if (retval < 128)
		writeb[5]++;
	else if (retval < 256)
		writeb[6]++;
	else if (retval < 512)
		writeb[7]++;
	else if (retval < 1024)
		writeb[8]++;
	else
		writeb[9]++;
#endif
	return(retval);
    }
