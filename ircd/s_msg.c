/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_msg.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
 * $Id: s_msg.c,v 6.1 1991/07/04 21:05:30 gruner stable gruner $
 *
 * $Log: s_msg.c,v $
 * Revision 6.1  1991/07/04  21:05:30  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:46  gruner
 * frozen beta revision 2.6.1
 *
 */

/* -- Jto -- 25 Oct 1990
 * Configuration file fixes. Wildcard servers. Lots of stuff.
 */

char s_msg_id[] = "s_msg.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#include "sys.h"
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <utmp.h>
#include <ctype.h>
#include <stdio.h>
#include "common.h"
#include "msg.h"
#include "numeric.h"
#include "whowas.h"
#include "channel.h"

extern aClient *client, me, *local[];
extern aClient *find_server(), *find_person();
extern aClient *find_userhost();
extern aConfItem *FindConf(), *FindConfName(), *find_admin();
extern aConfItem *conf;
extern int connect_server();
extern int CloseConnection();
extern aChannel *channel;
extern char *debugmode;
extern char *configfile;
extern int maxusersperchannel;
extern int autodie, highest_fd;

#define BIGBUFFERSIZE 2000

static char buf[BIGBUFFERSIZE];

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (??)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */

char *
MyNameForLink(name, conf)
char *name;
aConfItem *conf;
{
  int count;
  static char namebuf[HOSTLEN];
  char *start = name;
  if (!conf)
    return start;
  if (!name)
    return start;
  count = conf->port;
  if (count <= 0 || count > 5) {
    return start;
  }
  while (count--) {
    name++;
    name = index(name, '.');
  }
  namebuf[0] = '*';
  strncpy(&namebuf[1], name, HOSTLEN - 1);
  namebuf[HOSTLEN - 1] = '\0';
  return namebuf;
}

/*****
****** NOTE:	GetClientName should be moved to some other file!!!
******		It's now used from other places...  --msa
*****/

/*
** GetClientName
**	Return the name of the client for various tracking and
**	admin purposes. The main purpose of this function is to
**	return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "servername", if remote server or sockethost == servername
**	  "servername[sockethost]", for local servers otherwise
**	  "nickname", for remote users (or sockethost == nickname, never!)
**	  "nickname[sockethost]", for local users (non-servers)
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char *GetClientName(sptr,spaced)
aClient *sptr;
int spaced; /* ...actually a kludge for for m_server */
{
    static char nbuf[sizeof(sptr->name)+sizeof(sptr->sockhost)+3];

    if (MyConnect(sptr) && mycmp(sptr->name,sptr->sockhost) != 0)
	{
	sprintf(nbuf, "%s%s[%s]",
	    sptr->name, spaced ? " " : "", sptr->sockhost);
	return nbuf;
	}

    return sptr->name;
}

/*
** NextClient
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = NextClient(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
*/
static aClient *NextClient(next, ch)
char *ch;	/* Null terminated search string (may include wilds) */
aClient *next;	/* First client to check */
    {
	for ( ; next != NULL; next = next->next)
		if (matches(ch,next->name) == 0 || matches(next->name,ch) == 0)
			break;
	return next;
    }

/*
** HuntServer
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
*/
#define HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define HUNTED_PASS	1	/* if message passed onwards successfully */

static int HuntServer(cptr, sptr, command, server, parc, parv)
aClient *cptr, *sptr;
char *command;
int server;
int parc;
char *parv[];
    {
	aClient *acptr;

	/*
	** Assume it's me, if no server
	*/
	if (parc <= server || BadPtr(parv[server]) ||
	    matches(me.name, parv[server]) == 0 ||
	    matches(parv[server], me.name) == 0)
		return (HUNTED_ISME);
	for (acptr = client;
	     acptr = NextClient(acptr, parv[server]);
	     acptr = acptr->next)
	    {
		if (IsMe(acptr) || (MyConnect(acptr) &&
				    IsRegisteredUser(acptr)))
			return (HUNTED_ISME);
		if (IsRegistered(acptr) &&
		    acptr != cptr) /* Fix to prevent looping in case the
                                      parameter for some reason happens to
                                      match someone from the from link --jto */
		    { 
			sendto_one(acptr, command, parv[0],
				   parv[1], parv[2], parv[3], parv[4],
				   parv[5], parv[6], parv[7], parv[8]);
			return(HUNTED_PASS);
		    } 
	    }
	sendto_one(sptr, ":%s %d %s %s :*** No such server.", me.name,
			ERR_NOSUCHSERVER, parv[0], parv[server]);
	return(HUNTED_NOSUCH);
    }

/*
** 'DoNickName' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (0, if
**	nickname is illegal)
**
**  Nickname characters are in range
**	'A'..'}', '_', '-', '0'..'9'
**  anything outside the above set will terminate nickname.
**  In addition, the first character cannot be '-'
**  or a Digit.
**
**  Note:
**	'~'-character should be allowed, but
**	a change should be global, some confusion would
**	result if only few servers allowed it...
*/

static int DoNickName(nick)
char *nick;
{
    Reg1 char *ch;

    if (*nick == '-' || isdigit(*nick)) /* first character in [0-9-] */
	return 0;

#define isvalid(c) (((c) >= 'A' && (c) <= '}') || isdigit(c) || (c) == '-')

    for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
	if (!isvalid(*ch) || isspace(*ch))
	    break;

    *ch = '\0';

    return (ch - nick);
}

/*
** RegisterUser
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/
#define SendUser(to, nick, username, host, server, info)\
       sendto_serv_butone(to, ":%s USER %s %s %s :%s",\
			  nick, username, host, server, info)

static RegisterUser(cptr,sptr,nick)
aClient *cptr;
aClient *sptr;
char *nick;
    {
        char *tmp;
	short oldstatus = sptr->status;

	sptr->status = STAT_CLIENT;
	sptr->user->last = time((long *) 0);

	sendto_serv_butone(cptr, "NICK %s", nick);
	SendUser(cptr, nick, sptr->user->username, sptr->user->host,
		 sptr->user->server, sptr->info);
	AddHistory(sptr);
	if (MyConnect(sptr))
	    {
	      sendto_one(sptr,
			 "NOTICE %s :%s %s",
			 nick, "*** Welcome to the Internet Relay Network,",
			 nick);
	      sendto_one(sptr,
			 "NOTICE %s :*** Your host is %s, running version %s",
			 nick, GetClientName(&me,FALSE), version);
	      sendto_one(sptr,
			 "NOTICE %s :*** This server was created %s",
			 nick, creation);
	      tmp = "*";
	      m_lusers(sptr, sptr, 1, &tmp);
	      m_motd(sptr, sptr, 1, &tmp);
	      det_confs_butmask(cptr,
				CONF_CLIENT | CONF_OPERATOR | CONF_LOCOP);
	      det_I_lines_butfirst(cptr);
	      if (oldstatus == STAT_MASTER && MyConnect(sptr)) {
		char *parv[3];
		parv[0] = sptr->name;
		parv[1] = parv[2] = (char *) 0;
		m_oper(&me, sptr, 1, parv);
	      }
	    }
#ifdef MSG_NOTE
	check_messages(cptr, sptr, nick, 'a');
#endif
	return 0;
    }


/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
*/
m_nick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char *nick;
	
	if (parc < 2)
	    {
		sendto_one(sptr,":%s %d %s :No nickname given",
			   me.name, ERR_NONICKNAMEGIVEN, parv[0]);
		return 0;
	    }
	nick = parv[1];
	if (DoNickName(nick) == 0)
	    {
		sendto_one(sptr,":%s %d %s %s :Erroneus Nickname", me.name,
			   ERR_ERRONEUSNICKNAME, parv[0], parv[1]);
		return 0;
	    }
	/*
	** Check against nick name collisions.[a 'for' loop is used here
	** only to be able to use 'break', these tests and actions
	** would get quite hard to follow, if done with plain if's...]
	*/
	for (;;)
	    {
		if ((acptr = find_person(nick,(aClient *)NULL)) == NULL)
			break;  /* No collisions, all clear... */
		/*
		** This allows user to change the case of his/her nick,
		** when it differs only in case, but only if we have
		** properly defined source client (acptr == sptr).
		*/
		if (strncmp(acptr->name, nick, NICKLEN) != 0 && acptr == sptr)
			break;
		/*
		** If the older one is "non-person", the new entry is just
		** allowed to overwrite it. Just silently drop non-person,
		** and proceed with the nick. This should take care of the
		** "dormant nick" way of generating collisions...
		*/
		if (!IsPerson(acptr) && MyConnect(acptr) && acptr != sptr)
		    {
			ExitClient((aClient *)NULL,acptr,&me,"Overridden");
			break;
		    }
		/*
		** Decide, we really have a nick collision and deal with it
		*/
		if (!IsServer(cptr))
		    {
			/*
			** NICK is coming from local client connection. Just
			** send error reply and ignore the command.
			** [There is no point in sending this to remote
			** clients, the following KILL should be sufficient :-]
			*/
			sendto_one(sptr,
				   ":%s %d %s %s :Nickname is already in use.",
				   me.name, ERR_NICKNAMEINUSE,
				   parv[0], nick);
			return 0; /* NICK message ignored */
		    }
		/*
		** NICK was coming from a server connection. Means
		** that the same nick is registerd for different
		** users by different servers. This is either a
		** race condition (two users coming online about
		** same time, or net reconnecting) or just two
		** net fragments becoming joined and having same
		** nicks in use. We cannot have TWO users with
		** same nick--purge this NICK from the system
		** with a KILL... >;)
		*/
		if (sptr == cptr)
		    {
			/*
			** A new NICK being introduced by a neighbouring
			** server (e.g. message type "NICK new" received)
			*/
			sendto_serv_butone((aClient *)NULL, /* all servers */
					   "KILL %s :%s(%s <- %s)",
					   acptr->name,
					   me.name,
					   acptr->from->name,
					   /* NOTE: Cannot use GetClientName
					   ** twice here, it returns static
					   ** string pointer--the other info
					   ** would be lost
					   */
					   GetClientName(cptr, FALSE));
			acptr->flags |= FLAGS_KILLED;
			ExitClient((aClient *)NULL,acptr,&me,"Nick collision");
			return 0; /* Normal NICK collision handled */
		    }
		/*
		** A NICK change has collided (e.g. message type
		** ":old NICK new". This requires more complex cleanout.
		** Both clients must be purged from this server, the "new"
		** must be killed from the incoming connection, and "old" must
		** be purged from all outgoing connections.
		*/
		sendto_serv_butone(cptr, /* KILL old from outgoing servers */
				   "KILL %s :%s(%s(%s) <- %s)",
				   sptr->name,
				   me.name,
				   acptr->from->name,
				   acptr->name,
				   GetClientName(cptr, FALSE));
		acptr->flags |= FLAGS_KILLED;
		ExitClient((aClient *)NULL,acptr,&me,"Nick collision(new)");
		sendto_one(cptr, /* Kill new from incoming link */
			   "KILL %s :%s(%s <- %s(%s))",
			   acptr->name,
			   me.name,
			   acptr->from->name,
			   GetClientName(cptr, FALSE),
			   sptr->name);
		sptr->flags |= FLAGS_KILLED;
		return ExitClient(cptr,sptr,&me,"Nick collision(old)");
	    }
	if (IsServer(sptr))
	    {
		/* A server introducing a new client, change source */

		sptr = make_client(cptr);
		add_client_to_list(sptr);
	    }
	else if (sptr->name[0])
	    {
		/*
		** Client just changing his/her nick. If he/she is
		** on a channel, send note of change to all clients
		** on that channel. Propagate notice to other servers.
		*/
	      sendto_common_channels(sptr, ":%s NICK %s", parv[0], nick);
	      if (sptr->user)
		AddHistory(sptr);
	      sendto_serv_butone(cptr, ":%s NICK %s", parv[0], nick);
#ifdef MSG_NOTE
	      check_messages(cptr, sptr,nick,'n');
#endif
	    }
	else
	    {
		/* Client setting NICK the first time */

		/* This had to be copied here to avoid problems.. */
		strcpy(sptr->name, nick); /* correctness guaranteed
					     by 'DoNickName' */

		if (sptr->user != NULL)
			/*
			** USER already received, now we have NICK.
			** *NOTE* For servers "NICK" *must* precede the
			** user message (giving USER before NICK is possible
			** only for local client connection!).
			*/
			RegisterUser(cptr,sptr,nick);
	    }
	/*
	**  Finally set new nick name.
	*/
	if (sptr->name[0])
		delFromClientHashTable(sptr->name, sptr);
	strcpy(sptr->name, nick); /* correctness guaranteed by 'DoNickName' */
	addToClientHashTable(sptr->name, sptr);
	return 0;
    }

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/

static int
m_message(cptr, sptr, parc, parv, notice)
aClient *cptr, *sptr;
int parc;
char *parv[];
int notice;
{
    aClient *acptr;
    aChannel *chptr;
    char *nick, *server, *p, *cmd;

    if (notice)
	CheckRegistered(sptr);
    else
	CheckRegisteredUser(sptr);
    
    if (parc < 2 || *parv[1] == '\0')
	{
	sendto_one(sptr,":%s %d %s :No recipient given",
	       me.name, ERR_NORECIPIENT, parv[0]);
	return -1;
	}

    if (parc < 3 || *parv[2] == '\0')
	{
	sendto_one(sptr,":%s %d %s :No text to send",
	       me.name, ERR_NOTEXTTOSEND, parv[0]);
	return -1;
	}

    cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

    for (; nick = strtoken(&p, parv[1], ","); parv[1] = NULL)
	{

	/*
	** nickname addressed?
	*/

	if (acptr = find_person(nick, (aClient *)NULL))
	    {
	    if (!notice
		&& MyConnect(sptr)
		&& acptr->user
		&& acptr->user->away)
		sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_AWAY,
		       parv[0], acptr->name, acptr->user->away);

	    sendto_one(acptr, ":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
	    continue;
	    }

	/*
	** channel msg?
	*/

	if (chptr = find_channel(nick, NullChn))
	    {
	    if (CanSend(sptr, chptr) == 0)
		sendto_channel_butone(cptr, chptr, ":%s %s %s :%s", 
		    parv[0], cmd, chptr->chname, parv[2]);
	    else if (!notice)
		sendto_one(sptr, ":%s %d %s %s :Cannot send to channel",
		    me.name, ERR_CANNOTSENDTOCHAN, parv[0], chptr->chname);
	    continue;
	    }

	/*
	** the following two cases allow masks in NOTICEs
	** (for OPERs only)
	**
	** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
	*/

	if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
	    {
	    Reg1 char *s;

	    if (!(s = rindex(nick, '.')))
		{
		sendto_one(sptr, ":%s NOTICE %s :No toplevel domain specified",
			   me.name, parv[0]);
		continue;
		}
	    for(; *s; s++)
		if (*s == '*' || *s == '?' || *s == '.')
		    break;
	    if (*s == '*' || *s == '?')
		{
		sendto_one(sptr, ":%s NOTICE %s :Wildcard in toplevel Domain",
			   me.name, parv[0]);
		continue;
		}
	    sendto_match_butone(IsServer(cptr) ? cptr : NULL, nick + 1,
		(*nick == '#') ? MATCH_HOST : MATCH_SERVER,
		":%s %s %s :%s", parv[0], cmd, nick, parv[2]);
	    continue;
	    }

	/*
	** user%host@server addressed?
	*/

	if ((server = index(nick, '@')) &&
	    (acptr = find_server(server + 1, (aClient *)NULL)))
	    {
	    int count;

	    if (!IsMe(acptr))
		{
		sendto_one(acptr, ":%s %s %s :%s",
		    parv[0], cmd, nick, parv[2]);
		continue;
		}

	    *server = '\0';

	    if (server = index(nick, '%'))
		*server++ = '\0';

	    if (acptr = find_userhost(nick, server, (aClient *)NULL,
				&count))
		{
		if (count > 1)
		    {
		    if (!notice)
			sendto_one(sptr, ":%s %d %s %s :%s",
			    me.name, ERR_TOOMANYTARGETS, parv[0], nick,
			    "Duplicate recipients. No message delivered");
		    continue;
		    }
		else if (count == 1)
		    {
		    sendto_one(acptr, ":%s %s %s@%s :%s",
			parv[0], cmd, nick, server, parv[2]);
		    continue;
		    }
		}
	    }

      sendto_one(sptr, ":%s %d %s %s :No such nickname",
	  me.name, ERR_NOSUCHNICK, parv[0], nick);
      }
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

m_private(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int m_notice(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    return m_message(cptr, sptr, parc, parv, 1);
}

/*
** m_text
**
**	parv[0] = sender prefix
**	parv[1] = text
**
** This function is left ONLY for backward compatibility.
** Being removed SOON. --argv
*/

int
m_text(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    CheckRegistered(sptr);

    if (sptr->user->channel)
	{
	char *a[3];

	a[0] = parv[0];
	a[1] = sptr->user->channel->chname;
	a[2] = parv[1];

	return m_message(cptr, sptr, parc + 1, a, 0);
	}

    sendto_one(sptr, ":%s %d %s :You have not joined any channel",
	me.name, ERR_USERNOTINCHANNEL, parv[0]);
    return -1;
}

static void
DoWho(sptr, acptr, repchan)
aClient *sptr, *acptr;
aChannel *repchan;
{
  char stat[5];
  int i = 0;

  if (acptr->user->away)
    stat[i++] = 'G';
  else
    stat[i++] = 'H';
  if (IsAnOper(acptr))
    stat[i++] = '*';
  if (repchan && IsChanOp(acptr, repchan))
    stat[i++] = '@';
  stat[i] = '\0';
  sendto_one(sptr,"WHOREPLY %s %s %s %s %s %s :%s",
	     (repchan) ? (repchan->chname) : "*",
	     acptr->user->username,
	     acptr->user->host,
	     acptr->user->server, acptr->name,
	     stat, acptr->info);
}

/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
m_who(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *mychannel = (sptr->user != NULL) ?
		sptr->user->channel : NullChn;
	aClient *acptr;
	aChannel *chptr;
	char *channame = (char *) 0;
	int oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	Reg1 char *mask = parc > 1 ? parv[1] : NULL;
	
	/* Allow use of m_who without registering */
	
	/*
	**  Following code is some ugly hacking to preserve the
	**  functions of the old implementation. (Also, people
	**  will complain when they try to use masks like "12tes*"
	**  and get people on channel 12 ;) --msa
	*/
	if (mask == NULL || *mask == '\0')
		mask = NULL;
	else if (mask[1] == '\0' && mask[0] == '*')
	    {
		mask = NULL;
		if (mychannel)
			channame = mychannel->chname;
	    }
	else if (mask[1] == '\0' && mask[0] == '0') /* "WHO 0" for irc.el */
		mask = NULL;
	else
		channame = mask;
	sendto_one(sptr,"WHOREPLY * User Host Server Nickname S :Name");
	if (channame &&
	    (*channame == '+' || atoi(channame) || *channame == '#'))
	    {
		Link *link;
		/* List all users on a given channel */
		chptr = find_channel(channame, NullChn);
		if (chptr && (!SecretChannel(chptr) || IsMember(sptr, chptr)))
			for (link = chptr->members; link; link = link->next)
			    {
				if (oper && !IsAnOper((aClient *)link->value))
					continue;
				DoWho(sptr, (aClient *)(link->value), chptr);
			    }
	    }
	else for (acptr = client; acptr; acptr = acptr->next)
	    {
		if (!IsPerson(acptr) && !IsService(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;
		if (chptr = acptr->user->channel)
		    {
			if (SecretChannel(chptr) && chptr != mychannel)
				continue;
			else if (!PubChannel(chptr) && chptr != mychannel)
				chptr = NULL;
		    }
		/*
		** This is brute force solution, not efficient...? ;( 
		** Show entry, if no mask or any of the fields match
		** the mask. --msa
		*/
		if (mask == NULL ||
		    matches(mask,acptr->name) == 0 ||
		    matches(mask,acptr->user->username) == 0 ||
		    matches(mask,acptr->user->host) == 0 ||
		    matches(mask,acptr->user->server) == 0 ||
		    matches(mask,acptr->info) == 0)
			DoWho(sptr, acptr, chptr);
	    }
	sendto_one(sptr, ":%s %d %s %s :* End of /WHO list.", me.name,
		   RPL_ENDOFWHO, parv[0], parv[1]);
	return 0;
    }

/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
m_whois(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    int found, eol = 0, wilds;
    Reg1 char *nick, *tmp;
    char *p, buf[1024];
    aClient *acptr, *a2cptr;
    aChannel *chptr;

    CheckRegistered(sptr);

    if (parc < 2)
	{
	sendto_one(sptr,":%s %d %s :No nickname specified",
	    me.name, ERR_NONICKNAMEGIVEN, parv[0]);
	return 0;
	}

    if (parc > 2)
	{
	if (HuntServer(cptr,sptr,":%s WHOIS %s %s", 1,parc,parv) != HUNTED_ISME)
	    return 0;
	parv[1] = parv[2];
	}

    for (tmp = parv[1]; nick = strtoken(&p, tmp, ","); tmp = NULL)
	{
	found = 0;
	wilds = index(nick,'*') || index(nick,'?');

	/*
	**  Show all users matching one mask
	*/

	  for (acptr = client;
	       acptr = NextClient(acptr,nick);
	       acptr = acptr->next)
	    {
	      static anUser UnknownUser =
		{
		  "<Unknown>",	/* username */
		  "<Unknown>",	/* host */
		  "<Unknown>",	/* server */
		  NULL,	/* channel */
		  NULL,   /* invited */
		  1,      /* refcount */
		  NULL,	/* away */
		};
	      Reg3 anUser *user;
	      
	      if (IsServer(acptr) || IsMe(acptr))
		continue;
	      user = acptr->user ? acptr->user : &UnknownUser;
	      
	      chptr = user->channel;
	      
	      /* Secret users are visible only by specifying
	       ** exact nickname. Wild Card guessing would
	       ** make things just too easy...  -msa
	       */
	      if (chptr && SecretChannel(chptr) && wilds)
		continue;

	      found = eol = 1;

	      a2cptr = find_server(user->server, (aClient *)NULL);

	      sendto_one(sptr,":%s %d %s %s %s %s * :%s",
			 me.name, RPL_WHOISUSER,
			 parv[0], acptr->name,
			 user->username,
			 user->host,
			 acptr->info);
	      for (buf[0] = '\0', chptr = channel; chptr;
		   chptr = chptr->nextch) {
			if (ShowChannel(sptr, chptr) &&
			    IsMember(acptr, chptr)) {
				if (strlen(buf) + strlen(chptr->chname) > 400) {
					sendto_one(sptr, ":%s %d %s %s :%s",
						   me.name, RPL_WHOISCHANNELS,
						   parv[0], acptr->name,
						   buf);
					buf[0] = '\0';
				}
				if (IsChanOp(acptr, chptr))
					strcat(buf, "@");
				strcat(buf, chptr->chname);
				strcat(buf, " ");
			}
	      }
	      if (buf[0] != '\0')
		sendto_one(sptr, ":%s %d %s %s :%s",
			   me.name, RPL_WHOISCHANNELS,
			   parv[0], acptr->name,
			   buf);
	      sendto_one(sptr,":%s %d %s %s :%s",
			 me.name, 
			 RPL_WHOISSERVER,
			 parv[0],
			 user->server,
			 (a2cptr) ? a2cptr->info :
			 "*Not on this net*");
	      if (user->away)
		sendto_one(sptr,":%s %d %s %s :%s",
			   me.name, RPL_AWAY,
			   parv[0], acptr->name,
			   user->away);
	      if (IsAnOper(acptr))
		sendto_one(sptr,
			   ":%s %d %s %s :%s",
			   me.name, RPL_WHOISOPERATOR, parv[0],
			   acptr->name,
			   "has a connection to the twilight zone");
	      if (IsChanOp(acptr, NullChn))
		sendto_one(sptr,
			   ":%s %d %s %s :%s",
			   me.name, RPL_WHOISCHANOP, parv[0],
			   acptr->name,
			   "has been touched by magic forces");
	      if (acptr->user && acptr->user->last && MyConnect(acptr))
		sendto_one(sptr, ":%s %d %s %s %ld :%s",
			   me.name, RPL_WHOISIDLE,
			   parv[0], acptr->name,
			   time((void *) 0) - acptr->user->last,
			   "is the idle time for this user");
	    }
	if (!found)
	    sendto_one(sptr, ":%s %d %s %s :No such nickname",
		me.name, ERR_NOSUCHNICK, parv[0], nick);

	if (p)
	    p[-1] = ',';
	}

    if (eol)
	sendto_one(sptr, ":%s %d %s %s :* End of /WHOIS list.", me.name,
	     RPL_ENDOFWHOIS, parv[0], parv[1]);

    return 0;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
*/
m_user(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  aConfItem *aconf;
  char *username, *host, *server, *realname;
  
  if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
      *parv[3] == '\0' || *parv[4] == '\0') {
    sendto_one(sptr,":%s %d * :Not enough parameters", 
	       me.name, ERR_NEEDMOREPARAMS);
    return 0;
  }
  
  /* Copy parameters into better documenting variables */
  
  username = parv[1];
  host = parv[2];
  server = parv[3];
  realname = parv[4];
  
  if (!IsUnknown(sptr))
    {
      sendto_one(sptr,":%s %d * :Identity problems, eh ?",
		 me.name, ERR_ALREADYREGISTRED);
      return 0;
    }
  if (MyConnect(sptr)) { /* Also implies sptr==ctpr!! --msa */
    if (!(aconf = FindConf(cptr->confs, (char *) 0, CONF_CLIENT))) {
      sendto_one(sptr, ":%s %d * :Your host isn't among the privileged..",
		 me.name, ERR_NOPERMFORHOST);
      return ExitClient(sptr, sptr, sptr, "");
    } 
    if (!StrEq(sptr->passwd, aconf->passwd)) {
      sendto_one(sptr,
		 ":%s %d * :Only correct words will open this gate..",
		 me.name, ERR_PASSWDMISMATCH);
      return ExitClient(sptr, sptr, sptr, "");
    }
    host = sptr->sockhost;
    /*
     * following block for the benefit of time-dependent K:-lines
     */
    {
      int rc;
      char reply[128];
      extern int find_kill();
#ifdef R_LINES
      extern int find_restrict();
#endif		
      
      if (rc = find_kill(host, username, reply))
	{
	  sendto_one(sptr, reply, me.name, rc, "*");
	  if (rc == ERR_YOUREBANNEDCREEP)
	    {
/*	      SendUser(cptr, sptr->name, "zombie", host,
		       me.name, "*** active K-line ***"); */
	      return;
	    }
	}
#ifdef R_LINES
      if (find_restrict(host,username,reply))
	{
	  sendto_one(sptr,":%s %d :*** %s",me.name,
		     ERR_YOUREBANNEDCREEP,reply);
	  return ExitClient(sptr, sptr, sptr , "");
	}
#endif
    }
  }
  make_user(sptr);
  strncpyzt(sptr->user->username,username,sizeof(sptr->user->username));
  strncpyzt(sptr->user->server, cptr != sptr ? server : me.name,
	    sizeof(sptr->user->server));
  strncpyzt(sptr->user->host, host, sizeof(sptr->user->host));
  strncpyzt(sptr->info, realname, sizeof(sptr->info));

  if (sptr->name[0]) /* NICK already received, now we have USER... */
    RegisterUser(cptr,sptr,sptr->name);

  return 0;
}

/*
** m_list
**	parv[0] = sender prefix
**      parv[1] = channel
*/
m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aChannel *chptr;
	
	sendto_one(sptr,":%s %d %s :  Channel  : Users  Name",
		   me.name, RPL_LISTSTART, parv[0]);
	
	if (parc < 2 || BadPtr(parv[1])) {
	  for (chptr = channel; chptr; chptr = chptr->nextch) {
	    if (sptr->user == NULL ||
		(SecretChannel(chptr) && !IsMember(sptr, chptr)))
	      continue;
	    sendto_one(sptr,":%s %d %s %s %d :%s",
		       me.name, RPL_LIST, parv[0],
		       (ShowChannel(sptr, chptr) ? chptr->chname : "*"),
		       chptr->users, chptr->topic);
	  }
	} else {
	  chptr = find_channel(parv[1], NullChn);
	  if (chptr != NullChn && ShowChannel(sptr, chptr) &&
	      sptr->user != NULL)
	    sendto_one(sptr, ":%s %d %s %s %d :%s",
		       me.name, RPL_LIST, parv[0],
		       (ShowChannel(sptr, chptr) ? chptr->chname : "*"),
		       chptr->users, chptr->topic ? chptr->topic : "*");
	}
	sendto_one(sptr,":%s %d %s :End of /LIST",
		   me.name, RPL_LISTEND, parv[0]);
	return 0;
    }

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
m_version(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
    {
	if (HuntServer(cptr,sptr,":%s VERSION %s",1,parc,parv) == HUNTED_ISME)
	    sendto_one(sptr,":%s %d %s %s %s", me.name, RPL_VERSION,
	       parv[0], version, me.name);
	return 0;
    }

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	register char *comment = (parc > 1 && parv[1]) ? parv[1] : cptr->name;

	return IsServer(sptr) ? 0 : ExitClient(cptr, sptr, sptr, comment);
    }

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[2] = comment
*/
m_squit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char *server;
	aClient *acptr;
	char *comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

	if (!IsPrivileged(sptr) || IsLocOp(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :'tis is no game for mere mortal souls",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }

	if (parc > 1)
	    {
		/*
		** The following allows wild cards in SQUIT. Only usefull
		** when the command is issued by an oper.
		*/
		server = parv[1];
		for (acptr = client;
		     acptr = NextClient(acptr, server);
		     acptr = acptr->next)
			if (IsServer(acptr))
				break;
	    }
	else
	    {
		/*
		** This is actually protocol error. But, well, closing
		** the link is very proper answer to that...
		*/
		server = cptr->sockhost;
		acptr = cptr;
	    }
	/*
	** SQUIT semantics is tricky, be careful...
	**
	** The old (irc2.2PL1 and earlier) code just cleans away the
	** server client from the links (because it is never true
	** "cptr == acptr".
	**
	** This logic here works the same way until "SQUIT host" hits
	** the server having the target "host" as local link. Then it
	** will do a real cleanup spewing SQUIT's and QUIT's to all
	** directions, also to the link from which the orinal SQUIT
	** came, generating one unnecessary "SQUIT host" back to that
	** link.
	**
	** One may think that this could be implemented like
	** "HuntServer" (e.g. just pass on "SQUIT" without doing
	** nothing until the server having the link as local is
	** reached). Unfortunately this wouldn't work in the real life,
	** because either target may be unreachable or may not comply
	** with the request. In either case it would leave target in
	** links--no command to clear it away. So, it's better just
	** clean out while going forward, just to be sure.
	**
	** ...of course, even better cleanout would be to QUIT/SQUIT
	** dependant users/servers already on the way out, but
	** currently there is not enough information about remote
	** clients to do this...   --msa
	*/
	if (acptr == NULL)
	    {
		sendto_one(cptr,"ERROR :No such server (%s)",server);
		return 0;
	    }
	/*
	**  Notify all opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsAnOper(cptr))
	    sendto_ops_butone((aClient *)NULL,
		":%s WALLOPS :Received SQUIT %s from %s (%s)",
		me.name, server, GetClientName(sptr,FALSE), comment);

	return ExitClient(cptr, acptr, sptr, comment);
    }

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo
*/
m_server(cptr, sptr, parc, parv)
     aClient *cptr, *sptr;
     int parc;
     char *parv[];
{
  char *host;
  char *info;
  aClient *acptr, *bcptr;
  aConfItem *aconf, *bconf;
  aChannel *chptr;
  char *inpath;
  Reg1 char *ch;
  Reg2 int i;
  int  unknown;
  
  inpath = GetClientName(cptr,FALSE);
  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(cptr,"ERROR :No hostname");
      return 0;
    }
  host = parv[1];
  info = parc < 3 ? NULL : parv[2];
  /*
   ** Check for "FRENCH " infection ;-) (actually this should
   ** be replaced with routine to check the hostname syntax in
   ** general). [ This check is still needed, even after the parse
   ** is fixed, because someone can send "SERVER :foo bar " ].
   ** Also, changed to check other "difficult" characters, now
   ** that parse lets all through... --msa
   */
  for (ch = host; *ch; ch++)
    if (*ch <= ' ' || *ch > '~')
      {
	sendto_one(sptr,"ERROR :Bogus server name (%s)",host);
	sendto_ops("Bogus server name (%s) from %s",
		   host, GetClientName(cptr,FALSE));
	return 0;
      }
  
  if (IsPerson(cptr))
    {
      /*
       ** A local link that has been identified as a USER
       ** tries something fishy... ;-)
       */
      sendto_one(cptr,
		 "ERROR :Client may not currently become server");
      sendto_ops("User %s trying to become a server %s",
		 GetClientName(cptr,FALSE),host);
      return 0;
    }
  /* *WHEN* can it be that "cptr != sptr" ????? --msa */
  
  if ((acptr = find_server(host, (aClient *)NULL)) != NULL)
    {
      /*
       ** This link is trying feed me a server that I
       ** already have access through another path--
       ** multiple paths not accepted currently, kill
       ** this link immeatedly!!
       */
      sendto_one(cptr,"ERROR :Server %s already exists... ", host);
      sendto_ops("Link %s cancelled, server %s already exists",
		 inpath, host);
      return ExitClient(cptr, cptr, cptr, "");
    }
  if (IsServer(cptr))
    {
      /*
       ** Server is informing about a new server behind
       ** this link. Create REMOTE server structure,
       ** add it to list and propagate word to my other
       ** server links...
       */
      if (BadPtr(info))
	{
	  sendto_one(cptr,"ERROR :No server info specified");
	  return 0;
	}

      /*
       ** See if the newly found server is behind a guaranteed
       ** leaf (L-line). If so, close the link.
       */
      if (aconf = FindConfName( sptr->name, CONF_LEAF )) {
	sendto_ops("Leaf-only link %s issued second server command",
		   GetClientName(sptr, FALSE) );
	sendto_one(cptr, ":ERROR :Leaf-only link, sorry." );
	return ExitClient(cptr, cptr, cptr, "");
      }
      /*
       ** See if the newly found server has a Q line for it in
       ** our conf. If it does, lose the link that brought it
       ** into our network. Format:
       **
       ** Q:<unused>:<reason>:<servername>
       **
       ** Example:  Q:*:for the hell of it:eris.Berkeley.EDU
       */
      
      if (aconf = FindConfName(host, CONF_QUARANTINED_SERVER)) {
	sendto_ops_butone((aClient *) 0,
			  ":%s WALLOPS :%s brought in %s, %s %s",
			  me.name, GetClientName(sptr, FALSE),
			  host, "closing link because",
			  BadPtr(aconf->passwd) ?
			  "reason unspecified" : aconf->passwd);
	
	sendto_one(cptr, "ERROR :%s is not welcome because %s. %s",
		   host, BadPtr(aconf->passwd) ?
		   "reason unspecified" : aconf->passwd,
		   "Go away and get a life");
	return ExitClient(cptr, cptr, cptr, "");
      }
      acptr = make_client(cptr);
      strncpyzt(acptr->name,host,sizeof(acptr->name));
      strncpyzt(acptr->info,info,sizeof(acptr->info));
      SetServer(acptr);
      add_client_to_list(acptr);
      addToClientHashTable(acptr->name, acptr);
      /*
       ** Old sendto_serv_but_one() call removed because we now
       ** need to send different names to different servers
       ** (domain name matching)
       */
      for (i = 0; i <= highest_fd; i++)
	{
	  if (!(bcptr = local[i]) || !IsServer(bcptr) ||
	      bcptr == cptr || IsMe(bcptr))
	    continue;
	  if (bcptr->confs &&
	      matches(MyNameForLink(me.name, bcptr->confs->value),
		      acptr->name) == 0 )
	    continue;
	  sendto_one(bcptr,"SERVER %s %s",
		     acptr->name, acptr->info);
	}
      return 0;
    }
  if (!IsUnknown(cptr) && !IsHandshake(cptr))
    return 0;
  /*
   ** A local link that is still in undefined state wants
   ** to be a SERVER. Check if this is allowed and change
   ** status accordingly...
   */
  unknown = IsUnknown(cptr);
  SetServer(cptr);
  strncpyzt(cptr->name, host, sizeof(cptr->name));
  strncpyzt(cptr->info, BadPtr(info) ? me.name : info,
	    sizeof(cptr->info));
  inpath = GetClientName(cptr,TRUE); /* "refresh" inpath with host */
  if (!(aconf = FindConf(cptr->confs,host,CONF_NOCONNECT_SERVER)))
    {
      sendto_one(cptr,
		 "ERROR :Access denied. No N field for server %s",
		 inpath);
      sendto_ops("Access denied. No N field for server %s",
		 inpath);
      return ExitClient(cptr, cptr, cptr, "");
    }
  if (!(bconf = FindConf(cptr->confs,host,CONF_CONNECT_SERVER)))
    {
      sendto_one(cptr, "ERROR :Only N (no C) field for server %s", inpath);
      sendto_ops("Only N (no C) field for server %s",inpath);
      return ExitClient(cptr, cptr, cptr, "");
    }
  if (*(aconf->passwd) && !StrEq(aconf->passwd, cptr->passwd))
    {
      sendto_one(cptr, "ERROR :Access denied (passwd mismatch) %s",
		 inpath);
      sendto_ops("Access denied (passwd mismatch) %s", inpath);
      return ExitClient(cptr, cptr, cptr, "");
    }
  if (unknown)
    {
      if (bconf->passwd[0])
	sendto_one(cptr,"PASS %s",bconf->passwd);
      /*
       ** Pass my info to the new server
       */
      sendto_one(cptr,"SERVER %s %s",
		 MyNameForLink(me.name, aconf), 
		 (me.info[0]) ? (me.info) : "IRCers United");
    }
  /*
  ** *WARNING*
  ** 	In the following code in place of plain server's
  **	name we send what is returned by GetClientName
  **	which may add the "sockhost" after the name. It's
  **	*very* *important* that there is a SPACE between
  **	the name and sockhost (if present). The receiving
  **	server will start the information field from this
  **	first blank and thus puts the sockhost into info.
  **	...a bit tricky, but you have been warned, besides
  **	code is more neat this way...  --msa
  */
  det_confs_butone(cptr, aconf);
  /* Save this in order to get domain
   * wildcard match counts whenever needed */
  sendto_ops("Link with %s established.", inpath);
  addToClientHashTable(cptr->name, cptr);
  /*
   ** Old sendto_serv_but_one() call removed because we now
   ** need to send different names to different servers
   ** (domain name matching)
   */
  for (i = 0; i <= highest_fd; i++) 
    {
      if (!(acptr = local[i]) || !IsServer(acptr) ||
	  acptr == cptr || IsMe(acptr))
	continue;
      if (acptr->confs &&
	  matches(MyNameForLink(me.name, acptr->confs->value),
		  cptr->name) == 0)
	continue;
      sendto_one(acptr,"SERVER %s %s",
		 inpath, cptr->info);
    }
  
  /*
   ** Pass on my client information to the new server
   **
   ** First, pass only servers (idea is that if the link gets
   ** cancelled beacause the server was already there,
   ** there are no NICK's to be cancelled...). Of course,
   ** if cancellation occurs, all this info is sent anyway,
   ** and I guess the link dies when a read is attempted...? --msa
   ** 
   ** Note: Link cancellation to occur at this point means
   ** that at least two servers from my fragment are building
   ** up connection this other fragment at the same time, it's
   ** a race condition, not the normal way of operation...
   **
   ** ALSO NOTE: using the GetClientName for server names--
   **	see previous *WARNING*!!! (Also, original inpath
   **	is destroyed...)
   */
  
  /*
   ** Then pass all other servers I know of
   */
  for (acptr = client; acptr; acptr = acptr->next)
    {
      if (!IsServer(acptr) || acptr == cptr || IsMe(acptr))
	continue;
      if (cptr->confs &&
	  matches(MyNameForLink(me.name, cptr->confs->value),
		  acptr->name) == 0)
	continue;
      inpath = GetClientName(acptr, TRUE);
      sendto_one(cptr,"SERVER %s %s", inpath, acptr->info);
    }
  /*
   ** Second, pass all clients
   */
  for (acptr = client; acptr; acptr = acptr->next)
    {
      if (IsServer(acptr) || acptr == cptr || IsMe(acptr))
	continue;
      if (!IsPerson(acptr) && !IsService(acptr))
	continue;
      if (acptr->name[0] == '\0')
	continue; /* Yet unknowns cannot be passed */
      sendto_one(cptr,"NICK %s",acptr->name);
      sendto_one(cptr,":%s USER %s %s %s :%s", acptr->name,
		 acptr->user->username, acptr->user->host,
		 acptr->user->server, acptr->info);
      if (IsOper(acptr))
	sendto_one(cptr,":%s OPER",acptr->name);
      if (acptr->user->away != NULL)
	sendto_one(cptr,":%s AWAY %s", acptr->name,
		   acptr->user->away);
    }
  
  /*
   ** Third, pass all channels
   */
  for (chptr = channel; chptr; chptr = chptr->nextch) {
    Link *link;
    for (link = chptr->members; link; link = link->next) {
      sendto_one(cptr,":%s JOIN %s", ((aClient *) link->value)->name, 
		 chptr->chname);
    }
  }
  
  /*
   ** Last, pass all channels plus statuses
   */
  {
    aChannel *chptr;
    char modebuf[MODEBUFLEN];
    for (chptr = channel; chptr; chptr = chptr->nextch) {
      send_channel_ops(cptr, chptr);
      ChannelModes(modebuf, chptr);
      if (modebuf[1])
	sendto_one(cptr, ":%s MODE %s %s", me.name,
		   chptr->chname, modebuf);
    }
  }
  return 0;
}

/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim
**	parv[2] = kill path
*/
m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char *inpath = GetClientName(cptr,FALSE);
	char *user, *path;
	int chasing = 0;
	Reg1 int i;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No user specified",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }
	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */
	if (!IsLocOp(sptr) && !IsPrivileged(sptr))
	    {
		sendto_one(sptr,":%s %d %s :Death before dishonor ?",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }
	if (!(acptr = find_person(user, (aClient *)NULL)))
	    {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = GetHistory(user, (long)KILLCHASETIMELIMIT)))
		    {
			sendto_one(sptr, ":%s %d %s %s :Hunting for ghosts?",
				   me.name, ERR_NOSUCHNICK, parv[0],
				   user);
			return 0;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   me.name, parv[0], user, acptr->name);
		chasing = 1;
	    }

	if (!IsServer(cptr))
	    {
		/*
		** The kill originates from this server, initialize path.
		** (In which case the 'path' may contain user suplied
		** explanation ...or some nasty comment, sigh... >;-)
		**
		**	...!operhost!oper
		**	...!operhost!oper (comment)
		*/
		inpath = cptr->sockhost; /* Don't use GetClientName syntax */
		if (!BadPtr(path))
		    {
			sprintf(buf, "%s (%s)", cptr->name, path);
			path = buf;
		    }
		else
			path = cptr->name;
	    }
	else if (BadPtr(path))
		 path = "*no-path*"; /* Bogus server sending??? */
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/
	sendto_ops("Received KILL message for %s. Path: %s!%s",
		   acptr->name, inpath, path);

	if (IsLocOp(sptr) && !MyConnect(acptr)) {
	  sendto_one(sptr,":%s %d %s :Death before dishonor ?",
		     me.name, ERR_NOPRIVILEGES, parv[0]);
	  return 0;
	}
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	*/
	if (sptr != acptr)     /* Suicide kills are NOT passed on --SRB */
	  {
	    /* Just to make sure we use the link name as source.
	       Therefore we have to scan through all links.
	       The reason is wildcard servers and possible
	       problems with older servers handling wildcards --Jto */
	    aClient *ptr = client;
	    for (i = 0; i <= highest_fd; i++) {
	      if (!(ptr = local[i]) || !IsServer(ptr) || IsMe(ptr))
		continue;
	      if (cptr != ptr || chasing)
		sendto_one(ptr, ":%s KILL %s :%s!%s",
		    IsServer(sptr) ? "" : parv[0], acptr->name, inpath, path);
	    }
	}
	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_one(acptr,":%s KILL %s :%s!%s",
			   parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents ExitOneClient from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	if (sptr != acptr)  /* Suicide kills are simple signoffs --SRB */
	  acptr->flags |= FLAGS_KILLED;
	return ExitClient(cptr, acptr, sptr, "Killed");
    }

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_info(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
    char **text = infotext;

    if (HuntServer(cptr,sptr,":%s INFO %s",1,parc,parv) == HUNTED_ISME)
	{
	sendto_one(sptr, "NOTICE %s :***", parv[0]);

	while (*text)
	    sendto_one(sptr, "NOTICE %s :*** %s", parv[0], *text++);

	sendto_one(sptr, "NOTICE %s :***", parv[0]);
	sendto_one(sptr,
	    "NOTICE %s :*** This server was created %s, compile # %s",
		parv[0], creation, generation);
	sendto_one(sptr, "NOTICE %s :*** On-line since %s",
		parv[0], myctime(cptr->firsttime));
	sendto_one(sptr, "NOTICE %s :*** End of /INFO list.", parv[0]);
	}

    return 0;
}

/*
** m_links
**	parv[0] = sender prefix
**	parv[1] = servername mask
**      parv[2] = server to query
*/
m_links(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    char *mask;
    aClient *acptr;

    CheckRegisteredUser(sptr);
    
    if (parc > 2)
	{
	if (HuntServer(cptr, sptr, ":%s LINKS %s %s", 1, parc, parv)
						    != HUNTED_ISME)
	    return 0;
	mask = parv[2];
	}
    else
	mask = parc < 2 ? NULL : parv[1];

    for (acptr = client; acptr; acptr = acptr->next) 
	if ((IsServer(acptr)
	     || IsMe(acptr)) &&
	     (BadPtr(mask) || matches(mask, acptr->name) == 0))
		    sendto_one(sptr,":%s %d %s %s :%s",
			me.name, RPL_LINKS, parv[0], acptr->name,
			(acptr->info[0] ? acptr->info :
			"(Unknown Location)"));

    sendto_one(sptr, ":%s %d %s :* End of /LINKS list.", me.name,
	       RPL_ENDOFLINKS, parv[0]);
    return 0;
}

/*
** m_summon should be redefined to ":prefix SUMMON host user" so
** that "HuntServer"-function could be used for this too!!! --msa
**
**	parv[0] = sender prefix
**	parv[1] = user%client@servername
*/
m_summon(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char namebuf[10],linebuf[10],hostbuf[17],*host,*user;
	int fd, flag;

	CheckRegisteredUser(sptr);
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s (Summon) No recipient given",
			   me.name, ERR_NORECIPIENT,
			   parv[0]);
		return 0;
	    }
	user = parv[1];
	if ((host = index(user,'@')) == NULL)
		host = me.name;
	else 
		*(host++) = '\0';

	if (BadPtr(host) || matches(host,me.name) == 0)
	    {
#if RSUMMON
		if (index(user,'%') != NULL)
		    {
			rsummon(sptr,user);
			return 0;
		    }
#endif
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr,":%s NOTICE %s :Cannot open %s",
				   me.name, parv[0],UTMP);
			return 0;
		    }
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf)) == 0) 
			if (StrEq(namebuf,user))
				break;
		utmp_close(fd);
		if (flag == -1)
			sendto_one(sptr,":%s NOTICE %s :User %s not logged in",
				   me.name, parv[0], user);
		else
			summon(sptr, namebuf, linebuf);
		return 0;
	    }
	/*
	** Summoning someone on remote server, find out which link to
	** use and pass the message there...
	*/
	acptr = find_server(host,(aClient *)NULL);
	if (acptr == NULL)
		sendto_one(sptr, "NOTICE %s :SUMMON: No such host (%s) found",
			   parv[0], host);
	else if (!IsMe(acptr))
		sendto_one(acptr, ":%s SUMMON %s@%s",parv[0], user, host);
	else
	  sendto_one(sptr, "NOTICE %s :SUMMON: Summon bug bites", parv[0]);
	return 0;
    }


/*
** m_stats
**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
**	Currently supported are:
**		M = Message frequency (the old stat behaviour)
**		L = Local Link statistics
**              C = Report C and N configuration lines
*/
/*
** m_stats/stats_conf
**    Report N/C-configuration lines from this server. This could
**    report other configuration lines too, but converting the
**    status back to "char" is a bit akward--not worth the code
**    it needs...
**
**    Note:   The info is reported in the order the server uses
**            it--not reversed as in ircd.conf!
*/
static ReportConfiguredLinks(cptr, sptr, mask)
aClient *cptr, *sptr;
int mask;
{
  aConfItem *tmp;
  char flag;
  static char noinfo[] = "<NULL>";
  
  for (tmp = conf; tmp; tmp = tmp->next)
    {
      if (tmp->status & mask) {
	if (tmp->status == CONF_CONNECT_SERVER)
	  flag = 'C';
	else if (tmp->status == CONF_NOCONNECT_SERVER)
	  flag = 'N';
	else if (tmp->status == CONF_QUARANTINED_SERVER)
	  flag = 'Q';
	else if (tmp->status == CONF_KILL)
	  flag = 'K';
	else if (tmp->status == CONF_CLIENT)
	  flag = 'I';
	else
	  continue;
	sendto_one(sptr,"NOTICE %s :%c:%s:%c:%s:%d:%d", sptr->name, flag,
		   BadPtr(tmp->host) ? noinfo : tmp->host,
		   BadPtr(tmp->passwd) ? ' ' : '*',
		   BadPtr(tmp->name) ? noinfo : tmp->name,
		   (int)tmp->port, GetConfClass(tmp));
      }
    }
  return 0;
}

m_stats(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	struct Message *mptr;
	aClient *acptr;
	char *stat = parc > 1 ? parv[1] : "M";
	Reg1 int i;
	/*
	** Server should not format things, but this a temporary kludge
	*/
	static char Lheading[] = "NOTICE %s :%-15.15s%5s%7s%10s%7s%10s %s";
	static char Lformat[]  = "NOTICE %s :%-15.15s%5u%7u%10u%7u%10u %s";
	
	if (HuntServer(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!= HUNTED_ISME)
		return 0;

	switch (*stat)
	{
	case 'L' : case 'l' :
		/*
		** The following is not the most effiecient way of
		** generating the heading, buf it's damn easier to
		** get the field widths correct --msa
		*/
		sendto_one(sptr,Lheading, parv[0],
			   "Link", "SendQ", "SendM", "SendBytes",
			   "RcveM", "RcveBytes", "Open since");
		for (i = 0; i <= highest_fd; i++)
			if ((acptr = local[i]) && /* Includes IsMe! */
			    (IsOper(sptr) || !IsPerson(acptr) ||
			     IsLocOp(sptr) && IsLocOp(acptr))) {
				sendto_one(sptr,Lformat,
					   parv[0],
					   GetClientName(acptr,FALSE),
					   (int)dbuf_length(&acptr->sendQ),
					   (int)acptr->sendM,
					   (int)acptr->sendB,
					   (int)acptr->receiveM,
					   (int)acptr->receiveB,
					   myctime(acptr->firsttime));
			      }
		break;
	case 'C' : case 'c' :
                ReportConfiguredLinks(cptr,sptr,
				      CONF_CONNECT_SERVER|
				      CONF_NOCONNECT_SERVER);
		break;
	case 'I' : case 'i' :
		if (IsOper(sptr))
			ReportConfiguredLinks(cptr,sptr,CONF_CLIENT);
		break;
	case 'K' : case 'k' :
		if (IsOper(sptr))
			ReportConfiguredLinks(cptr,sptr,CONF_KILL);
		break;
	case 'Q' : case 'q' :
		if (IsOper(sptr))
			ReportConfiguredLinks(cptr,sptr,
					      CONF_QUARANTINED_SERVER);
		break;
	case 'Y' : case 'y' :
		if (IsOper(sptr))
			ReportClasses(cptr,sptr);
		break;
	default :
		for (mptr = msgtab; mptr->cmd; mptr++)
		   if (mptr->count)
			sendto_one(sptr,
			  "NOTICE %s :%s has been used %d times after startup",
				   parv[0], mptr->cmd, mptr->count);
		break;
	}
	return 0;
    }

/*
** m_users
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_users(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    char namebuf[10],linebuf[10],hostbuf[17];
    int fd, flag = 0;

    CheckRegisteredUser(sptr);

    if (HuntServer(cptr,sptr,":%s USERS %s",1,parc,parv) == HUNTED_ISME)
	{
	if ((fd = utmp_open()) == -1)
	    {
	    sendto_one(sptr,"NOTICE %s Cannot open %s", parv[0],UTMP);
	    return 0;
	    }

	sendto_one(sptr,"NOTICE %s :UserID   Terminal  Host", parv[0]);
	while (utmp_read(fd, namebuf, linebuf, hostbuf) == 0)
	    {
	    flag = 1;
	    sendto_one(sptr,"NOTICE %s :%-8s %-9s %-8s",
	       parv[0], namebuf, linebuf, hostbuf);
	    }
	if (flag == 0) 
	    sendto_one(sptr,"NOTICE %s :Nobody logged in on %s",
	       parv[0], parv[1]);

	utmp_close(fd);
	}
    return 0;
}

/*
** ExitClient
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
*/
int ExitClient(cptr, sptr, from, comment)
aClient *cptr;	/*
		** The local client originating the exit or NULL, if this
		** exit is generated by this server for internal reasons.
		** This will not get any of the generated messages.
		*/
aClient *sptr;	/* Client exiting */
aClient *from;	/* Client firing off this Exit, never NULL! */
char *comment;	/* Reason for the exit */
    {
	Reg1 aClient *acptr;
	Reg2 aClient *next;
	static int ExitOneClient();

	if (MyConnect(sptr))
	    {
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
#ifdef FNAME_USERLOG
	    {
		FILE *userlogfile;
		struct stat stbuf;
		long	on_for;

		/*
 		 * This conditional makes the logfile active only after
		 * it's been created - thus logging can be turned off by
		 * removing the file.
		 */
		if (IsPerson(sptr) && !stat(FNAME_USERLOG, &stbuf) &&
		    (userlogfile = fopen(FNAME_USERLOG, "a")))
		    {
			on_for = time(NULL) - sptr->firsttime;
			fprintf(userlogfile, "%s (%3d:%02d:%02d): %s@%s\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->user->username, sptr->user->host);
			fclose(userlogfile);
		    }
		/* Modification by stealth@caen.engin.umich.edu */
	    }
#endif
		CloseConnection(sptr);
		/*
		** First QUIT all NON-servers which are behind this link
		**
		** Note	There is no danger of 'cptr' being exited in
		**	the following loops. 'cptr' is a *local* client,
		**	all dependants are *remote* clients.
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (!IsServer(acptr) && acptr->from == sptr)
				ExitOneClient((aClient *)0,acptr,&me,me.name);
		    }
		/*
		** Second SQUIT all servers behind this link
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				ExitOneClient((aClient *)0,acptr,&me,me.name);
		    }
	    }

	ExitOneClient(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static ExitOneClient(cptr, sptr, from, comment)
aClient *sptr;
aClient *cptr;
aClient *from;
char *comment;
    {
	aClient *acptr;
	Reg1 int i;

#ifdef MSG_NOTE
	check_messages(cptr, sptr, sptr->name,'e');
#endif

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
		return 0;	/* ...must *never* exit self!! */
	else if (IsServer(sptr)) {
	  /*
	   ** Old sendto_serv_but_one() call removed because we now
	   ** need to send different names to different servers
	   ** (domain name matching)
	   */
	  for (i = 0; i <= highest_fd; i++)
	    {
	      if (!(acptr = local[i]) || !IsServer(acptr) ||
		  acptr == cptr || IsMe(acptr))
		continue;
	      if (acptr->confs &&
		  matches(MyNameForLink(me.name, acptr->confs->value),
			  sptr->name) == 0)
		continue;
	      if (sptr->from == acptr)
		      /*
		      ** SQUIT going "upstream". This is the remote
		      ** squit still hunting for the target. Use prefixed
		      ** form. "from" will be either the oper that issued
		      ** the squit or some server along the path that didn't
		      ** have this fix installed. --msa
		      */
		      sendto_one(acptr, ":%s SQUIT %s :%s",
				 from->name, sptr->name, comment);
	      else
		      sendto_one(acptr,"SQUIT %s :%s", sptr->name, comment);
	    }
	} else if (!IsPerson(sptr)) /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty whole in the server... --msa
				    */
		 ; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
			sendto_serv_butone(cptr,":%s QUIT :%s",
					   sptr->name, comment);
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user) {
		  aChannel *chptr = channel, *tmpch;
		  sendto_common_channels(sptr, ":%s QUIT :%s",
					 sptr->name, comment);
		  while (chptr) {
		    if (IsMember(sptr, chptr)) {
		      tmpch = chptr->nextch;
		      RemoveUserFromChannel(sptr, chptr);
		      chptr = tmpch;
		    } else
    		      chptr = chptr->nextch;
		  }
		}
	    }

	/* Clean up invitefield */
	if (sptr->user && sptr->user->invited) {
	  DelInvite(sptr);
	}

	/* remove sptr from the client list */
	delFromClientHashTable(sptr->name, sptr);
	remove_client_from_list(sptr);
	return (0);
    }

/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions (like
** m_whoreply--check later --msa)
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/
m_error(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	/* temp kludge, should actually pass &parv[1] to sendto_ops? --msa */

	char *para = parc > 1 && *parv[1] != '\0' ? parv[1] : "";
	
	debug(DEBUG_ERROR,"Received ERROR message from %s: %s",
	      sptr->name, para);
	/*
	** Ignore error messages generated by normal user clients
	** (because ill-behaving user clients would flood opers
	** screen otherwise). Pass ERROR's from other sources to
	** the local operator...
	*/
	if (IsPerson(cptr) || IsUnknown(cptr) || IsService(cptr))
		/* just drop it */ ;
	else if (cptr == sptr)
		sendto_ops("ERROR from %s: %s",GetClientName(cptr,FALSE),para);
	else
		sendto_ops("ERROR from %s via %s: %s", sptr->name,
			   GetClientName(cptr,FALSE), para);
	return 0;
    }

/*
** m_help
**	parv[0] = sender prefix
*/
m_help(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int i;

	for (i = 0; msgtab[i].cmd; i++)
		sendto_one(sptr,"NOTICE %s :%s",parv[0],msgtab[i].cmd);
	return 0;
    }

m_restart(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (IsOper(sptr) && MyConnect(sptr))
		restart();
    }

m_die(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (IsOper(sptr) && MyConnect(sptr)) {
	  sendto_ops("NOTICE %s :*** %s has pulled the plug on us sir!\n",
		     me.name, parv[0]);
	  terminate();
	}
    }

m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
	int m_count = 0;
	aClient *acptr;
	
	for (acptr = client; acptr; acptr = acptr->next)
	  switch (acptr->status)
	    {
	    case STAT_SERVER:
	    case STAT_ME:
	      s_count++;
	      break;
	    case STAT_OPER:
	    case STAT_OPER | STAT_CHANOP:
	    case STAT_OPER | STAT_LOCOP | STAT_CHANOP:
	    case STAT_OPER | STAT_LOCOP:
	    case STAT_OPER | STAT_CLIENT:
	    case STAT_OPER | STAT_CHANOP | STAT_CLIENT:
	    case STAT_OPER | STAT_LOCOP | STAT_CHANOP | STAT_CLIENT:
	    case STAT_OPER | STAT_LOCOP | STAT_CLIENT:
	      o_count++;
	    case STAT_CLIENT:
	    case STAT_CHANOP | STAT_CLIENT:
	    case STAT_CHANOP:
	      if (PubChannel(acptr->user->channel) ||
		  HiddenChannel(acptr->user->channel)) {
		if (MyConnect(acptr))
		  m_count++;
		c_count++;
	      } else {
		if (IsOper(sptr) && MyConnect(acptr))
		  m_count++;
		i_count++;
	      }
	      break;
	    default:
	      u_count++;
	      break;
	    }
	if (IsAnOper(sptr) && i_count)
		sendto_one(sptr,
	"NOTICE %s :*** There are %d users (and %d invisible) on %d servers",
			   parv[0], c_count, i_count, s_count);
	else
		sendto_one(sptr,
			   "NOTICE %s :*** There are %d users on %d servers",
			   parv[0], c_count, s_count);
	if (o_count)
		sendto_one(sptr,
		   "NOTICE %s :*** %d user%s connection to the twilight zone",
			   parv[0], o_count,
			   (o_count > 1) ? "s have" : " has");
	if (u_count > 0)
		sendto_one(sptr,
			"NOTICE %s :*** There are %d yet unknown connections",
			   parv[0], u_count);
	if (m_count > 0)
		sendto_one(sptr,
			"NOTICE %s :*** There are %d clients connected to %s",
			   parv[0], m_count, me.name);
	sendto_one(sptr, "NOTICE %s :*** There are %d channels.",
		   parv[0], count_channels(sptr));
    }

  
/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *	      but perhaps it's worth the load it causes to net.
 *	      This requires flooding of the whole net like NICK,
 *	      USER, WALL, etc messages...  --msa
 ***********************************************************************/

/*
** m_away
**	parv[0] = sender prefix
**	parv[1] = away message
*/
m_away(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	CheckRegisteredUser(sptr);
	if (sptr->user->away)
	    {
		free(sptr->user->away);
		sptr->user->away = NULL;
	    }
	if (parc < 2 || *parv[1] == '\0')
	    {
		/* Marking as not away */

		sendto_serv_butone(cptr, ":%s AWAY", parv[0]);
		if (MyConnect(sptr))
			sendto_one(sptr,
			   "NOTICE %s :You are no longer marked as being away",
				   parv[0]);
		return 0;
	    }

	/* Marking as away */

	sendto_serv_butone(cptr, ":%s AWAY :%s", parv[0], parv[1]);
	sptr->user->away = (char *) malloc((unsigned int) (strlen(parv[1])+1));
	if (sptr->user->away == NULL)
	    {
		sendto_one(sptr,
		       "ERROR :Randomness of the world has proven its power!");
		return 0;
	    }
	strcpy(sptr->user->away, parv[1]);
	if (MyConnect(sptr))
		sendto_one(sptr,
			   "NOTICE %s :You have been marked as being away",
			   parv[0]);
	return 0;
    }

/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************/

/*
** m_connect
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = port number
**	parv[3] = remote server
*/
m_connect(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int port, tmpport, retval;
	aConfItem *aconf;
	aClient *acptr;

	if (!IsLocOp(sptr) && !IsPrivileged(sptr))
	    {
		sendto_one(sptr,"NOTICE %s :Connect: Privileged command",
			   parv[0]);
		return -1;
	    }

	if (IsLocOp(sptr) && parc > 3) /* Only allow LocOps to make */
	  return 0;                    /* local CONNECTS --SRB      */

	if (HuntServer(cptr,sptr,":%s CONNECT %s %s %s",
		       3,parc,parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
		 "NOTICE %s :Connect: Syntax error--use CONNECT host port",
			   parv[0]);
		return -1;
	    }

	if ((acptr = find_server(parv[1], (aClient *)NULL)) != NULL) {
	  sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
		     me.name, parv[0], parv[1], "already exists from",
		     acptr->from->name);
	  return -1;
	}
	/*
	** Notify all operators about remote connect requests
	*/
	if (!IsAnOper(cptr))
	  sendto_ops_butone((aClient *)0,
			    ":%s WALLOPS :Remote 'CONNECT %s %s' from %s",
			    me.name,
			    parv[1], parv[2] ? parv[2] : "",
			    GetClientName(sptr,FALSE));

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status == CONF_CONNECT_SERVER &&
		    (matches(parv[1],aconf->name) == 0 ||
		     matches(parv[1],aconf->host) == 0))
		  break;

	if (!aconf)
	    {
	      sendto_one(sptr,
			 "NOTICE %s :Connect: Host %s not listed in irc.conf",
			 parv[0], parv[1]);
	      return 0;
	    }
	/*
	** Get port number from user, if given. If not specified,
	** use the default form configuration structure. If missing
	** from there, then use the precompiled default.
	*/
	tmpport = port = aconf->port;
	if (parc > 2 && !BadPtr(parv[2]))
	    {
		if ((port = atoi(parv[2])) <= 0)
		    {
			sendto_one(sptr,
				   "NOTICE %s :Connect: Illegal port number",
				   parv[0]);
			return -1;
		    }
	    }
	else if (port <= 0 && (port = PORTNUM) <= 0)
	    {
		sendto_one(sptr,"NOTICE %s :Connect: Port number required",
			   parv[0]);
		return -1;
	    }
	aconf->port = port;
	switch (retval = connect_server(aconf))
	    {
	    case 0:
		sendto_one(sptr,
			   "NOTICE %s :*** Connecting to %s[%s] activated.",
			   parv[0], aconf->host, aconf->host);
		break;
	    case -1:
		sendto_one(sptr,
			   "NOTICE %s :*** Couldn't connect to %s.",
			   parv[0], conf->host);
		break;
	    case -2:
		sendto_one(sptr,
			   "NOTICE %s :*** Host %s is unknown.",
			   parv[0], aconf->host);
		break;
	    default:
		sendto_one(sptr,
			   "NOTICE %s :*** Connection to %s failed: %s",
			   parv[0], aconf->host, strerror(retval));
	    }
	aconf->port = tmpport;
	return 0;
    }

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
m_ping(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char *origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,"ERROR :No origin specified.");
		return -1;
	    }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if (acptr = find_server(destination,(aClient *)NULL))
			sendto_one(acptr,"PING %s %s", origin, destination);
		else
		    {
			sendto_one(sptr,"ERROR :PING No such host (%s) found",
				   destination);
			return -1;
		    }
	    }
	else
		sendto_one(sptr,"PONG %s %s", 
			   (destination) ? destination : me.name, origin);
	return 0;
    }

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
m_pong(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char *origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,"ERROR :No origin specified.");
		return -1;
	    }
	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
		if (acptr = find_server(destination,(aClient *)NULL))
			sendto_one(acptr,"PONG %s %s", origin, destination);
		else
		    {
			sendto_one(sptr,"ERROR :PONG No such host (%s) found",
				   destination);
			return -1;
		    }
	else
		debug(DEBUG_NOTICE, "PONG: %s %s", origin,
		      destination ? destination : "*");
	return 0;
    }


/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
m_oper(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aConfItem *aconf;
	char *name, *password;

	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;
	if (!IsClient(sptr) || (BadPtr(name) || BadPtr(password)) &&
	    cptr == sptr )
	    {
		sendto_one(sptr, ":%s %d %s :Dave, don't do that...",
			   me.name, ERR_NEEDMOREPARAMS,
			   parv[0]);
		return 0;
	    }
	
	/* if message arrived from server, trust it, and set to oper */

	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	    {
		SetOper(sptr);
		sendto_serv_butone(cptr, ":%s OPER", parv[0]);
#ifdef MSG_NOTE
		check_messages(cptr, sptr, sptr->name,'o');
#endif
		if (IsMe(cptr))
		  sendto_one(sptr, ":%s %d %s :Good afternoon, gentleman. %s",
			     me.name, RPL_YOUREOPER, parv[0],
			     "I am a HAL 9000 computer.");
		return 0;
	    }
	if (!name || !(aconf = FindConf(sptr->confs, name,
					CONF_OPERATOR | CONF_LOCOP))) {
	  sendto_one(sptr,
		     ":%s %d %s :Only few of mere mortals may try to %s",
		     me.name, ERR_NOOPERHOST, parv[0],
		     "enter the twilight zone");
	  return 0;
	} 
	if ((aconf->status & (CONF_OPERATOR | CONF_LOCOP))
	    && StrEq(password, aconf->passwd))
	    {
 		sendto_one(sptr, ":%s %d %s :Good afternoon, gentleman. %s",
 			   me.name, RPL_YOUREOPER, parv[0],
 			   "I am a HAL 9000 computer.");
		if (aconf->status == CONF_LOCOP)
		  SetLocOp(sptr);
		else {
		  SetOper(sptr);
		  sendto_serv_butone(cptr, ":%s OPER", parv[0]);
#ifdef MSG_NOTE
		  check_messages(cptr, sptr, sptr->name, 'o');
#endif
		}
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only real wizards do know the %s",
			   me.name, ERR_PASSWDMISMATCH, parv[0],
			   "spells to open the gates of paradise");
	return 0;
    }

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
m_pass(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char *password = parc > 1 ? parv[1] : NULL;

	if (BadPtr(password))
	    {
		sendto_one(cptr,":%s %d %s :No password is not good password",
			   me.name, ERR_NEEDMOREPARAMS,
			   parv[0]);
		return 0;
	    }
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	    {
		sendto_one(cptr,
			   ":%s %d %s :Trying to unlock the door twice, eh ?",
			   me.name, ERR_ALREADYREGISTRED, parv[0]);
		return 0;
	    }
	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	return 0;
    }

/*
** m_wall
**	parv[0] = sender prefix
**	parv[1] = message text
*/
m_wall(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char *message = parc > 1 ? parv[1] : NULL;

	if (!IsAnOper(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :Only real wizards know the correct %s",
			   me.name, ERR_NOPRIVILEGES, parv[0],
			   "spells to open this gate");
		return 0;
	    }
	if (BadPtr(message))
	    {
		sendto_one(sptr, ":%s %d %s :Empty WALL message",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }
#ifndef WALL
	if (IsServer(cptr))
#endif
	sendto_all_butone(IsServer(cptr) ? cptr : NULL,
			  ":%s WALL :%s", parv[0], message);
	return 0;
    }
/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
m_wallops(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char *message = parc > 1 ? parv[1] : NULL;

	CheckRegistered(sptr);
	
	if (BadPtr(message))
	    {
		sendto_one(sptr, ":%s %d %s :Empty WALLOPS message",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL,
			":%s WALLOPS :%s", parv[0], message);
	return 0;
    }

/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_time(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	CheckRegisteredUser(sptr);
	if (HuntServer(cptr,sptr,":%s TIME %s",1,parc,parv) == HUNTED_ISME)
	    sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_TIME,
		   parv[0], me.name, date(0));
	return 0;
    }

m_rehash(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (!IsOper(sptr) || !MyConnect(sptr))
	    {
		sendto_one(sptr,":%s %d %s :Use the force, Luke !",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return -1;
	    }
	sendto_one(sptr,":%s %d %s :Rehashing %s",
		   me.name, RPL_REHASHING, parv[0], configfile);
	rehash();
#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
	{
	  register aClient *cptr;
	  extern int find_restrict();
	  char reply[128];
	  
	  for (cptr=client;cptr;cptr=cptr->next)
	    
	    if (MyConnect(cptr) && !IsMe(cptr) && IsPerson(cptr) &&
		find_restrict(cptr->user->host,cptr->user->username,reply))
	      {
		sendto_ops("Restricting %s (%s), closing link",
			   GetClientName(cptr,FALSE),reply);
		sendto_one(cptr,":%s %d :*** %s",me.name,
			   ERR_YOUREBANNEDCREEP,reply);
		return ExitClient((aClient *)NULL, cptr, cptr, "");
	      }
	}
#endif
    }

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    { 
	aChannel *chptr;
	aClient *c2ptr;
	int idx, flag;
	aChannel *mychannel = (sptr->user) ? sptr->user->channel :
	  (aChannel *) 0;
	char *para = parc > 1 ? parv[1] : NULL;

	/* Allow NAMES without registering */

	/* First, do all visible channels (public and the one user self is) */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
	        Link *link;
		if (!BadPtr(para) && !ChanIs(chptr, para))
			continue; /* -- wanted a specific channel */
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		sprintf(buf, "%s %s ", 
		    PubChannel(chptr) ? "=" : "*",
		    chptr->chname);

		idx = strlen(buf);
		flag = 0;
		for (link = chptr->members; link; link = link->next)
		    {
		        if (TestChanOpFlag(link->flags))
			  strcat(buf, "@");
			strncat(buf, ((aClient *) link->value)->name, NICKLEN);
			idx += strlen(((aClient *) link->value)->name) + 1;
			flag = 1;
			strcat(buf," ");
			if (idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, "NAMREPLY %s", buf);
				if (!PubChannel(chptr))
					sprintf(buf, "* %s ", chptr->chname);
				else
					sprintf(buf, "= %s ", chptr->chname);
				idx = strlen(buf);
				flag = 0;
			    }
		    }
		if (flag)
			sendto_one(sptr, "NAMREPLY %s", buf);
	    }
	if (!BadPtr(para)) {
	  sendto_one(sptr, ":%s %d %s :* End of /NAMES list.", me.name,
		     RPL_ENDOFNAMES,parv[0]);
	  return(1);
	}

	/* Second, do all non-public, non-secret channels in one big sweep */

	strcpy(buf, "* * ");
	idx = strlen(buf);
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
  	        aChannel *ch3ptr = channel;
		int showflag = 0;
		if (!IsPerson(c2ptr) && !IsService(c2ptr))
		  continue;
		while (ch3ptr) {
		  if (IsMember(c2ptr, ch3ptr)) {
		    if (PubChannel(ch3ptr))
		      showflag = 1;
		    if (ch3ptr == mychannel)
		      showflag = 1;
		  }
		  ch3ptr = ch3ptr->nextch;
		}
		if (!showflag) {
		  ch3ptr = channel;
		  while (ch3ptr) {
		    if (IsMember(c2ptr, ch3ptr) && SecretChannel(ch3ptr))
		      showflag = 1;
		    ch3ptr = ch3ptr->nextch;
		  }
		}
		if (showflag) {
		  continue;
		}
		strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		strcat(buf," ");
		flag = 1;
		if (idx + NICKLEN > BUFSIZE - 2)
		    {
			sendto_one(sptr, "NAMREPLY %s", buf);
			strcpy(buf, "* * ");
			idx = strlen(buf);
			flag = 0;
		    }
	    }
	if (flag)
		sendto_one(sptr, "NAMREPLY %s", buf);
	sendto_one(sptr, ":%s %d %s :* End of /NAMES list.", me.name,
		   RPL_ENDOFNAMES, parv[0]);
	return(1);
    }

/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_admin(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aConfItem *aconf;

	if (HuntServer(cptr,sptr,":%s ADMIN :%s",1,parc,parv) != HUNTED_ISME)
		return 0;
	if (aconf = find_admin())
	    {
		sendto_one(sptr,
			   "NOTICE %s :### Administrative info about %s",
			   parv[0], me.name);
		sendto_one(sptr,
			   "NOTICE %s :### %s", parv[0], aconf->host);
		sendto_one(sptr,
			   "NOTICE %s :### %s", parv[0], aconf->passwd);
		sendto_one(sptr,
			   "NOTICE %s :### %s", parv[0], aconf->name);
	    }
	else
		sendto_one(sptr, 
	    "NOTICE %s :### No administrative info available about server %s",
			   parv[0], me.name);
	return 0;
    }

/*
** m_trace
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_trace(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	aClass *cltmp;
	int doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	Reg1 int i;
	Reg2 aClient *acptr2;

	switch (HuntServer(cptr,sptr,":%s TRACE :%s", 1,parc,parv))
	    {
	    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
		sendto_one(sptr, "NOTICE %s :*** Link %s<%s>%s ==> %s",
			   parv[0], me.name, version, debugmode, parv[1]);
		return 0;
	    case HUNTED_ISME:
		break;
	    default:
		return 0;
	    }
	doall = (parv[1] && (parc > 1)) ? !matches(parv[1],me.name): TRUE;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;

	if (doall && IsOper(sptr))
		for (acptr2 = client; acptr2; acptr2 = acptr2->next)
			if (IsPerson(acptr2))
				link_u[acptr2->from->fd]++;
			else if (IsServer(acptr2))
				link_s[acptr2->from->fd]++;

	/* report all direct connections */
	
	for (i = 0; i <= highest_fd; i++)
	  {
	    char *name;
	    int class;
	    
	    if (!(acptr = local[i])) /* Local Connection? */
	      continue;
	    name = GetClientName(acptr,FALSE);
	    if (!doall && matches(parv[1],acptr->name))
	      continue;
	    class = GetClientClass(acptr);
	    switch(acptr->status)
	      {
	      case STAT_CONNECTING:
		sendto_one(sptr,
			   "NOTICE %s :*** Try. Class[%d] ==> %s",
			   parv[0], class, name);
		break;
	      case STAT_HANDSHAKE:
		sendto_one(sptr,
			   "NOTICE %s :*** H.S. Class[%d] ==> %s",
			   parv[0], class, name);
		break;
	      case STAT_ME:
		break;
	      case STAT_UNKNOWN:
		if (IsAnOper(sptr))
		  sendto_one(sptr,
			     "NOTICE %s :*** ???? Class[%d] ==> %s",
			     parv[0], class, name);
		break;
	      case STAT_CLIENT:
		if (IsOper(sptr)) /* Only opers see users */
		  sendto_one(sptr,
			     "NOTICE %s :*** User Class[%d] ==> %s",
			     parv[0], class, name);
		break;
	      case STAT_CLIENT | STAT_OPER:
	      case STAT_CLIENT | STAT_LOCOP:
	      case STAT_CLIENT | STAT_OPER | STAT_LOCOP:
	      case STAT_OPER | STAT_LOCOP:
	      case STAT_LOCOP:
	      case STAT_OPER:
		if (IsAnOper(sptr)) /* Only opers see users */
		  sendto_one(sptr,
			     "NOTICE %s :*** Oper Class[%d] ==> %s",
			     parv[0], class, name);
		break;
	      case STAT_SERVER:
		if (IsOper(sptr))
		    sendto_one(sptr,
		  	       "NOTICE %s :*** Serv Class[%d] ==> %s %dS %dC",
				parv[0], class, name, link_s[i], link_u[i]);
		else
		  sendto_one(sptr,
			     "NOTICE %s :*** Serv Class[%d] ==> %s", 
			     parv[0], class, name);
		break;
	      default:
		/* ...we actually shouldn't come here... --msa */
		sendto_one(sptr,
			   "NOTICE %s :*** <newtype> ==> %s",
			   parv[0], name);
		break;
	      }
	  }
	/*
	 * Add these lines to summarize the above which can get rather long
         * and messy when done remotely - Avalon
         */
	for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
	  if (Links(cltmp) > 0)
	    sendto_one(sptr, "NOTICE %s :*** Class %4d %s: %d",
		       parv[0], Class(cltmp),
		       "Entries linked", Links(cltmp));
	check_class();
	return 0;
    }

/*
** m_motd
**	parv[0] = sender prefix
**	parv[1] = servername
*/
m_motd(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	FILE *fptr;
	char line[80], *tmp;

	if (HuntServer(cptr, sptr, ":%s MOTD :%s", 1,parc,parv) != HUNTED_ISME)
		return 0;
#ifndef MOTD
	sendto_one(sptr,
		"NOTICE %s :*** No message-of-today is available on host %s",
			parv[0], me.name);
#else
	if (!(fptr = fopen(MOTD, "r")))
	    {
		sendto_one(sptr,
			"NOTICE %s :*** Message-of-today is missing on host %s",
			        parv[0], me.name);
		return 0;
	    } 
	sendto_one(sptr, "NOTICE %s :MOTD - %s Message of the Day - ",
		parv[0], me.name);
	while (fgets(line, 80, fptr))
	    {
		if (tmp = index(line,'\n'))
			*tmp = '\0';
		if (tmp = index(line,'\r'))
			*tmp = '\0';
		sendto_one(sptr,"NOTICE %s :MOTD - %s", parv[0], line);
	    }
	sendto_one(sptr, "NOTICE %s :* End of /MOTD command.", parv[0]);
	fclose(fptr);
#endif
	return 0;
    }

/*
** m_service()
** parv[0] - sender prefix
** parv[1] - service name
** parv[2] - service password
*/
m_service(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  aConfItem *aconf;
  char *name, *password;
  
  name = parc > 1 ? parv[1] : NULL;
  password = parc > 2 ? parv[2] : NULL;
  if (!IsClient(sptr) || (BadPtr(name) || BadPtr(password)) &&
      cptr == sptr )
    {
      sendto_one(sptr, ":%s %d %s :Change balls, please",
		 me.name, ERR_NEEDMOREPARAMS,
		 parv[0]);
      return 0;
    }
  
  /* if message arrived from server, trust it, and set to service */
  /* Currently this should not happen yet                   --SRB */
  
  if (IsServer(cptr))
    {
      SetService(sptr);
      sendto_serv_butone(cptr, ":%s SERVICE", parv[0]);
      return 0;
    }
  if (!name || !(aconf = FindConfName( name, CONF_SERVICE))) {
    sendto_one(sptr,
	       ":%s %d %s :Only few of mere robots may try to %s",
	       me.name, ERR_NOSERVICEHOST, parv[0],
	       "serve");
    return 0;
  }
  if ((aconf->status & CONF_SERVICE) && StrEq(password, aconf->passwd))
    {
      sendto_one(sptr, ":%s %d %s :Net. %s",
		 me.name, RPL_YOURESERVICE, parv[0],
		 "First service");
      SetService(sptr);
      /*
       ** For now, services are only known locally --SRB       */
      /*
       ** sendto_serv_butone(cptr, ":%s SERVICE", parv[0]);
       */
    }
  else
    sendto_one(sptr, ":%s %d %s :Only real players know how %s",
	       me.name, ERR_PASSWDMISMATCH, parv[0],
	       "to serve");
  return 0;
}

m_whoreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    return m_error(cptr, sptr, parc, parv);
}

m_namreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    return m_error(cptr, sptr, parc, parv);
}

m_linreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    return m_error(cptr, sptr, parc, parv);
}

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
m_userhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	char	*output, buf[USERHOST_REPLYLEN], *p;
	aClient	*acptr;
	register	char *s;
	register	int i;

	CheckRegisteredUser(sptr);

	if (parc < 2) {
		sendto_one(sptr, ":%s %d %s :USERHOST not enough params",
			   me.name, ERR_NEEDMOREPARAMS, sptr->name );
		return 0;
	}
	output = (char *)MyMalloc(strlen(me.name) + strlen(sptr->name) + 3 +
				  USERHOST_REPLYLEN * 5 + 10);
	sprintf(output, ":%s %d %s :", me.name, RPL_USERHOST, sptr->name);
	for (i = 5, s = strtoken(&p, parv[1], " "); i && s;
	     s = strtoken(&p, NULL, " "), i--)
		if (acptr = find_person(s, (aClient *)NULL)) {
			sprintf(buf, "%s%s=%c%s@%s ",
				acptr->name,
				IsOper(acptr) ? "*" : "",
				(acptr->user->away) ? '-' : '+',
				acptr->user->username,
				acptr->user->host);
			strcat(output, buf);
		}
	sendto_one(sptr, output);
	free(output);
	return 0;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches of 5.
 * format:
 * ISON :nicklist
 */

m_ison(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	char *reply, *p;
	register aClient *acptr;
	register char *s;

	CheckRegisteredUser(sptr);

	if (parc != 2) {
		sendto_one(sptr, ":%s %d %s :ISON not enough args",
			   me.name, ERR_NEEDMOREPARAMS, sptr->name);
		return 0;
	}
	reply = (char *)MyMalloc(strlen(me.name) + strlen(sptr->name) + 3 +
				 strlen(parv[1]) + 10);

	sprintf(reply, ":%s %d %s :", me.name, RPL_ISON, sptr->name);
	for (s = strtoken(&p, parv[1], " "); s; s = strtoken(&p, NULL, " "))
		if (acptr = find_person(s, (aClient *)NULL)) {
			strcat(reply, acptr->name);
			strcat(reply, " ");
		}
	sendto_one(sptr, reply);
	free(reply);
	return 0;
}
