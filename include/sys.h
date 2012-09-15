/************************************************************************
 *   IRC - Internet Relay Chat, include/sys.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
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

#ifndef	__sys_include__
#define __sys_include__
#ifdef ISC202
#include <net/errno.h>
#else
#include <sys/errno.h>
#endif

#include "setup.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef CDEFSH
#include "cdefs.h"
#endif
#include "bitypes.h"

#ifdef	UNISTDH
#include <unistd.h>
#endif
#ifdef	STDLIBH
#include <stdlib.h>
#endif

#ifdef	STRINGSH
#include <strings.h>
#else
# ifdef	STRINGH
# include <string.h>
# endif
#endif
#define	strcasecmp	mycmp
#define	strncasecmp	myncmp
#ifdef NOINDEX
#define   index   strchr
#define   rindex  strrchr
#endif
extern	char	*index __P((char *, char));
extern	char	*rindex __P((char *, char));

#ifdef AIX
# include <sys/select.h>
#endif
#if defined(HPUX )
# include <time.h>
#else
# include <sys/time.h>
#endif

#if !defined(DEBUGMODE) || defined(CLIENT_COMPILE)
#define MyFree(x)       if ((x) != NULL) free(x)
#else
#define	free(x)		MyFree(x)
#endif

#ifdef NEXT
#define VOIDSIG int	/* whether signal() returns int of void */
#else
#define VOIDSIG void	/* whether signal() returns int of void */
#endif

extern	VOIDSIG	dummy();

#ifdef	DYNIXPTX
#define	NO_U_TYPES
#endif

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#ifdef	USE_VARARGS
#include <varargs.h>
#endif

#endif /* __sys_include__ */
