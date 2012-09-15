/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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

char channel_id[] = "channel.c v2.0 (c) 1990 University of Oulu, Computing\
 Center and Jarkko Oikarinen";

#include <stdio.h>
#include "struct.h"
#include "sys.h"
#include "common.h"
#include "numeric.h"
#include "channel.h"

aChannel *channel = NullChn;

extern	char	*get_client_name PROTO((aClient *, int));
extern	int	maxusersperchannel;
extern	aClient	*find_person PROTO((char *, aClient *));
extern	aClient	*find_client PROTO((char *, aClient *));
extern	aClient	*get_history PROTO((char *, long));
extern	aClient	me, *client;
extern	Link	*find_user_link PROTO((Link *, aClient *));

extern	aChannel *hash_find_channel PROTO((char *, aChannel *));
static	void	sub1_from_channel PROTO((aChannel *));
static	int	add_banid PROTO((aChannel *, char *));
static	int	del_banid PROTO((aChannel *, char *));
static int set_mode PROTO((aClient *,aChannel *,int,char **,char *,char *));
static	Link	*is_banned PROTO((aClient *, aChannel *));

static	char	*PartFmt = ":%s PART %s";
static	char	namebuf[NICKLEN+USERLEN+HOSTLEN+3];
#define BIGBUFFERSIZE 2000
static	char	buf[BIGBUFFERSIZE];

static	int	list_length(tlink)
Link	*tlink;
{
	Reg1	Link *link;
	int	count = 0;

	for (link = tlink; link; link = link->next)
		count++;

	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
static	aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char	*user;
int	*chasing;
    {
	aClient *who = find_client(user, (aClient *) 0);

	if (chasing != NULL)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	    {
		sendto_one(sptr, ":%s %d %s %s :No such nick (channel)",
			   me.name, ERR_NOSUCHNICK, sptr->name, user);
		return NULL;
	    }
	if (chasing != NULL)
		*chasing = 1;
	return who;
    }

static	char *make_nick_user_host(nick, name, host)
char	*nick, *name, *host;
{
	static	char	star[2] = "*";

	bzero(namebuf, sizeof(namebuf));
	sprintf(namebuf, "%.9s!%.10s@%.50s", BadPtr(nick) ? star : nick,
		     BadPtr(name) ? star : name, BadPtr(host) ? star : host);
	return (namebuf);
}

/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static	int	add_banid(chptr, banid)
aChannel *chptr;
char	*banid;
    {
	Reg1 Link *ban;

	for (ban = chptr->banlist; ban; ban = ban->next)
		if (mycmp(banid, ban->value.cp) == 0)
			return -1;
	ban = (Link *)MyMalloc(sizeof(Link));
	bzero(ban, sizeof(Link));
	ban->next = chptr->banlist;
	ban->value.cp = (char *)MyMalloc(strlen(banid)+1);
	strcpy(ban->value.cp, banid);
	chptr->banlist = ban;
	return 0;
    }

/*
 * del_banid - delete an id belonging to cptr
 * if banid is null, deleteall banids belonging to cptr.
 */
static int del_banid(chptr, banid)
aChannel *chptr;
char	*banid;
    {
	Reg1 Link **ban;
	Reg2 Link *tmp;

	if (!chptr || !banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->value.cp)==0) {
			tmp = *ban;
			*ban = tmp->next;
			free(tmp->value.cp);
			free(tmp);
			break;
		    }

	return 0;
    }

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
static Link *is_banned(cptr, chptr)
aClient *cptr;
aChannel *chptr;
    {
	Reg1 Link *tmp;

	if (!IsPerson(cptr))
		return NULL;

	make_nick_user_host(cptr->name, cptr->user->username,
			    cptr->user->host);

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if (matches(tmp->value.cp, namebuf) == 0)
			break;
	return (tmp);
    }

static void add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
unsigned char flags;
{
  Reg1 Link *ptr;

  if (who->user)
    {
      ptr = (Link *)MyMalloc(sizeof(Link));
      ptr->value.cptr = who;
      ptr->flags = flags;
      ptr->next = chptr->members;
      chptr->members = ptr;
      chptr->users++;

      ptr = (Link *)MyMalloc(sizeof(Link));
      ptr->value.chptr = chptr;
      ptr->next = who->user->channel;
      who->user->channel = ptr;
    }
}

void	remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
  Reg1 Link **curr;
  Reg2 Link *tmp;

  for (curr = &(chptr->members); *curr; curr = &((*curr)->next))
    if ((*curr)->value.cptr == sptr)
     {
      tmp = *curr;
      *curr = tmp->next;
      free(tmp);
      break;
     }
  for (curr = &(sptr->user->channel); *curr; curr = &((*curr)->next))
    if ((*curr)->value.chptr == chptr)
     {
      tmp = *curr;
      *curr = tmp->next;
      free(tmp);
      break;
    }
  sub1_from_channel(chptr);
}

static void clear_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link;
  if (link = find_user_link(chptr->members, cptr))
    link->flags &= ~FLAG_CHANOP;
}

static void set_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link;
  if (link = find_user_link(chptr->members, cptr))
    link->flags |= FLAG_CHANOP;
}

int is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *link;

  if (chptr)
    if (link = find_user_link(chptr->members, cptr))
    	return (link->flags & FLAG_CHANOP);

  return 0;
}

int can_send(user, chan)
aClient *user;
aChannel *chan;
{
  int member;

  member = IsMember(user, chan);
  if (chan->mode.mode & MODE_MODERATED
      && !is_chan_op(user, chan) && member)
    return (MODE_MODERATED);

  if (chan->mode.mode & MODE_NOPRIVMSGS && !member)
    return (MODE_NOPRIVMSGS);

  return (0);
}

aChannel *find_channel(chname, para)
char *chname;
aChannel *para;
{
  aChannel *ch2ptr;

  ch2ptr = hash_find_channel(chname, para);
  return (ch2ptr == para) ? para : ch2ptr;
}

int channel_modes(modebuf, parabuf, chptr)
char *modebuf, *parabuf;
aChannel *chptr;
{
  int i = 0;
  modebuf[i++] = '+';
  if (chptr->mode.mode & MODE_PRIVATE)
    modebuf[i++] = 'p';
  if (chptr->mode.mode & MODE_SECRET)
    modebuf[i++] = 's';
  if (chptr->mode.mode & MODE_MODERATED)
    modebuf[i++] = 'm';
  if (chptr->mode.mode & MODE_TOPICLIMIT)
    modebuf[i++] = 't';
  if (chptr->mode.mode & MODE_INVITEONLY)
    modebuf[i++] = 'i';
  if (chptr->mode.mode & MODE_NOPRIVMSGS)
    modebuf[i++] = 'n';
  if (chptr->mode.limit) {
    modebuf[i++] = 'l'; 
    sprintf(parabuf,"%d", chptr->mode.limit);
  }
  modebuf[i] = '\0';
  return i;
}

void send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
    {
	Reg1 Link *link;
	aClient *acptr;
	char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], *cp;
	int count = 0, send = 0;

	*modebuf = *parabuf = '\0';
	channel_modes(modebuf, parabuf, chptr);
	cp = modebuf + strlen(modebuf);
	if (*parabuf)	/* mode +l xx */
		count = 1;
	for (link = chptr->members ; link; )
	    {
		acptr = link->value.cptr;
		if (!is_chan_op(acptr, chptr))
		    {
			link = link->next;
			continue;
		    }
		if (strlen(parabuf) + strlen(acptr->name) + 10 <
		    MODEBUFLEN)
		    {
			strcat(parabuf, " ");
			strcat(parabuf, acptr->name);
			count++;
			*cp++ = 'o';
			*cp = '\0';
			link = link->next;
		    }
		else if (*parabuf)
			send = 1;
		if (count == 3)
			send = 1;
		if (send)
		    {
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);
			send = 0;
			count = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			*cp = '\0';
		    }
	    }
	for (link = chptr->banlist ; link; )
	    {
		if (strlen(parabuf) + strlen(acptr->name) + 10 <
		    MODEBUFLEN)
		    {
			strcat(parabuf, " ");
			strcat(parabuf, link->value.cp);
			count++;
			*cp++ = 'b';
			*cp = '\0';
			link = link->next;
		    }
		else if (*parabuf)
			send = 1;
		if (count == 3)
			send = 1;
		if (send)
		    {
			sendto_one(cptr, ":%s MODE %s %s %s",
				   me.name, chptr->chname, modebuf, parabuf);
			send = 0;
			count = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			*cp = '\0';
		    }
	    }
	if (modebuf[1] || *parabuf)
		sendto_one(cptr, ":%s MODE %s %s %s",
			   me.name, chptr->chname, modebuf, parabuf);
    }

/*
 * m_mode
 * parv[0] - sender
 * parv[1] - channel
 */

int m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
	int	mcount = 0, chanop;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	/* Now, try to find the channel in question */
	if (parc > 1)
	    {
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
		    {
			m_umode(cptr, sptr, parc, parv);
			return 0;
		    }
	    }
	else
	    {
		sendto_one(sptr, ":%s %d %s :No object name given",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
 	 	return 0;
	    }

	chanop = is_chan_op(sptr, chptr);

	if (parc > 2)
		mcount = set_mode(sptr, chptr, parc - 2, parv + 2,
				  modebuf, parabuf);
	else if (parc < 3)
	    {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(modebuf, parabuf, chptr);
		sendto_one(sptr, ":%s %d %s %s %s %s",
		me.name, RPL_CHANNELMODEIS, parv[0],
		chptr->chname, modebuf, parabuf);
		return 0;
	    }

	if (strlen(modebuf) > 1)
	    {
		if (MyConnect(sptr) && !IsServer(sptr) && !chanop)
		    {
			sendto_one(sptr,
				   ":%s %d %s %s :You're not channel operator",
				   me.name, ERR_CHANOPRIVSNEEDED,
				   parv[0], chptr->chname);
			return 0;
		    }
		if ((IsServer(cptr) && !IsServer(sptr) && !chanop) ||
		    mcount == -1)
			sendto_ops("Hack: %s MODE %s %s %s",
				   parv[0], parv[1], modebuf, parabuf);
		sendto_serv_butone(sptr, ":%s MODE %s %s %s", parv[0],
				   chptr->chname, modebuf, parabuf);
		sendto_channel_butserv(chptr, sptr,
					":%s MODE %s %s %s", parv[0],
					chptr->chname, modebuf, parabuf);
	    }
	return 0;
}

static	int set_mode(cptr, chptr, parc, parv, modebuf, parabuf)
aClient *cptr;
aChannel *chptr;
int	parc;
char	*parv[];
char	*modebuf;
char	*parabuf;
{
  char	*curr = parv[0];
  char	*tmp;
  unsigned char new, old;
  int	whatt = MODE_ADD;
  int	limitset = 0;
  int	nusers, ischop, count = 0;
  int	chasing = 0;
  Link	*addops = (Link *)NULL, *delops = (Link *)NULL, *tmplink;
  Link	*addban = (Link *)NULL, *delban = (Link *)NULL;
  aClient *who;
  static char flags[] = {   MODE_PRIVATE,    'p', MODE_SECRET,     's',
			    MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
			    MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
			    0x0, 0x0 };
  Mode	*mode;

  *modebuf = *parabuf = '\0';
  if (!chptr)
    return 0;
  if (parc < 1)
    return 0;

  mode = &(chptr->mode);
  ischop = is_chan_op(cptr, chptr) || IsServer(cptr);
  new = old = mode->mode;

  while (curr && *curr && count >= 0) {
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
	who = find_chasing(cptr, parv[0], &chasing);
	if (who) {
	  if (IsMember(who, chptr)) {
	    if (chasing) {
	      /*
	      ** If this server noticed the nick change, the information
	      ** must be propagated back upstream.
	      ** This is a bit early, but at most this will generate
	      ** just some extra messages if nick appeared more than
	      ** once in the MODE message... --msa
	      */
	      sendto_one(cptr, ":%s MODE %s %s %s", me.name, chptr->chname,
			 whatt == MODE_ADD ? "+o" : "-o", who->name);
	    }
	    if (whatt == MODE_ADD) {
	      tmplink = (Link *)MyMalloc(sizeof(Link));
	      tmplink->next = addops;
	      tmplink->value.cptr = who;
	      addops = tmplink;
	    } else if (whatt == MODE_DEL) {
	      tmplink = (Link *)MyMalloc(sizeof(Link));
	      tmplink->next = delops;
	      tmplink->value.cptr = who;
	      delops = tmplink;
	    }
	    count++;
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
    case 'b':
      parc--;
      if (parc > 0) {
	parv++;
	if (whatt == MODE_ADD) {
	  tmplink = (Link *)MyMalloc(sizeof(Link));
	  tmplink->next = addban;
	  tmplink->value.cp = parv[0];
	  addban = tmplink;
	} else if (whatt == MODE_DEL) {
	  tmplink = (Link *)MyMalloc(sizeof(Link));
	  tmplink->next = delban;
	  tmplink->value.cp = parv[0];
	  delban = tmplink;
	}
	count++;
      } else if (chptr) {
	  Reg1 Link *tmp;

	  for (tmp = chptr->banlist; tmp; tmp = tmp->next)
	    sendto_one(cptr, ":%s %d %s %s %s",
		       me.name, RPL_BANLIST, cptr->name, chptr->chname,
		       tmp->value.cp);
	  sendto_one(cptr, ":%s %d %s :End of Channel Ban List",
		     me.name, RPL_ENDOFBANLIST, cptr->name);
      }
      break;
    case 'l':
      if (whatt == MODE_ADD) {
	parc--;
	if (parc > 0) {
	  parv++;
	  nusers = atoi(parv[0]);
	  limitset = 1;
	  count++;
	  break;
	}
	if (MyClient(cptr))
	  sendto_one(cptr, ":%s %d %s :Number of users on limited channel %s",
	     me.name, ERR_NEEDMOREPARAMS, cptr->name, "not given.");
	} else if (whatt == MODE_DEL) {
	  limitset = 1;
	  nusers = 0;
	}
      break;
    case 'i' : /* falls through for default case */
      if (whatt == MODE_DEL) {
	Link **link;

	if (!ischop) {
	  count = -1;
	  break;
	}
	for (link = &(chptr->invites); *link; ) {
	  tmplink = *link;
	  *link = (*link)->next;
	  free(tmplink);
	}
      }
    default:
      for (tmp = flags; tmp[0]; tmp += 2) {
	if (tmp[1] == *curr)
	  break;
      }
      if (tmp[0]) {
	if (whatt == MODE_ADD) {
	  new |= tmp[0];
	} else {
	  new &= ~tmp[0];
	}
	count++;
      } else {
	if (MyClient(cptr))
	  sendto_one(cptr, ":%s %d %s %c is unknown mode char to me",
		     me.name, ERR_UNKNOWNMODE, cptr->name, *curr);
      }
    }
    curr++;
  } /* end of while loop for MODE processing */
  whatt = 0;
  for (tmp = flags; tmp[0]; tmp += 2) {
    if ((tmp[0] & new) && !(tmp[0] & old)) {
      if (whatt == 0) {
	*modebuf++ = '+';
	whatt = 1;
      }
      if (ischop)
	mode->mode |= tmp[0];
      *modebuf++ = tmp[1];
    }
  }
  for (tmp = flags; tmp[0]; tmp += 2) {
    if ((tmp[0] & old) && !(tmp[0] & new)){
      if (whatt != -1) {
	*modebuf++ = '-';
	whatt = -1;
      }
      if (ischop)
	mode->mode &= ~tmp[0];
      *modebuf++ = tmp[1];
    }
  }
  if (limitset) {
    if (nusers == 0) {
      if (whatt != -1) {
	*modebuf++ = '-';
	whatt = -1;
      }
      *modebuf++ = 'l';
    } else {
      if (whatt != 1) {
	*modebuf++ = '+';
	whatt = 1;
      }
      *modebuf++ = 'l';
      sprintf(parabuf, "%d ", nusers);
    }
    if (ischop)
      mode->limit = nusers;
  }
  if (addops) {
    if (whatt != 1) {
      *modebuf++ = '+';
      whatt = 1;
    }
    while (addops) {
      tmplink = addops;
      addops = addops->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN) {
	*modebuf++ = 'o';
	if (ischop)
	  set_chan_op(tmplink->value.cptr, chptr);
	strcat(parabuf, tmplink->value.cptr->name);
	strcat(parabuf, " ");
      }
      free(tmplink);
    }
  }
  if (delops) {
    if (whatt != -1) {
      *modebuf++ = '-';
      whatt = -1;
    }
    while (delops) {
      tmplink = delops;
      delops = delops->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN) {
	*modebuf++ = 'o';
	if (ischop)
	  clear_chan_op(tmplink->value.cptr, chptr);
	strcat(parabuf, tmplink->value.cptr->name);
	strcat(parabuf, " ");
      }
      free(tmplink);
    }
  }
  if (addban) {
    if (whatt != 1) {
      *modebuf++ = '+';
      whatt = 1;
    }
    while (addban) {
      tmplink = addban;
      addban = addban->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 14 < MODEBUFLEN)
	if (ischop) {
	  char *nick, *user, *host, *buf;
	  nick = tmplink->value.cp;
	  user = (char *)index(nick, '!');
	  if ((host = (char *)index(user ? user : nick, '@'))!=NULL)
	    *host++ = '\0';
	  if (user)
	    *user++ = '\0';
	  buf = make_nick_user_host(nick, user, host);
	  if (!add_banid(chptr, buf)) {
	    *modebuf++ = 'b';
	    strcat(parabuf, buf);
	    strcat(parabuf, " ");
	  }
        }
      free(tmplink);
    }
  }
  if (delban) {
    if (whatt != -1)
      *modebuf++ = '-';
    while (delban) {
      tmplink = delban;
      delban = delban->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN)
	if (ischop) {
	  del_banid(chptr, tmplink->value.cp);
	  *modebuf++ = 'b';
	  strcat(parabuf, tmplink->value.cp);
	  strcat(parabuf, " ");
	}
      free(tmplink);
    }
  }
  *modebuf++ = '\0';
  *modebuf++ = '\0';
  *modebuf++ = '\0';
  *modebuf = '\0';
  return ischop ? count : -1;
}

int can_join(sptr, channel)
aClient *sptr;
aChannel *channel;
{
  Reg1 Link *link;

  if (is_banned(sptr, channel))
    return (ERR_BANNEDFROMCHAN);
  if (channel->mode.mode & MODE_INVITEONLY) {
    for (link = sptr->user->invited; link; link = link->next)
      if (link->value.chptr == channel)
	break;
    if (!link)
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

static void clean_channelname(cn)
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
static aChannel *get_channel(cptr, chname, flag)
aClient *cptr;
char *chname;
int flag;
    {
	Reg1 aChannel *chptr;

	if (chname == (char *)NULL)
		return (aChannel *)NULL;

	if (chptr = hash_find_channel(chname, (aChannel *)NULL))
		return (chptr);
	if (flag == CREATE) {
	  chptr = (aChannel *)MyMalloc(sizeof(aChannel) + strlen(chname));
	  bzero(chptr, sizeof(aChannel));
	  strcpy(chptr->chname, chname);
	  if (channel)
	    channel->prevch = chptr;
	  chptr->prevch = (aChannel *)NULL;
	  chptr->nextch = channel;
	  channel = chptr;
	  add_to_channel_hash_table(chname, chptr);
	}
	return chptr;
    }

void add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *inv, **tmp;

  del_invite (cptr,chptr);
  /*
   * delete last link in chain if the list is max length
   */
  if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER) {
    inv = cptr->user->invited;
    cptr->user->invited = inv->next;
    free(inv);
  }
  /*
   * add client to channel invite list
   */
  inv = (Link *)MyMalloc(sizeof(Link));
  inv->value.cptr = cptr;
  inv->next = chptr->invites;
  chptr->invites = inv;
  /*
   * add channel to the end of the client invite list
   */
  for (tmp = &(cptr->user->invited); *tmp && (*tmp)->next;
       tmp = &((*tmp)->next))
    ;
  inv = (Link *)MyMalloc(sizeof(Link));
  inv->value.chptr = chptr;
  inv->next = (Link *)NULL;
  (*tmp) = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void del_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link **inv, *tmp;

  for (inv = &(chptr->invites); *inv; inv = &((*inv)->next)) {
    if ((*inv)->value.cptr == cptr) {
      tmp = *inv;
      *inv = tmp->next;
      free(tmp);
      break;
    }
  }
  for (inv = &(cptr->user->invited); *inv; inv = &((*inv)->next)) {
    if ((*inv)->value.chptr == chptr) {
      tmp = *inv;
      *inv = tmp->next;
      free(tmp);
      break;
    }
  }
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty).
*/
static void sub1_from_channel(xchptr)
aChannel *xchptr;
    {
	Reg1 aChannel *chptr = xchptr;
	Reg2 Link *tmp;
	Link *obtmp;

	if (--chptr->users <= 0) {
	  /*
	   * Now, find all invite links from channel structure
	   */
	  while (tmp = chptr->invites)
	    del_invite(tmp->value.cptr, xchptr);

	  tmp = chptr->banlist;
	  while (tmp != (Link *)NULL) {
	    obtmp = tmp;
	    tmp = tmp->next;
	    free(obtmp->value.cp);
	    free(obtmp);
	  }
	  if (chptr->prevch != (aChannel *)NULL)
	    chptr->prevch->nextch = chptr->nextch;
	  else
	    channel = chptr->nextch;
	  if (chptr->nextch != (aChannel *)NULL)
	    chptr->nextch->prevch = chptr->prevch;
	  del_from_channel_hash_table(chptr->chname, chptr);
	  free(chptr);
	}
    }

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = new mode
*/
int m_join(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	int	i, flags = 0;
	Reg1	Link	*link;
	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], *name, *p = NULL;

	if (check_registered(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
			   "NOTICE %s :*** What channel do you want to join?",
			   parv[0]);
		return 0;
	    }

	for (; (name = strtoken(&p, parv[1], ",")) != NULL; parv[1] = NULL)
	  {
	    parv[1] = name;
	    clean_channelname(name);

	    if (*name != '#')
	      {
		/*
		** JOIN 0 sends out a part for all channels a user
		** has joined.
		*/
		if (*name == '0' && !atoi(name))
		  {
		    while ((link = sptr->user->channel) != NULL)
		      {
			chptr = link->value.chptr;
			sendto_channel_butserv(chptr, sptr, PartFmt,
						parv[0], chptr->chname);
			sendto_serv_butone(chptr, PartFmt,
					   parv[0], chptr->chname);
			remove_user_from_channel(sptr, chptr);
		      }
		    continue;
		  }
		if (MyClient(sptr))
		    sendto_one(sptr, ":%s %d %s %s :Not a valid channel",
				me.name, ERR_NOSUCHCHANNEL, parv[0], name);
		continue;
	      }

	    if (MyConnect(sptr))
	      {
		/*
		** local client is first to enter prviously nonexistant
		** channel so make them (rightfully) the Channel Operator.
		*/
		if (!ChannelExists(parv[1]))
		    flags = FLAG_CHANOP;

		if (list_length(sptr->user->channel) >= MAXCHANNELSPERUSER)
		  {
		    sendto_one(sptr, ":%s %d %s %s :%s",
				me.name, ERR_TOOMANYCHANNELS, parv[0], name,
				"You have joined too many channels");
		    return 0;
		  }
	      }
	    chptr = get_channel(sptr, name, CREATE);
	    if (IsMember(sptr, chptr))
		continue;

	    if (!chptr || (cptr == sptr && (i = can_join(sptr, chptr))))
	      {
		sendto_one(sptr,":%s %d %s %s :Sorry, cannot join channel.",
			   me.name, i, parv[0], name);
		continue;
	      }

	    del_invite(sptr, chptr);
	    /*
	    **  Complete user entry to the new channel (if any)
	    */
	    add_user_to_channel(chptr, sptr, flags);

	    /* notify all other users on the new channel */
	    sendto_channel_butserv(chptr, sptr, ":%s JOIN %s", parv[0], name);
	    sendto_serv_butone(cptr, ":%s JOIN %s", parv[0], name);
	    if (parc > 2)
		set_mode(sptr, chptr, parc - 2, parv + 2, modebuf, parabuf);
	    else
		*modebuf = *parabuf = '\0';

	    if (flags == FLAG_CHANOP)
		sendto_serv_butone(cptr, ":%s MODE %s +o%s %s %s",
				   me.name, name, modebuf, parv[0], parabuf);
	    if (MyClient(sptr))
	      {
		if (chptr->topic[0] != '\0')
		    sendto_one(sptr, ":%s %d %s :%s", me.name, RPL_TOPIC,
				parv[0], chptr->topic);
		m_names(cptr, sptr, parc, parv);
	      }
	  } /* end for (; name = strtoken(&p, parv[1], ",");...) */
	return 0;
      }

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	Reg1 aChannel *chptr;
	char *p = NULL, *name;

	if (check_registered(sptr))
	    return 0;

	if (parc < 2 || parv[1][0] == '\0')
	  {
	    sendto_one(sptr,
		       "NOTICE %s :*** What channel do you want to part?",
		       parv[0]);
	    return 0;
	  }

	for (; (name = strtoken(&p, parv[1], ",")) != NULL; parv[1] = NULL)
	  {
	    chptr = get_channel(sptr, name, 0);
	    if (!chptr)
	      {
		sendto_one(sptr, ":%s %d %s %s :No such channel exists",
			   me.name, ERR_NOSUCHCHANNEL, parv[0], name);
		return 0;
	      }
	    if (!IsMember(sptr, chptr))
	      {
		if (MyClient(sptr))
		    sendto_one(sptr, ":%s %d %s %s :You're not on channel",
		    		me.name, ERR_NOTONCHANNEL, parv[0], name);
		return 0;
	      }
	    /*
	    **  Remove user from the old channel (if any)
	    */
	    sendto_serv_butone(cptr, PartFmt, parv[0], chptr->chname);
	    sendto_channel_butserv(chptr, sptr, PartFmt, parv[0],
				   chptr->chname);
	    remove_user_from_channel(sptr, chptr);
	  }
	return 0;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *who;
	aChannel *chptr;
	int chasing = 0;

	if (check_registered(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
			   "NOTICE %s :%s", parv[0],
		       "*** Who do you want to kick off from which channel?");
		return 0;
	    }

	chptr = get_channel(sptr, parv[1], !CREATE);
	if (!chptr)
	    {
		sendto_one(sptr, ":%s %d %s :You haven't joined that channel",
			   me.name, ERR_USERNOTINCHANNEL, parv[0]);
		return 0;
	    }
	if (!IsServer(sptr) && !is_chan_op(sptr, chptr))
	    {
		sendto_one(sptr, ":%s %d %s %s :You're not channel operator",
			   me.name, ERR_CHANOPRIVSNEEDED, parv[0],
			   chptr->chname);
		return 0;
	    }
	if (!(who = find_chasing(sptr, parv[2], &chasing)))
		return 0; /* No such user left! */
	if (chasing)
		sendto_one(sptr,":%s NOTICE %s :KICK changed from %s to %s",
			   me.name, parv[0], parv[2], who->name);
	if (IsMember(who, chptr))
	    {
		sendto_channel_butserv(chptr, sptr, ":%s KICK %s %s", parv[0],
				       chptr->chname, who->name);
		sendto_serv_butone(cptr, ":%s KICK %s %s", parv[0],
				   chptr->chname, who->name);
		remove_user_from_channel(who, chptr);
	    }
	else
		sendto_one(sptr, ":%s %d %s %s %s :isn't on your channel !",
			   me.name, ERR_NOTONCHANNEL, parv[0], who->name,
			   chptr->chname);
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

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
*/
int m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr = NullChn;
	char *topic = (char *)NULL;
	
	if (check_registered(sptr))
	  return 0;

	if (parc < 2) {
	  sendto_one(sptr, ":%s %d %s :No Channel name given for topic",
		     me.name, ERR_NEEDMOREPARAMS, parv[0]);
	  return 0;
	}
	
	if (parc > 1 && IsChannelName(parv[1])) {
	  chptr = find_channel(parv[1], NullChn);
	  if (!chptr || !IsMember(sptr, chptr)) {
	      sendto_one(sptr,":%s %d %s %s :Not On Channel",
			 me.name, ERR_NOTONCHANNEL, parv[0], parv[1]);
	      return 0;
	  }
	  if (parc > 2)
	    topic = parv[2];
	}

	if (!chptr)
	    {
		sendto_one(sptr, ":%s %d %s :Bad Craziness",
			   me.name, RPL_NOTOPIC, parv[0]);
		return 0;
	    }
	
	if (!topic)  /* only asking  for topic  */
	    {
		if (chptr->topic[0] == '\0')
			sendto_one(sptr, ":%s %d %s %s :No topic is set.", 
				   me.name, RPL_NOTOPIC, parv[0],
				   chptr->chname);
		else
			sendto_one(sptr, ":%s %d %s %s :%s",
				   me.name, RPL_TOPIC, parv[0],
				   chptr->chname, chptr->topic);
	    } 
	else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
		  is_chan_op(sptr, chptr)) && topic)
	    {
		/* setting a topic */
		strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
		sendto_serv_butone(cptr,":%s TOPIC %s :%s",
				   parv[0], chptr->chname, chptr->topic);
		sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s",
				       parv[0],
				       chptr->chname, chptr->topic);
	    }
	else
	    {
	      sendto_one(sptr, ":%s %d %s %s :Cannot set topic, %s",
			 me.name, ERR_CHANOPRIVSNEEDED, parv[0],
			 chptr->chname, "not channel OPER");
	    }
	return 0;
    }

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :Not enough parameters", me.name,
			   ERR_NEEDMOREPARAMS, parv[0]);
		return -1;
	    }

	if ((acptr = find_person(parv[1],(aClient *)NULL))==NULL)
	    {
		sendto_one(sptr,":%s %d %s %s :No such nickname",
			   me.name, ERR_NOSUCHNICK, parv[0], parv[1]);
		return 0;
	    }
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s %s",
				  parv[0], parv[1], parv[2]);
		return 0;
	    }

	if (chptr && !IsMember(sptr, chptr))
	    {
		sendto_one(sptr, ":%s %d %s %s :You're not on channel %s",
			   me.name, ERR_NOTONCHANNEL, parv[0], chptr->chname,
			   chptr->chname);
		return -1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :%s is already on channel %s",
			   me.name, ERR_USERONCHANNEL, parv[0],
			   parv[1], parv[2]);
		return 0;
	    }
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	    {
		if (!is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr,
				   ":%s %d %s %s :You're not channel operator",
				   me.name, ERR_CHANOPRIVSNEEDED, parv[0],
				   chptr->chname);
			return -1;
		    }
		else if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr,
				   ":%s %d %s %s :Channel is invite-only.",
				   me.name, ERR_CHANOPRIVSNEEDED, parv[0],
				   ((chptr) ? (chptr->chname) : parv[2]));
			return -1;
		    }
	    }

	if (MyConnect(sptr))
	    {
		sendto_one(sptr,":%s %d %s %s %s", me.name,
			   RPL_INVITING, parv[0], acptr->name,
			   ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr,":%s %d %s %s :%s", me.name,
				   RPL_AWAY, parv[0], acptr->name,
				   acptr->user->away);
	    }
	if (MyConnect(acptr))
		if (chptr && (chptr->mode.mode & MODE_INVITEONLY) &&
		    sptr->user && is_chan_op(sptr, chptr))
			add_invite(acptr, chptr);
	sendto_prefix_one(acptr, sptr, ":%s INVITE %s %s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
    }


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aChannel *chptr;

	sendto_one(sptr,":%s %d %s :  Channel  : Users  Name",
		   me.name, RPL_LISTSTART, parv[0]);

	if (parc < 2 || BadPtr(parv[1]))
	    {
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (sptr->user == NULL ||
			    (SecretChannel(chptr) && !IsMember(sptr, chptr)))
				continue;
			sendto_one(sptr,":%s %d %s %s %d :%s",
				   me.name, RPL_LIST, parv[0],
				   ShowChannel(sptr, chptr)?chptr->chname:"*",
				   chptr->users,
				   ShowChannel(sptr, chptr)?chptr->topic:"");
		    }
	    }
	else
	    {
		chptr = find_channel(parv[1], NullChn);
		if (chptr != NullChn && ShowChannel(sptr, chptr) &&
		    sptr->user != NULL)
			sendto_one(sptr, ":%s %d %s %s %d :%s",
				   me.name, RPL_LIST, parv[0],
				   ShowChannel(sptr,chptr)?chptr->chname : "*",
				   chptr->users,
				   chptr->topic ? chptr->topic : "*");
	     }
	sendto_one(sptr,":%s %d %s :End of /LIST",
		   me.name, RPL_LISTEND, parv[0]);
	return 0;
    }


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    { 
	Reg1	aChannel *chptr;
	Reg2	aClient *c2ptr;
	Reg3	Link	*link;
	aChannel *ch2ptr = NULL;
	int	idx, flag;
	char	*para = parc > 1 ? parv[1] : NULL;

	if (!BadPtr(para))
		ch2ptr = find_channel(para, (aChannel *)NULL);

	/* Allow NAMES without registering */

	/* First, do all visible channels (public and the one user self is) */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (!BadPtr(para) && ch2ptr != chptr)
			continue; /* -- wanted a specific channel */
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		sprintf(buf, "* %s :", chptr->chname);

		if (PubChannel(chptr))
			*buf = '=';
		else if (SecretChannel(chptr))
			*buf = '@';
		idx = strlen(buf);
		flag = 0;
		for (link = chptr->members; link; link = link->next)
		    {
			c2ptr = link->value.cptr;
			if (IsInvisible(c2ptr) && !IsMember(sptr,chptr))
				continue;
		        if (TestChanOpFlag(link->flags))
				strcat(buf, "@");
			strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			strcat(buf," ");
			if (idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, ":%s %d %s %s",
					   me.name, RPL_NAMREPLY, parv[0],
					   buf);
				sprintf(buf, "* %s :", chptr->chname);
				if (PubChannel(chptr))
					*buf = '=';
				else if (SecretChannel(chptr))
					*buf = '@';
				idx = strlen(buf);
				flag = 0;
			    }
		    }
		if (flag)
			sendto_one(sptr, ":%s %d %s %s",
				   me.name, RPL_NAMREPLY, parv[0], buf);
	    }
	if (!BadPtr(para)) {
		sendto_one(sptr, ":%s %d %s :* End of /NAMES list.", me.name,
			   RPL_ENDOFNAMES,parv[0]);
		return(1);
	}

	/* Second, do all non-public, non-secret channels in one big sweep */

	strcpy(buf, "* * :");
	idx = strlen(buf);
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
  	        aChannel *ch3ptr;
		int showflag = 0, secret = 0;

		if (!IsPerson(c2ptr) || IsInvisible(c2ptr))
			continue;
		link = c2ptr->user->channel;
		/*
		 * dont show a client if they are on a secret channel or
		 * they are on a channel sptr is on since they have already
		 * been show earlier. -avalon
		 */
		while (link) {
			ch3ptr = link->value.chptr;
			if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
				showflag = 1;
			if (SecretChannel(ch3ptr))
				secret = 1;
			link = link->next;
		}
		if (showflag) /* have we already shown them ? */
		  continue;
		if (secret) /* on any secret channels ? */
		  continue;
		strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		strcat(buf," ");
		flag = 1;
		if (idx + NICKLEN > BUFSIZE - 2)
		    {
			sendto_one(sptr, ":%s %d %s %s",
				   me.name, RPL_NAMREPLY, parv[0], buf);
			strcpy(buf, "* * :");
			idx = strlen(buf);
			flag = 0;
		    }
	    }
	if (flag)
		sendto_one(sptr, ":%s %d %s %s",
			   me.name, RPL_NAMREPLY, parv[0], buf);
	sendto_one(sptr, ":%s %d %s :* End of /NAMES list.", me.name,
		   RPL_ENDOFNAMES, parv[0]);
	return(1);
    }
