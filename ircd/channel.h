/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.h
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

/*
 * $Id: channel.h,v 6.1 1991/07/04 21:05:11 gruner stable gruner $
 *
 * $Log: channel.h,v $
 * Revision 6.1  1991/07/04  21:05:11  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:34  gruner
 * frozen beta revision 2.6.1
 *
 */

#define CREATE 1	/* whether a channel should be
			   created or just tested for existance */

#define FLAG_CHANOP 0x1
#define MODEBUFLEN	100

#define NullChn	((aChannel *) 0)

#define TestChanOpFlag(flag)	(flag & FLAG_CHANOP)
#define ChanIs(chan, name) 	(mycmp(chan->chname, name) == 0)
#define ChannelExists(chname)   (find_channel(chname, NullChn) != NullChn)

extern aChannel *find_channel PROTO((char *, aChannel *));
extern int ChannelModes PROTO((char *, aChannel *));

extern int is_chan_op PROTO((aClient *, aChannel *));
extern int can_send PROTO((aClient *, aChannel *));
extern int can_join PROTO((aClient *, aChannel *));

extern void remove_user_from_channel PROTO((aClient *, aChannel *));

extern void add_invite PROTO((aClient *, aChannel *));
extern void del_invite PROTO((aClient *, aChannel *));
