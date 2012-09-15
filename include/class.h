/************************************************************************
 *   IRC - Internet Relay Chat, include/class.h
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

/*
 * $Id: class.h,v 6.1 1991/07/04 21:04:24 gruner stable gruner $
 *
 * $Log: class.h,v $
 * Revision 6.1  1991/07/04  21:04:24  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:04:58  gruner
 * frozen beta revision 2.6.1
 *
 */

#ifndef PROTO
#if __STDC__
#       define PROTO(x) x
#else
#       define PROTO(x) ()
#endif
#endif

typedef struct Class {
  int class;
  int conFreq;
  int pingFreq;
  int maxLinks;
  int links;
  struct Class *next;
} aClass;

#define	Class(x)	((x)->class)
#define	ConFreq(x)	((x)->conFreq)
#define	PingFreq(x)	((x)->pingFreq)
#define	MaxLinks(x)	((x)->maxLinks)
#define	Links(x)	((x)->links)

#define	ConfLinks(x)	(Class(x)->links)
#define	ConfMaxLinks(x)	(Class(x)->maxLinks)
#define	ConfClass(x)	(Class(x)->class)
#define	ConfConFreq(x)	(Class(x)->conFreq)
#define	ConfPingFreq(x)	(Class(x)->pingFreq)

#define  FirstClass() 	classes
#define  NextClass(x)	((x)->next)

extern aClass *classes;
extern aClass *find_class PROTO((int));
extern int GetConfClass PROTO((aConfItem *));

extern int GetClientClass PROTO((aClient *));
extern int GetClientPing PROTO((aClient *));

extern int GetConFreq PROTO((aClass *));
extern void AddClass PROTO((int, int, int, int));
extern void check_class PROTO((void));
extern void initclass PROTO((void));
