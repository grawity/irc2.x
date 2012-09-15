/************************************************************************
 *   IRC - Internet Relay Chat, common/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
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

#ifndef lint
static  char sccsid[] = "@(#)match.c	2.2 7/3/93 2 %S (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"

/*
**  Compare if a given string (name) matches the given
**  mask (which can contain wild cards: '*' - match any
**  number of chars, '?' - match any single character.
**
**	return	0, if match
**		1, if no match
*/
int	matches(ma, na)
char *ma, *na;
    {
	Reg1 unsigned char m, *mask = (unsigned char *)ma;
	Reg2 unsigned char c, *name = (unsigned char *)na;

	for (;; mask++, name++)
	    {
		m = tolower(*mask);
		c = tolower(*name);
		if (c == '\0')
			break;
		if ((m != '?' && m != c) || c == '*')
			break;
	    }
	if (m == '*')
	    {
		for ( ; *mask == '*'; mask++);
		if (*mask == '\0')
			return(0);
		for (; *name && matches((char *)mask, (char *)name); name++);
		return(*name ? 0 : 1);
	    }
	else
		return ((m == '\0' && c == '\0') ? 0 : 1);
    }
