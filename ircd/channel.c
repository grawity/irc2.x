/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

char channel_id[] = "channel.c v2.0 (c) 1990 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
extern aChannel *channel;


int
chan_conv(name)
char *name;
{
  return (atoi(name));
}

aChannel *
find_channel(chname, para)
char *chname;
aChannel *para;
{
  aChannel *ch2ptr;
  char *ch;
  int chan;
  for (ch = chname; *ch; ch++) 
    if (*ch < '0' || *ch > '9')
      break;

  if (*ch)
    return (para);
  if ((chan = atoi(chname)) == 0)
    return (para);
  for (ch2ptr = channel; ch2ptr; ch2ptr = ch2ptr->nextch) 
    if (ch2ptr->channo == chan)
      break;

  return ch2ptr ? ch2ptr : para;
}


aChannel *
GetChannel(chnum)
int chnum;
{
  aChannel *c;
  for (c = channel; c; c = c->nextch)
    if (c->channo == chnum)
      break;
  return c;
}
