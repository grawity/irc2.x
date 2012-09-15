/************************************************************************
 *   IRC - Internet Relay Chat, VM/sys.c
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
#include "sys.h"
#include "struct.h"
 
static char buf[BUFFERLEN];
 
sendto_one(cptr, pattern, a, b, c, d, e, f, g, h, i, j)
aClient *cptr;
char *pattern, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    sprintf(buf, pattern, a, b, c, d, e, f, g, h, i, j);
    strcat(buf, NEWLINE);
    return(write(cptr->fd, buf, strlen(buf)) != strlen(buf));
}
