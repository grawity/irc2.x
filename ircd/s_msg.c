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

/* -- Jto -- 25 Oct 1990
 * Configuration file fixes. Wildcard servers. Lots of stuff.
 */

char s_msg_id[] = "s_msg.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "whowas.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <sys/stat.h>
#include <stdio.h>
#include <utmp.h>

extern aClient *client, me, *local[];
extern aClient *find_server(), *find_person(), *find_service();
extern aClient *find_userhost(), *find_client(), *find_name();
extern aConfItem *find_conf(), *find_conf_name(), *find_admin();
extern aConfItem *conf, *find_conf_exact(), *find_conf_host();
extern int connect_server();
extern int close_connection();
extern aChannel *channel;
extern char *debugmode;
extern char *configfile;
extern int maxusersperchannel;
extern int autodie, highest_fd;

extern char *date PROTO((long));
extern char *strerror();
extern void send_channel_modes();
extern void terminate();
extern struct SLink *find_user_link();

void send_umode();

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

char *my_name_for_link(name, conf)
char *name;
aConfItem *conf;
{
  int count = conf->port;
  static char namebuf[HOSTLEN];
  char *start = name;
  if (count <= 0 || count > 5) {
    return start;
  }
  while (count--) {
    name++;
    name = index(name, '.');
  }
  if (!name)
    return start;
  namebuf[0] = '*';
  strncpy(&namebuf[1], name, HOSTLEN - 1);
  namebuf[HOSTLEN - 1] = '\0';
  return namebuf;
}

/*****
****** NOTE:	get_client_name should be moved to some other file!!!
******		It's now used from other places...  --msa
*****/

/*
** get_client_name
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
char *get_client_name(sptr,spaced)
aClient *sptr;
int spaced; /* ...actually a kludge for for m_server */
{
    static char nbuf[sizeof(sptr->name)+sizeof(sptr->sockhost)+3];

    if (MyConnect(sptr) && mycmp(sptr->name,sptr->sockhost) != 0)
	{
	sprintf(nbuf, "%s[%s]", sptr->name, sptr->sockhost);
	return nbuf;
	}

    return sptr->name;
}

/*
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
*/
static aClient *next_client(next, ch)
char *ch;	/* Null terminated search string (may include wilds) */
aClient *next;	/* First client to check */
    {
	for ( ; next != NULL; next = next->next)
	    {
		if (IsService(next))
			continue;
		if (matches(ch,next->name) == 0 || matches(next->name,ch) == 0)
			break;
	    }
	return next;
    }

/*
** hunt_server
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

static int hunt_server(cptr, sptr, command, server, parc, parv)
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
	     acptr = next_client(acptr, parv[server]);
	     acptr = acptr->next)
	    {
		if (IsMe(acptr) || MyClient(acptr))
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
** 'do_nick_name' ensures that the given parameter (nick) is
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

static int do_nick_name(nick)
char *nick;
{
    Reg1 char *ch;

    if (*nick == '-' || isdigit(*nick)) /* first character in [0-9-] */
	return 0;

    for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
	if (!isvalid(*ch) || isspace(*ch))
	    break;

    *ch = '\0';

    return (ch - nick);
}


/*
** register_user
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

static register_user(cptr,sptr,nick)
aClient *cptr;
aClient *sptr;
char *nick;
    {
        char *parv[3];
	short oldstatus = sptr->status;
	aConfItem *aconf = NULL;
	anUser *user;

	sptr->user->last = time((long *) 0);
	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

	add_history(sptr);
	if (MyConnect(sptr))
	  {
	    if (check_access(cptr, CONF_CLIENT, 0))
	      {
		sendto_ops("Received unauthorized connection from %s.",
			   get_client_name(cptr,FALSE));
		return exit_client(sptr, sptr, sptr, "No Authorization");
	      } 
	    det_confs_butmask(cptr, CONF_CLIENT);
	    det_I_lines_butfirst(cptr);
	    if (cptr->confs == (Link *)NULL)
	      {
		sendto_one(sptr, ":%s %d * :%s",
			   me.name, ERR_NOPERMFORHOST,
			   "Your host isn't among the privileged..");
		return exit_client(sptr, sptr, sptr, "Unpriveledged Host");
	      }
	    aconf = cptr->confs->value.aconf;
	    if (!StrEq(sptr->passwd, aconf->passwd))
	      {
		sendto_one(sptr, ":%s %d * :%s",
			   me.name, ERR_PASSWDMISMATCH,
			   "Only correct words will open this gate..");
		return exit_client(sptr, sptr, sptr, "Bad Password");
	      }
	    strncpyzt(sptr->user->host, sptr->sockhost, HOSTLEN);
	    if (oldstatus == STAT_MASTER && MyConnect(sptr))
		m_oper(&me, sptr, 1, parv);
	  }
	SetClient(sptr);
	if (MyConnect(sptr))
	  {
	    sendto_one(sptr, "NOTICE %s :%s %s",
			nick, "*** Welcome to the Internet Relay Network,",
			nick);
	    sendto_one(sptr,
			"NOTICE %s :*** Your host is %s, running version %s",
			nick, get_client_name(&me,FALSE), version);
	    sendto_one(sptr,
			"NOTICE %s :*** This server was created %s",
			nick, creation);
	    m_lusers(sptr, sptr, 1, parv);
	    m_motd(sptr, sptr, 1, parv);
	  }
	sendto_serv_butone(cptr, "NICK %s %d", nick, sptr->hopcount+1);
	user = sptr->user;
	sendto_serv_butone(cptr, ":%s USER %s %s %s %s", nick,
			   user->username, user->host,
			   user->server, sptr->info);
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
	if (MyConnect(sptr) && (nick = (char *)index(parv[1], '~')))
		*nick = '\0';
	nick = parv[1];
	if (do_nick_name(nick) == 0)
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
	/*
	** Put this 'if' here so that the nesting goes nicely on the screen :)
	** We check against server name list before determining if the nickname
	** is present in the nicklist (due to the way the below for loop is
	** constructed). -avalon
	*/
	if ((acptr = find_server(nick,(aClient *)NULL)) != NULL)
		if (MyConnect(sptr))
		    {
			sendto_one(sptr,
				   ":%s %d %s %s :Nickname is already in use.",
				   me.name, ERR_NICKNAMEINUSE,
				   BadPtr(parv[0]) ? "*" : parv[0], nick);
			return 0; /* NICK message ignored */
		    }
	for (;;)
	    {
		/*
		** acptr already has result from previous find_server()
		*/
		if (acptr)
		    {
			/*
			** We have a nickname trying to use the same name as
			** a server. Send out a nick collision KILL to remove
			** the nickname. As long as only a KILL is sent out,
			** there is no danger of the server being disconnected.
			** Ultimate way to jupiter a nick ? >;-). -avalon
			*/
			sendto_ops("Nick collision on %s(%s <- %s)",
				   sptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));
			sendto_serv_butone((aClient *)NULL, /* all servers */
					   "KILL %s :%s (%s <- %s)",
					   sptr->name,
					   me.name,
					   sptr->from->name,
					   /* NOTE: Cannot use get_client_name
					   ** twice here, it returns static
					   ** string pointer--the other info
					   ** would be lost
					   */
					   get_client_name(cptr, FALSE));
			sptr->flags |= FLAGS_KILLED;
			exit_client((aClient *)NULL,sptr,&me,
				    "Nick/Server collision");
			return 0; /* NICK-Server collision handled */
		    }
		if ((acptr = find_client(nick,(aClient *)NULL)) == NULL)
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
			exit_client((aClient *)NULL,acptr,&me,"Overridden");
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
		/*
		** The client indicated by 'acptr' is dead meat, give
		** at least some indication of the reason why we are just
		** dropping it cold. ERR_NICKNAMEINUSE is not exactly the
		** right, should have something like ERR_NICKNAMECOLLISION..
		*/
		sendto_one(acptr, ":%s %d %s %s :Nickname collision, sorry.",
			   me.name, ERR_NICKNAMEINUSE,
			   acptr->name, acptr->name);
		if (sptr == cptr)
		    {
			sendto_ops("Nick collision on %s(%s <- %s)",
				   acptr->name, acptr->from->name,
				   get_client_name(cptr, FALSE));
			/*
			** A new NICK being introduced by a neighbouring
			** server (e.g. message type "NICK new" received)
			*/
			sendto_serv_butone((aClient *)NULL, /* all servers */
					   "KILL %s :%s (%s <- %s)",
					   acptr->name,
					   me.name,
					   acptr->from->name,
					   /* NOTE: Cannot use get_client_name
					   ** twice here, it returns static
					   ** string pointer--the other info
					   ** would be lost
					   */
					   get_client_name(cptr, FALSE));
			acptr->flags |= FLAGS_KILLED;
			exit_client((aClient *)NULL,acptr,&me,"Nick collision");
			return 0; /* Normal NICK collision handled */
		    }
		/*
		** A NICK change has collided (e.g. message type
		** ":old NICK new". This requires more complex cleanout.
		** Both clients must be purged from this server, the "new"
		** must be killed from the incoming connection, and "old" must
		** be purged from all outgoing connections.
		*/
		sendto_ops("Nick change collision from %s to %s(%s <- %s)",
			   sptr->name, acptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		sendto_serv_butone(cptr, /* KILL old from outgoing servers */
				   "KILL %s :%s (%s(%s) <- %s)",
				   sptr->name,
				   me.name,
				   acptr->from->name,
				   acptr->name,
				   get_client_name(cptr, FALSE));
		sendto_one(cptr, /* Kill new from incoming link */
			   "KILL %s :%s (%s <- %s(%s))",
			   acptr->name,
			   me.name,
			   acptr->from->name,
			   get_client_name(cptr, FALSE),
			   sptr->name);
		acptr->flags |= FLAGS_KILLED;
		exit_client((aClient *)NULL,acptr,&me,"Nick collision(new)");
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr,sptr,&me,"Nick collision(old)");
	    }
	if (IsServer(sptr))
	    {
		/* A server introducing a new client, change source */

		sptr = make_client(cptr);
		add_client_to_list(sptr);
		if (parc > 2)
			sptr->hopcount = atoi(parv[2]);
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
		add_history(sptr);
	      sendto_serv_butone(cptr, ":%s NICK %s", parv[0], nick);
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
			register_user(cptr,sptr,nick);
	    }
	/*
	**  Finally set new nick name.
	*/
	if (sptr->name[0])
		del_from_client_hash_table(sptr->name, sptr);
	strcpy(sptr->name, nick); /* correctness guaranteed by 'DoNickName' */
	add_to_client_hash_table(sptr->name, sptr);
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

static int m_message(cptr, sptr, parc, parv, notice)
aClient *cptr, *sptr;
int parc;
char *parv[];
int notice;
{
    aClient *acptr;
    aChannel *chptr;
    char *nick, *server, *p, *cmd, *host;

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

    for (; (nick = strtoken(&p, parv[1], ","))!=NULL; parv[1] = NULL)
	{
	/*
	** nickname addressed?
	*/
	if ((acptr = find_client(nick, (aClient *)NULL)) != NULL)
	    {
	    if (!notice
		&& MyConnect(sptr)
		&& acptr->user
		&& acptr->user->away)
		sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_AWAY,
		       parv[0], acptr->name, acptr->user->away);

	     sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
				parv[0], cmd, nick, parv[2]);
	    continue;
	    }
	/*
	** to me, from another server ?
	*/
	if (mycmp(me.name, nick)==0 && MyConnect(sptr) && IsServer(sptr))
	    {
	    sendto_ops(":%s %s :*** %s", parv[0], cmd, parv[2]);
	    continue;
	    }

	/*
	** channel msg?
	*/

	if ((chptr = find_channel(nick, NullChn)) != NullChn)
	    {
	    if (can_send(sptr, chptr) == 0)
		sendto_channel_butone(cptr, sptr, chptr, ":%s %s %s :%s", 
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
		sendto_one(sptr, ":%s %d %s :No toplevel domain specified",
			   me.name, ERR_NOTOPLEVEL, parv[0]);
		continue;
		}
	    for (s++ ; *s; s++)
		if (*s == '.' || *s == '*' || *s == '?')
		    break;
	    if (*s == '*' || *s == '?')
		{
		sendto_one(sptr, ":%s %d %s :Wildcard in toplevel Domain",
			   me.name, ERR_WILDTOPLEVEL, parv[0]);
		continue;
		}
	    sendto_match_butone(IsServer(cptr) ? cptr : NULL, sptr, nick + 1,
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
		sendto_one(acptr,":%s %s %s :%s",parv[0], cmd, nick, parv[2]);
		continue;
		}

	    *server = '\0';

	    if (host = index(nick, '%'))
		*host++ = '\0';

	    if (acptr = find_userhost(nick, host, (aClient *)NULL,
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
		    sendto_prefix_one(acptr, sptr, ":%s %s %s@%s :%s",
				      parv[0], cmd, nick, server, parv[2]);
		    continue;
		    }
		}
	    }

      sendto_one(sptr, ":%s %d %s %s :No such nickname",
	  me.name, ERR_NOSUCHNICK, parv[0], nick);
      }
    return 0;
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
  if (repchan && is_chan_op(acptr, repchan))
    stat[i++] = '@';
  stat[i] = '\0';
  sendto_one(sptr,":%s %d %s %s %s %s %s %s %s :%d %s",
	     me.name, RPL_WHOREPLY, sptr->name,
	     (repchan) ? (repchan->chname) : "*",
	     acptr->user->username,
	     acptr->user->host,
	     acptr->user->server, acptr->name,
	     stat, acptr->hopcount, acptr->info);
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
	Reg1 aClient *acptr;
	Reg2 char *mask = parc > 1 ? parv[1] : NULL;
	Reg3 Link *link;
	aChannel *chptr;
	aChannel *mychannel;
	char *channame = (char *) 0;
	int oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int member;

	mychannel = NullChn;
	if (sptr->user)
		if (link = sptr->user->channel)
			mychannel = link->value.chptr;

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
	sendto_one(sptr,":%s %d %s Channel User Host Server Nickname S :Name",
		    me.name, RPL_WHOREPLY, parv[0]);
	if (channame && *channame == '#')
	    {
		/* List all users on a given channel */
		chptr = find_channel(channame, NullChn);
		if (chptr && (!SecretChannel(chptr) ||
			      (member = IsMember(sptr, chptr)) ) )
			for (link = chptr->members; link; link = link->next)
			    {
				if (oper && !IsAnOper(link->value.cptr))
					continue;
				if (IsInvisible(link->value.cptr) && !member)
					continue;
				DoWho(sptr, link->value.cptr, chptr);
			    }
	    }
	else for (acptr = client; acptr; acptr = acptr->next)
	    {
		aChannel *ch2ptr = NULL;
		int showperson, isinvis;

		if (!IsPerson(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;
		showperson = 0;
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
			showperson = 1;
		if (!showperson)
			continue;
		showperson = 0;
		/*
		 * Show user if they are on the same channel, or not
		 * invisible and on a non secret channel (if any).
		 */
		isinvis = IsInvisible(acptr);
		for (link = acptr->user->channel; link; link = link->next)
		    {
			chptr = link->value.chptr;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (member || !isinvis && PubChannel(chptr))
			    {
				ch2ptr = chptr;
				showperson = 1;
				break;
			    }
			if (HiddenChannel(chptr) && !isinvis)
				showperson = 1;
		    }
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		if (showperson)
			DoWho(sptr, acptr, ch2ptr);
	    }
	sendto_one(sptr, ":%s %d %s %s :* End of /WHO list.", me.name,
		   RPL_ENDOFWHO, parv[0], parv[1] ? parv[1] : "*");
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
    int found, eol = 0, wilds, cont;
    Reg1 char *nick, *tmp;
    char *p;
    aClient *acptr, *a2cptr;
    Reg2 Link *link;
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
	if (hunt_server(cptr,sptr,":%s WHOIS %s %s", 1,parc,parv) !=
	    HUNTED_ISME)
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
	       acptr = next_client(acptr,nick);
	       acptr = acptr->next)
	    {
	      static anUser UnknownUser =
		{
		  "<Unknown>",	/* username */
		  "<Unknown>",	/* host */
		  NULL,	/* server */
		  NULL,	/* channel */
		  NULL,   /* invited */
		  1,      /* refcount */
		  NULL,	/* away */
		};
	      Reg3 anUser *user;
	      
	      if (IsServer(acptr) || IsMe(acptr))
		continue;
	      user = acptr->user ? acptr->user : &UnknownUser;
	      
	      /* Secret users are visible only by specifying
	       ** exact nickname. Wild Card guessing would
	       ** make things just too easy...  -msa
	       */
	      cont = 0;
	      link = user->channel;
	      while (link) {
		if (link->value.chptr &&
		    (IsInvisible(acptr) || SecretChannel(link->value.chptr)) &&
		    wilds)
		cont = 1;
		link = link->next;
	      }
	      if (cont)
		continue;
	      found = eol = 1;

	      a2cptr = find_server(user->server, NULL);

	      sendto_one(sptr,":%s %d %s %s %s %s * :%s",
			 me.name, RPL_WHOISUSER,
			 parv[0], acptr->name,
			 user->username,
			 user->host,
			 acptr->info);
	      for (buf[0] = '\0', link = user->channel; link;
		   link = link->next) {
			chptr = link->value.chptr;
			if (ShowChannel(sptr, chptr) &&
			    IsMember(acptr, chptr)) {
				if (strlen(buf) + strlen(chptr->chname) > 400) {
					sendto_one(sptr, ":%s %d %s %s :%s",
						   me.name, RPL_WHOISCHANNELS,
						   parv[0], acptr->name,
						   buf);
					buf[0] = '\0';
				}
				if (is_chan_op(acptr, chptr))
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
	      sendto_one(sptr,":%s %d %s %s %s :%s",
			 me.name, 
			 RPL_WHOISSERVER,
			 parv[0], acptr->name,
			 user->server,
			 a2cptr ? a2cptr->info : "*Not On This Net*");
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
  char *username, *host, *server, *realname;
 
  if (parc > 2 && (username = index(parv[1],'@')))
    *username = '\0'; 
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

  if (MyConnect(sptr))
    host = sptr->sockhost;
  if (!IsUnknown(sptr))
    {
      sendto_one(sptr,":%s %d * :Identity problems, eh ?",
		 me.name, ERR_ALREADYREGISTRED);
      return 0;
    }
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
   if (MyConnect(sptr)) {   
      if (rc = find_kill(host, username, reply))
	{
	  sendto_one(sptr, reply, me.name, rc, "*");
	  if (rc == ERR_YOUREBANNEDCREEP)
	    {
	      return 0;
	    }
	}
#ifdef R_LINES
      if (find_restrict(host,username,reply))
	{
	  sendto_one(sptr,":%s %d :*** %s",me.name,
		     ERR_YOUREBANNEDCREEP,reply);
	  return exit_client(sptr, sptr, sptr , "");
	}
#endif
    }
  }
  make_user(sptr);
  strncpyzt(sptr->user->username, username, USERLEN);
  strncpyzt(sptr->info, realname, REALLEN);
  strncpyzt(sptr->user->server, cptr != sptr ? server : me.name, HOSTLEN);
  strncpyzt(sptr->user->host, host, HOSTLEN);

  if (sptr->name[0]) /* NICK already received, now we have USER... */
    register_user(cptr,sptr,sptr->name);

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
		       chptr->users,
		       (ShowChannel(sptr, chptr) ? chptr->topic : ""));
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
	if (hunt_server(cptr,sptr,":%s VERSION %s",1,parc,parv) == HUNTED_ISME)
	    sendto_one(sptr,":%s %d %s %s.%s %s", me.name, RPL_VERSION,
	       parv[0], version, debugmode, me.name);
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

	return IsServer(sptr) ? 0 : exit_client(cptr, sptr, sptr, comment);
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
		     acptr = next_client(acptr, server);
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
	** "hunt_server" (e.g. just pass on "SQUIT" without doing
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
		sendto_one(sptr,":%s NOTICE %s :No such server (%s)",
			   me.name, parv[0], server);
		return 0;
	    }
	/*
	**  Notify all opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsAnOper(cptr))
	    sendto_ops_butone((aClient *)NULL, &me,
		":%s WALLOPS :Received SQUIT %s from %s (%s)",
		me.name, server, get_client_name(sptr,FALSE), comment);

	return exit_client(cptr, acptr, sptr, comment);
    }

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo/hopcount
**      parv[3] = serverinfo
*/
m_server(cptr, sptr, parc, parv)
     aClient *cptr, *sptr;
     int parc;
     char *parv[];
{
  char *host, info[REALLEN+1], *inpath;
  aClient *acptr, *bcptr;
  aConfItem *aconf, *bconf;
  Reg1 char *ch;
  Reg2 int i;
  int hop, split;

  info[0] = '\0';
  inpath = get_client_name(cptr,FALSE);
  if (parc < 2 || *parv[1] == '\0')
    {
      sendto_one(cptr,"ERROR :No servername");
      return 0;
    }
  hop = 0;
  host = parv[1];
  if (parc > 3 && atoi(parv[2]))
    {
      hop = atoi(parv[2]);
      strncpy(info, parv[3], REALLEN);
    }
  else if (parc > 2)
    {
      strncpy(info, parv[2], REALLEN);
      if (parc > 3)
       {
	i = strlen(info);
	strncat(info, " ", REALLEN - i - 1);
	strncat(info, parv[3], REALLEN - i - 2);
       }
    }
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
	sendto_one(sptr,":%s NOTICE %d :Bogus server name (%s)",
		   me.name, parv[0], host);
	sendto_ops("Bogus server name (%s) from %s",
		   host, get_client_name(cptr,FALSE));
	return 0;
      }
  
  if (IsPerson(cptr))
    {
      /*
       ** A local link that has been identified as a USER
       ** tries something fishy... ;-)
       */
      sendto_one(cptr,
		 "%s NOTICE %s :Client may not currently become server",
		 me.name, parv[0]);
      sendto_ops("User %s trying to become a server %s",
		 get_client_name(cptr,FALSE),host);
      return 0;
    }
  /* *WHEN* can it be that "cptr != sptr" ????? --msa */
  
  if ((acptr = find_name(host, (aClient *)NULL)) != NULL)
    {
      /*
       ** This link is trying feed me a server that I
       ** already have access through another path--
       ** multiple paths not accepted currently, kill
       ** this link immeatedly!!
       */
      sendto_one(cptr,":%s NOTICE %s :Server %s already exists... ",
		 me.name, parv[0], host);
      sendto_ops("Link %s cancelled, server %s already exists",
		 inpath, host);
      return exit_client(cptr, cptr, cptr, "Server Exists");
    }
  if ((acptr = find_client(host, (aClient *)NULL)) != NULL)
    {
      /*
      ** Server trying to use the same name as a person. Would
      ** cause a fair bit of confusion. Enough to make it hellish
      ** for a while and servers to send stuff to the wrong place.
      */
      sendto_one(cptr,"ERROR :Nickname %s already exists!", host);
      sendto_ops("Link %s cancelled: Server/nick collision on %s",
		 inpath, host);
      return exit_client(cptr, cptr, cptr, "Nickname using Servername");
    }

  if (IsServer(cptr))
    {
      /*
       ** Server is informing about a new server behind
       ** this link. Create REMOTE server structure,
       ** add it to list and propagate word to my other
       ** server links...
       */
      if (parc == 1 || info[0] == '\0')
	{
	  sendto_one(cptr,"ERROR :No server info specified for %s", host);
	  return 0;
	}

      /*
       ** See if the newly found server is behind a guaranteed
       ** leaf (L-line). If so, close the link.
       */
      if (aconf = find_conf_name(sptr->name, CONF_LEAF)) {
	sendto_ops("Leaf-only link %s issued second server command",
		   get_client_name(sptr, FALSE) );
	sendto_one(cptr, "ERROR :Leaf-only link, sorry." );
	return exit_client(cptr, cptr, cptr, "Leaf Only");
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
      
      if (aconf = find_conf_name(host, CONF_QUARANTINED_SERVER)) {
	sendto_ops_butone((aClient *) 0, &me,
			  ":%s WALLOPS :%s brought in %s, %s %s",
			  me.name, get_client_name(sptr, FALSE),
			  host, "closing link because",
			  BadPtr(aconf->passwd) ?
			  "reason unspecified" : aconf->passwd);
	
	sendto_one(cptr, "ERROR :%s is not welcome because %s. %s",
		   host, BadPtr(aconf->passwd) ?
		   "reason unspecified" : aconf->passwd,
		   "Go away and get a life");
	return exit_client(cptr, cptr, cptr, "Q-Lined Server");
      }
      
      acptr = make_client(cptr);
      acptr->hopcount = hop;
      strncpyzt(acptr->name,host,sizeof(acptr->name));
      strncpyzt(acptr->info,info,sizeof(acptr->info));
      SetServer(acptr);
      add_client_to_list(acptr);
      add_to_client_hash_table(acptr->name, acptr);
      
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
	  if (matches(my_name_for_link(me.name, bcptr->confs->value.aconf),
		      acptr->name) == 0)
	    continue;
	  sendto_one(bcptr,"SERVER %s %d :%s",
		     acptr->name, hop+1, acptr->info);
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
  strncpyzt(cptr->name, host, sizeof(cptr->name));
  strncpyzt(cptr->info, info[0] ? info:me.name, sizeof(cptr->info));

  if (check_access(cptr, CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER, 1))
    {
      sendto_ops("Received unauthorized connection from %s.",
                 get_client_name(cptr,FALSE));
      return exit_client(cptr, cptr, cptr, "No C/N conf lines");
    }
  inpath = get_client_name(cptr,TRUE); /* "refresh" inpath with host */
  split = mycmp(cptr->name, cptr->sockhost);
  if (!(aconf = find_conf(cptr->confs,host,CONF_NOCONNECT_SERVER)))
    {
      sendto_one(cptr,
		 "ERROR :Access denied. No N field for server %s",
		 inpath);
      sendto_ops("Access denied. No N field for server %s",
		 inpath);
      return exit_client(cptr, cptr, cptr, "");
    }
  if (!(bconf = find_conf(cptr->confs,host,CONF_CONNECT_SERVER)))
    {
      sendto_one(cptr, "ERROR :Only N (no C) field for server %s", inpath);
      sendto_ops("Only N (no C) field for server %s",inpath);
      return exit_client(cptr, cptr, cptr, "");
    }
  if (*(aconf->passwd) && !StrEq(aconf->passwd, cptr->passwd))
    {
      sendto_one(cptr, "ERROR :Access denied (passwd mismatch) %s",
		 inpath);
      sendto_ops("Access denied (passwd mismatch) %s", inpath);
      return exit_client(cptr, cptr, cptr, "");
    }
  if (IsUnknown(cptr))
    {
      if (bconf->passwd[0])
	sendto_one(cptr,"PASS %s",bconf->passwd);
      /*
       ** Pass my info to the new server
       */
      sendto_one(cptr,":%s SERVER %s 1 :%s",
		 me.name, my_name_for_link(me.name, aconf), 
		 (me.info[0]) ? (me.info) : "IRCers United");
    }
  /*
   ** *WARNING*
   ** 	In the following code in place of plain server's
   **	name we send what is returned by get_client_name
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
  SetServer(cptr);
  cptr->hopcount = hop;
  sendto_ops("Link with %s established.", inpath);
  add_to_client_hash_table(cptr->name, cptr);
  /*
   ** Old sendto_serv_but_one() call removed because we now
   ** need to send different names to different servers
   ** (domain name matching)
   ** Send new server to other servers.
   */
  for (i = 0; i <= highest_fd; i++) 
    {
      if (!(acptr = local[i]) || !IsServer(acptr) ||
	  acptr == cptr || IsMe(acptr))
	continue;
      if (matches(my_name_for_link(me.name, acptr->confs->value.aconf),
		  cptr->name) == 0)
	continue;
      if (split)
	sendto_one(acptr,"SERVER %s 2 :[%s] %s",
		   cptr->name, cptr->sockhost, cptr->info);
      else
	sendto_one(acptr,"SERVER %s 2 :%s",
		   cptr->name, cptr->info);
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
   ** ALSO NOTE: using the get_client_name for server names--
   **	see previous *WARNING*!!! (Also, original inpath
   **	is destroyed...)
   */
  
  for (acptr = client; acptr; acptr = acptr->next)
   {
    if (acptr->from == cptr) /* acptr->from == acptr for acptr == cptr */
      continue;
    if (IsServer(acptr))
     {
      if (matches(my_name_for_link(me.name, cptr->confs->value.aconf),
		  acptr->name) == 0)
	continue;
      split = (MyConnect(acptr) && mycmp(acptr->name, acptr->sockhost));
      if (split)
	sendto_one(cptr,"SERVER %s %d :[%s] %s",
		   acptr->name, acptr->hopcount+1,
		   acptr->sockhost, acptr->info);
      else
	sendto_one(cptr, "SERVER %s %d :%s",
		   acptr->name, acptr->hopcount+1, acptr->info);
     }
   }
  for (acptr = &me; acptr; acptr = acptr->prev)
   {
    if (acptr->from == cptr) /* acptr->from == acptr for acptr == cptr */
      continue;
    if (IsPerson(acptr)) {
      Link *link;
      /*
      ** IsPerson(x) is true only when IsClient(x) is true. These are
      ** only true when *BOTH* NICK and USER have been received. -avalon
      */
      sendto_one(cptr,"NICK %s %d",acptr->name, acptr->hopcount + 1);
      sendto_one(cptr,":%s USER %s %s %s :%s", acptr->name,
		 acptr->user->username, acptr->user->host,
		 acptr->user->server, acptr->info);
      if (IsOper(acptr))
	sendto_one(cptr,":%s OPER",acptr->name);
      send_umode(cptr,acptr);
      if (acptr->user->away != NULL)
	sendto_one(cptr,":%s AWAY :%s", acptr->name,
		   acptr->user->away);
      for (link = acptr->user->channel; link; link = link->next)
	sendto_one(cptr,":%s JOIN %s", acptr->name, link->value.chptr->chname);
    } else if (IsService(acptr)) {
      sendto_one(cptr,"NICK %s %d", acptr->name, acptr->hopcount + 1);
      sendto_one(cptr,":%s SERVICE * * :%s", acptr->name, acptr->info);
    }
  }
  /*
   ** Last, pass all channels plus statuses
   */
  {
    Reg1 aChannel *chptr;
    for (chptr = channel; chptr; chptr = chptr->nextch)
      send_channel_modes(cptr, chptr);
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
	char *inpath = get_client_name(cptr,FALSE);
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
	if (!IsLocOp(sptr) && !IsPrivileged(sptr) && !IsServer(cptr))
	    {
		sendto_one(sptr,":%s %d %s :Death before dishonor ?",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }
	if (!(acptr = find_client(user, (aClient *)NULL)))
	    {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = get_history(user, (long)KILLCHASETIMELIMIT)))
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
		inpath = cptr->sockhost; /* Don't use get_client_name syntax */
		if (!BadPtr(path))
		    {
			sprintf(buf, "%s%s (%s)",
				cptr->name,
				IsOper(sptr) ? "" : "(L)",
				path);
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
	if (IsLocOp(sptr) && !MyConnect(acptr))
	  {
	    sendto_one(sptr,":%s %d %s :Death before dishonor ?",
		     me.name, ERR_NOPRIVILEGES, parv[0]);
	    return 0;
	  }
	sendto_ops("Received KILL message for %s. Path: %s!%s",
		   acptr->name, inpath, path);
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
		sendto_prefix_one(ptr, sptr, ":%s KILL %s :%s!%s",
				  IsServer(sptr) ? "" : parv[0],
				  acptr->name, inpath, path);
	    }
	}
	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_prefix_one(acptr, sptr,":%s KILL %s :%s!%s",
				  parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents exit_one_client from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	if (sptr != acptr)  /* Suicide kills are simple signoffs --SRB */
	  acptr->flags |= FLAGS_KILLED;
	return exit_client(cptr, acptr, sptr, "Killed");
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

    if (hunt_server(cptr,sptr,":%s INFO %s",1,parc,parv) == HUNTED_ISME)
	{
	sendto_one(sptr, ":%s NOTICE %s :***", me.name, parv[0]);

	while (*text)
		sendto_one(sptr, ":%s NOTICE %s :*** %s",
			   me.name, parv[0], *text++);

	sendto_one(sptr, ":%s NOTICE %s :***", me.name, parv[0]);
	sendto_one(sptr,
		":%s NOTICE %s :*** This server was created %s, compile # %s",
		me.name, parv[0], creation, generation);
	sendto_one(sptr, ":%s NOTICE %s :*** On-line since %s",
		me.name, parv[0], myctime(cptr->firsttime));
	sendto_one(sptr, ":%s NOTICE %s :*** End of /INFO list.",
		me.name, parv[0]);
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
	if (hunt_server(cptr, sptr, ":%s LINKS %s %s", 1, parc, parv)
						    != HUNTED_ISME)
	    return 0;
	mask = parv[2];
	}
    else
	mask = parc < 2 ? NULL : parv[1];

    for (acptr = client; acptr; acptr = acptr->next) 
	if ((IsServer(acptr) || IsMe(acptr)) &&
	     (BadPtr(mask) || matches(mask, acptr->name) == 0))
		    sendto_one(sptr,":%s %d %s %s :%d %s",
			me.name, RPL_LINKS, parv[0], acptr->name,
			acptr->hopcount,
			(acptr->info[0] ? acptr->info :
			"(Unknown Location)"));

    sendto_one(sptr, ":%s %d %s :* End of /LINKS list.", me.name,
	       RPL_ENDOFLINKS, parv[0]);
    return 0;
}

/*
** m_summon should be redefined to ":prefix SUMMON host user" so
** that "hunt_server"-function could be used for this too!!! --msa
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
#ifdef LEAST_IDLE
        char linetmp[10], ttyname[15]; /* Ack */
        struct stat stb;
        time_t ltime = (time_t)0;
#endif
	int fd, flag = 0;

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
#ifndef LEAST_IDLE
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf)) == 0) 
			if (StrEq(namebuf,user))
				break;
#else
                /* use least-idle tty, not the first
                 * one we find in utmp. 10/9/90 Spike@world.std.com
                 * (loosely based on Jim Frost jimf@saber.com code)
                 */
		
                while ((flag = utmp_read(fd, namebuf, linetmp, hostbuf)) == 0){
                  if (StrEq(namebuf,user)) {
                    sprintf(ttyname,"/dev/%s",linetmp);
                    if (stat(ttyname,&stb) == -1) {
                      sendto_one(sptr,":%s NOTICE %s Cannot stat %s",
				 me.name, sptr->name,ttyname);
                      return 0;
                    }
                    if (!ltime) {
                      ltime= stb.st_mtime;
                      strcpy(linebuf,linetmp);
                    }
                    else if (stb.st_mtime > ltime) { /* less idle */
                      ltime= stb.st_mtime;
                      strcpy(linebuf,linetmp);
                    }
                  }
                }
#endif
		utmp_close(fd);
#ifdef LEAST_IDLE
                if (ltime == 0)
#else
		if (flag == -1)
#endif
			sendto_one(sptr,":%s NOTICE %s :User %s not logged in",
				   me.name, parv[0], user);
		else
			summon(sptr, user, linebuf);
		return 0;
	    }
	/*
	** Summoning someone on remote server, find out which link to
	** use and pass the message there...
	*/
	acptr = find_server(host,(aClient *)NULL);
	if (acptr == NULL)
		sendto_one(sptr, ":%s NOTICE %s :SUMMON No such host %s found",
			   me.name, parv[0], host);
	else if (!IsMe(acptr))
		sendto_prefix_one(acptr, sptr, ":%s SUMMON %s@%s",
				  parv[0], user, host);
	else
	  sendto_one(sptr, ":%s NOTICE %s :SUMMON: Summon bug bites",
		     me.name, parv[0]);
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

static int report_array[6][3] = {
		{ CONF_CONNECT_SERVER,    (int)'C', RPL_STATSCLINE},
		{ CONF_NOCONNECT_SERVER,  (int)'N', RPL_STATSNLINE},
		{ CONF_CLIENT,            (int)'I', RPL_STATSILINE},
		{ CONF_KILL,              (int)'K', RPL_STATSKLINE},
		{ CONF_QUARANTINED_SERVER,(int)'Q', RPL_STATSQLINE},
		{ 0, 0, 0}
				};

static report_configured_links(sptr, mask)
aClient *sptr;
int mask;
{
  aConfItem *tmp;
  char flag;
  int rpl, *p;
  static char noinfo[] = "<NULL>";
  
  for (tmp = conf; tmp; tmp = tmp->next)
    {
      if (tmp->status & mask) {
	for (p = &report_array[0][0]; *p; p += 3)
	  if (*p == tmp->status)
	    break;
      if (!*p)
	continue;
      flag = p[1];
      rpl = p[2];
      sendto_one(sptr,":%s %d %s %c %s * %s %d %d",
		 me.name, rpl, sptr->name, flag,
		 BadPtr(tmp->host) ? noinfo : tmp->host,
		 BadPtr(tmp->name) ? noinfo : tmp->name,
		 (int)tmp->port, get_conf_class(tmp));
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
	static char Lheading[] = ":%s %d %s %s %s %s %s %s %s :%s";
	static char Lformat[]  = ":%s %d %s %s %u %u %u %u %u :%s";
	
	if (hunt_server(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!= HUNTED_ISME)
		return 0;

	switch (*stat)
	{
	case 'L' : case 'l' :
		/*
		** The following is not the most effiecient way of
		** generating the heading, buf it's damn easier to
		** get the field widths correct --msa
		*/
		sendto_one(sptr,Lheading, me.name,
			   RPL_STATSLINKINFO, parv[0],
			   "Link", "SendQ", "SendM", "SendBytes",
			   "RcveM", "RcveBytes", "Open since");
		for (i = 0; i <= highest_fd; i++)
			if ((acptr = local[i]) && /* Includes IsMe! */
			    (IsOper(sptr) || !IsPerson(acptr) ||
			     IsLocOp(sptr) && IsLocOp(acptr))) {
				sendto_one(sptr,Lformat,
					   me.name,RPL_STATSLINKINFO,parv[0],
					   get_client_name(acptr,FALSE),
					   (int)DBufLength(&acptr->sendQ),
					   (int)acptr->sendM,
					   (int)acptr->sendB,
					   (int)acptr->receiveM,
					   (int)acptr->receiveB,
					   myctime(acptr->firsttime));
			      }
		break;
	case 'C' : case 'c' :
                report_configured_links(sptr, CONF_CONNECT_SERVER|
				      CONF_NOCONNECT_SERVER);
		break;
	case 'Q' : case 'q' :
		if (IsOper(sptr))
			report_configured_links(sptr, CONF_QUARANTINED_SERVER);
		break;
	case 'K' : case 'k' :
		if (IsOper(sptr))
			report_configured_links(sptr,CONF_KILL);
		break;
	case 'Y' : case 'y' :
		if (IsOper(sptr))
			report_classes(sptr);
		break;
	case 'I' : case 'i' :
		report_configured_links(sptr,CONF_CLIENT);
		break;
	default :
		for (mptr = msgtab; mptr->cmd; mptr++)
		   if (mptr->count)
			sendto_one(sptr,
			  ":%s %d %s %s %d",
			  me.name, RPL_STATSCOMMANDS,
			  parv[0], mptr->cmd, mptr->count);
		break;
	}
	sendto_one(sptr, ":%s %d %s :* End of /STATS report",
		   me.name, RPL_ENDOFSTATS, parv[0]);
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

    if (hunt_server(cptr,sptr,":%s USERS %s",1,parc,parv) == HUNTED_ISME)
	{
	if ((fd = utmp_open()) == -1)
	    {
	    sendto_one(sptr,":%s NOTICE %s Cannot open %s",
			me.name, parv[0], UTMP);
	    return 0;
	    }

	sendto_one(sptr,":%s NOTICE %s :UserID   Terminal  Host",
		me.name, parv[0]);
	while (utmp_read(fd, namebuf, linebuf, hostbuf) == 0)
	    {
	    flag = 1;
	    sendto_one(sptr,":%s NOTICE %s :%-8s %-9s %-8s",
			me.name, parv[0], namebuf, linebuf, hostbuf);
	    }
	if (flag == 0) 
	    sendto_one(sptr,":%s NOTICE %s :Nobody logged in on %s",
			me.name, parv[0], parv[1]);

	utmp_close(fd);
	}
    return 0;
}

/*
** exit_client
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
int exit_client(cptr, sptr, from, comment)
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
	static int exit_one_client();

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
		/*
		 * stop NFS hangs...most systems should be able to open a
		 * file in 3 seconds. -avalon (curtesy of wumpus)
		 */
		alarm(3);
		if (IsPerson(sptr) && !stat(FNAME_USERLOG, &stbuf) &&
		    (userlogfile = fopen(FNAME_USERLOG, "a")))
		    {
			alarm(0);
			on_for = time(NULL) - sptr->firsttime;
			fprintf(userlogfile, "%s (%3d:%02d:%02d): %s@%s\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->user->username, sptr->user->host);
			fclose(userlogfile);
		    }
		alarm(0);
		/* Modification by stealth@caen.engin.umich.edu */
	    }
#endif
		close_connection(sptr);
		/*
		** Note	There is no danger of 'cptr' being exited in
		**	the following loops. 'cptr' is a *local* client,
		**	all dependants are *remote* clients.
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (!IsServer(acptr) && acptr->from == sptr)
				exit_one_client((aClient *)0,acptr,
						&me,me.name);
		    }
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				exit_one_client((aClient *)0,acptr,
						&me,me.name);
		    }
	    }

	exit_one_client(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static exit_one_client(cptr, sptr, from, comment)
aClient *sptr;
aClient *cptr;
aClient *from;
char *comment;
    {
	aClient *acptr;
	Reg1 int i;
	Reg2 Link *link;

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
	      if (matches(my_name_for_link(me.name, acptr->confs->value.aconf),
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
		sendto_one(acptr, "SQUIT %s :%s", sptr->name, comment);
	    }
	} else if (!IsRegistered(sptr))
				    /* ...this test is *dubious*, would need
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
		  Link *tmp;
		  sendto_common_channels(sptr, ":%s QUIT :%s",
					 sptr->name, comment);
		  link = sptr->user->channel;
		  while (link) {
		    tmp = link->next;
		    remove_user_from_channel(sptr, link->value.chptr);
		    sptr->user->channel = tmp;
		    /* free(link); */
		    link = tmp;
		
		  }
		  /* Clean up invitefield */
		  for (link = sptr->user->invited; link; link = tmp) {
		    tmp = link->next;
		    del_invite(sptr, link->value.chptr);
		    /* no need to free link, del_invite does that */
		  }
		}
	    }
	/* Remove sptr from the client list */
	del_from_client_hash_table(sptr->name, sptr);
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
		sendto_ops("ERROR from %s: %s",get_client_name(cptr,FALSE),para);
	else
		sendto_ops("ERROR from %s via %s: %s", sptr->name,
			   get_client_name(cptr,FALSE), para);
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
	else
		sendto_one(sptr, ":%s %d %s :Only Janitors can %s",
			   me.name, ERR_NOPRIVILEGES, parv[0],
			   "emtpy the rubbish bins"); 
    }

m_die(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (IsOper(sptr) && MyConnect(sptr))
	    {
		sendto_ops("*** %s has pulled the plug on us sir!\n", parv[0]);
		terminate();
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only those with guns can %s",
			   me.name, ERR_NOPRIVILEGES, parv[0],
			   "shoot to kill.");

    }

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
	int m_client = 0, m_server = 0;
	aClient *acptr;

	if (parc > 2)
	  if(hunt_server(cptr, sptr, ":%s LUSERS %s %s", 2, parc, parv))
	    return 0;

	for (acptr = client; acptr; acptr = acptr->next)
	 {
	  if (parc>1)
	    if (!IsServer(acptr) && acptr->user)
	     {
	      if (matches(parv[1], acptr->user->server))
		continue;
	     } else {
	      if (matches(parv[1], acptr->name))
		continue;
	     }
	  switch (acptr->status)
	   {
	    case STAT_SERVER:
	      if (MyConnect(acptr))
		m_server++;
	    case STAT_ME:
	      s_count++;
	      break;
	    case STAT_CLIENT:
	      if (IsOper(acptr))
	        o_count++;
	      if (MyConnect(acptr))
	       {
		if (IsInvisible(acptr))
		 {
		  if (IsOper(sptr))
		    m_client++;
		 }
		else
		  m_client++;
	       }
	      if (!IsInvisible(acptr))
		c_count++;
	      else
		i_count++;
	      break;
	    default:
	      u_count++;
	      break;
	    }
	 }
	if (IsAnOper(sptr) && i_count)
		sendto_one(sptr,
	":%s NOTICE %s :There are %d users (%d invisible) on %d servers",
			   me.name, parv[0], c_count, i_count, s_count);
	else
		sendto_one(sptr,
			   ":%s NOTICE %s :There are %d users on %d servers",
			   me.name, parv[0], c_count, s_count);
	if (o_count)
		sendto_one(sptr,
		   ":%s NOTICE %s :%d user%s connection to the twilight zone",
			   me.name, parv[0], o_count,
			   (o_count > 1) ? "s have" : " has");
	if (u_count > 0)
		sendto_one(sptr,
			":%s NOTICE %s :There are %d yet unknown connections",
			   me.name, parv[0], u_count);
	if ((c_count = count_channels(sptr))>0)
		sendto_one(sptr, ":%s NOTICE %s :There are %d channels.",
			   me.name, parv[0], count_channels(sptr));
	sendto_one(sptr, ":%s NOTICE %s :I have %d clients and %d servers",
		   me.name, parv[0], m_client, m_server);
	return 0;
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
		   ":%s NOTICE %s :You are no longer marked as being away",
				   me.name, parv[0]);
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
			   ":%s NOTICE %s :You have been marked as being away",
			   me.name, parv[0]);
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
		sendto_one(sptr,":%s %d %s :Connect: Privileged command",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return -1;
	    }

	if (IsLocOp(sptr) && parc > 3) /* Only allow LocOps to make */
	  return 0;                    /* local CONNECTS --SRB      */

	if (hunt_server(cptr,sptr,":%s CONNECT %s %s %s",
		       3,parc,parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
		 ":%s NOTICE %s :Connect: Syntax error--use CONNECT host port",
			   me.name, parv[0]);
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
	  sendto_ops_butone((aClient *)0, &me,
			    ":%s WALLOPS :Remote 'CONNECT %s %s' from %s",
			    me.name,
			    parv[1], parv[2] ? parv[2] : "",
			    get_client_name(sptr,FALSE));

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
			   ":%s NOTICE %s :*** Connecting to %s[%s] activated.",
			   me.name, parv[0], aconf->host, aconf->host);
		break;
	    case -1:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Couldn't connect to %s.",
			   me.name, parv[0], aconf->host);
		break;
	    case -2:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Host %s is unknown.",
			   me.name, parv[0], aconf->host);
		break;
	    default:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connection to %s failed: %s",
			   me.name, parv[0], aconf->host, strerror(retval));
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
	    sendto_one(sptr,":%s NOTICE %s :No origin specified.",
			me.name, parv[0]);
	    return -1;
	  }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	  {
	    if (acptr = find_server(destination,(aClient *)NULL))
		sendto_one(acptr,":%s PING %s %s", parv[0],
			   origin, destination);
	    else
	      {
		sendto_one(sptr, ":%s NOTICE %s :PING No such host (%s) found",
			   me.name, parv[0], destination);
		return -1;
	      }
	  }
	else
	    sendto_one(sptr,":%s PONG %s %s", me.name,
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
	    sendto_one(sptr,":%s NOTICE %s :No origin specified.",
			me.name, parv[0]);
	    return -1;
	  }
	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    if (acptr = find_server(destination,(aClient *)NULL))
		sendto_one(acptr,":%s PONG %s %s",
			   parv[0], origin, destination);
	    else
	      {
		sendto_one(sptr,":%s NOTCE %s :PONG No such host (%s) found",
			   me.name, parv[0], destination);
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
	char *name, *password, *encr;
#ifdef CRYPT_OPER_PASSWORD
	char salt[3];
	extern char *crypt();
#endif /* CRYPT_OPER_PASSWORD */


	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;
	if (!IsClient(sptr) || (BadPtr(name) || BadPtr(password)) &&
	    cptr == sptr )
	    {
		sendto_one(sptr, ":%s %d %s :You are lame.",
			   me.name, ERR_NEEDMOREPARAMS,
			   parv[0]);
		return 0;
	    }
	
	/* 
	 * If message arrived from server, trust it, reset oper if already
	 * oper and parv[1][0] == '-'
	 */

	if ((IsServer(cptr) || IsMe(cptr)) && IsOper(sptr))
		if (!BadPtr(name) && *name == '-')
		    {
			ClearOper(sptr);
			sendto_serv_butone(cptr, ":%s OPER -", parv[0]);
			return 0;
		    }
	/* if message arrived from server, trust it, and set to oper */
	    
	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	    {
		sptr->flags |= FLAGS_SERVNOTICE|FLAGS_OPER|FLAGS_WALLOP;
		sendto_serv_butone(cptr, ":%s OPER", parv[0]);
		if (IsMe(cptr))
		  sendto_one(sptr, ":%s %d %s :Here is your broom... %s",
			     me.name, RPL_YOUREOPER, parv[0],
			     "You are now an IRC janitor.");
		return 0;
	    }
	else if (IsOper(sptr))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr,
				   ":%s %d %s :Lost your broom huh ? Lamer..",
				   me.name, RPL_YOUREOPER, parv[0]);
		return 0;
	    }
	if (!name)
		name = sptr->name;
	if (!(aconf = find_conf_exact(name, sptr->sockhost,
				      (CONF_OPERATOR|CONF_LOCOP)) ))
	    {
		sendto_one(sptr, ":%s %d %s :You dont have a valid passport!",
			   me.name, ERR_NOOPERHOST, parv[0]);
		return 0;
	    }
#ifdef CRYPT_OPER_PASSWORD
        /* use first two chars of the password they send in as salt */

        /* passwd may be NULL. Head it off at the pass... */
        salt[0]='\0';
        if(password != NULL)
	    {
        	salt[0]=aconf->passwd[0];
		salt[1]=aconf->passwd[1];
		salt[2]='\0';
		encr = crypt(password, salt);
	    }
	else
		encr = "";
#else
	encr = password;
#endif  /* CRYPT_OPER_PASSWORD */

	if ((aconf->status & (CONF_OPERATOR | CONF_LOCOP))
	    && StrEq(encr, aconf->passwd))
	    {
		sptr->flags |= FLAGS_SERVNOTICE|FLAGS_WALLOP;
		attach_conf(aconf,sptr);
		if (aconf->status == CONF_LOCOP)
		  SetLocOp(sptr);
		else {
		  SetOper(sptr);
		  sendto_serv_butone(cptr, ":%s OPER", parv[0]);
		}
 		sendto_one(sptr, ":%s %d %s :Here is your broom... %s %s",
 			   me.name, RPL_YOUREOPER, parv[0],
 			   "You are now",
			   IsOper(sptr) ? "a full time IRC Janitor." :
					  "an apprentice IRC Janitor");
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only real lusers know the %s",
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
#ifdef WALL
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
	if (IsServer(cptr))
	sendto_all_butone(IsServer(cptr) ? cptr : NULL, sptr,
			  ":%s WALL :%s", parv[0], message);
	return 0;
    }
#endif
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

#ifndef WALLOPS
	if (!IsServer(cptr))
		return 0;
#endif
	if (BadPtr(message))
	    {
		sendto_one(sptr, ":%s %d %s :Empty WALLOPS message",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
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
	if (hunt_server(cptr,sptr,":%s TIME %s",1,parc,parv) == HUNTED_ISME)
	    sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_TIME,
		   parv[0], me.name, date((long)0));
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
	sendto_ops("Rehashing Server config file (%s)",configfile);
	rehash();
#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
	{
	  register aClient *cptr;
	  extern int find_restrict();
	  char reply[128];
	  
	  for (cptr=client;cptr;cptr=cptr->next)
	    
	    if (MyClient(cptr) && !IsMe(cptr) &&
		find_restrict(cptr->user->host,cptr->user->username,reply))
	      {
		sendto_ops("Restricting %s (%s), closing link",
			   get_client_name(cptr,FALSE),reply);
		sendto_one(cptr,":%s %d :*** %s",me.name,
			   ERR_YOUREBANNEDCREEP,reply);
		return exit_client((aClient *)NULL, cptr, cptr, "");
	      }
	}
#endif
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
m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    { 
	Reg1 aChannel *chptr;
	Reg2 aClient *c2ptr;
	Reg3 Link *link;
	int idx, flag;
	char *para = parc > 1 ? parv[1] : NULL;

	/* Allow NAMES without registering */

	/* First, do all visible channels (public and the one user self is) */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (!BadPtr(para) && !ChanIs(chptr, para))
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

	if (hunt_server(cptr,sptr,":%s ADMIN :%s",1,parc,parv) != HUNTED_ISME)
		return 0;
	if (aconf = find_admin())
	    {
		sendto_one(sptr,
			   ":%s NOTICE %s :### Administrative info about %s",
			   me.name, parv[0], me.name);
		sendto_one(sptr,
			   ":%s NOTICE %s :### %s",
			   me.name, parv[0], aconf->host);
		sendto_one(sptr,
			   ":%s NOTICE %s :### %s",
			   me.name, parv[0], aconf->passwd);
		sendto_one(sptr,
			   ":%s NOTICE %s :### %s",
			   me.name, parv[0], aconf->name);
	    }
	else
		sendto_one(sptr, 
	    ":%s NOTICE %s :No administrative info available about server %s",
			   me.name, parv[0], me.name);
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

	switch (hunt_server(cptr,sptr,":%s TRACE :%s", 1,parc,parv))
	    {
	    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
		sendto_one(sptr, ":%s %d %s Link %s%s %s",
			   me.name, RPL_TRACELINK, parv[0],
			   version, debugmode, parv[1]);
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
			if (IsPerson(acptr2) &&
			    (!IsInvisible(acptr2) || IsOper(sptr)))
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
	    name = get_client_name(acptr,FALSE);
	    if (!doall && matches(parv[1],acptr->name))
	      continue;
	    class = get_client_class(acptr);
	    switch(acptr->status)
	      {
	      case STAT_CONNECTING:
		sendto_one(sptr,
			   ":%s %d %s Try. %d %s", me.name,
			   RPL_TRACECONNECTING, parv[0], class, name);
		break;
	      case STAT_HANDSHAKE:
		sendto_one(sptr,
			   ":%s %d %s H.S. %d %s", me.name,
			   RPL_TRACEHANDSHAKE, parv[0], class, name);
		break;
	      case STAT_ME:
		break;
	      case STAT_UNKNOWN:
		if (IsAnOper(sptr))
		  sendto_one(sptr,
			     ":%s %d %s ???? %d %s", me.name,
			     RPL_TRACEUNKNOWN, parv[0], class, name);
		break;
	      case STAT_CLIENT:
		if (IsOper(sptr)) /* Only opers see users */
		 {
		  if (IsAnOper(acptr))
		    sendto_one(sptr,
				":%s %d %s Oper %d %s", me.name,
				RPL_TRACEOPERATOR, parv[0], class, name);
		  else
		    sendto_one(sptr,
				":%s %d %s User %d %s", me.name,
				RPL_TRACEUSER, parv[0], class, name);
		 }
		break;
	      case STAT_SERVER:
		sendto_one(sptr,
	 		   ":%s %d %s Serv %d %dS %dC %s", me.name,
			   RPL_TRACESERVER, parv[0], class,
			   link_s[i], link_u[i], name);
		break;
	      case STAT_SERVICE:
		if (IsAnOper(sptr))
		    sendto_one(sptr,
				":%s %d %s Service %d %s", me.name,
				RPL_TRACESERVICE, parv[0], class, name);
		break;
	      default: /* ...we actually shouldn't come here... --msa */
		sendto_one(sptr,
			   ":%s %d %s <newtype> 0 %s", me.name,
			   RPL_TRACENEWTYPE, parv[0], name);
		break;
	      }
	  }
	/*
	 * Add these lines to summarize the above which can get rather long
         * and messy when done remotely - Avalon
         */
       	if (!IsAnOper(sptr))
	  return 0;
	for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
	  if (Links(cltmp) > 0)
	    sendto_one(sptr, ":%s %d %s Class %d %d", me.name,
		       RPL_TRACECLASS, parv[0], Class(cltmp), Links(cltmp));
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

	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
		return 0;
#ifndef MOTD
	sendto_one(sptr,
		   ":%s NOTICE %s :*** No message-of-today %s %s",
		   me.name, parv[0], "is available on host", me.name);
#else
	/*
	 * stop NFS hangs...most systems should be able to open a file in
	 * 3 seconds. -avalon (curtesy of wumpus)
	 */
	alarm(3);
	if (!(fptr = fopen(MOTD, "r")))
	    {
		alarm(0);
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Message-of-today is %s %s",
			   me.name, parv[0], "is missing on", me.name);
		return 0;
	    }
	alarm(0);
	sendto_one(sptr, ":%s NOTICE %s :MOTD - %s Message of the Day - ",
		me.name, parv[0], me.name);
	while (fgets(line, 80, fptr))
	    {
		if (tmp = index(line,'\n'))
			*tmp = '\0';
		if (tmp = index(line,'\r'))
			*tmp = '\0';
		sendto_one(sptr,":%s NOTICE %s :MOTD - %s",
			   me.name, parv[0], line);
	    }
	sendto_one(sptr, ":%s NOTICE %s :* End of /MOTD command.",
		   me.name, parv[0]);
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
	char *password, *info, *name, *mask;
  
	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;
	info = parc > 3 ? parv[3] : NULL;

	if (IsRegistered(sptr))
	    {
		Reg1 aClient *acptr;

		if (parc > 1)
			mask = parv[1];
		else
			mask = NULL;
		for (acptr = client; acptr; acptr = acptr->next)
		    {
			if (!IsService(acptr))
				continue;
			if (mask && matches(mask, acptr->name))
				continue;
			sendto_one(sptr,":%s %d %s %s %s",
				   me.name, RPL_SERVICEINFO, parv[0],
				   acptr->name, acptr->info);
		    }
		sendto_one(sptr, ":%s %d %s :End of service list",
			   me.name, RPL_ENDOFSERVICES, parv[0]);
		return 0;
	    }

	if (!IsUnknown(sptr) || (parc < 4 && MyConnect(sptr)) ||
	    !sptr->name[0])
	    {
		sendto_one(sptr, ":%s %d %s :Change balls, please",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }
  
	/* if message arrived from server, trust it, and set to service */

	if (IsServer(cptr))
	    {
		sendto_serv_butone(cptr, ":%s NICK %s %d",
				   parv[0],parv[0],sptr->hopcount+1);
		sendto_serv_butone(cptr, ":%s SERVICE %s * :%s",
				   parv[0],parv[1],info);
		strncpy(sptr->info, info, REALLEN);
		SetService(sptr);
		return 0;
	    }
	if (!name || !(aconf = find_conf_name(name, CONF_SERVICE)))
	    {
		sendto_one(sptr,
			   ":%s %d %s :Only few of mere robots %s",
			   me.name, ERR_NOSERVICEHOST, parv[0],
	       "may try to serve");
		return 0;
	    }
	if ((aconf->status & CONF_SERVICE) && StrEq(password, aconf->passwd))
	    {
		attach_conf(aconf,sptr);
		sendto_one(sptr, ":%s %d %s :Net. %s",
			   me.name, RPL_YOURESERVICE, parv[0],
			   "First service");
		SetService(sptr);
		strncpy(sptr->info, info, REALLEN);
		sendto_serv_butone(cptr, ":%s NICK %s 1", parv[0], parv[0]);
		sendto_serv_butone(cptr, ":%s SERVICE * * :%s",
				   parv[0], info);
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
	char	*output, uhbuf[USERHOST_REPLYLEN], *p;
	aClient	*acptr;
	register	char *s;
	register	int i;

	CheckRegistered(sptr);

	if (parc < 2) {
		sendto_one(sptr, ":%s %d %s :USERHOST not enough parameters",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	}
	output = (char *)MyMalloc(strlen(me.name) + strlen(parv[0]) + 3 +
				  USERHOST_REPLYLEN * 5 + 10);
	sprintf(output, ":%s %d %s :", me.name, RPL_USERHOST, parv[0]);
	for (i = 5, s = strtoken(&p, parv[1], " "); i && s;
	     s = strtoken(&p, NULL, " "), i--)
		if (acptr = find_person(s, (aClient *)NULL)) {
			sprintf(uhbuf, "%s%s=%c%s@%s ",
				acptr->name,
				IsOper(acptr) ? "*" : "",
				(acptr->user->away) ? '-' : '+',
				acptr->user->username,
				acptr->user->host);
			strcat(output, uhbuf);
		}
	sendto_one(sptr, output);
	free(output);
	return 0;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
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

	CheckRegistered(sptr);

	if (parc != 2) {
		sendto_one(sptr, ":%s %d %s :ISON not enough parameters",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	}
	reply = (char *)MyMalloc(strlen(me.name) + strlen(parv[0]) + 3 +
				 strlen(parv[1]) + 10);

	sprintf(reply, ":%s %d %s :", me.name, RPL_ISON, parv[0]);
	for (s = strtoken(&p, parv[1], " "); s; s = strtoken(&p, NULL, " "))
		if (acptr = find_person(s, (aClient *)NULL)) {
			strcat(reply, acptr->name);
			strcat(reply, " ");
		}
	sendto_one(sptr, reply);
	free(reply);
	return 0;
}

static int user_modes[]	     = { FLAGS_OPER, 'o',
				 FLAGS_INVISIBLE, 'i',
				 FLAGS_WALLOP, 'w',
				 FLAGS_SERVNOTICE, 's',
				 0, 0 };

/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
m_umode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	Reg1 int flag;
	Reg2 int *s;
	aClient *acptr;
	int what = 0, setflags = 0;
	char flagbuf[20];
	char **p, *m;

	CheckRegisteredUser(sptr);

	what = MODE_ADD;
	bzero(flagbuf, sizeof(flagbuf));
	if (parc < 2)
	  {
	    sendto_one(sptr, ":%s %d %s :No object name given",
		       me.name, ERR_NEEDMOREPARAMS, parv[0]);
	    return 0;
	  }
	acptr = find_person(parv[1], (aClient *)NULL);
	if (!acptr)
	  {
	    if (MyConnect(sptr))
		sendto_one(sptr,":%s %d %s %s :Object name not found",
			   me.name, ERR_NOSUCHCHANNEL, parv[0], parv[1]);
	    return 0;
	  }
	if (IsServer(sptr) || sptr != acptr || acptr->from != sptr->from)
	  {
	    if (IsServer(cptr))
		sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :MODE for User %s From %s!%s",
				  me.name, parv[1],
				  get_client_name(cptr, FALSE), sptr->name);
	    else
		sendto_one(sptr,":%s %d %s :Cant change mode for other users",
			   me.name, ERR_USERSDONTMATCH, parv[0]);
	    return 0;
	  }
 
	if (parc < 3)
	  {
	    m = flagbuf;
	    *flagbuf = '\0';
	    *m++ = '+';
	    for (s = user_modes; (flag = *s) && (m < flagbuf+18); s += 2)
		if (sptr->flags & flag)
		    *m++ = (char)(*(s+1));
	    sendto_one(sptr, ":%s %d %s %s",
			me.name, RPL_UMODEIS, parv[0], flagbuf);
	    return 0;
	}

	/* find flags already set for user */
	setflags = 0;
	for (s = user_modes; flag = *s; s += 2)
	    if (sptr->flags & flag)
		setflags |= flag;

	for (p = &parv[2]; p && *p; p++ )
	    for (m = *p; *m; m++)
		switch(*m)
		{
		case '+' :
		    what = MODE_ADD;
		    break;
		case '-' :
		    what = MODE_DEL;
		    break;	
		/* we may not get these, but they shouldnt be in default */
		case ' ' :
		case '\n' :
		case '\r' :
		case '\t' :
		    break;
		default :
		    for (s = user_modes; flag = *s; s += 2)
			if (*m == (char)(*(s+1)))
			  {
			    if (what == MODE_ADD)
				sptr->flags |= flag;
			    else
				sptr->flags &= ~flag;	
			    break;
			  }
		    if (flag == 0 && MyConnect(sptr))
			 sendto_one(sptr, ":%s %d %s :Unknown MODE flag",
				    me.name, ERR_UMODEUNKNOWNFLAG, parv[0]);
		    break;
		}
	/* stop users making themselves operators too easily */
	if (!(setflags & FLAGS_OPER) && IsOper(sptr) && !IsServer(cptr))
	    ClearOper(sptr);
	if ((setflags & FLAGS_OPER) && !IsOper(sptr) && MyConnect(sptr))
	    {
		aConfItem *aconf;

		if (aconf = find_conf_host(sptr->confs, sptr->sockhost,
					   CONF_OPERATOR|CONF_LOCOP))
			detach_conf(sptr, aconf);
	    }
	m = flagbuf;
	*m = '\0';
	what = MODE_NULL;
	/*
	 * compare new flags with old flags and send string which will cause
	 * servers to update correctly.
	 */
	for (s = user_modes; flag = *s; s += 2)
		if ((flag & setflags) && !(sptr->flags & flag))
		    {
			if (what == MODE_DEL)
				*m++ = *(s+1);
			else
			    {
				what = MODE_DEL;
				*m++ = '-';
				*m++ = *(s+1);
			    }
		    }
		else if (!(flag & setflags) && (sptr->flags & flag))
		    {
			if (what == MODE_ADD)
				*m++ = *(s+1);
			else
			    {
				what = MODE_ADD;
				*m++ = '+';
				*m++ = *(s+1);
			    }
		    }
	/* send MODE to other servers */
	if (strlen(flagbuf))
		sendto_serv_butone(sptr, ":%s MODE %s %s",
				   parv[0], parv[1], flagbuf);
	/* always send a reply back to user even if it is empty */
	if (MyConnect(sptr))
		sendto_one(sptr, ":%s MODE %s %s",
			   me.name, parv[0], flagbuf);
	return 0;
}

/*
 * send the MODE string for user (acptr) to connection cptr
 * -avalon
 */
void	send_umode(cptr, acptr)
aClient *cptr, *acptr;
{
	char	umode_buf[20];
	Reg1 int *s;
	Reg2 char *m;

	m = umode_buf;
	*m++ = '+';
	*m = '\0';
	for (s = user_modes; *s; s += 2)
		if (acptr->flags & *s)
		    {
			*m++ = (char)(*(s+1));
			*m = '\0';
		    }
	if (umode_buf[1])
		sendto_one(cptr, ":%s MODE %s %s",
			   acptr->name, acptr->name, umode_buf);
}

m_deop(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	aConfItem *aconf;

	CheckRegisteredUser(sptr);

	if (!MyConnect(sptr))
	    return 0;
	aconf = find_conf_host(sptr->confs, sptr->sockhost,
				CONF_OPERATOR|CONF_LOCOP);
	if ((aconf == (aConfItem *)NULL) || !IsOper(sptr))
	  {
	    sendto_one(sptr,":%s %d %s :You cant go back a way you didnt come",
			me.name, ERR_NOPRIVILEGES, parv[0]);
	    return 0;
	  }
	detach_conf(sptr, aconf);
	ClearOper(sptr);
	sendto_one(sptr, ":%s %d %s :You're Nolonger an IRC Janitor",
		   me.name, RPL_NOTOPERANYMORE, parv[0]);
	sendto_serv_butone(cptr, ":%s OPER -", parv[0]);
	return 0;
}
