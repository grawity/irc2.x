/* $Header: s_msg.c,v 2.4 90/05/04 23:54:44 chelsea Exp $ */

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
 * $Log:	s_msg.c,v $
 * Revision 2.4  90/05/04  23:54:44  chelsea
 * New function m_wallops, bug fixes, reformat of a few things.
 * More comments, code cleaning, etc
 * 
 */

#ifndef lint
static char rcsid[] = "$Header: s_msg.c,v 2.4 90/05/04 23:54:44 chelsea Exp $";
#endif /* not lint */

char s_msg_id[] = "s_msg.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <time.h>
#include "struct.h"
#include "sys.h"
#include <sys/types.h>
#include <sys/file.h>
#if APOLLO
#include <sys/types.h>
#endif
#include <utmp.h>
#include <ctype.h>
#include <stdio.h>
#include "msg.h"
#include "numeric.h"
#include "whowas.h"

#ifndef NULL
#define NULL 0
#endif
#define TRUE 1
#define FALSE 0

extern aClient *client, me;
extern aClient *find_client(), *find_server(), *find_person();
extern aConfItem *find_conf(), *find_admin();
extern aConfItem *conf;
extern aChannel *find_channel();
extern int connect_server();
extern int CloseConnection();
aChannel *channel = (aChannel *) 0;
extern int maxusersperchannel;
extern char *debugmode;
extern char *configfile;

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
		strcpy(nbuf,sptr->name);
		strcat(nbuf, spaced ? " [" : "[");
		strcat(nbuf,sptr->sockhost);
		strcat(nbuf,"]");
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
		if (matches(ch,next->name) == 0)
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
	if (parc <= server || BadPtr(parv[server]))
		return (HUNTED_ISME);
	for (acptr = client;
	     acptr = NextClient(acptr, parv[server]);
	     acptr = acptr->next)
	    {
		if (IsMe(acptr))
			return (HUNTED_ISME);
		if (IsServer(acptr))
		    {
			parv[server] = acptr->name;
			sendto_one(acptr, command, sptr->name,
				   parv[1], parv[2], parv[3], parv[4],
				   parv[5], parv[6], parv[7], parv[8]);
			return(HUNTED_PASS);
		    }
	    }
	sendto_one(sptr, "NOTICE %s :*** No such server (%s).", sptr->name,
		   parv[server]);
	return(HUNTED_NOSUCH);
    }

/*
** 'DoNickName' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (ZERO, if
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
	register char *ch;
	register int length;

	ch = nick;
	length = 0;
	if ((*ch < '0' || *ch > '9') && *ch != '-')
		while ((*ch >= 'A' && *ch <= '}') || /* ASCII assumed --msa */
		       (*ch >= '0' && *ch <= '9') ||
		       (*ch == '-'))
		    {
			++length;
			++ch;
		    }
	*ch = '\0';
	if (length > NICKLEN)
	    {
		nick[NICKLEN] = '\0';
		length = NICKLEN;
	    }
	return(length);
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
static RegisterUser(cptr,sptr,nick)
aClient *cptr;
aClient *sptr;
char *nick;

    {
	sptr->status = STAT_CLIENT;
	sendto_serv_butone(cptr,":%s USER %s %s %s :%s", nick,
			   sptr->user->username, sptr->user->host,
			   sptr->user->server, sptr->info);
	AddHistory(sptr);
	if (MyConnect(sptr))
	    {
		sendto_one(sptr,
		  "NOTICE %s :*** Welcome to the Internet Relay Network, %s",
			   nick, nick);
		sendto_one(sptr,
		  "NOTICE %s :*** Your host is %s, running version %s",
			   nick, GetClientName(&me,FALSE), version);
		sendto_one(sptr,
		  "NOTICE %s :*** This server was created %s\n",
			   nick, creation);
		m_lusers(sptr, sptr, 0, (char **)NULL);
	    }
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
			   me.name, ERR_NONICKNAMEGIVEN, sptr->name);
		return 0;
	    }
	nick = parv[1];
	if (DoNickName(nick) == 0)
	    {
		sendto_one(sptr,":%s %d %s :Erroneus Nickname", me.name,
			   ERR_ERRONEUSNICKNAME, sptr->name);
		return 0;
	    }
	if ((acptr = find_person(nick,(aClient *)NULL)) && 
	    /*
	    ** This allows user to change the case of his/her nick,
	    ** when it differs only in case, but only if we have
	    ** properly defined source client (acptr == sptr).
	    */
	    (strncmp(acptr->name, nick, NICKLEN) == 0 || acptr != sptr))
	    {
		sendto_one(sptr, ":%s %d %s %s :Nickname is already in use.",
			   me.name, ERR_NICKNAMEINUSE,
			   sptr->name, nick);
		if (IsServer(cptr))
		    {
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
			sendto_serv_butone((aClient *)NULL, /* all servers */
					   "KILL %s :%s(%s <- %s)",
					   acptr->name,
					   me.name,
					   acptr->from->name,
					   cptr->name);
			ExitClient((aClient *)NULL, acptr);
		    }
		return 0; /* Duplicate NICKNAME handled */
	    }
	if (IsServer(sptr))
	    {
		/* A server introducing a new client, change source */

		sptr = make_client(cptr);
		sptr->next = client;
		client = sptr;
		sendto_serv_butone(cptr, "NICK %s", nick);
	    }
	else if (sptr->name[0])
	    {
		/*
		** Client just changing his/her nick. If he/she is
		** on a channel, send note of change to all clients
		** on that channel. Propagate notice to other servers.
		*/
		if (IsPerson(sptr))
		    {
			if (!HoldChannel(sptr->user->channel))
				sendto_channel_butserv(sptr->user->channel,
						       ":%s NICK %s",
						       sptr->name, nick);
			AddHistory(sptr);
		    }
		sendto_serv_butone(cptr, ":%s NICK %s", sptr->name, nick);
	    }
	else
	    {
		/* Client setting NICK the first time */

		sendto_serv_butone(cptr, "NICK %s", nick);
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
	strcpy(sptr->name, nick); /* correctness guaranteed by 'DoNickName' */
	return 0;
    }

/*
** CheckRegisteredUser is a macro to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
#define CheckRegisteredUser(x) \
  if (!IsRegisteredUser(x)) \
    { \
	sendto_one(sptr, ":%s %d * :You have not registered as a user", \
		   me.name, ERR_NOTREGISTERED); \
	return -1;\
    }

/*
** CheckRegistered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
#define CheckRegistered(x) \
  if (!IsRegistered(x)) \
    { \
	sendto_one(sptr, ":%s %d * :You have not registered yourself yet", \
		   me.name, ERR_NOTREGISTERED); \
	return -1;\
    }

/*
** m_text
**	parv[0] = sender prefix
**	parv[1] = message text
*/
m_text(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	CheckRegisteredUser(sptr);
	
	if (parc < 2 || *parv[1] == '\0')
		sendto_one(sptr,":%s %d %s :No text to send",
			   me.name, ERR_NOTEXTTOSEND,
			   sptr->name);
	else if (sptr->user->channel != 0)
		sendto_channel_butone(cptr, sptr->user->channel, ":%s MSG :%s",
				      sptr->name, parv[1]);
	else
		sendto_one(sptr,":%s %d %s :You have not joined any channel",
			   me.name, ERR_USERNOTINCHANNEL, sptr->name);
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
	aClient *acptr;
	aChannel *chptr;
	char *nick, *tmp;

	CheckRegisteredUser(sptr);
	
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No recipient given",
			   me.name, ERR_NORECIPIENT, sptr->name);
		return -1;
	    }
	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No text to send",
			   me.name, ERR_NOTEXTTOSEND, sptr->name);
		return -1;
	    }
	for (tmp = parv[1]; (nick = tmp) != NULL && *nick; )
	    {
		if (tmp = index(nick, ','))
			*tmp++ = '\0';
		if (acptr = find_person(nick, (aClient *)NULL))
		    {
			if (MyConnect(sptr) &&
			    acptr->user && acptr->user->away)
				sendto_one(sptr,":%s %d %s %s :%s",
					   me.name, RPL_AWAY,
					   sptr->name, acptr->name,
					   acptr->user->away);
			sendto_one(acptr, ":%s PRIVMSG %s :%s",
				   sptr->name, nick, parv[2]);
		    }
		else if (chptr = find_channel(nick, (aChannel *)NULL))
			sendto_channel_butone(cptr, chptr->channo,
					      ":%s PRIVMSG %d :%s", 
					      sptr->name,
					      chptr->channo, parv[2]);
		else
			sendto_one(sptr, ":%s %d %s %s :No such nickname",
				   me.name, ERR_NOSUCHNICK,
				   sptr->name, nick);
	    }
	return 0;
    }

int m_xtra(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	/* As long as XTRA is implemented the same as PRIVMSG,
	** there is no sense to dublicate code physically.. --msa
	*/
	return m_private(cptr, sptr, parc, parv);
    }

int m_grph(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	/* As long as GRPH is implemented the same as PRIVMSG,
	** there is no sense to dublicate code physically.. --msa
	*/
	return m_private(cptr, sptr, parc, parv);
    }

int m_voice(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	/* As long as VOICE is implemented the same as PRIVMSG,
	** there is no sense to dublicate code physically.. --msa
	*/
	return m_private(cptr, sptr, parc, parv);
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
	aClient *acptr;
	aChannel *chptr;
	char *nick, *tmp;

	CheckRegistered(sptr);
	
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No recipient given",
			   me.name, ERR_NORECIPIENT, sptr->name);
		return -1;
	    }
	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No text to send",
			   me.name, ERR_NOTEXTTOSEND, sptr->name);
		return -1;
	    }
	for (tmp = parv[1]; (nick = tmp) != NULL && *tmp ;)
	    {
		if (tmp = index(nick, ','))
			*(tmp++) = '\0';
		if (acptr = find_person(nick, (aClient *)NULL))
			sendto_one(acptr, ":%s NOTICE %s :%s",
				   sptr->name, nick, parv[2]);
		else if (chptr = find_channel(nick, (aChannel *)NULL))
			sendto_channel_butone(cptr, chptr->channo,
					      ":%s NOTICE %d :%s", 
					      sptr->name,
					      chptr->channo, parv[2]);
		else
			sendto_one(sptr, ":%s %d %s %s :No such nickname",
				   me.name, ERR_NOSUCHNICK,
				   sptr->name, nick);
	    }
	return 0;
    }

/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
*/
m_who(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int i = 0;
	int mychannel = (sptr->user != NULL) ? sptr->user->channel : 0;
	aClient *acptr;
	register char *mask = parc > 1 ? parv[1] : NULL;

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
		i = mychannel;
	else if (mask[1] == '\0' && mask[0] == '0') /* "WHO 0" for irc.el */
		mask = NULL;
	else
		i = atoi(mask);
	if (i)
		mask = NULL; /* Old style "who", no search mask */

	sendto_one(sptr,"WHOREPLY * User Host Server Nickname S :Name");
	for (acptr = client; acptr; acptr = acptr->next)
	    {
		register int repchan;

		if (!IsPerson(acptr))
			continue;
		repchan = acptr->user->channel;
		if (i && repchan != i)
			continue;
		if (repchan != mychannel)
			if (SecretChannel(repchan))
				continue;
			else if (!PubChannel(repchan))
				repchan = 0;
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
			sendto_one(sptr,"WHOREPLY %d %s %s %s %s %c%c :%s",
				   repchan, acptr->user->username,
				   acptr->user->host,
				   acptr->user->server, acptr->name,
				   (acptr->user->away) ? 'G' : 'H',
				   IsOper(acptr) ? '*' : ' ', acptr->info);
	    }
	sendto_one(sptr, ":%s %d %s :* End of /WHO list.", me.name,
		   RPL_ENDOFWHO, sptr->name);
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
	aClient *acptr, *a2cptr;
	int found = 0, wilds;
	int mychannel = sptr->user ? sptr->user->channel : 0;
	register char *nick, *tmp;

	/* Allow use of m_whois without registering */

	if (parc < 2)
	    {
		sendto_one(sptr,":%s %d %s :No nickname specified",
			   me.name, ERR_NONICKNAMEGIVEN, sptr->name);
		return 0;
	    }
	for (tmp = parv[1]; (nick = tmp) != NULL && *tmp; )
	    {
		while (*nick == ' ')
			nick++;
		if (*nick == '\0')
			break;
		tmp = index(nick, ',');
		if (tmp != NULL)
			*tmp++ = '\0';
		wilds = (index(nick,'*') != NULL) || (index(nick,'?') != NULL);
		/*
		**  Show all users matching one mask
		*/
		for (acptr = client;
		     acptr = NextClient(acptr,nick);
		     acptr = acptr->next)
		    {
			static anUser UnknownUser =
			    {
				"*",	/* username */
				"*",	/* host */
				"*",	/* server */
				0,	/* channel */
				NULL,	/* away */
			    };
			register anUser *user;
			
			if (IsServer(acptr) || IsMe(acptr))
				continue;
			user = acptr->user ? acptr->user : &UnknownUser;
			
			/* Secret users are visible only by specifying
			** exact nickname. Wild Card guessing would
			** make things just too easy...  -msa
			*/
			if (SecretChannel(user->channel) && wilds)
				continue;
			++found;
			a2cptr = find_server(user->server, (aClient *)NULL);
			if (PubChannel(user->channel) ||
			    (user->channel == mychannel &&
			     user->channel != 0))
				sendto_one(sptr,":%s %d %s %s %s %s %d :%s",
					   me.name, RPL_WHOISUSER,
					   sptr->name, acptr->name,
					   user->username,
					   user->host,
					   user->channel,
					   acptr->info);
			else
				sendto_one(sptr,":%s %d %s %s %s %s * :%s",
					   me.name, RPL_WHOISUSER,
					   sptr->name, acptr->name,
					   user->username,
					   user->host,
					   acptr->info);
			if (a2cptr)
				sendto_one(sptr,":%s %d %s %s :%s",
					   me.name, 
					   RPL_WHOISSERVER,
					   sptr->name,
					   a2cptr->name,
					   a2cptr->info);
			else
				sendto_one(sptr,":%s %d %s * *", me.name, 
					   RPL_WHOISSERVER, sptr->name);
			if (user->away)
				sendto_one(sptr,":%s %d %s %s :%s",
					   me.name, RPL_AWAY,
					   sptr->name, acptr->name,
					   user->away);
			if (IsOper(acptr))
				sendto_one(sptr,
					   ":%s %d %s %s :is an operator",
					   me.name,
					   RPL_WHOISOPERATOR, sptr->name,
					   acptr->name);
		    }
	    }
	if (found == 0)
	    {
		sendto_one(sptr, ":%s %d %s %s :No such nickname",
			   me.name, ERR_NOSUCHNICK,
			   sptr->name, parv[1]);
		m_whowas(cptr,sptr,parc,parv);
	    }
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
	    *parv[3] == '\0' || *parv[4] == '\0')
	    {
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
	if (MyConnect(sptr))	/* Also implies sptr==ctpr!! --msa */
	    {
		if (!(aconf = find_conf(sptr->sockhost, (aConfItem *)NULL,
					(char *)NULL, CONF_CLIENT)))
		    {
			sendto_one(sptr,
			    ":%s %d * :Your host isn't among the privileged..",
				   me.name,
				   ERR_NOPERMFORHOST);
			return ExitClient(sptr, sptr);
		    } 
		if (!StrEq(sptr->passwd, aconf->passwd))
		    {
			sendto_one(sptr,
			 ":%s %d * :Only correct words will open this gate..",
				   me.name,
				   ERR_PASSWDMISMATCH);
			return ExitClient(sptr, sptr);
		    }
		host = sptr->sockhost;
		if (find_conf(host, (aConfItem *)NULL, username, CONF_KILL))
		    {
			sendto_one(sptr,
				  ":%s %d * :Ghosts are not allowed on irc...",
				   me.name, ERR_YOUREBANNEDCREEP);
			return ExitClient(sptr, sptr);
		    }
	    }
	make_user(sptr);
	strncpyzt(sptr->user->username,username,sizeof(sptr->user->username));
	strncpyzt(sptr->user->server, cptr != sptr ? server : me.name,
		  sizeof(sptr->user->server));
	strncpyzt(sptr->user->host, host, sizeof(sptr->user->host));
	strncpyzt(sptr->info, realname, sizeof(sptr->info));
	if (sptr->name[0])

		/* NICK already received, now we have USER... */

		RegisterUser(cptr,sptr,sptr->name);
	return 0;
    }

/*
** m_list
**	parv[0] = sender prefix
*/
m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char buf2[80];
	aChannel *chptr;
	
	sendto_one(sptr,":%s %d %s :  Channel  : Users  Name",
		   me.name, RPL_LISTSTART, sptr->name);
	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (sptr->user == NULL)
			continue;
		if (ShowChannel(sptr, chptr->channo)) 
			sprintf(buf2,"%d",chptr->channo);
		else
			strcpy(buf2,"*");
		sendto_one(sptr,":%s %d %s %s %d :%s",
			   me.name, RPL_LIST, sptr->name,
			   buf2, chptr->users, chptr->name);
	    }
	sendto_one(sptr,":%s %d %s :End of /LIST",
		   me.name, RPL_LISTEND, sptr->name);
	return 0;
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
	aChannel *chptr;
	
	CheckRegisteredUser(sptr);
	
	chptr = GetChannel(sptr->user->channel);
	if (!chptr)
	    {
		sendto_one(sptr, ":%s %d %s :Bad Craziness", 
			   me.name, RPL_NOTOPIC, sptr->name);
		return 0;
	    }
	
	if (parc < 2 || *parv[1] == '\0')  /* only asking  for topic  */
	    {
		if (chptr->name[0] == '\0')
			sendto_one(sptr, ":%s %d %s :No topic is set.", 
				   me.name, RPL_NOTOPIC, sptr->name);
		else
			sendto_one(sptr, ":%s %d %s :%s",
				   me.name, RPL_TOPIC, sptr->name,
				   chptr->name);
	    } 
	else
	    {
		/* setting a topic */
		sendto_serv_butone(cptr,":%s TOPIC :%s",sptr->name,parv[1]);
		strncpyzt(chptr->name, parv[1], sizeof(chptr->name));
		sendto_channel_butserv(chptr->channo, ":%s TOPIC :%s",
				       sptr->name,
				       chptr->name);
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
	int i;

	CheckRegisteredUser(sptr);
	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :Not enough parameters", me.name,
			   ERR_NEEDMOREPARAMS, sptr->name);
		return -1;
	    }
	
	if (parc < 3)
		i = 0;
	else 
		i = atoi(parv[2]);
	if (i == 0)
		i = sptr->user->channel;
	acptr = find_person(parv[1],(aClient *)NULL);
	if (acptr == NULL)
	    {
		sendto_one(sptr,":%s %d %s %s :No such nickname",
			   me.name, ERR_NOSUCHNICK, sptr->name, parv[1]);
		return 0;
	    }
	if (MyConnect(sptr))
	    {
		sendto_one(sptr,":%s %d %s %s %d", me.name,
			   RPL_INVITING, sptr->name, acptr->name, i);
		/* 'find_person' does not guarantee 'acptr->user' --msa */
		if (acptr->user && acptr->user->away)
			sendto_one(sptr,":%s %d %s %s :%s", me.name,
				   RPL_AWAY, sptr->name, acptr->name,
				   acptr->user->away);
	    }
	sendto_one(acptr,":%s INVITE %s %d",sptr->name, acptr->name, i);
	return 0;
    }

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static aChannel *get_channel(i)
int i;
    {
	register aChannel *chptr = channel;

	if (i <= 0)
		return NULL;

	for ( ; ; chptr = chptr->nextch)
	    {
		if (chptr == NULL)
		    {
			if (!(chptr = (aChannel *) malloc(sizeof(aChannel))))
			    {
				perror("malloc");
				debug(DEBUG_FATAL,
				     "Out of memory. Cannot allocate channel");
				restart();
			    }
			chptr->nextch = channel;
			chptr->channo = i;
			chptr->name[0] = '\0';
			chptr->users = 0;
			channel = chptr;
			break;
		    }
		if (chptr->channo == i)
			break;
	    }
	return chptr;
    }

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty). Currently negative
**  channels don't have the channel block allocated. But,
**  just in case they some day will have that, it's better
**  call this for those too.
*/
static void sub1_from_channel(i)
int i;
    {
	register aChannel *chptr, **pchptr;

	if (i <= 0)
		return;	/* ..dispatch negative channels fast */

	for (pchptr = &channel; chptr = *pchptr; pchptr = &(chptr->nextch))
		if (chptr->channo == i)
		    {
			if (--chptr->users <= 0)
			    {
				*pchptr = chptr->nextch;
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
	/*
	** Extension hook: If the channel name is not a number, pass
	** the message through this server as is, no action here. --msa
	*/
	if ((*ch < '0' || *ch > '9') && *ch != '+' && *ch != '-')
	    {
		sendto_serv_butone(cptr, ":%s CHANNEL %s", sptr->name, ch);
		return 0;
	    }
	i = atoi(ch);
	chptr = get_channel(i);
	if (cptr == sptr && chptr != NULL && is_full(i, chptr->users))
	        {
			sendto_one(sptr,":%s %d %s %d :Sorry, Channel is full.",
				   me.name, ERR_CHANNELISFULL,
				   sptr->name, i);
			return 0;
	   	 }
	    else if (chptr != NULL)
		    ++chptr->users;
	/*
        **  Note: At this point it *is* guaranteed that
	**	if (i > 0), then also (chptr != NULL) and points to
	**	the corresponding channel block. They both exactly
	**	define which channel we are going to. If (i<=0) then
	**	value of chptr is undefined--do not use it.
	*/

	/*
	**  Remove user from the old channel (if any)
	*/
	sendto_serv_butone(cptr, ":%s CHANNEL %d", sptr->name, i);
	if (sptr->user->channel != 0)
	    {
		sub1_from_channel(sptr->user->channel);
		sendto_channel_butserv(sptr->user->channel,
				       ":%s CHANNEL 0", sptr->name);
	    }
	/*
	**  Complete user entry to the new channel (if any)
	*/
	sptr->user->channel = i;
	if (i != 0)
	    {
		/* notify all other users on the new channel */
		sendto_channel_butserv(i, ":%s CHANNEL %d", sptr->name, i);
#ifdef AUTOTOPIC
		/* Note: neg channels don't have a topic--no block allocated */
		if (i > 0 && MyClient(sptr) && IsPerson(sptr)
		    && chptr->name[0] != '\0')
			sendto_one(sptr, ":%s %d %s :%s",
				   me.name, RPL_TOPIC,
				   sptr->name, chptr->name);
#endif
	    }
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
	if (HuntServer(cptr,sptr,":%s VERSION %s",1,parc,parv) != HUNTED_ISME)
		return 0;
	sendto_one(sptr,":%s %d %s %s %s", me.name, RPL_VERSION,
			   sptr->name, version, me.name);
	return 0;
    }

/*
** m_quit
**	parv[0] = sender prefix
*/
m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	return IsServer(sptr) ? 0 : ExitClient(cptr, sptr);
    }

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
*/
m_squit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	char *server;
	aClient *acptr;
	
	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :'tis is no game for mere mortal souls",
			   me.name, ERR_NOPRIVILEGES, sptr->name);
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
	**  Notify local opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsOper(cptr))
		sendto_ops("Received SQUIT %s from %s",server,
			   GetClientName(sptr,FALSE));
	return ExitClient(cptr, acptr);
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
	aClient *acptr;
	aConfItem *aconf, *bconf;
	char *inpath;
	register char *ch;
	
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
		return ExitClient(cptr, cptr);
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
		acptr = make_client(cptr);
		strncpyzt(acptr->name,host,sizeof(acptr->name));
		strncpyzt(acptr->info,info,sizeof(acptr->info));
		acptr->status = STAT_SERVER;
		acptr->next = client;
		client = acptr;
		sendto_serv_butone(cptr,"SERVER %s %s",
				   acptr->name, acptr->info);
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
	strncpyzt(cptr->info, BadPtr(info) ? me.name : info,
		  sizeof(cptr->info));
	inpath = GetClientName(cptr,TRUE); /* "refresh" inpath with host */
	if (!(aconf=find_conf(cptr->sockhost, (aConfItem *)NULL,
			      host,CONF_NOCONNECT_SERVER)))
	    {
		sendto_one(cptr,
			   "ERROR :Access denied (no such server enabled) %s",
			   inpath);
		sendto_ops("Access denied (no such server enabled) %s",inpath);
		return ExitClient(cptr, cptr);
	    }
	if (!(bconf=find_conf(cptr->sockhost, (aConfItem *)NULL, host,
			      CONF_CONNECT_SERVER)))
	    {
		sendto_one(cptr, "ERROR :Only N field for server %s", inpath);
		sendto_ops("Only N field for server %s",inpath);
		return ExitClient(cptr, cptr);
	    }
	if (*(aconf->passwd) && !StrEq(aconf->passwd, cptr->passwd))
	    {
		sendto_one(cptr, "ERROR :Access denied (passwd mismatch) %s",
			   inpath);
		sendto_ops("Access denied (passwd mismatch) %s", inpath);
		return ExitClient(cptr, cptr);
	    }
	if (IsUnknown(cptr))
	    {
		if (bconf->passwd[0])
			sendto_one(cptr,"PASS %s",bconf->passwd);
		sendto_one(cptr,"SERVER %s %s", me.name, 
			   (me.info[0]) ? (me.info) : "Master");
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
	SetServer(cptr);
	sendto_ops("Link with %s established.", inpath);
	sendto_serv_butone(cptr,"SERVER %s %s", inpath, cptr->info);
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
	inpath = NULL; /* Just to be sure it's not used :) --msa */
	for (acptr = client; acptr; acptr = acptr->next)
	    {
		if (!IsServer(acptr) || acptr == cptr || IsMe(acptr))
			continue;
		sendto_one(cptr,"SERVER %s %s",
			   GetClientName(acptr,TRUE), acptr->info);
	    }
	/*
	** Second, pass non-server clients.
	*/
	for (acptr = client; acptr; acptr = acptr->next)
	    {
		if (IsServer(acptr) || acptr == cptr || IsMe(acptr))
			continue;
		if (acptr->name[0] == '\0')
			continue; /* Yet unknowns cannot be passed */
		sendto_one(cptr,"NICK %s",acptr->name);
		if (!IsPerson(acptr))
			continue; /* Not fully introduced yet... */
		sendto_one(cptr,":%s USER %s %s %s :%s", acptr->name,
			   acptr->user->username, acptr->user->host,
			   acptr->user->server, acptr->info);
		sendto_one(cptr,":%s CHANNEL %d",acptr->name,
			   acptr->user->channel);
		if (IsOper(acptr))
			sendto_one(cptr,":%s OPER",acptr->name);
		if (acptr->user->away != NULL)
			sendto_one(cptr,":%s AWAY %s", acptr->name,
				   acptr->user->away);
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

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,":%s %d %s :No user specified",
			   me.name, ERR_NEEDMOREPARAMS,
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
		return 0;
	    }
	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */
	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr,":%s %d %s :Death before dishonor ?",
			   me.name, ERR_NOPRIVILEGES,
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
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
				   me.name, ERR_NOSUCHNICK, sptr->name,
				   user);
			return 0;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   me.name, sptr->name, user, acptr->name);
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
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	*/
	sendto_serv_butone(chasing ? (aClient *)NULL : cptr,
			   ":%s KILL %s :%s!%s",
			   sptr->name, acptr->name,inpath, path);
	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_one(acptr,":%s KILL %s :%s!%s",
			   sptr->name, acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents ExitOneClient from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	acptr->flags |= FLAGS_KILLED;
	return ExitClient(cptr, acptr);
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

	if (HuntServer(cptr,sptr,":%s INFO %s",1,parc,parv)!= HUNTED_ISME)
		return 0;

	sendto_one(sptr, "NOTICE %s :***", sptr->name);
	while (*text)
	    {
		sendto_one(sptr, "NOTICE %s :*** %s", sptr->name, *text);
		text++;
	    }
	sendto_one(sptr, "NOTICE %s :***", sptr->name);
	sendto_one(sptr, "NOTICE %s :*** This server was created %s, compile # %s",
		sptr->name, creation, generation);
	sendto_one(sptr, "NOTICE %s :*** On-line since %s",
		sptr->name, ctime((long *)&cptr->since));
	sendto_one(sptr, "NOTICE %s :*** End of /INFO list.", sptr->name);
	return 0;
    }

/*
** m_links
**	parv[0] = sender prefix
**	parv[1] = servername mask
*/
m_links(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	aClient *acptr;
	char *mask = parc < 2 ? NULL : parv[1];

	CheckRegisteredUser(sptr);
	
	for (acptr = client; acptr; acptr = acptr->next) 
		if ((IsServer(acptr) || IsMe(acptr)) &&
		    (BadPtr(mask) || matches(mask, acptr->name) == 0))
			sendto_one(sptr,"LINREPLY %s %s", acptr->name,
				   (acptr->info[0] ? acptr->info :
				    "(Unknown Location)"));
	sendto_one(sptr, ":%s %d %s :* End of /LINKS list.", me.name,
		   RPL_ENDOFLINKS, sptr->name);
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
			   sptr->name);
		return 0;
	    }
	user = parv[1];
	if ((host = index(user,'@')) == NULL)
		host = me.name;
	else 
		*(host++) = '\0';

	if (BadPtr(host) || mycmp(host,me.name) == 0)
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
			sendto_one(sptr,"NOTICE %s Cannot open %s",
				   sptr->name,UTMP);
			return 0;
		    }
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf)) == 0) 
			if (StrEq(namebuf,user))
				break;
		utmp_close(fd);
		if (flag == -1)
			sendto_one(sptr,"NOTICE %s :User %s not logged in",
				   sptr->name, user);
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
		sendto_one(sptr, "ERROR :SUMMON No such host (%s) found",
			   host);
	else
		sendto_one(acptr, ":%s SUMMON %s@%s",sptr->name, user, host);
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
*/
m_stats(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	struct Message *mptr;
	aClient *acptr;
	char *stat = parc > 1 ? parv[1] : "M";
	/*
	** Server should not format things, but this a temporary kludge
	*/
	static char Lheading[] = "NOTICE %s :%-15.15s%5s%7s%10s%7s%10s %s";
	static char Lformat[]  = "NOTICE %s :%-15.15s%5u%7u%10u%7u%10u %s";
	
	if (HuntServer(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!= HUNTED_ISME)
		return 0;

	if (*stat == 'L' || *stat == 'l')
	    {
		/*
		** The following is not the most effiecient way of
		** generating the heading, buf it's damn easier to
		** get the field widths correct --msa
		*/
		sendto_one(sptr,Lheading,sptr->name,
			   "Link", "SendQ", "SendM", "SendBytes",
			   "RcveM", "RcveBytes", "Open since");
		for (acptr = client; acptr; acptr = acptr->next)
			if (MyConnect(acptr) && /* Includes IsMe! */
			    (IsOper(sptr) || !IsPerson(acptr)))
				sendto_one(sptr,Lformat,
					   sptr->name,
					   GetClientName(acptr,FALSE),
					   (int)dbuf_length(&acptr->sendQ),
					   (int)acptr->sendM,
					   (int)acptr->sendB,
					   (int)acptr->receiveM,
					   (int)acptr->receiveB,
					   ctime((long *)&acptr->since));
	    }
	else
		for (mptr = msgtab; mptr->cmd; mptr++)
			sendto_one(sptr,
			  "NOTICE %s :%s has been used %d times after startup",
				   sptr->name, mptr->cmd, mptr->count);
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

	if (HuntServer(cptr,sptr,":%s USERS %s",1,parc,parv) != HUNTED_ISME)
		return 0;

	if ((fd = utmp_open()) == -1)
	    {
		sendto_one(sptr,"NOTICE %s Cannot open %s",
			   sptr->name,UTMP);
		return 0;
	    }
	sendto_one(sptr,"NOTICE %s :UserID   Terminal  Host", sptr->name);
	while (utmp_read(fd, namebuf, linebuf, hostbuf) == 0)
	    {
		flag = 1;
		sendto_one(sptr,"NOTICE %s :%-8s %-9s %-8s",
			   sptr->name, namebuf, linebuf, hostbuf);
	    }
	if (flag == 0) 
		sendto_one(sptr,"NOTICE %s :Nobody logged in on %s",
			   sptr->name, parv[1]);
	utmp_close(fd);
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
int ExitClient(cptr, sptr)
aClient *cptr;	/*
		** The local client originating the exit or NULL, if this
		** exit is generated by this server for internal reasons.
		** This will not get any of the generated messages.
		*/
aClient *sptr; /* Client exiting */
    {
	register aClient *acptr, *next;
	extern detach_conf();

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
				ExitOneClient(cptr, acptr);
		    }
		/*
		** Second SQUIT all servers behind this link
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				ExitOneClient(cptr, acptr);
		    }
	    }

	ExitOneClient(cptr, sptr);
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static ExitOneClient(cptr, sptr)
aClient *sptr;
aClient *cptr;
    {
	aClient *acptr, **pacptr;
	char *from = (cptr == NULL) ? me.name : cptr->name;
	int i;
	
	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from)
	*/
	if (IsMe(sptr))
		return 0;	/* ...must *never* exit self!! */
	else if (IsServer(sptr))
		sendto_serv_butone(cptr,"SQUIT %s :%s!%s",sptr->name,
				   from, me.name);
	else if (IsHandshake(sptr) || IsConnecting(sptr))
		 ; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
			sendto_serv_butone(cptr,":%s QUIT :%s!%s",sptr->name,
					   from, me.name);
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user && (i = sptr->user->channel) != 0)
		    {
			sptr->user->channel = 0;
			sendto_channel_butserv(i,":%s QUIT :%s!%s",
					       sptr->name,
					       from, me.name);
			sub1_from_channel(i);
		    }
	    }

	/* remove sptr from the client list */

	for (pacptr = &client; acptr = *pacptr; pacptr = &(acptr->next))
		if (acptr == sptr)
		    {
			*pacptr = sptr->next;
			if (sptr->user != NULL)
			    {
				OffHistory(sptr);
				free_user(sptr->user);
			    }
			free(sptr);
			return 0;
		    }
	debug(DEBUG_FATAL, "List corrupted");
	restart();
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
	if (IsPerson(cptr) || IsUnknown(cptr))
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
		sendto_one(sptr,"NOTICE %s :%s",sptr->name,msgtab[i].cmd);
	return 0;
    }

m_whoreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	m_error(cptr, sptr, parc, parv); /* temp kludge --msa */
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
	if (IsOper(sptr) && MyConnect(sptr))
		exit(-1);
    }

m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
	aClient *acptr;
	
	for (acptr = client; acptr; acptr = acptr->next)
		switch (acptr->status)
		    {
		    case STAT_SERVER:
		    case STAT_ME:
			s_count++;
			break;
		    case STAT_OPER:
			/* if (acptr->user->channel >= 0 || IsOper(sptr)) */
			o_count++;
		    case STAT_CLIENT:
			if (acptr->user->channel >= 0)
				c_count++;
			else
				i_count++;
			break;
		    default:
			u_count++;
			break;
		    }
	if (IsOper(sptr) && i_count)
		sendto_one(sptr,
	"NOTICE %s :*** There are %d users (and %d invisible) on %d servers",
			   sptr->name, c_count, i_count, s_count);
	else
		sendto_one(sptr,
			   "NOTICE %s :*** There are %d users on %d servers",
			   sptr->name, c_count, s_count);
	if (o_count)
		sendto_one(sptr,
		   "NOTICE %s :*** %d user%s connection to the twilight zone",
			   sptr->name, o_count,
			   (o_count > 1) ? "s have" : " has");
	if (u_count > 0)
		sendto_one(sptr,
			"NOTICE %s :*** There are %d yet unknown connections",
			   sptr->name, u_count);
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

		sendto_serv_butone(cptr, ":%s AWAY", sptr->name);
		if (MyConnect(sptr))
			sendto_one(sptr,
			   "NOTICE %s :You are no longer marked as being away",
				   sptr->name);
		return 0;
	    }

	/* Marking as away */

	sendto_serv_butone(cptr, ":%s AWAY :%s", sptr->name, parv[1]);
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
			   sptr->name);
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
	extern char *sys_errlist[];

	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr,"NOTICE %s :Connect: Privileged command",
			   sptr->name);
		return -1;
	    }

	if (HuntServer(cptr,sptr,":%s CONNECT %s %s %s",
		       3,parc,parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr,
		 "NOTICE %s :Connect: Syntax error--use CONNECT host port",
			   sptr->name);
		return -1;
	    }

	/*
	** Notify local operator(s) about remote connect requests
	*/
	if (!IsOper(cptr))
		sendto_ops("Remote 'CONNECT %s %s' from %s",
			   parv[1], parv[2] ? parv[2] : "",
			   GetClientName(sptr,FALSE));

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status == CONF_CONNECT_SERVER &&
		    matches(parv[1],aconf->name) == 0)
			break;
	if (aconf == NULL)
	    {
		sendto_one(sptr,
			  "NOTICE %s :Connect: Host %s not listed in irc.conf",
			   sptr->name, parv[1]);
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
				   sptr->name);
			return -1;
		    }
	    }
	else if (port <= 0 && (port = PORTNUM) <= 0)
	    {
		sendto_one(sptr,"NOTICE %s :Connect: Port number required",
			   sptr->name);
		return -1;
	    }
	aconf->port = port;
	switch (retval = connect_server(aconf))
	    {
	    case 0:
		sendto_one(sptr,
			   "NOTICE %s :*** Connection to %s established.",
			   sptr->name, aconf->host);
		break;
	    case -1:
		sendto_one(sptr,
			   "NOTICE %s :*** Couldn't connect to %s.",
			   sptr->name, conf->host);
		break;
	    case -2:
		sendto_one(sptr,
			   "NOTICE %s :*** Host %s is unknown.",
			   sptr->name, aconf->host);
		break;
	    default:
		sendto_one(sptr,
			   "NOTICE %s :*** Connection to %s failed: %s",
			   sptr->name, aconf->host,
			   sys_errlist[retval]);
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
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
		return 0;
	    }
	
	/* if message arrived from server, trust it, and set to oper */

	if (IsServer(cptr))
	    {
		SetOper(sptr);
		sendto_serv_butone(cptr, ":%s OPER", sptr->name);
		return 0;
	    }
	if (!(aconf = find_conf(cptr->sockhost, (aConfItem *)NULL, name,
				CONF_OPERATOR)) &&
	    !(aconf = find_conf(cptr->name, (aConfItem *)NULL, name,
				CONF_OPERATOR)))
	    {
		sendto_one(sptr,
			   ":%s %d %s :Only few of mere mortals may try to %s",
			   me.name, ERR_NOOPERHOST, sptr->name,
			   "enter twilight zone");
		return 0;
	    } 
	if (aconf->status == CONF_OPERATOR && StrEq(password, aconf->passwd))
	    {
 		sendto_one(sptr, ":%s %d %s :Good afternoon, gentleman. %s",
 			   me.name, RPL_YOUREOPER, sptr->name,
 			   "I am a HAL 9000 computer.");
		sendto_one(sptr,
		  "NOTICE %s :*** Operator privileges are now active.",
			    sptr->name);
		SetOper(sptr);
		sendto_serv_butone(cptr, ":%s OPER", sptr->name);
	    }
	else
		sendto_one(sptr, ":%s %d %s :Only real wizards do know the %s",
			   me.name, ERR_PASSWDMISMATCH, sptr->name,
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
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
		return 0;
	    }
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	    {
		sendto_one(cptr,
			   ":%s %d %s :Trying to unlock the door twice, eh ?",
			   me.name, ERR_ALREADYREGISTRED,
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
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

	if (!IsOper(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d %s :Only real wizards know the correct %s",
			   me.name, ERR_NOPRIVILEGES, 
			   (sptr->name[0] == '\0') ? "*" : sptr->name,
			   "spells to open this gate");
		return 0;
	    }
	if (BadPtr(message))
	    {
		sendto_one(sptr, ":%s %d %s :Empty WALL message",
			   me.name, ERR_NEEDMOREPARAMS, sptr->name);
		return 0;
	    }
	sendto_all_butone(IsServer(cptr) ? cptr : NULL,
			  ":%s WALL :%s", sptr->name, message);
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

	CheckRegisteredUser(sptr);
	
	if (BadPtr(message))
	    {
		sendto_one(sptr, ":%s %d %s :Empty WALLOPS message",
			   me.name, ERR_NEEDMOREPARAMS, sptr->name);
		return 0;
	    }
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL,
			":%s WALLOPS :%s", sptr->name, message);
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
	if (HuntServer(cptr,sptr,":%s TIME %s",1,parc,parv) != HUNTED_ISME)
		return 0;
	sendto_one(sptr,":%s %d %s %s :%s", me.name, RPL_TIME,
		   sptr->name, me.name, date());
	return 0;
    }

m_rehash(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	if (!IsOper(sptr) && MyConnect(sptr))
	    {
		sendto_one(sptr,":%s %d %s :Use the force, Luke !",
			   me.name, ERR_NOPRIVILEGES,
			   (sptr->name[0] == '\0') ? "*" : sptr->name);
		return -1;
	    }
	sendto_one(sptr,":%s %d %s :Rehashing %s",
		   me.name, RPL_REHASHING, sptr->name, configfile);
	rehash();
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
	int mychannel = (sptr->user != NULL) ? sptr->user->channel : 0;
	char *para = parc > 1 ? parv[1] : NULL;

	/* Allow NAMES without registering */

	/* First, do all visible channels (public and the one user self is) */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (!BadPtr(para) && (chptr->channo != chan_conv(para)))
			continue; /* -- wanted a specific channel */
		if (!PubChannel(chptr->channo) && (chptr->channo != mychannel))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		if (!PubChannel(chptr->channo))
			sprintf(buf, "* %d ", chptr->channo);
		else
			sprintf(buf, "= %d ", chptr->channo);
		idx = strlen(buf);
		flag = 0;
		for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
		    {
			if (!IsPerson(c2ptr) ||
			    (chptr->channo != c2ptr->user->channel))
				continue;
			strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			strcat(buf," ");
			if (idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, "NAMREPLY %s", buf);
				if (!PubChannel(chptr->channo))
					sprintf(buf, "* %d ", chptr->channo);
				else
					sprintf(buf, "= %d ", chptr->channo);
				idx = strlen(buf);
				flag = 0;
			    }
		    }
		if (flag)
			sendto_one(sptr, "NAMREPLY %s", buf);
	    }
	if (!BadPtr(para)) {
	  sendto_one(sptr, ":%s %d %s :* End of /NAMES list.", me.name,
		     RPL_ENDOFNAMES, sptr->name);
	  return(1);
	}

	/* Second, do all non-public, non-secret channels in one big sweep */

	strcpy(buf, "* * ");
	idx = strlen(buf);
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
		if (!IsPerson(c2ptr))
			continue;
		if (mychannel != 0 && mychannel == c2ptr->user->channel)
		    {
			if (mychannel > 0)
				continue; /* Was handled in previous loop */
			/* ..but show nicks on same neg channnel. Previous
			** loop cannot show them, because neg channels
			** don't currently have Channel structure allocated!
			*/
		    }
		else if (!HiddenChannel(c2ptr->user->channel))
			/*
			** Note: HiddenChannel includes channel 0 by current
			** definition--watch out if that is changed --msa
			*/
			continue;
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
		   RPL_ENDOFNAMES, sptr->name);
	return(1);
    }

m_namreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	m_error(cptr, sptr, parc, parv); /* temp. kludge --msa */
    }

m_linreply(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
    {
	m_error(cptr, sptr, parc, parv); /* temp. kludge --msa */
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
			   sptr->name, me.name);
		sendto_one(sptr,
			   "NOTICE %s :### %s", sptr->name, aconf->host);
		sendto_one(sptr,
			   "NOTICE %s :### %s", sptr->name, aconf->passwd);
		sendto_one(sptr,
			   "NOTICE %s :### %s", sptr->name, aconf->name);
	    }
	else
		sendto_one(sptr, 
	    "NOTICE %s :### No administrative info available about server %s",
			   sptr->name, me.name);
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

	if (!IsOper(sptr))
	    {
		sendto_one(sptr, "NOTICE %s :*** Error: %s", sptr->name,
			"No mere mortals may trace the nets of the universe");
		return -1;
	    }
	switch (HuntServer(cptr,sptr,":%s TRACE :%s", 1,parc,parv))
	    {
	    case HUNTED_PASS: /* note: gets here only if parv[1] exists */
		sendto_one(sptr, "NOTICE %s :*** Link %s<%s>%s ==> %s",
			   sptr->name, me.name, version, debugmode, parv[1]);
		return 0;
	    case HUNTED_ISME:
		break;
	    default:
		return 0;
	    }
	
	/* report all direct connections */
	
	for (acptr = client; acptr; acptr = acptr->next)
	    {
		char *name;

		if (acptr->from != acptr) /* Local Connection? */
			continue;
		name = GetClientName(acptr,FALSE);
		switch(acptr->status)
		    {
		    case STAT_CONNECTING:
			sendto_one(sptr,
				   "NOTICE %s :*** Connecting %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    case STAT_HANDSHAKE:
			sendto_one(sptr,
				   "NOTICE %s :*** Handshake %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    case STAT_ME:
			break;
		    case STAT_UNKNOWN:
			sendto_one(sptr,
				   "NOTICE %s :*** Unknown %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    case STAT_CLIENT:
		    case STAT_OPER:
			if (!IsOper(sptr))
				break; /* Only opers see users */
			sendto_one(sptr,
				   "NOTICE %s :*** User %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    case STAT_SERVER:
			sendto_one(sptr,
				   "NOTICE %s :*** Connection %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    default:
			/* ...we actually shouldn't come here... --msa */
			sendto_one(sptr,
				   "NOTICE %s :*** <newtype> %s ==> %s",
				   sptr->name, me.name, name);
			break;
		    }
	    }
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
			sptr->name, me.name);
#else
	if (!(fptr = fopen(MOTD, "r")))
	    {
		sendto_one(sptr,
			"NOTICE %s :*** Message-of-today is missing on host %s",
				sptr->name, me.name);
		return 0;
	    } 
	sendto_one(sptr, "NOTICE %s :MOTD - %s Message of the Day - ",
		sptr->name, me.name);
	while (fgets(line, 80, fptr))
	    {
		if (tmp = index(line,'\n'))
			*tmp = '\0';
		if (tmp = index(line,'\r'))
			*tmp = '\0';
		sendto_one(sptr,"NOTICE %s :MOTD - %s", sptr->name, line);
	    }
	sendto_one(sptr, "NOTICE %s :* End of /MOTD command.", sptr->name);
	fclose(fptr);
#endif
	return 0;
    }
