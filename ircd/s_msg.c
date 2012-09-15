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
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <utmp.h>
#ifdef IDENT
#include "authuser.h"
#include <errno.h>
# if defined(MIPS) || defined(pyr)
extern int errno;   /* stupid mips doesn't include this in errno.h */
# endif /* mips || pyr */
#endif /* IDENT */

extern	aClient	*client, me, *local[];
extern	aClient	*find_server PROTO((char *, aClient *));
extern	aClient	*find_person PROTO((char *, aClient *));
extern	aClient	*find_service PROTO((char *, aClient *));
extern	aClient *find_userhost PROTO((char *, char *, aClient *, int *));
extern	aClient	*get_history PROTO((char *, long));
extern	aClient	*find_client PROTO((char *, aClient *));
extern	aClient	*find_name PROTO((char *, aClient *));
extern	aConfItem *conf, *find_conf PROTO((Link *, char *, int));
extern	aConfItem *find_conf_name PROTO((char *, int)), *find_admin();
extern	aConfItem *find_conf_exact PROTO((char *, char *, int));
extern	aConfItem *find_conf_host PROTO((Link *, char *, int));
extern	anUser	*make_user PROTO((aClient *));
extern	aChannel *channel;
extern	int	connect_server PROTO((aConfItem *));
extern	int	close_connection PROTO((aClient *));
extern	int	check_client PROTO((aClient *, int));
extern	int	find_kill PROTO((aClient *));
extern	char	*debugmode, *configfile;
extern	char	*get_client_name PROTO((aClient *, int));
extern	char	*my_name_for_link PROTO((char *, aConfItem *));
extern	int	highest_fd, debuglevel;
extern	long	nextping;

#ifdef	R_LINES
extern	int	find_restrict PROTO((aClient *));
#endif		

#ifdef IDENT
extern int  auth_fd2();
extern char *auth_tcpuser3();
#endif /* IDENT */

extern	char	*date PROTO((long));
extern	char	*strerror();
extern	void	send_channel_modes PROTO((aClient *, aChannel *));
extern	void	terminate();
extern	Link	*find_user_link PROTO((Link *, aClient *));

void send_umode PROTO((aClient *, aClient *));

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
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
*/
static	aClient *next_client(next, ch)
char	*ch;	/* Null terminated search string (may include wilds) */
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

int	hunt_server(cptr, sptr, command, server, parc, parv)
aClient	*cptr, *sptr;
char	*command, *parv[];
int	server, parc;
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

static	int do_nick_name(nick)
char	*nick;
{
	Reg1 char *ch;

	if (*nick == '-' || isdigit(*nick)) /* first character in [0..9-] */
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

static	int	register_user(cptr,sptr,nick)
aClient	*cptr;
aClient	*sptr;
char	*nick;
    {
	Reg1	int	i;
	Reg2	aClient	*acptr;
        char	*parv[3];
	short	oldstatus = sptr->status;
	aConfItem *aconf;
	anUser	*user = sptr->user;

	sptr->user->last = time((long *)NULL);
	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

	if (MyConnect(sptr))
	  {
	    if (check_client(cptr, CONF_CLIENT))
	      {
		sendto_ops("Received unauthorized connection from %s.",
			   get_client_name(cptr,FALSE));
		return exit_client(cptr, sptr, sptr, "No Authorization");
	      } 
	    det_confs_butmask(cptr, CONF_CLIENT);
	    det_I_lines_butfirst(cptr);
	    if (cptr->confs == (Link *)NULL)
	      {
		sendto_one(sptr, ":%s %d %s :%s",
			   me.name, ERR_NOPERMFORHOST, parv[0],
			   "Your host isn't among the privileged..");
		return exit_client(cptr, sptr, sptr, "Unpriveledged Host");
	      }
	    aconf = cptr->confs->value.aconf;
	    if (!BadPtr(aconf->passwd) && !StrEq(sptr->passwd, aconf->passwd))
	      {
		sendto_one(sptr, ":%s %d %s :%s",
			   me.name, ERR_PASSWDMISMATCH, parv[0],
			   "Only correct words will open this gate..");
		return exit_client(cptr, sptr, sptr, "Bad Password");
	      }
	    /*
	     * following block for the benefit of time-dependent K:-lines
	     */
	    if (find_kill(sptr))
		return exit_client(cptr,sptr,sptr,"K-lined");
#ifdef R_LINES
	    if (find_restrict(sptr))
		return exit_client(cptr, sptr, sptr , "R-lined");
#endif
	    if (IsUnixSocket(sptr))
		strncpyzt(user->host, me.sockhost, HOSTLEN+1);
	    else
		strncpyzt(user->host, sptr->sockhost, HOSTLEN+1);

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
	    sendto_one(sptr, "NOTICE %s :*** This server was created %s",
			nick, creation);
	    m_lusers(sptr, sptr, 1, parv);
	    m_motd(sptr, sptr, 1, parv);
	    nextping = time(NULL);
	  }
	sendto_serv_butone(cptr, "NICK %s %d", nick, sptr->hopcount+1);
	sendto_serv_butone(cptr, ":%s USER %s %s %s :%s", nick,
			   user->username, user->host,
			   user->server, sptr->info);
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (acptr == cptr || !IsServer(acptr))
			continue;
		send_umode(acptr, sptr);
	    }
	if (MyConnect(sptr))
		send_umode(sptr, sptr);

	return 0;
    }

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
*/
int m_nick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char	nick[NICKLEN+2], *s;
	
	if (parc < 2)
	    {
		sendto_one(sptr,":%s %d %s :No nickname given",
			   me.name, ERR_NONICKNAMEGIVEN, parv[0]);
		return 0;
	    }
	if (MyConnect(sptr) && (s = (char *)index(parv[1], '~')))
		*s = '\0';
	strncpyzt(nick, parv[1], NICKLEN+1);
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick) == 0 || IsServer(cptr) && strcmp(nick, parv[1]))
	    {
		sendto_one(sptr,":%s %d %s %s :Erroneus Nickname",
			   me.name, ERR_ERRONEUSNICKNAME, parv[0], parv[1]);

		if (IsServer(cptr))
		    {
			sendto_ops("Bad Nick: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
				   me.name, parv[1], me.name, parv[1],
				   nick, cptr->name);
			if (sptr != cptr) /* bad nick change */
			    {
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					me.name, parv[0], me.name,
					get_client_name(cptr, FALSE),
					parv[0],
					sptr->user?sptr->user->username:"",
					sptr->user?sptr->user->server:
						   cptr->name);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			    }
		    }
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
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
				   me.name,
				   sptr->name,
				   me.name,
				   acptr->from->name,
				   /* NOTE: Cannot use get_client_name
				   ** twice here, it returns static
				   ** string pointer--the other info
				   ** would be lost
				   */
				   get_client_name(cptr, FALSE));
			sptr->flags |= FLAGS_KILLED;
			return exit_client(cptr,sptr,&me,
					   "Nick/Server collision");
		    }
		if ((acptr = find_client(nick,(aClient *)NULL)) == NULL)
			break;  /* No collisions, all clear... */
		/*
		** If acptr == sptr, then we have a client doing a nick
		** change between *equivalent* nicknames as far as server
		** is concerned (user is changing the case of his/her
		** nickname or somesuch)
		*/
		if (acptr == sptr)
		    {
			if (strcmp(acptr->name, nick) != 0)
				/*
				** Allows change of case in his/her nick
				*/
				break; /* -- go and process change */
			else
				/*
				** This is just ':old NICK old' type thing.
				** Just forget the whole thing here. There is
				** no point forwarding it to anywhere,
				** especially since servers prior to this
				** version would treat it as nick collision.
				*/
				return 0; /* NICK Message ignored */
		    }
		/*
		** Note: From this point forward it can be assumed that
		** acptr != sptr (point to different client structures).
		*/
		/*
		** If the older one is "non-person", the new entry is just
		** allowed to overwrite it. Just silently drop non-person,
		** and proceed with the nick. This should take care of the
		** "dormant nick" way of generating collisions...
		*/
		if (IsUnknown(acptr) && MyConnect(acptr))
		    {
			exit_client(cptr, acptr, &me, "Overridden");
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
		/*
		** This seemingly obscure test (sptr == cptr) differentiates
		** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
		*/
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
					   ":%s KILL %s :%s (%s <- %s)",
					   me.name, acptr->name,
					   me.name,
					   acptr->from->name,
					   /* NOTE: Cannot use get_client_name
					   ** twice here, it returns static
					   ** string pointer--the other info
					   ** would be lost
					   */
					   get_client_name(cptr, FALSE));
			acptr->flags |= FLAGS_KILLED;
			return exit_client(cptr,acptr,&me,"Nick collision");
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
		sendto_serv_butone(NULL, /* KILL old from outgoing servers */
				   ":%s KILL %s :%s (%s(%s) <- %s)",
				   me.name, sptr->name,
				   me.name,
				   acptr->from->name,
				   acptr->name,
				   get_client_name(cptr, FALSE));
		sendto_serv_butone(NULL, /* Kill new from incoming link */
			   ":%s KILL %s :%s (%s <- %s(%s))",
			   me.name, acptr->name,
			   me.name,
			   acptr->from->name,
			   get_client_name(cptr, FALSE),
			   sptr->name);
		acptr->flags |= FLAGS_KILLED;
		exit_client(cptr,acptr,&me,"Nick collision(new)");
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
		strcpy(sptr->name, nick);
		if (sptr->user != NULL)
			/*
			** USER already received, now we have NICK.
			** *NOTE* For servers "NICK" *must* precede the
			** user message (giving USER before NICK is possible
			** only for local client connection!). register_user
			** may reject the client and call exit_client for it
			** --must test this and exit m_nick too!!!
			*/
			if (register_user(cptr, sptr, nick) == FLUSH_BUFFER)
				return FLUSH_BUFFER;
	    }
	/*
	**  Finally set new nick name.
	*/
	if (sptr->name[0])
		del_from_client_hash_table(sptr->name, sptr);
	strcpy(sptr->name, nick);
	add_to_client_hash_table(nick, sptr);
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
      {
	if (check_registered(sptr))
	    return 0;
      }
    else if (check_registered_user(sptr))
	    return 0;

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
	if ((acptr = find_person(nick, (aClient *)NULL)) != NULL)
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

	    if (!(s = (char *)rindex(nick, '.')))
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

	if ((server = (char *)index(nick, '@')) &&
	    (acptr = find_server(server + 1, (aClient *)NULL)))
	    {
	    int count;

	    if (!IsMe(acptr))
		{
		sendto_one(acptr,":%s %s %s :%s",parv[0], cmd, nick, parv[2]);
		continue;
		}

	    *server = '\0';

	    if (host = (char *)index(nick, '%'))
		*host++ = '\0';

	    if (acptr = find_userhost(nick, host, (aClient *)NULL, &count))
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

int m_private(cptr, sptr, parc, parv)
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
do_who(sptr, acptr, repchan)
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
int m_who(cptr, sptr, parc, parv)
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
		/*
		 * List all users on a given channel
		 */
		chptr = find_channel(channame, (aChannel *)NULL);
		if (chptr != (aChannel *)NULL)
		    if ((member = IsMember(sptr, chptr)) ||
			  !SecretChannel(chptr))
			for (link = chptr->members; link; link = link->next)
			    {
				if (oper && !IsAnOper(link->value.cptr))
					continue;
				if (IsInvisible(link->value.cptr) && !member)
					continue;
				do_who(sptr, link->value.cptr, chptr);
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
			if (HiddenChannel(chptr) && !isinvis &&
			    !SecretChannel(chptr))
				showperson = 1;
		    }
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		if (showperson)
			do_who(sptr, acptr, ch2ptr);
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
int m_whois(cptr, sptr, parc, parv)
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

    if (check_registered(sptr))
	return 0;

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

	      if (wilds && !MyConnect(acptr) && !MyConnect(sptr))
		continue;
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
int m_user(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#define	UFLAGS	(FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_SERVNOTICE)

	char	*username, *host, *server, *realname;
	anUser	*user;
#ifdef IDENT
        char    *authuser = NULL;
        unsigned long authlocaladdr,authremoteaddr;
        unsigned short authlocalport,authremoteport;
        int authtimeout = AUTHTIMEOUT;
        int errtmp;
#endif /* IDENT */
        
 
	if (parc > 2 && (username = (char *)index(parv[1],'@')))
		*username = '\0'; 
	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :Not enough parameters", 
	    		   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
				   parv[0], get_client_name(cptr, FALSE));
		else
			return 0;
	    }
  
	/* Copy parameters into better documenting variables */
  
	username = (parc < 2 || BadPtr(parv[1])) ? "<bad boy>" : parv[1];
	host = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
	realname = (parc < 5 || BadPtr(parv[4])) ? "<bad realname>" : parv[4];

	if (MyConnect(sptr))
          {
            if (!IsUnknown(sptr))
              {
                sendto_one(sptr,":%s %d %s :Identity problems, eh ?",
                           me.name, ERR_ALREADYREGISTRED, parv[0]);
                return 0;
              }
#ifndef	NO_DEFAULT_INVISIBLE
            sptr->flags |= FLAGS_INVISIBLE;
#endif
            sptr->flags |= (UFLAGS & atoi(host));
            if (IsUnixSocket(sptr))
              host = me.sockhost;
            else
              {
                host = sptr->sockhost;
#ifdef IDENT
                if (auth_fd2(sptr->fd,
                             &authlocaladdr,&authremoteaddr,
                             &authlocalport,&authremoteport) == -1)
                  {
                    errtmp=errno;
                    report_error("authfd2 blew, host %s: %s",sptr);
                    return exit_client(sptr, sptr, sptr, "authfd2 blew");
                  }
                authuser = auth_tcpuser3(authlocaladdr,authremoteaddr,
                                         authlocalport,authremoteport,
                                         authtimeout);
                if (authuser != NULL)
                  username = authuser;
#endif /* IDENT */
              }
            server = me.name;
          }
        
  	user = make_user(sptr);
        user = sptr->user;
	strncpyzt(user->username, username, sizeof(user->username));
#ifdef IDENT
        if ((authuser == NULL) && MyConnect(sptr))
          {
            strcpy(sptr->info, "!id "); /* doesn't need strncpy - constant */
            strncat(sptr->info, realname, sizeof(sptr->info)-5);
          }
        else
#endif /* !IDENT */
          strncpyzt(sptr->info, realname, sizeof(sptr->info));
	strncpyzt(user->server, server, sizeof(user->server));
	strncpyzt(user->host, host, sizeof(user->host));
        
	if (sptr->name[0]) /* NICK already received, now we have USER... */
          return register_user(cptr,sptr,sptr->name);
        
	return 0;
      }

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
int m_version(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
    {
	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s VERSION %s",1,parc,parv)==HUNTED_ISME)
		sendto_one(sptr,":%s %d %s %s.%s %s", me.name, RPL_VERSION,
			   parv[0], version, debugmode, me.name);
	return 0;
    }

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
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
int	m_squit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char	*server;
	aClient *acptr;
	char	*comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

	if (!IsPrivileged(sptr) || IsLocOp(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :'tis is no game for mere mortal souls",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }

	if (parc > 1)
	    {
		server = parv[1];
		/*
		** To accomodate host masking, a squit for a masked server
		** name is expanded if the incoming mask is the same as
		** the server name for that link to the name of link.
		*/
		while (*server == '*')
		    {
			Reg1	aConfItem *aconf;
			aconf = find_conf(cptr->confs, cptr->name,
					  CONF_NOCONNECT_SERVER);
			if (aconf == (aConfItem *)NULL)
				break;
			if (!mycmp(server, my_name_for_link(me.name, aconf)))
				server = cptr->name;
			break; /* WARNING is normal here */
		    }
		/*
		** The following allows wild cards in SQUIT. Only usefull
		** when the command is issued by an oper.
		*/
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
	  {
	    sendto_ops_butone((aClient *)NULL, &me,
		":%s WALLOPS :Received SQUIT %s from %s (%s)",
		me.name, server, get_client_name(sptr,FALSE), comment);
#if defined(USE_SYSLOG) && defined(SYSLOG_SQUIT)
	    syslog(LOG_DEBUG,"SQUIT From %s : %s (%s)",
		   parv[0], server, comment);
#endif
	  }

	return exit_client(cptr, acptr, sptr, comment);
    }

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo/hopcount
**      parv[3] = serverinfo
*/
int	m_server(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
  char	*host, info[REALLEN+1], *inpath;
  aClient *acptr, *bcptr;
  aConfItem *aconf, *bconf;
  Reg1	char *ch;
  Reg2	int i;
  int	hop, split;

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
      if (aconf = find_conf_name(sptr->name, CONF_LEAF))
	  if (aconf->port == 0 || hop >= aconf->port)
	    {
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

  if (check_server(cptr))
    {
      sendto_ops("Received unauthorized connection from %s.",
                 get_client_name(cptr,FALSE));
      return exit_client(cptr, cptr, cptr, "No C/N conf lines");
    }
  inpath = get_client_name(cptr,TRUE); /* "refresh" inpath with host */
  split = mycmp(cptr->name, cptr->sockhost);
  if (!(aconf = find_conf(cptr->confs,host,CONF_NOCONNECT_SERVER)))
    {
      sendto_one(cptr, "ERROR :Access denied. No N field for server %s",
		 inpath);
      sendto_ops("Access denied. No N field for server %s",
		 inpath);
      return exit_client(cptr, cptr, cptr, "No N line for server");
    }
  if (!(bconf = find_conf(cptr->confs,host,CONF_CONNECT_SERVER)))
    {
      sendto_one(cptr, "ERROR :Only N (no C) field for server %s", inpath);
      sendto_ops("Only N (no C) field for server %s",inpath);
      return exit_client(cptr, cptr, cptr, "No C line for server");
    }
  if (*(aconf->passwd) && !StrEq(aconf->passwd, cptr->passwd))
    {
      sendto_one(cptr, "ERROR :Access denied (passwd mismatch) %s",
		 inpath);
      sendto_ops("Access denied (passwd mismatch) %s", inpath);
      return exit_client(cptr, cptr, cptr, "Bad Password");
    }
#ifndef	HUB
  for (i = 0; i <= highest_fd; i++)
    if (local[i] && IsServer(local[i]))
      {
	sendto_one(cptr, "ERROR :I'm a leaf not a hub");
	return exit_client(cptr, cptr, cptr, "I'm a leaf");
      }
#endif
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
  nextping = time(NULL);
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
int	m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aClient *acptr;
	char	*inpath = get_client_name(cptr,FALSE);
	char	*user, *path;
	int	chasing = 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No user specified",
			   me.name, ERR_NEEDMOREPARAMS, parv[0]);
		return 0;
	    }

	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */

#ifdef	OPER_KILL
	if (!IsPrivileged(cptr))
	    {
		sendto_one(sptr,":%s %d %s :Death before dishonor ?",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }
#else
	if (!IsServer(cptr))
	    {
		sendto_one(sptr,":%s %d %s :Death before dishonor ?",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
	    }
#endif

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
	if (IsServer(acptr) || IsMe(acptr))
	    {
		sendto_one(sptr,":%s %d %s :You cant kill a server!",
			   me.name, ERR_NOPRIVILEGES, parv[0]);
		return 0;
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
		if (IsUnixSocket(cptr)) /* Don't use get_client_name syntax */
			inpath = me.sockhost;
		else
			inpath = cptr->sockhost;
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
	sendto_ops("Received KILL message for %s. From %s Path: %s!%s",
		   acptr->name, parv[0], inpath, path);
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
		syslog(LOG_DEBUG,"KILL From %s For %s Path %s!%s",
			parv[0], acptr->name, inpath, path);
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsOper(sptr))
	    {
		sendto_serv_butone(chasing ? NULL : cptr,":%s KILL %s :%s!%s",
				   parv[0], acptr->name, inpath, path);
		acptr->flags |= FLAGS_KILLED;
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
	if (MyConnect(acptr) && MyConnect(sptr) && IsOper(sptr))
		return exit_client(cptr, acptr, sptr, "Local Kill");
	else
		return exit_client(cptr, acptr, sptr, "Killed");
    }

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int m_info(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
    char **text = infotext;

    if (check_registered(sptr))
	return 0;

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
int m_links(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
    char *mask;
    aClient *acptr;

    if (check_registered_user(sptr))
	return 0;
    
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
** As of 2.7.1e, this was the case. -avalon
**
**	parv[0] = sender prefix
**	parv[1] = server
**	parv[2] = user
*/
int m_summon(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char	*host,*user;
#ifdef ENABLE_SUMMON
	char	hostbuf[17], namebuf[10], linebuf[10];
#ifdef LEAST_IDLE
        char linetmp[10], ttyname[15]; /* Ack */
        struct stat stb;
        time_t ltime = (time_t)0;
#endif
	int fd, flag = 0;
#endif

	if (check_registered_user(sptr))
		return 0;
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :(Summon) No recipient given",
			   me.name, ERR_NORECIPIENT,
			   parv[0]);
		return 0;
	    }
	user = parv[1];
	if ((host = (char *)index(user,'@')) == NULL)
		host = me.name;
	else 
		*(host++) = '\0';

	if (BadPtr(host) || matches(host,me.name) == 0)
	    {
#ifdef ENABLE_SUMMON
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr,":%s NOTICE %s :Cannot open %s",
				   me.name, parv[0],UTMP);
			return 0;
		    }
#ifndef LEAST_IDLE
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf,
					 sizeof(hostbuf))) == 0) 
			if (StrEq(namebuf,user))
				break;
#else
                /* use least-idle tty, not the first
                 * one we find in utmp. 10/9/90 Spike@world.std.com
                 * (loosely based on Jim Frost jimf@saber.com code)
                 */
		
                while ((flag = utmp_read(fd, namebuf, linetmp, hostbuf,
					 sizeof(hostbuf))) == 0){
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
#endif /* ENABLE_SUMMON */
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

static int report_array[7][3] = {
		{ CONF_CONNECT_SERVER,    (int)'C', RPL_STATSCLINE},
		{ CONF_NOCONNECT_SERVER,  (int)'N', RPL_STATSNLINE},
		{ CONF_CLIENT,            (int)'I', RPL_STATSILINE},
		{ CONF_KILL,              (int)'K', RPL_STATSKLINE},
		{ CONF_QUARANTINED_SERVER,(int)'Q', RPL_STATSQLINE},
		{ CONF_LEAF,		  (int)'L', RPL_STATSLLINE},
		{ 0, 0, 0}
				};

static int report_configured_links(sptr, mask)
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

int m_stats(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	static	char	Lheading[] = ":%s %d %s %s %s %s %s %s %s :%s";
	static	char	Lformat[]  = ":%s %d %s %s %u %u %u %u %u :%s";
	struct	Message	*mptr;
	aClient	*acptr;
	char	stat = parc > 1 ? parv[1][0] : '\0';
	Reg1	int	i;
	int	doall;
	char	*name = parc > 2 ? parv[2] : me.name;

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!= HUNTED_ISME)
		return 0;

	if (parc > 2)
		if (matches(name, me.name) == 0)
			doall = 1;
		else
			doall = 0;

	switch (stat)
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
			    (doall || !mycmp(acptr->name, name)) &&
			    (IsOper(sptr) || !IsPerson(acptr) ||
			     (sptr == acptr) ||
			     IsLocOp(sptr) && IsLocOp(acptr))) {
				sendto_one(sptr,Lformat,
					   me.name,RPL_STATSLINKINFO,parv[0],
					   (isupper(stat)) ?
					   get_client_name(acptr, TRUE) :
					   get_client_name(acptr, FALSE),
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
				      CONF_NOCONNECT_SERVER|CONF_LEAF);
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
	case 'R' : case 'r' :
#ifdef DEBUGMODE
		send_usage(sptr,parv[0]);
#endif
		break;
	case 'Z' : case 'z' :
#ifdef	DEBUGMODE
		count_memory(sptr, parv[0]);
#endif
		break;
	case 'U' : case 'u' :
	    {
		register long now;

		now = time(0) - me.since;
		sendto_one(sptr,
			   ":%s NOTICE %s :Server Up %d days, %d:%02d:%02d",
			   me.name, parv[0],
			   now/86400, (now/3600)%24, (now/60)%60, now%60);
		break;
	    }
	case 'M' : case 'm' :
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
int m_users(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
#ifdef ENABLE_USERS
    char namebuf[10],linebuf[10],hostbuf[17];
    int fd, flag = 0;
#endif

    if (check_registered_user(sptr))
	return 0;

    if (hunt_server(cptr,sptr,":%s USERS %s",1,parc,parv) == HUNTED_ISME)
	{
#ifdef ENABLE_USERS
	if ((fd = utmp_open()) == -1)
	    {
	    sendto_one(sptr,":%s NOTICE %s Cannot open %s",
			me.name, parv[0], UTMP);
	    return 0;
	    }

	sendto_one(sptr,":%s NOTICE %s :UserID   Terminal  Host",
		me.name, parv[0]);
	while (utmp_read(fd, namebuf, linebuf, hostbuf, sizeof(hostbuf)) == 0)
	    {
	    flag = 1;
	    sendto_one(sptr,":%s NOTICE %s :%-8s %-9s %-8s",
			me.name, parv[0], namebuf, linebuf, hostbuf);
	    }
	if (flag == 0) 
	    sendto_one(sptr,":%s NOTICE %s :Nobody logged in on %s",
			me.name, parv[0], parv[1]);

	utmp_close(fd);
#endif
	}
    return 0;
}

/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions (like
** m_whoreply--check later --msa)
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/
int m_error(cptr, sptr, parc, parv)
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
int m_help(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int i;

	for (i = 0; msgtab[i].cmd; i++)
		sendto_one(sptr,"NOTICE %s :%s",parv[0],msgtab[i].cmd);
	return 0;
    }

int m_restart(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (IsOper(sptr) && MyConnect(sptr))
	    {
#ifdef USE_SYSLOG
		syslog(LOG_WARNING, "Server RESTART by %s\n",
       			get_client_name(sptr,FALSE));
#endif
		restart();
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only Janitors can %s",
			   me.name, ERR_NOPRIVILEGES, parv[0],
			   "emtpy the rubbish bins"); 
	return 0;
    }

int m_die(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (IsOper(sptr) && MyConnect(sptr))
	    {
		sendto_ops("*** %s has pulled the plug on us sir!", parv[0]);
#ifdef USE_SYSLOG
		syslog(LOG_CRIT, "Server DIE by %s\n",
			get_client_name(sptr, FALSE));
#endif
		terminate();
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only those with guns can %s",
			   me.name, ERR_NOPRIVILEGES, parv[0],
			   "shoot to kill.");

	return 0;
    }

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
int m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
	int m_client = 0, m_server = 0;
	aClient *acptr;

	if (check_registered_user(sptr))
	  return 0;

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
#ifdef	SHOW_INVISIBLE_LUSERS
	      if (MyConnect(acptr))
		  m_client++;
	      if (!IsInvisible(acptr))
		c_count++;
	      else
		i_count++;
#else
	      if (MyConnect(acptr))
		{
		  if (IsInvisible(acptr))
		    {
		      if (IsAnOper(sptr))
			  m_client++;
		    }
		  else
		      m_client++;
		}
	      if (!IsInvisible(acptr))
		c_count++;
	      else
		i_count++;
#endif
	      break;
	    default:
	      u_count++;
	      break;
	    }
	 }
#ifndef	SHOW_INVISIBLE_LUSERS
	if (IsAnOper(sptr) && i_count)
#endif
	sendto_one(sptr,
	":%s NOTICE %s :There are %d users and %d invisible on %d servers",
			   me.name, parv[0], c_count, i_count, s_count);
#ifndef	SHOW_INVISIBLE_LUSERS
	else
		sendto_one(sptr,
			":%s NOTICE %s :There are %d users on %d servers",
			   	me.name, parv[0], c_count, s_count);
#endif
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
int	m_away(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char	*away;

	if (check_registered_user(sptr))
		return 0;

	away = sptr->user->away;

	if (parc < 2 || *parv[1] == '\0')
	    {
		/* Marking as not away */

		if (away)
		    {
			free(away);
			sptr->user->away = NULL;
		    }
		sendto_serv_butone(cptr, ":%s AWAY", parv[0]);
		if (MyConnect(sptr))
			sendto_one(sptr,
		   ":%s NOTICE %s :You are no longer marked as being away",
				   me.name, parv[0]);
		return 0;
	    }

	/* Marking as away */

	sendto_serv_butone(cptr, ":%s AWAY :%s", parv[0], parv[1]);

	if (away)
		away = (char *)MyRealloc(away, strlen(parv[1])+1);
	else
		away = (char *)MyMalloc(strlen(parv[1])+1);

	sptr->user->away = away;
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
int m_connect(cptr, sptr, parc, parv)
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
	/*
	** Notify all operators about remote connect requests
	*/
	if (!IsAnOper(cptr))
	 {
	  sendto_ops_butone((aClient *)0, &me,
			    ":%s WALLOPS :Remote 'CONNECT %s %s' from %s",
			    me.name,
			    parv[1], parv[2] ? parv[2] : "",
			    get_client_name(sptr,FALSE));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
	  syslog(LOG_DEBUG, "CONNECT From %s : %s %d",
		 parv[0], parv[1], parv[2] ? parv[2] : "");
#endif
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
int m_ping(cptr, sptr, parc, parv)
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
int m_pong(cptr, sptr, parc, parv)
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
int m_oper(cptr, sptr, parc, parv)
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
		attach_conf(sptr,aconf);
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
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
		syslog(LOG_INFO, "OPER (%s) (%s) by (%s!%s@%s)",
			name, encr,
			parv[0], sptr->user->username, sptr->sockhost);
#endif
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
int m_pass(cptr, sptr, parc, parv)
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
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int m_wallops(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	static	int	wallc = 0;
	char *message = parc > 1 ? parv[1] : NULL;

	if (check_registered(sptr))
		return 0;

	if (!IsServer(sptr))
		return 0;
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
int m_time(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (check_registered_user(sptr))
	    return 0;
	if (hunt_server(cptr,sptr,":%s TIME %s",1,parc,parv) == HUNTED_ISME)
	    sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_TIME,
		   parv[0], me.name, date((long)0));
	return 0;
    }

int m_rehash(cptr, sptr, parc, parv)
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
#ifdef USE_SYSLOG
	syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif

	rehash();

#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
	{
	  Reg1	aClient	*acptr;
	  Reg2	int	i;

	  
	  for (i = 0; i <= highest_fd; i++)
	    
	    if ((acptr = local[i]) && !IsMe(acptr))
	      {
		if (find_restrict(acptr))
		  {
		    sendto_ops("Restricting %s, closing link",
				get_client_name(acptr,FALSE));
		    if (exit_client(cptr, acptr, sptr, "R-lined")
			== FLUSH_BUFFER)
			    /* Oh My! An oper R-lined himself!! :-) --msa */
			    return FLUSH_BUFFER;
		  }
	      }
	}
#endif
	return 0;
    }

/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int m_admin(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aConfItem *aconf;

	if (check_registered(sptr))
		return 0;

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
int m_trace(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	aClass *cltmp;
	int doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	Reg1 int i;
	Reg2 aClient *acptr2;

	if (check_registered(sptr))
		return 0;

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

	if (doall)
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
int m_motd(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int fd;
	char line[80], *tmp;

	if (check_registered(sptr))
		return 0;

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
	if (!(fd = open(MOTD, O_RDONLY)))
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
	while (dgets(fd, line, sizeof(line) - 1))
	    {
		if (tmp = (char *)index(line,'\n'))
			*tmp = '\0';
		if (tmp = (char *)index(line,'\r'))
			*tmp = '\0';
		sendto_one(sptr,":%s NOTICE %s :MOTD - %s",
			   me.name, parv[0], line);
	    }
	sendto_one(sptr, ":%s NOTICE %s :* End of /MOTD command.",
		   me.name, parv[0]);
	close(fd);
#endif
	return 0;
    }

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int m_userhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	char	*output, uhbuf[USERHOST_REPLYLEN], *p;
	aClient	*acptr;
	register	char *s;
	register	int i;

	if (check_registered(sptr))
		return 0;

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

int m_ison(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	char *reply, *p;
	register aClient *acptr;
	register char *s;

	if (check_registered(sptr))
		return 0;

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

#define	SEND_UMODES	(FLAGS_OPER|FLAGS_INVISIBLE|FLAGS_WALLOP)

#ifndef NPATH
int	m_note(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aClient *acptr;
	Reg2	int	i = 0;
	int	wilds = 0;
	char	*c, buf[50];

	if (parc < 2)
		return 0;

	c = parv[1];

	while (*c && *c != ' ' && i < 49)
	    {
        	if (*c == '*' || *c == '?')
			wilds = 1;
	  	buf[i++] = *c++;
	    }

	buf[i] = 0;

	if (IsOper(sptr) && wilds)
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(acptr = local[i]))
				continue;
        		if (IsServer(acptr) && acptr != cptr)
				sendto_one(acptr, ":%s NOTE %s",
					   parv[0], parv[1]);
		    }
	else
		for (acptr = client; acptr; acptr = acptr->next)
			if (IsServer(acptr) && acptr != cptr
			    && !mycmp(buf, acptr->name))
			    {
				sendto_one(acptr, ":%s NOTE %s",
					   parv[0], parv[1]);
				break;
			    }
	return 0;
}
#endif

/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int m_umode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	Reg1 int flag;
	Reg2 int *s;
	aClient *acptr;
	int what, setflags;
	char flagbuf[20];
	char **p, *m;

	if (check_registered_user(sptr))
		return 0;

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
		if ((acptr->flags & *s) && (*s & SEND_UMODES))
		    {
			*m++ = (char)(*(s+1));
			*m = '\0';
		    }
	if (umode_buf[1])
		sendto_one(cptr, ":%s MODE %s %s",
			   acptr->name, acptr->name, umode_buf);
}
