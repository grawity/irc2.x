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

#if HPUX | VMS | AIX
#include <string.h>
#define bcopy(a,b,s)  memcpy(b,a,s)
#define bzero(a,s)    memset(a,0,s)
extern char *strchr(), *strrchr();
extern char *inet_ntoa();
#define index strchr
#define rindex strrchr
#else 
#include <strings.h>
#endif
#include <ctype.h>
#include <pwd.h>

#if AIX
#include <sys/select.h>
#endif
#if HPUX | AIX
#include <time.h>
#if AIX
#include <sys/time.h>
#endif
#else
#include <sys/time.h>
#endif

#if NEXT
#define VOIDSIG int	/* whether signal() returns int of void */
#else
#define VOIDSIG void	/* whether signal() returns int of void */
#endif

extern VOIDSIG dummy(), restart();
