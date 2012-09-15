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

/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */

char channel_id[] = "channel.c v2.0 (c) 1990 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#include "numeric.h"

extern aChannel *channel;
extern int maxusersperchannel;
extern aClient *find_person(), me;

int
CanSend(user, chan)
aClient *user;
aChannel *chan;
{
  if ((chan->mode.mode & MODE_MODERATED) && !IsChanOp(user)
      && user->user->channel == chan)
    return (0);
  if (chan->mode.mode & MODE_NOPRIVMSGS)
    return (2);
  return (1);
}

int
ChanIs(chan, name)
aChannel *chan;
char *name;
{
  if (!mycmp(chan->chname, name))
    return 1;
  return 0;
}

int
ChanSame(cptr, cptr2)
aClient *cptr, *cptr2;
{
  if (cptr->user->channel && cptr2->user->channel &&
      !mycmp(cptr->user->channel->chname, cptr2->user->channel->chname))
    return 1;
  return 0;
}

aChannel *
find_channel(chname, para)
char *chname;
aChannel *para;
{
  aChannel *ch2ptr;

  for (ch2ptr = channel; ch2ptr; ch2ptr = ch2ptr->nextch) 
    if (mycmp(ch2ptr->chname, chname) == 0)
      break;

  return ch2ptr ? ch2ptr : para;
}

ChannelModes(modebuf, chptr)
char *modebuf;
aChannel *chptr;
{
  int i = 0;
  modebuf[i++] = '+';
  if (chptr->mode.mode & MODE_PRIVATE)
    modebuf[i++] = 'p';
  if (chptr->mode.mode & MODE_SECRET)
    modebuf[i++] = 's';
  if (chptr->mode.mode & MODE_ANONYMOUS)
    modebuf[i++] = 'a';
  if (chptr->mode.mode & MODE_MODERATED)
    modebuf[i++] = 'm';
  if (chptr->mode.mode & MODE_TOPICLIMIT)
    modebuf[i++] = 't';
  if (chptr->mode.mode & MODE_INVITEONLY)
    modebuf[i++] = 'i';
  if (chptr->mode.mode & MODE_NOPRIVMSGS)
    modebuf[i++] = 'n';
  modebuf[i] = '\0';
  if (chptr->mode.limit)
    sprintf(&(modebuf[strlen(modebuf)]), " %d", chptr->mode.limit);
}

#define MODEBUFLEN 100

m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int parc;
char *parv[];
{
  int i, mcount = 0;
  char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
  aChannel *chptr;

  CheckRegistered(sptr);
  /* Now, try to find the channel in question */
  if (parc > 1)
    chptr = find_channel(parv[1], (aChannel *) 0);
  else {
    if (IsClient(sptr) || IsOper(sptr))
      chptr = sptr->user->channel;
    else
      chptr = (aChannel *) 0;
  }
  if (!chptr) {
    sendto_one(sptr, ":%s %d %s :Channel not found",
	       me.name, ERR_NOSUCHCHANNEL, sptr->name);
    return -1;
  }
  if (parc > 2)
    if (IsServer(sptr) || IsServer(cptr) ||
	(IsChanOp(sptr) && sptr->user->channel == chptr)) {
      mcount = SetMode(sptr, chptr, parc - 2, parv + 2, modebuf, parabuf);
    } else {
      sendto_one(sptr, ":%s %d %s :You're not channel operator",
		 me.name, ERR_NOPRIVILEGES, sptr->name);
      return -1;
    }

  if (parc < 3) {
    i = 0;
    ChannelModes(modebuf, chptr);
    sendto_one(sptr, ":%s %d %s %s",
	       me.name, RPL_CHANNELMODEIS, sptr->name, modebuf);
    return 0;
  }
  if (modebuf[0]) {
    sendto_serv_butone(sptr, ":%s MODE %s %s %s", sptr->name,
		       chptr->chname, modebuf, parabuf);
    sendto_channel_butserv(chptr,
			   ":%s MODE %s %s %s", sptr->name,
			   chptr->chname, modebuf, parabuf);
  }
  return 0;
}

int
SetMode(cptr, chptr, parc, parv, modebuf, parabuf)
aClient *cptr;
aChannel *chptr;
int parc;
char *parv[];
char *modebuf;
char *parabuf;
{
#define MODE_ADD       1
#define MODE_DEL       2
  char *curr = parv[0];
  int whatt = MODE_ADD;
  int nusers, count = 0, i = 0;
  aClient *who;
  Mode *mode = &(chptr->mode);
  if (parc < 1) {
    return -1;
  }
  parabuf[0] = modebuf[0] = '\0';
  while (curr && *curr) {
    switch (*curr) {
    case '+':
      whatt = MODE_ADD;
      modebuf[i++] = '+';
      break;
    case '-':
      whatt = MODE_DEL;
      modebuf[i++] = '-';
      break;
    case 'p':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_PRIVATE;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_PRIVATE;
      modebuf[i++] = 'p';
      count++;
      break;
    case 's':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_SECRET;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_SECRET;
      modebuf[i++] = 's';
      count++;
      break;
    case 'm':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_MODERATED;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_MODERATED;
      modebuf[i++] = 'm';
      count++;
      break;
    case 'a':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_ANONYMOUS;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_ANONYMOUS;
      modebuf[i++] = 'a';
      count++;
      break;
    case 't':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_TOPICLIMIT;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_TOPICLIMIT;
      modebuf[i++] = 't';
      count++;
      break;
    case 'i':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_INVITEONLY;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_INVITEONLY;
      modebuf[i++] = 'i';
      count++;
      break;
    case 'n':
      if (whatt == MODE_ADD)
	mode->mode |= MODE_NOPRIVMSGS;
      if (whatt == MODE_DEL)
	mode->mode &= ~MODE_NOPRIVMSGS;
      modebuf[i++] = 'n';
      count++;
      break;
    case 'o':
      parc--;
      if (parc > 0) {
	parv++;
	who = find_person(parv[0], (aClient *) 0);
	if (who) {
	  if (IsServer(cptr) || ChanSame(cptr, who)) {
	    if (whatt == MODE_ADD) {
	      who->status |= STAT_CHANOP;
	      modebuf[i++] = 'o';
	      strcat(parabuf, who->name);
	      strcat(parabuf, " ");
	      count++;
	    } else if (whatt == MODE_DEL) {
	      who->status &= ~STAT_CHANOP;
	      modebuf[i++] = 'o';
	      strcat(parabuf, who->name);
	      strcat(parabuf, " ");
	      count++;
	    }
	  } else if (MyClient(cptr)) {
	    sendto_one(cptr,
		       ":%s %d %s %s :%s is not here", me.name,
		       ERR_NOTONCHANNEL, cptr->name, parv[0], parv[0]);
	  }
	} else if (MyClient(cptr))
	  sendto_one(cptr,
		     ":%s %d %s %s :Nickname doesn't exist, %s", me.name,
		     ERR_NOSUCHNICK, cptr->name, parv[0],
		     "cannot take privileges");
      }
      break;
    case 'l':
      if (whatt == MODE_ADD) {
	parc--;
	if (parc > 0) {
	  parv++;
	  nusers = atoi(parv[0]);
	  if (nusers > 0) {
	    modebuf[i++] = 'l';
	    sprintf(&(parabuf[strlen(parabuf)]), "%d ", nusers);
	    mode->limit = nusers;
	  }
	  break;
	}
	if (MyClient(cptr))
	  sendto_one(cptr, "ERROR: Number of users on limited channel %s",
		     "not given...");
      } else
	if (whatt == MODE_DEL) {
	  modebuf[i++] = 'l';
	  mode->limit = 0;
	}
      count++;
      break;
    default:
      if (MyClient(cptr))
	sendto_one(cptr, ":%s %d %s %c is unknown mode char to me",
		   me.name, ERR_UNKNOWNMODE, cptr->name, *curr);
      modebuf[i] = '\0';
      return -1;
    }
    curr++;
  }
  modebuf[i] = '\0';
}

int CanJoin(sptr, channel)
aClient *sptr;
aChannel *channel;
{
  if (channel->mode.mode & MODE_INVITEONLY) {
    if (sptr->user->invited != channel)
      return (ERR_INVITEONLYCHAN);
  }
  if (!(channel->mode.limit)) {
    return (0);
  }
  if (channel->users >= channel->mode.limit) {
    return(ERR_CHANNELISFULL);
  }
  return(0);
}


/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static aChannel *get_channel(cptr, i)
aClient *cptr;
char *i;
    {
	register aChannel *chptr = channel;

	char *ptr = i;
	int number;

	if (i == (char *) 0)
		return (aChannel *) 0;

	while (*ptr) {
	  if ((*ptr < 'a' || *ptr > 'z') &&
	      (*ptr < 'A' || *ptr > 'Z') &&
	      *ptr != '+' && *ptr != '{' && *ptr != '}' &&
	      *ptr != '[' && *ptr != ']' && *ptr != '^' && 
	      *ptr != '~' && *ptr != '(' && *ptr != ')' && *ptr != '*' &&
	      *ptr != '!' && *ptr != '@' && *ptr != '.' && *ptr != '-' &&
	      *ptr != ',' && (*ptr < '0' || *ptr > '9') && *ptr != '_')
	    *ptr = '\0';
	  ptr++;
	}

	for ( ; ; chptr = chptr->nextch)
	    {
		if (chptr == (aChannel *) 0)
		    {
		      if (!(chptr = (aChannel *) malloc(sizeof(aChannel) +
							strlen(i))))
			{
			  perror("malloc");
			  debug(DEBUG_FATAL,
				"Out of memory. Cannot allocate channel");
			  restart();
			}
		      chptr->nextch = channel;
		      strcpy(chptr->chname, i);
		      chptr->topic[0] = '\0';
		      chptr->users = 0;
		      chptr->mode.limit = 0;
		      chptr->members = (Link *) 0;
		      chptr->invites = (Invites *) 0;
		      channel = chptr;
		      number = atoi(chptr->chname);

		      /* Clear possible previous operator status */
		      ClearChanOp(cptr);

		      if (number < 0)
			chptr->mode.mode = MODE_SECRET;
		      else if (number > 0)
			if (number < 1000)
			  chptr->mode.mode = 0x0;
			else
			  chptr->mode.mode = MODE_PRIVATE;
		      else {
			SetChanOp(cptr);
			chptr->mode.mode = 0x0;
		      }
		      break;
		    }
		if (mycmp(chptr->chname, i) == 0) {
		  ClearChanOp(cptr);
		  break;
		}
	    }
	return chptr;
    }

AddInvite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Invites *inv = chptr->invites;
  DelInvite (cptr);
  inv = (Invites *) malloc(sizeof(Invites));
  if (!inv)
    restart();
  inv->user = cptr;
  inv->next = chptr->invites;
  chptr->invites = inv;
  cptr->user->invited = chptr;
}

DelInvite(cptr)
aClient *cptr;
{
  Invites **inv, *tmp;
  if (cptr->user->invited) {
    for (inv = &(cptr->user->invited->invites); *inv;
	 inv = &((*inv)->next)) {
      if ((*inv)->user == cptr) {
	tmp = *inv;
	*inv = (*inv)->next;
	free(tmp);
	break;
      }
    }
    cptr->user->invited = (aChannel *) 0;
  }
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty). Currently negative
**  channels don't have the channel block allocated. But,
**  just in case they some day will have that, it's better
**  call this for those too.
*/
void sub1_from_channel(xchptr, who)
aChannel *xchptr;
aClient *who;
    {
	register aChannel *chptr, **pchptr;

	for (pchptr = &channel; chptr = *pchptr; pchptr = &(chptr->nextch))
	  if (chptr == xchptr)
	    {
	      register Link *curr, *prev = (Link *) 0;
	      curr = chptr->members;
	      while (curr) {
		if (curr->user == who) {
		  if (prev)
		    prev->next = curr->next;
		  else
		    chptr->members = curr->next;
		  free(curr);
		}
		prev = curr;
		curr = curr->next;
	      }
	      if (--chptr->users <= 0)
		{
		  Invites *oldinv = (Invites *) 0, *inv = chptr->invites;
		  *pchptr = chptr->nextch;
		  /* Now, find all invite links from channel structure */
		  while (inv) {
		    inv->user->user->invited = (aChannel *) 0;
		    oldinv = inv;
		    inv = inv->next;
		    free(oldinv);
		  }
		  free(chptr);
		}
	      break;
	    }
	/*
	**  Actually, it's a bug, if the channel is not found and
	**  the loop terminates with "chptr == NULL"
	*/
    }

/*
** m_channel
**	parv[0] = sender prefix
**	parv[1] = channel
*/
m_channel(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	int i;
	register char *ch;

	CheckRegisteredUser(sptr);

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
			   "NOTICE %s :*** What channel do you want to join?",
			   sptr->name);
		return 0;
	    }
	ch = parv[1];

	if (atoi(parv[1]) == 0 && parv[1][0] != '+') {
	  if (parv[1][0] == '0' && parv[1][1] == '\0') {
	    if (sptr->user->channel) {
	      sendto_serv_butone(cptr, ":%s CHANNEL 0", sptr->name);
	      sub1_from_channel(sptr->user->channel);
	      sendto_channel_butserv(sptr->user->channel,
				     ":%s CHANNEL 0", sptr->name);
	      sptr->user->channel = (aChannel *) 0;
	      ClearChanOp(sptr);
	    }
	  } else 
	    sendto_one(sptr, ":%s %d %s",
		       me.name, ERR_NOSUCHCHANNEL, sptr->name);
	  return 0;
	}

	chptr = get_channel(sptr, parv[1]);
	if (cptr == sptr && chptr && (i = CanJoin(sptr, chptr)))
	  {
	    sendto_one(sptr,":%s %d %s %s :Sorry, cannot join channel.",
		       me.name, i, sptr->name, chptr->chname);
	    return 0;
	  }

	chptr->users++;
	/*
	**  Remove user from the old channel (if any)
	*/
	sendto_serv_butone(cptr, ":%s CHANNEL %s", sptr->name, chptr->chname);
	if (sptr->user->channel != (aChannel *) 0)
	    {
		sub1_from_channel(sptr->user->channel);
		sendto_channel_butserv(sptr->user->channel,
				       ":%s CHANNEL 0", sptr->name);
	    }
	/*
	**  Complete user entry to the new channel (if any)
	*/
	DelInvite(sptr);
	sptr->user->channel = chptr;
	if (chptr)
	  {
	    /* notify all other users on the new channel */
	    sendto_channel_butserv(chptr, ":%s CHANNEL %s", sptr->name,
				   chptr->chname);
#ifdef AUTOTOPIC
	    /* Note: neg channels don't have a topic--no block allocated */
	    if (MyClient(sptr) && IsPerson(sptr)
		&& chptr->topic[0] != '\0')
	      sendto_one(sptr, ":%s %d %s :%s",
			 me.name, RPL_TOPIC,
			 sptr->name, chptr->topic);
#endif
	  }
	return 0;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
*/
m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  aClient *who;
  CheckRegisteredUser(sptr);

  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(sptr,
		 "NOTICE %s :%s", sptr->name,
		 "*** Who do you want to kick off from which channel?");
      return 0;
    }
  if (!sptr->user->channel) {
    sendto_one(sptr, ":%s %d %s :You haven't joined any channel",
	       me.name, ERR_USERNOTINCHANNEL, sptr->name);
    return 0;
  }
  who = find_person(parv[2], (aClient *) 0);
  if (!who) {
    if (MyClient(sptr))
      sendto_one(sptr,
		 ":%s %d %s %s :%s", me.name,
		 ERR_NOSUCHNICK, sptr->name, parv[0],
		 "Cannot kick user off channel");
    return 0;
  }
  if (ChanSame(sptr, who) && IsChanOp(sptr)) {
    sendto_channel_butserv(sptr->user->channel, ":%s KICK %s %s", sptr->name,
			   sptr->user->channel->chname, who->name);
    sendto_serv_butone(cptr, ":%s KICK %s %s", sptr->name,
		       sptr->user->channel->chname, who->name);
    sub1_from_channel(sptr->user->channel, who);
    who->user->channel = (aChannel *) 0;
    ClearChanOp(who);
  } else if (!IsChanOp(sptr)) {
    sendto_one(sptr, ":%s %d %s :You're not channel operator",
	       me.name, ERR_NOPRIVILEGES, sptr->name);
  } else
    sendto_one(sptr, ":%s %d %s %s :isn't on your channel !",
	       me.name, ERR_NOTONCHANNEL, sptr->name, who->name);
  return (0);
}
