/************************************************************************
 *   IRC - Internet Relay Chat, lib/ircd/packet.c
 *   Copyright (C) 1990  Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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

char packet_id[]="packet.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";
 
#include "struct.h"
#include "common.h"
#include "msg.h"
extern aClient me;
 
/*
** Note:
**	It is implicitly assumed that dopacket is called only
**	with cptr of "local" variation, which contains all the
**	necessary fields (buffer etc..)
*/
int	dopacket(cptr, buffer, length)
aClient *cptr;
char	*buffer;
int	length;
    {
	Reg1 char *ch1;
	Reg2 char *ch2;
 
	me.receiveB += length; /* Update bytes received */
	cptr->receiveB += length;
	if (cptr->acpt != &me)
		cptr->acpt->receiveB += length;
	ch1 = cptr->buffer + cptr->count;
	ch2 = buffer;
	while (--length >= 0)
	    {
		*ch1 = *ch2++;
		if (*ch1 == '\r' || *ch1 == '\n')
		    {
			if (ch1 == cptr->buffer)
				continue; /* Skip extra LF/CR's */
			*ch1 = '\0';
			me.receiveM += 1; /* Update messages received */
			cptr->receiveM += 1;
			if (cptr->acpt != &me)
				cptr->acpt->receiveM += 1;
			cptr->count = 0; /* ...just in case parse returns with
					 ** FLUSH_BUFFER without removing the
					 ** structure pointed by cptr... --msa
					 */
			if (parse(cptr, cptr->buffer,
				  ch1 - cptr->buffer,
				  msgtab) == FLUSH_BUFFER)
				/*
				** FLUSH_BUFFER means actually
				** that cptr structure *does*
				** not exist anymore!!! --msa
				*/
				return(1);
#ifndef CLIENT_COMPILE
			if (cptr->flags & FLAGS_DEADSOCKET)
			    {
				exit_client(NULL, cptr, &me, "Dead Socket");
				return (-1);
			    }
#endif
			ch1 = cptr->buffer;
		    }
		else if (ch1 < cptr->buffer + (sizeof(cptr->buffer)-1))
			ch1++; /* There is always room for the null */
	    }
	cptr->count = ch1 - cptr->buffer;
	return 0;
    }
 
