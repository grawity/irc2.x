/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_numeric.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   Numerous fixes by Markku Savela
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

char numeric_id[] = "numeric.c (c) 1989 Jarkko Oikarinen";

#include "config.h"
#include "sys.h" 
#include "struct.h"
#include "numeric.h"

#define NULL 0

extern aClient *find_client();
extern aChannel *find_channel();

static char buffer[1024];

/*
** DoNumeric (replacement for the old do_numeric)
**
**	parc	number of arguments ('sender' counted as one!)
**	parv[0]	pointer to 'sender' (may point to empty string) (not used)
**	parv[1]..parv[parc-1]
**		pointers to additional parameters, this is a NULL
**		terminated list (parv[parc] == NULL).
**
** *WARNING*
**	Numerics are mostly error reports. If there is something
**	wrong with the message, just *DROP* it! Don't even think of
**	sending back a neat error message -- big danger of creating
**	a ping pong error message...
*/
int DoNumeric(numeric, cptr, sptr, parc, parv)
int numeric;
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	aChannel *chptr;
	char *nick, *tmp;
	int i;

	if (parc < 1 || sptr->status < 0)
		return 0;
	/*
	** Prepare the parameter portion of the message into 'buffer'.
	** (Because the buffer is twice as large as the message buffer
	** for the socket, no overflow can occur here... ...on current
	** assumptions--bets are off, if these are changed --msa)
	** Note: if buffer is non-empty, it will begin with SPACE.
	*/
	buffer[0] = '\0';
	if (parc > 1)
	    {
		for (i = 2; i < (parc - 1); i++)
		    {
			strcat(buffer, " ");
			strcat(buffer, parv[i]);
		    }
		strcat(buffer, " :");
		strcat(buffer, parv[parc-1]);
	    }
	for (tmp = parv[1]; (nick = tmp) != NULL && *tmp; )
	    {
		if (tmp = index(nick, ','))
			*(tmp++) = '\0';
		if (acptr = find_client(nick, (aClient *)NULL))
		    {
			if (IsMe(acptr))
				continue; /*
					  ** Drop to bit bucket if for me...
					  ** ...one might consider sendto_ops
					  ** here... --msa
					  */
			sendto_one(acptr,":%s %d %s%s", sptr->name,
				   numeric, nick, buffer);
		    }
		else if (chptr = find_channel(nick, (aClient *)NULL))
			sendto_channel_butone(cptr,chptr->channo,":%s %d %d%s",
					      sptr->name,
					      numeric, chptr->channo, buffer);
	    }
	return 0;
}


