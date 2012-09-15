/************************************************************************
 *   IRC - Internet Relay Chat, lib/ircd/dbuf.c
 *   Copyright (C) 1990 Markku Savela
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 10 May 1990
 * Changed memcpy into bcopy and removed the declaration of memset
 * because it was unnecessary.
 * Added the #includes for "struct.h" and "sys.h" to get bcopy/memcpy
 * work
 */

/*
** For documentation of the *global* functions implemented here,
** see the header file (dbuf.h).
**
*/

#include <stdio.h>
#include "config.h"
#include "common.h"
#include "dbuf.h"
#include "sys.h"

int	dbufalloc = 0, dbufblocks = 0;
static	dbufbuf	*freelist = NULL;

/* This is a dangerous define because a broken compiler will set DBUFSIZ
** to 4, which will work but will be very inefficient. However, there
** are other places where the code breaks badly if this is screwed
** up, so... -- Wumpus
*/

/*#define DBUFSIZ sizeof(((dbufbuf *)0)->data)*/
#define	DBUFSIZ	2044

/*
** dbuf_alloc - allocates a dbufbuf structure either from freelist or
** creates a new one.
*/
static dbufbuf *dbuf_alloc()
{
	Reg1	dbufbuf	*dbptr;

	dbufalloc++;
	if (dbptr = freelist)
	    {
		freelist = freelist->next;
		return dbptr;
	    }
	dbufblocks++;
	return (dbufbuf *)calloc(1,sizeof(dbufbuf));
}
/*
** dbuf_free - return a dbufbuf structure to the freelist
*/
static	void	dbuf_free(ptr)
Reg1	dbufbuf	*ptr;
{
	dbufalloc--;
	ptr->next = freelist;
	freelist = ptr;
}
/*
** This is called when malloc fails. Scrap the whole content
** of dynamic buffer and return -1. (malloc errors are FATAL,
** there is no reason to continue this buffer...). After this
** the "dbuf" has consistent EMPTY status... ;)
*/
static int dbuf_malloc_error(dyn)
dbuf *dyn;
    {
	dbufbuf *p;

	dyn->length = 0;
	dyn->offset = 0;
	while ((p = dyn->head) != NULL)
	    {
		dyn->head = p->next;
		free((void *)p);
	    }
	return -1;
    }


int dbuf_put(dyn, buf, length)
dbuf *dyn;
char *buf;
long length;
    {
	Reg1 dbufbuf **h, *d;
	Reg2 long int nbr, off;
	Reg3 int chunk;

	off = (dyn->offset + dyn->length) % DBUFSIZ;
	nbr = (dyn->offset + dyn->length) / DBUFSIZ;
	/*
	** Locate the last non-empty buffer. If the last buffer is
	** full, the loop will terminate with 'd==NULL'. This loop
	** assumes that the 'dyn->length' field is correctly
	** maintained, as it should--no other check really needed.
	*/
	for (h = &(dyn->head); (d = *h) && --nbr >= 0; h = &(d->next));
	/*
	** Append users data to buffer, allocating buffers as needed
	*/
	chunk = DBUFSIZ - off;
	dyn->length += length;
	for ( ;length > 0; h = &(d->next))
	    {
		if ((d = *h) == NULL)
		    {
			if ((d = (dbufbuf *)dbuf_alloc()) == NULL)
				return dbuf_malloc_error(dyn);
			*h = d;
			d->next = NULL;
		    }
		if (chunk > length)
			chunk = length;
		bcopy(buf,d->data + off,chunk);
		length -= chunk;
		buf += chunk;
		off = 0;
		chunk = DBUFSIZ;
	    }
	return 1;
    }


char *dbuf_map(dyn,length)
dbuf *dyn;
long *length;
    {
	if (dyn->head == NULL)
	    {
		*length = 0;
		return NULL;
	    }
	*length = DBUFSIZ - dyn->offset;
	if (*length > dyn->length)
		*length = dyn->length;
	return (dyn->head->data + dyn->offset);
    }

int	dbuf_delete(dyn,length)
dbuf	*dyn;
long	length;
    {
	dbufbuf *d;
	int chunk;

	if (length > dyn->length)
		length = dyn->length;
	chunk = DBUFSIZ - dyn->offset;
	while (length > 0)
	    {
		if (chunk > length)
			chunk = length;
		length -= chunk;
		dyn->offset += chunk;
		dyn->length -= chunk;
		if (dyn->offset == DBUFSIZ || dyn->length == 0)
		    {
			d = dyn->head;
			dyn->head = d->next;
			dyn->offset = 0;
			dbuf_free(d);
		    }
		chunk = DBUFSIZ;
	    }
	return 0;
    }

long	dbuf_get(dyn,buf,length)
dbuf	*dyn;
char	*buf;
long	length;
    {
	long int moved = 0;
	long int chunk;
	char *b;

	while (length > 0 && (b = dbuf_map(dyn,&chunk)) != NULL)
	    {
		if (chunk > length)
			chunk = length;
		bcopy(buf,b,(int)chunk);
		dbuf_delete(dyn,chunk);
		buf += chunk;
		length -= chunk;
		moved += chunk;
	    }
	return moved;
    }
