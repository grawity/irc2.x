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

/*
 * $Id: channel.c,v 6.1 1991/07/04 21:05:10 gruner stable gruner $
 *
 * $Log: channel.c,v $
 * Revision 6.1  1991/07/04  21:05:10  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:33  gruner
 * frozen beta revision 2.6.1
 *
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
#include "sys.h"
#include "common.h"
#include "numeric.h"
#include "channel.h"

aChannel *channel = NullChn;

extern int maxusersperchannel;
extern aClient *find_person(), me;

extern aChannel *hash_find_channel();
static void sub1_from_channel();
static int SetMode();

static void InitBufs(cptr, chptr, buf1, buf2)
aClient *cptr;
aChannel *chptr;
char *buf1, *buf2;
{
  if (IsChanOp(cptr, chptr) && IsMember(cptr, chptr) ||
      IsServer(cptr) || !MyConnect(cptr)) {
    *buf1 = *buf2 == '\0';
  } else {
    exit(0);
  }
}

static Link *FindLink(link, ptr)
Link *link;
char *ptr;
{
  while (link && ptr) {
    if (link->value == ptr)
      break;
    link = link->next;
  }
  return link;
}

static int CountChannels(user)
aClient *user;
{
  aChannel *chptr;
  int count = 0;
  for (chptr = channel; chptr; chptr = chptr->nextch) {
    if (FindLink(chptr->members, user))
      count++;
  }
  return count;
}

static void AddUserToChannel(chptr, who, flags)
aChannel *chptr;
aClient *who;
unsigned char flags;
{
  Reg1 Link *curr = (Link *) malloc(sizeof(Link));
  curr->next = chptr->members;
  chptr->members = curr;
  curr->value = (char *) who;
  curr->flags = flags;
  chptr->users++;
}

void RemoveUserFromChannel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
  Reg1 Link **curr = &(chptr->members);
  Reg2 Link *tmp;
  if (sptr->user->channel == chptr) {
    sptr->user->channel = NullChn;
  }
  while (*curr) {
    if ((aClient *) ((*curr)->value) == sptr) {
      tmp = (*curr)->next;
      free(*curr);
      *curr = tmp;
      break;
    } else
      curr = &((*curr)->next);
  }
  sub1_from_channel(chptr);
}

static void ClearChanOp(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link = FindLink(chptr->members, cptr);
  link->flags &= ~FLAG_CHANOP;
}

static void SetChanOp(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link = FindLink(chptr->members, cptr);
  link->flags |= FLAG_CHANOP;
}

int IsChanOp(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link;

  if (chptr)
    if (link = FindLink(chptr->members, cptr))
    	return (link->flags & FLAG_CHANOP);

  return 0;
}

int
CanSend(user, chan)
aClient *user;
aChannel *chan;
{
  if (chan->mode.mode & MODE_MODERATED
      && !IsChanOp(user, chan)
      && IsMember(user, chan))
    return (MODE_MODERATED);

  if (chan->mode.mode & MODE_NOPRIVMSGS
      && !IsMember(user, chan))
    return (MODE_NOPRIVMSGS);

  return (0);
}

int
IsMember(user, chptr)
aClient *user;
aChannel *chptr;
{
  Link *curr = chptr->members;
  while (curr) {
    if ((aClient *) (curr->value) == user)
      break;
    curr = curr->next;
  }
  return(curr ? 1 : 0);
}

aChannel *
find_channel(chname, para)
char *chname;
aChannel *para;
{
  aChannel *ch2ptr;

  ch2ptr = hash_find_channel(chname, para);
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
    sprintf(&(modebuf[strlen(modebuf)]), "l %d", chptr->mode.limit);
}

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
    if (IsRegisteredUser(sptr) || IsAnOper(sptr))
      chptr = sptr->user->channel;
    else
      chptr = (aChannel *) 0;
  }
  if (!chptr) {
    sendto_one(sptr, ":%s %d %s %s :Channel not found",
	       me.name, ERR_NOSUCHCHANNEL, parv[0], parv[1]);
    return -1;
  }
  if (parc > 2)
    if (IsServer(sptr) || IsServer(cptr) ||
	(IsChanOp(sptr, chptr) && IsMember(sptr, chptr))) {
      mcount = SetMode(sptr, chptr, parc - 2, parv + 2, modebuf, parabuf);
    } else {
      sendto_one(sptr, ":%s %d %s %s :You're not channel operator",
		 me.name, ERR_CHANOPRIVSNEEDED, parv[0], chptr->chname);
      return -1;
    }

  if (parc < 3) {
    i = 0;
    ChannelModes(modebuf, chptr);
    sendto_one(sptr, ":%s %d %s %s %s",
	       me.name, RPL_CHANNELMODEIS, parv[0], chptr->chname, modebuf);
    return 0;
  }
  if (modebuf[0]) {
    sendto_serv_butone(sptr, ":%s MODE %s %s %s", parv[0],
		       chptr->chname, modebuf, parabuf);
    sendto_channel_butserv(chptr,
			   ":%s MODE %s %s %s", parv[0],
			   chptr->chname, modebuf, parabuf);
  }
  return 0;
}

static int
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
  char *tmp;
  unsigned char added = '\0', deleted = '\0';
  int whatt = MODE_ADD;
  int limitset = 0;
  int nusers, i = 0;
  Link *addops = (Link *) 0, *delops = (Link *) 0, *tmplink;
  aClient *who;
  static char flags[] = { MODE_PRIVATE, 'p', MODE_SECRET, 's',
			    MODE_MODERATED, 'm', MODE_ANONYMOUS, 'a',
			    MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
			    MODE_NOPRIVMSGS, 'n', 0x0, 0x0 };
  Mode *mode = &(chptr->mode);
  if (parc < 1) {
    return -1;
  }
  InitBufs(cptr, chptr, parabuf, modebuf);
  while (curr && *curr && i < MODEBUFLEN - 1) {
    switch (*curr) {
    case '+':
      whatt = MODE_ADD;
      break;
    case '-':
      whatt = MODE_DEL;
      break;
    case 'o':
      parc--;
      if (parc > 0) {
	parv++;
	who = find_person(parv[0], (aClient *) 0);
	if (who) {
	  if (IsMember(who, chptr)) {
	    if (whatt == MODE_ADD) {
	      tmplink = (Link *) malloc(sizeof(Link));
	      if (!tmplink)
		restart();
	      tmplink->next = addops;
	      tmplink->value = who->name;
	      addops = tmplink;
	      SetChanOp(who, chptr);
	    } else if (whatt == MODE_DEL) {
	      tmplink = (Link *) malloc(sizeof(Link));
	      if (!tmplink)
		restart();
	      tmplink->next = delops;
	      tmplink->value = who->name;
	      delops = tmplink;
	      ClearChanOp(who, chptr);
	    }
	  } else if (MyClient(cptr)) {
	    sendto_one(cptr,
		       ":%s %d %s %s %s :%s is not here", me.name,
		       ERR_NOTONCHANNEL, cptr->name, parv[0], chptr->chname,
		       parv[0]);
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
	  limitset = 1;
	  break;
	}
	if (MyClient(cptr))
	  sendto_one(cptr, ":%s %d %s :Number of users on limited channel %s",
	     me.name, ERR_NEEDMOREPARAMS, parv[0], "not given.");
      } else
	if (whatt == MODE_DEL) {
	  limitset = 1;
	  nusers = 0;
	}
      break;
    default:
      for (tmp = flags; tmp[0]; tmp += 2) {
	if (tmp[1] == *curr)
	  break;
      }
      if (tmp[0]) {
	if (whatt == MODE_ADD) {
	  if (deleted | tmp[0])
	    deleted &= ~tmp[0];
	  added |= tmp[0];
	} else {
	  if (added | tmp[0])
	    added &= ~tmp[0];
	  deleted |= tmp[0];
	}
      } else {
	if (MyClient(cptr))
	  sendto_one(cptr, ":%s %d %s %c is unknown mode char to me",
		     me.name, ERR_UNKNOWNMODE, cptr->name, *curr);
	modebuf[i] = '\0';
	return -1;
      }
    }
    curr++;
  }
  whatt = 0;
  parabuf[0] = '\0';
  for (tmp = flags; tmp[0]; tmp += 2) {
    if (tmp[0] & added) {
      if (whatt == 0) {
	*(modebuf++) = '+';
	whatt = 1;
      }
      chptr->mode.mode |= tmp[0];
      *(modebuf++) = tmp[1];
    }
  }
  for (tmp = flags; tmp[0]; tmp += 2) {
    if (tmp[0] & deleted) {
      if (whatt != -1) {
	*(modebuf++) = '-';
	whatt = -1;
      }
      chptr->mode.mode &= ~tmp[0];
      *(modebuf++) = tmp[1];
    }
  }
  if (limitset) {
    if (nusers == 0) {
      if (whatt != -1) {
	*(modebuf++) = '-';
	whatt = -1;
      }
      *(modebuf++) = 'l';
    } else {
      if (whatt != 1) {
	*(modebuf++) = '+';
	whatt = 1;
      }
      *(modebuf++) = 'l';
      sprintf(parabuf, "%d ", nusers);
    }
    chptr->mode.limit = nusers;
  }
  if (addops) {
    if (whatt != 1) {
      *(modebuf++) = '+';
      whatt = 1;
    }
    while (addops) {
      tmplink = addops;
      addops = addops->next;
      if (strlen(parabuf) + strlen(tmplink->value) + 10 < MODEBUFLEN) {
	*(modebuf++) = 'o';
	strcat(parabuf, tmplink->value);
	strcat(parabuf, " ");
      }
      free(tmplink);
    }
  }
  if (delops) {
    if (whatt != -1) {
      *(modebuf++) = '-';
      whatt = -1;
    }
    while (delops) {
      tmplink = delops;
      delops = delops->next;
      if (strlen(parabuf) + strlen(tmplink->value) + 10 < MODEBUFLEN) {
	*(modebuf++) = 'o';
	strcat(parabuf, tmplink->value);
	strcat(parabuf, " ");
      }
      free(tmplink);
    }
  }
  *modebuf = '\0';
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
** Remove bells and commas from channel name
*/

static void
clean_channelname(cn)
char *cn;
{
    for (; *cn; cn++)
	if (*cn == '\007' || *cn == ' ' || *cn == ',')
	    {
	    *cn = '\0';
	    return;
	    }
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static aChannel *get_channel(cptr, i, flag)
aClient *cptr;
char *i;
int flag;
    {
	Reg1 aChannel *chptr = channel;
	int number;

	if (i == (char *) 0)
		return NullChn;

	if (chptr = hash_find_channel(i, (aChannel *)NULL))
		return (chptr);
	if (flag == CREATE) {
	  if (!(chptr = (aChannel *) malloc(sizeof(aChannel) +
					    strlen(i))))
	    {
	      debug(DEBUG_FATAL,
		    "Out of memory. Cannot allocate channel");
	      restart();
	    }
	  strcpy(chptr->chname, i);
	  chptr->topic[0] = '\0';
	  chptr->users = 0;
	  chptr->mode.limit = 0;
	  chptr->members = (Link *) 0;
	  chptr->invites = (Invites *) 0;
	  if (channel)
	    channel->prevch = chptr;
	  chptr->prevch = NullChn;
	  chptr->nextch = channel;
	  channel = chptr;
	  number = atoi(chptr->chname);
	  addToChannelHashTable(i, chptr);
	  
	  if (number < 0)
	    chptr->mode.mode = MODE_SECRET;
	  else if (number > 0)
	    if (number < 1000)
	      chptr->mode.mode = 0x0;
	    else
	      chptr->mode.mode = MODE_PRIVATE;
	  else {
	    chptr->mode.mode = 0x0;
	  }
	}
	return chptr;
    }

void AddInvite(cptr, chptr)
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

void DelInvite(cptr)
aClient *cptr;
{
  Invites **inv, *tmp;
  if (cptr->user->invited) {
    for (inv = &(cptr->user->invited->invites); inv && *inv;
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
static void sub1_from_channel(xchptr)
aChannel *xchptr;
    {
	Reg1 aChannel *chptr = xchptr;

	if (--chptr->users <= 0) {
	  Invites *oldinv = (Invites *) 0, *inv = chptr->invites;
	  /* Now, find all invite links from channel structure */
	  while (inv) {
	    inv->user->user->invited = NullChn;
	    oldinv = inv;
	    inv = inv->next;
	    free(oldinv);
	  }
	  if (chptr->prevch)
	    chptr->prevch->nextch = chptr->nextch;
	  else
	    channel = chptr->nextch;
	  if (chptr->nextch)
	    chptr->nextch->prevch = chptr->prevch;
	  delFromChannelHashTable(chptr->chname, chptr);
	  free(chptr);
	}
    }

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
*/
m_join(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	int i, flags = 0;

	CheckRegisteredUser(sptr);

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
			   "NOTICE %s :*** What channel do you want to join?",
			   parv[0]);
		return 0;
	    }

	clean_channelname(parv[1]);

	if (atoi(parv[1]) == 0 && parv[1][0] != '+' && parv[1][0] != '#') {
	  if (parv[1][0] == '0' && sptr->user->channel) {
	    sendto_serv_butone(cptr, ":%s CHANNEL 0", parv[0]);
	    sendto_channel_butserv(sptr->user->channel,
				   ":%s PART %s", parv[0],
				   sptr->user->channel->chname);
	    RemoveUserFromChannel(sptr, sptr->user->channel);
	    sptr->user->channel = (aChannel *) 0;
	    return 0;
	  }
	  sendto_one(sptr, ":%s %d %s %s :Not a valid channel",
		     me.name, ERR_NOSUCHCHANNEL, parv[0], parv[1]);
	  return 0;
	}

	if (MyConnect(sptr)) {
	  if (!ChannelExists(parv[1]))
	    flags = FLAG_CHANOP;

	  if (CountChannels(sptr) >= MAXCHANNELSPERUSER) {
	    sendto_one(sptr, ":%s %d %s %s :You have joined too many channels",
		       me.name, ERR_TOOMANYCHANNELS, parv[0], parv[1]);
	    return 0;
	  }
	}
	chptr = get_channel(sptr, parv[1], CREATE);
	if (!chptr || (cptr == sptr && (i = CanJoin(sptr, chptr))))
	  {
	    sendto_one(sptr,":%s %d %s %s :Sorry, cannot join channel.",
		       me.name, i, parv[0], chptr->chname);
	    return 0;
	  }
	
	if (IsMember(sptr, chptr)) {
	  return 0;
	}
	if (parv[1][0] == '#')
	  sendto_serv_butone(cptr, ":%s JOIN %s", parv[0], chptr->chname);
	else {
	  sendto_serv_butone(cptr, ":%s CHANNEL %s", parv[0],
			     chptr->chname);
	  if (sptr->user->channel) {
	    sendto_channel_butserv(sptr->user->channel,
				   ":%s PART %s", parv[0],
				   sptr->user->channel->chname);
	    RemoveUserFromChannel(sptr, sptr->user->channel);
	  }
	  sptr->user->channel = chptr;
	}
	DelInvite(sptr);
	/*
	 **  Complete user entry to the new channel (if any)
	 */
	AddUserToChannel(chptr, sptr, flags);
	/* notify all other users on the new channel */
	sendto_channel_butserv(chptr, ":%s JOIN %s", parv[0],
			       chptr->chname);
	if (parc > 2) {
	  m_mode(cptr, sptr, parc, parv);
	}
	if (flags == FLAG_CHANOP)
		sendto_serv_butone(cptr, ":%s MODE %s +o %s",
				   me.name, parv[1], parv[0]);
#ifdef AUTOTOPIC
	if (MyClient(sptr) && IsPerson(sptr)) {
	  if (chptr->topic[0] != '\0')
	    sendto_one(sptr, ":%s %d %s :%s", me.name, RPL_TOPIC,
		       parv[0], chptr->topic);
	  m_names(cptr, sptr, parc, parv);
	}
#endif
#ifdef MSG_NOTE
	check_messages(cptr, sptr,sptr->name,'r');
#endif
	return 0;
      }

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
*/
m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	int i;
	Reg1 char *ch;

	CheckRegisteredUser(sptr);

	if (parc < 2 || *parv[1] == '\0')
	  {
	    sendto_one(sptr,
		       "NOTICE %s :*** What channel do you want to part?",
		       parv[0]);
	    return 0;
	  }
	ch = parv[1];

	chptr = get_channel(sptr, parv[1], 0);
	if (!chptr) {
	  sendto_one(sptr, ":%s %d %s %s :No such channel exists",
		     me.name, ERR_NOSUCHCHANNEL, parv[0], parv[1]);
	  return 0;
	}
	if (!IsMember(sptr, chptr)) {
	  sendto_one(sptr, ":%s %d %s %s %s :You're not on channel",
		     me.name, ERR_NOTONCHANNEL, parv[0], parv[1],
		     chptr->chname);
	  return 0;
	}
	/*
	**  Remove user from the old channel (if any)
	*/
	if (parv[1][0] == '#') {
	  sendto_serv_butone(cptr, ":%s PART %s", parv[0], chptr->chname);
	} else {
	  sendto_serv_butone(cptr, ":%s CHANNEL 0", parv[0],
			     chptr->chname);
	}
	sendto_channel_butserv(chptr, ":%s PART %s", parv[0],
			       chptr->chname);
	RemoveUserFromChannel(sptr, chptr);
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
  aChannel *chptr;
  CheckRegisteredUser(sptr);

  if (parc < 3 || *parv[1] == '\0')
    {
      sendto_one(sptr,
		 "NOTICE %s :%s", parv[0],
		 "*** Who do you want to kick off from which channel?");
      return 0;
    }
  chptr = get_channel(sptr, parv[1], !CREATE);
  if (!chptr) {
    sendto_one(sptr, ":%s %d %s :You haven't joined that channel",
	       me.name, ERR_USERNOTINCHANNEL, parv[0]);
    return 0;
  }
  who = find_person(parv[2], (aClient *) 0);
  if (!who) {
    if (MyClient(sptr))
      sendto_one(sptr,
		 ":%s %d %s %s %s :%s", me.name,
		 ERR_NOTONCHANNEL, parv[0], parv[2], chptr->chname,
		 "Cannot kick user off channel");
    return 0;
  }
  if (IsMember(who, chptr) && IsChanOp(sptr, chptr)) {
    sendto_channel_butserv(chptr, ":%s KICK %s %s", parv[0],
			   chptr->chname, who->name);
    sendto_serv_butone(cptr, ":%s KICK %s %s", parv[0],
		       chptr->chname, who->name);
    RemoveUserFromChannel(who, chptr);
  } else if (!IsChanOp(sptr, chptr)) {
    sendto_one(sptr, ":%s %d %s %s :You're not channel operator",
	       me.name, ERR_CHANOPRIVSNEEDED, parv[0], chptr->chname);
  } else
    sendto_one(sptr, ":%s %d %s %s %s :isn't on your channel !",
	       me.name, ERR_NOTONCHANNEL, parv[0], who->name, chptr->chname);
  return (0);
}

int	count_channels(sptr)
aClient	*sptr;
{
	Reg1	aChannel	*chptr;
	Reg2	int	count = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
		if (SecretChannel(chptr)) {
			if (IsAnOper(sptr))
				count++;
		}
		else
			count++;
	return (count);
}
