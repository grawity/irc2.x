/************************************************************************
 *   IRC - Internet Relay Chat, include/common.h
 *   Copyright (C) 1990 Armin Gruner
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
 * $Id: common.h,v 6.1 1991/07/04 21:04:25 gruner stable gruner $
 *
 * $Log: common.h,v $
 * Revision 6.1  1991/07/04  21:04:25  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:04:59  gruner
 * frozen beta revision 2.6.1
 *
 */

#ifndef PROTO
#if __STDC__
#	define PROTO(x)	x
#else
#	define PROTO(x)	()
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

extern char *malloc();
extern void free();

#if NEED_INET_ADDR
extern unsigned long inet_addr PROTO((char *));
#endif

#if NEED_INET_NTOA || NEED_INET_NETOF
#include <sys/types.h>
#include <netinet/in.h>
#endif

#if NEED_INET_NTOA
extern char *inet_ntoa PROTO((struct in_addr));
#endif

#if NEED_INET_NETOF
extern int inet_netof PROTO((struct in_addr));
#endif

extern long time();
extern char *myctime PROTO((long));
extern char *strtoken PROTO((char **, char *, char *));

#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define MIN(a, b)	((a) < (b) ? (a) : (b))

#define MyFree(x)       if ((x) != NULL) free(x)
#define DupString(x,y) do { x = MyMalloc(strlen(y)+1); strcpy(x, y);}while (0)
