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
/*
** At this point includes are done so that if "SYSV" is defined,
** it uses SYSV files, otherwise ANSI C standard include files
** are assumed, for some systems one may have to twiddle with
** these. The recommended action is actually to make those
** ANSI include files, instead of cluttering this with complicated
** ifdef's... >;)   --msa
**
** For documentation of the *global* functions implemented here,
** see the header file (dbuf.h).
**
*/
#if HAS_ANSI_INCLUDES
#	include <stdlib.h>
#	include <string.h>
#else
#if HAS_SYSV_INCLUDES
#	include <string.h>
#	include <memory.h>
#	include <malloc.h>
#else
char *malloc();
free();
char *memset();
#endif
#endif
#include "dbuf.h"

#ifndef NULL
#	define NULL 0
#endif

#define BUFSIZ sizeof(((dbufbuf *)0)->data)

/*
** This is called when malloc fails. Scrap the whole content
** of dynamic buffer and return -1. (malloc errors are FATAL,
** there is no reason to continue this buffer...). After this
** the "dbuf" has consistent EMPTY status... ;)
*/
static int malloc_error(dyn)
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
long int length;
    {
	dbufbuf **h, *d;
	long int nbr, off;
	int chunk;

	off = (dyn->offset + dyn->length) % BUFSIZ;
	nbr = (dyn->offset + dyn->length) / BUFSIZ;
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
	chunk = BUFSIZ - off;
	dyn->length += length;
	for ( ;length > 0; h = &(d->next))
	    {
		if ((d = *h) == NULL)
		    {
			if ((d = (dbufbuf *)malloc(sizeof(dbufbuf))) == NULL)
				return malloc_error(dyn);
			*h = d;
			d->next = NULL;
		    }
		if (chunk > length)
			chunk = length;
		memcpy(d->data + off,buf,chunk);
		length -= chunk;
		buf += chunk;
		off = 0;
		chunk = BUFSIZ;
	    }
	return 1;
    }

		
char *dbuf_map(dyn,length)
dbuf *dyn;
long int *length;
    {
	if (dyn->head == NULL)
	    {
		*length = 0;
		return NULL;
	    }
	*length = BUFSIZ - dyn->offset;
	if (*length > dyn->length)
		*length = dyn->length;
	return (dyn->head->data + dyn->offset);
    }

int dbuf_delete(dyn,length)
dbuf *dyn;
long int length;
    {
	dbufbuf *d;
	int chunk;

	if (length > dyn->length)
		length = dyn->length;
	chunk = BUFSIZ - dyn->offset;
	while (length > 0)
	    {
		if (chunk > length)
			chunk = length;
		length -= chunk;
		dyn->offset += chunk;
		dyn->length -= chunk;
		if (dyn->offset == BUFSIZ || dyn->length == 0)
		    {
			d = dyn->head;
			dyn->head = d->next;
			dyn->offset = 0;
			free((void *)d);
		    }
		chunk = BUFSIZ;
	    }
	return 0;
    }

long int dbuf_get(dyn,buf,length)
dbuf *dyn;
char *buf;
long int length;
    {
	long int moved = 0;
	long int chunk;
	char *b;

	while (length > 0 && (b = dbuf_map(dyn,&chunk)) != NULL)
	    {
		if (chunk > length)
			chunk = length;
		memcpy(buf,b,(int)chunk);
		dbuf_delete(dyn,chunk);
		buf += chunk;
		length -= chunk;
		moved += chunk;
	    }
	return moved;
    }


