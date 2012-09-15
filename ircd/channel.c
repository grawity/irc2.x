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
#include "whowas.h"
#include "channel.h"

aChannel *channel = NullChn;

extern char *get_client_name();
extern int maxusersperchannel;
extern aClient *find_person(), me, *find_client();
extern Link *find_user_link();

extern aChannel *hash_find_channel();
static void sub1_from_channel();
static int add_banid(), del_banid();
static int set_mode();
static Link *is_banned();

static char *PartFmt = ":%s PART %s";
static char namebuf[NICKLEN+USERLEN+HOSTLEN+3];


static int list_length(tlink)
Link *tlink;
{
  Reg1 Link *link;
  int count = 0;

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
static aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char *user;
int *chasing;
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

static char *make_nick_user_host(nick, name, host)
char *nick, *name, *host;
{
	static	char	star[2] = "*";

	bzero(namebuf, sizeof(namebuf));
	sprintf(namebuf, "%s!%s@%s", BadPtr(nick) ? star : nick,
				     BadPtr(name) ? star : name,
				     BadPtr(host) ? star : host);
	return (namebuf);
}

/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static int add_banid(chptr, banid)
aChannel *chptr;
char *banid;
    {
	Reg1 Link *ban;

	for (ban = chptr->banlist; ban; ban = ban->next)
		if (mycmp(banid, ban->value.cp)==0)
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
char *banid;
    {
	Reg1 Link **ban = NULL;
	Reg2 Link *tmp = NULL;

	if (!chptr || !banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ) {
		tmp = *ban;
		if (mycmp(banid, tmp->value.cp)==0) {
			*ban = tmp->next;
			free(tmp->value.cp);
			free(tmp);
		    }
		else
			ban = &(tmp->next);
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
		if (matches(tmp->value.cp, namebuf)==0)
			break;
	return (tmp);
    }

static void add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
unsigned char flags;
{
  Reg1 Link *curr = (Link *)MyMalloc(sizeof(Link));
  Reg2 Link *ptr;

  curr->value.cptr = who;
  curr->flags = flags;
  curr->next = chptr->members;
  chptr->members = curr;
  chptr->users++;

  ptr = (Link *)MyMalloc(sizeof(Link));
  ptr->value.chptr = chptr;
  ptr->next = who->user->channel;
  who->user->channel = ptr;
}

void remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
  Reg1 Link **curr = &(chptr->members);
  Reg2 Link *tmp;

  while (*curr) {
    if ((*curr)->value.cptr == sptr) {
      tmp = (*curr)->next;
      free(*curr);
      *curr = tmp;
      break;
    } else
      curr = &((*curr)->next);
  }
  for (curr = &(sptr->user->channel); *curr; curr = &((*curr)->next))
    if ((*curr)->value.chptr == chptr) {
      tmp = (*curr)->next;
      free(*curr);
      *curr = tmp;
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

  if (chan->mode.mode & MODE_NOPRIVMSGS
      && !member)
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

channel_modes(modebuf, parabuf, chptr)
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

m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int parc;
char *parv[];
{
  int mcount = 0, chanop;
  char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
  aChannel *chptr;
  aClient *acptr;

  CheckRegistered(sptr);
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
      mcount = set_mode(sptr, chptr, parc - 2, parv + 2, modebuf, parabuf);
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
	  sendto_one(sptr, ":%s %d %s %s :You're not channel operator",
		     me.name, ERR_CHANOPRIVSNEEDED, parv[0], chptr->chname);
	  return 0;
	}
      if ((IsServer(cptr) && !IsServer(sptr) && !chanop) || mcount == -1)
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

static int set_mode(cptr, chptr, parc, parv, modebuf, parabuf)
aClient *cptr;
aChannel *chptr;
int parc;
char *parv[];
char *modebuf;
char *parabuf;
{
  char *curr = parv[0];
  char *tmp;
  unsigned char new = '\0', old = '\0';
  int whatt = MODE_ADD;
  int limitset = 0;
  int nusers, i = 0, ischop = 0, count = 0;
  int chasing = 0;
  Link *addops = (Link *)NULL, *delops = (Link *)NULL, *tmplink;
  Link *addban = (Link *)NULL, *delban = (Link *)NULL;
  aClient *who;
  static char flags[] = {   MODE_PRIVATE,    'p', MODE_SECRET,     's',
			    MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
			    MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
			    0x0, 0x0 };
  Mode *mode;

  *modebuf = *parabuf = '\0';
  if (!chptr)
    return 0;
  if (parc < 1)
    return 0;

  mode = &(chptr->mode);
  ischop = is_chan_op(cptr, chptr) || IsServer(cptr);
  new = old = mode->mode;

  while (curr && *curr && count >= 0 && i < MODEBUFLEN - 1) {
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
	for (link = &(chptr->invites); link && *link; ) {
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
	modebuf[i] = '\0';
      }
    }
    curr++;
  } /* end of while loop for MODE processing */
  whatt = 0;
  parabuf[0] = '\0';
  for (tmp = flags; tmp[0]; tmp += 2) {
    if ((tmp[0] & new) && !(tmp[0] & old)) {
      if (whatt == 0) {
	*(modebuf++) = '+';
	whatt = 1;
      }
      if (ischop)
	mode->mode |= tmp[0];
      *(modebuf++) = tmp[1];
    }
  }
  for (tmp = flags; tmp[0]; tmp += 2) {
    if ((tmp[0] & old) && !(tmp[0] & new)){
      if (whatt != -1) {
	*(modebuf++) = '-';
	whatt = -1;
      }
      if (ischop)
	mode->mode &= ~tmp[0];
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
    if (ischop)
      mode->limit = nusers;
  }
  if (addops) {
    if (whatt != 1) {
      *(modebuf++) = '+';
      whatt = 1;
    }
    while (addops) {
      tmplink = addops;
      addops = addops->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN) {
	*(modebuf++) = 'o';
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
      *(modebuf++) = '-';
      whatt = -1;
    }
    while (delops) {
      tmplink = delops;
      delops = delops->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN) {
	*(modebuf++) = 'o';
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
      *(modebuf++) = '+';
      whatt = 1;
    }
    while (addban) {
      tmplink = addban;
      addban = addban->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 14 < MODEBUFLEN)
	if (ischop) {
	  char *nick, *user, *host, *buf;
	  nick = tmplink->value.cp;
	  user = index(nick, '!');
	  if ((host = index(user ? user : nick, '@'))!=NULL)
	    *host++ = '\0';
	  if (user)
	    *user++ = '\0';
	  buf = make_nick_user_host(nick, user, host);
	  if (!add_banid(chptr, buf)) {
	    *(modebuf++) = 'b';
	    strcat(parabuf, buf);
	    strcat(parabuf, " ");
	  }
        }
      free(tmplink);
    }
  }
  if (delban) {
    if (whatt != -1) {
      *(modebuf++) = '-';
      whatt = -1;
    }
    while (delban) {
      tmplink = delban;
      delban = delban->next;
      if (strlen(parabuf) + strlen(tmplink->value.cp) + 10 < MODEBUFLEN)
	if (ischop) {
	  del_banid(chptr, tmplink->value.cp);
	  *(modebuf++) = 'b';
	  strcat(parabuf, tmplink->value.cp);
	  strcat(parabuf, " ");
	}
      free(tmplink);
    }
  }
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
static aChannel *get_channel(cptr, i, flag)
aClient *cptr;
char *i;
int flag;
    {
	Reg1 aChannel *chptr = channel;

	if (i == (char *) 0)
		return NullChn;

	if (chptr = hash_find_channel(i, NullChn))
		return (chptr);
	if (flag == CREATE) {
	  chptr = (aChannel *)MyMalloc(sizeof(aChannel) + strlen(i));
	  strcpy(chptr->chname, i);
	  chptr->topic[0] = '\0';
	  chptr->users = 0;
	  chptr->mode.limit = 0;
	  chptr->members = (Link *)NULL;
	  chptr->invites = (Link *)NULL;
	  chptr->banlist = (Link *)NULL;
	  if (channel)
	    channel->prevch = chptr;
	  chptr->prevch = NullChn;
	  chptr->nextch = channel;
	  channel = chptr;
	  add_to_channel_hash_table(i, chptr);
	  chptr->mode.mode = 0x0;
	}
	return chptr;
    }

void add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
  Link *inv, **tmp;

  del_invite (cptr,chptr);
  /* delete last link in chain if the list is max length */
  if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER) {
    inv = cptr->user->invited;
    cptr->user->invited = inv->next;
    free(inv);
  }
  /* add client to channel invite list */
  inv = (Link *)MyMalloc(sizeof(Link));
  inv->value.cptr = cptr;
  inv->next = chptr->invites;
  chptr->invites = inv;
  /* add channel to the end of the client invite list */
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

  for (inv = &(chptr->invites); inv && *inv; inv = &((*inv)->next)) {
    if ((*inv)->value.cptr == cptr) {
      tmp = *inv;
      *inv = tmp->next;
      free(tmp);
      break;
    }
  }
  for (inv = &(cptr->user->invited); inv && *inv; inv = &((*inv)->next)) {
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
**  block, if channel became empty). Currently negative
**  channels don't have the channel block allocated. But,
**  just in case they some day will have that, it's better
**  call this for those too.
*/
static void sub1_from_channel(xchptr)
aChannel *xchptr;
    {
	Reg1 aChannel *chptr = xchptr;
	Reg2 Link *btmp;
	Link *oldinv = (Link *)NULL, *inv = (Link *)NULL;
	Link *obtmp;

	if (--chptr->users <= 0) {
	  /*
	   * Now, find all invite links from channel structure
	   */
	  for (inv = chptr->invites; inv; inv = oldinv) {
	    oldinv = inv->next;
	    del_invite(inv->value.cptr, xchptr);
	  }
	  btmp = chptr->banlist;
	  while (btmp != (Link *)NULL) {
	    obtmp = btmp;
	    btmp = btmp->next;
	    free(obtmp);
	  }
	  if (chptr->prevch != NullChn)
	    chptr->prevch->nextch = chptr->nextch;
	  else
	    channel = chptr->nextch;
	  if (chptr->nextch != NullChn)
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
m_join(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	int i, flags = 0;
	Reg1 Link *link;
	Link *link2;
	char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], *name, *p = NULL;

	CheckRegisteredUser(sptr);

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
		    link = sptr->user->channel;
		    while (link)
		      {
			chptr = link->value.chptr;
			sendto_channel_butserv(chptr, sptr, PartFmt,
						parv[0], chptr->chname);
			sendto_serv_butone(chptr, PartFmt,
					   parv[0], chptr->chname);
			remove_user_from_channel(sptr, chptr);
			link2 = link->next;
			free(link);
			sptr->user->channel = link2;
			link = link2; 
		      }
		    sptr->user->channel = (Link *) 0;
		    return 0;
		  }
		if (MyClient(sptr))
		    sendto_one(sptr, ":%s %d %s %s :Not a valid channel",
				me.name, ERR_NOSUCHCHANNEL, parv[0], name);
		return 0;
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
		return 0;

	    if (!chptr || (cptr == sptr && (i = can_join(sptr, chptr))))
	      {
		sendto_one(sptr,":%s %d %s %s :Sorry, cannot join channel.",
			   me.name, i, parv[0], name);
		return 0;
	      }

	    del_invite(sptr, chptr);
	    /*
	    **  Complete user entry to the new channel (if any)
	    */
	    add_user_to_channel(chptr, sptr, flags);

	    /* notify all other users on the new channel */
	    sendto_channel_butserv(chptr, sptr, ":%s JOIN %s",
				   parv[0], name);

	    sendto_serv_butone(cptr, ":%s JOIN %s", parv[0], name);
	    if (parc > 2)
		set_mode(sptr, chptr, parc - 2, parv + 2, modebuf, parabuf);

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
m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	Reg1 aChannel *chptr;
	char *p = NULL, *name;

	CheckRegisteredUser(sptr);

	if (parc < 2 || parv[1][0] == '\0')
	  {
	    sendto_one(sptr,
		       "NOTICE %s :*** What channel do you want to part?",
		       parv[0]);
	    return 0;
	  }
	if ((parv[1][0] != '#') && IsServer(cptr)) /* drop these on the floor */
	    return 0;

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
m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *who;
	aChannel *chptr;
	int chasing = 0;
	CheckRegistered(sptr);

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
			   "NOTICE %s :%s", parv[0],
		       "*** Who do you want to kick off from which channel?");
		return 0;
	    }
/* delete this later. here now to stop large error notice flow -avalon*/
	if (*parv[1] == '+')
	    {
		if (MyClient(sptr))
			sendto_one(sptr, ":%s %d %s %s :Not A Valid Channel",
				   me.name, ERR_NOSUCHCHANNEL,
				   parv[0], parv[1]);
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
m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr = NullChn;
	char *topic = (char *)NULL;
	
	CheckRegisteredUser(sptr);

	if (parc < 2) {
	  sendto_one(sptr, ":%s %d %s :No Channel name given for topic",
		     me.name, ERR_NEEDMOREPARAMS, parv[0]);
	  return 0;
	}
	
	if ((parv[1][0] == '+') || atoi(parv[1]) ||
	    (parc < 3 && parv[1][0] != '#'))
		return 0; /* stop whining to those on 2.6.* */

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
m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	aChannel *chptr = NullChn;

	CheckRegisteredUser(sptr);
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
		sendto_one(acptr, ":%s INVITE %s %s",
			   parv[0], parv[1], parv[2]);
		return 0;
	    }

	if (chptr && !IsMember(sptr, chptr)) {
	  sendto_one(sptr, ":%s %d %s %s :You're not on channel %s",
		     me.name, ERR_NOTONCHANNEL, parv[0], chptr->chname,
		     chptr->chname);
	  return -1;
	}

	if (chptr && (chptr->mode.mode & MODE_INVITEONLY)) {
	  if (!is_chan_op(sptr, chptr)) {
	    sendto_one(sptr, ":%s %d %s %s :You're not channel operator",
		       me.name, ERR_CHANOPRIVSNEEDED, parv[0],
		       chptr->chname);
	    return -1;
	  } else if (!IsMember(sptr, chptr)) {
	    sendto_one(sptr, ":%s %d %s %s :Channel is invite-only.",
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
		/* 'find_person' does not guarantee 'acptr->user' --msa */
		if (acptr->user && acptr->user->away)
			sendto_one(sptr,":%s %d %s %s :%s", me.name,
				   RPL_AWAY, parv[0], acptr->name,
				   acptr->user->away);
	    }
	if (MyConnect(acptr))
	  if (chptr && (chptr->mode.mode & MODE_INVITEONLY) &&
	      sptr->user && is_chan_op(sptr, chptr))
	    add_invite(acptr, chptr);
	sendto_one(acptr,":%s INVITE %s %s",parv[0],
		   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
    }


