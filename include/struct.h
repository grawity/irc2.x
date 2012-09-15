/************************************************************************
 *   IRC - Internet Relay Chat, include/struct.h
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

/* -- Jto -- 16 Jun 1990
 * String Channel modifications...
 * Gonzo's PRIVMSG enchantments
 */

/* -- Jto -- 03 Jun 1990
 * Added Channel modes...
 */

#include "config.h"

#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */

#define MAXUSERSPERCHANNEL 10 /* 10 is currently recommended. If this is */
                              /* zero or negative, no restrictions exist */
                              /* If you are connected to other ircds, do */
                              /* NOT change this from default without    */
                              /* asking from other irc administrators    */
                              /* first !                                 */

#define HOSTLEN   50  	      /* Length of hostname.  Updated to         */
                              /* comply with RFC1123                     */

#define NICKLEN   9		/* Necessary to put 9 here instead of 10
				** if s_msg.c/m_nick has been corrected.
				** This preserves compatibility with old
				** servers --msa
				*/
#define USERLEN   10
#define REALLEN   50
#define HEADERLEN 200
#define PASSWDLEN 20

#define MAXRECIPIENTS 20
#define CHANNELLEN 50
#define BUFSIZE  256
#define MAXBUFLEN 512

#define STAT_CONNECTING -4
#define STAT_HANDSHAKE -3
#define STAT_ME        -2
#define STAT_UNKNOWN   -1
#define STAT_SERVER     0
#define STAT_CLIENT     1
#define STAT_LOG        2
#define STAT_SERVICE    3      /* Services not implemented yet */
#define STAT_OPER       4      /* Operator */
#define STAT_CHANOP     8      /* Channel operator */

/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define elementsof(x) (sizeof(x)/sizeof(x[0]))

#define IsRegisteredUser(x) ((x)->status > STAT_SERVER)
#define IsRegistered(x) ((x)->status >= STAT_SERVER)
#define IsConnecting(x)	((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)	((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)		((x)->status == STAT_ME)
#define	IsUnknown(x)	((x)->status == STAT_UNKNOWN)
#define	IsServer(x)	((x)->status == STAT_SERVER)
#define	IsClient(x)	((~STAT_CHANOP & (x)->status) == STAT_CLIENT)
#define	IsLog(x)	((x)->status == STAT_LOG)
#define	IsService(x)	((x)->status == STAT_SERVICE)
#define	IsOper(x)	((~STAT_CHANOP & (x)->status) == STAT_OPER)
#define IsPerson(x)	(IsClient(x) || IsOper(x))
#define IsPrivileged(x)	(IsOper(x) || IsServer(x))
#define IsChanOp(x)     ((x)->status > 0 && (x)->status & STAT_CHANOP)

#define	SetHandshake(x)	((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)	((x)->status = STAT_ME)
#define	SetUnknown(x)	((x)->status = STAT_UNKNOWN)
#define	SetServer(x)	((x)->status = STAT_SERVER)
#define	SetClient(x)	((x)->status = STAT_CLIENT)
#define	SetLog(x)	((x)->status = STAT_LOG)
#define	SetService(x)	((x)->status = STAT_SERVICE)
#define	SetOper(x)	((x)->status = (STAT_OPER|((x)->status&STAT_CHANOP)))
#define SetChanOp(x)    ((x)->status |= STAT_CHANOP)

#define ClearChanOp(x)  ((x)->status &= ~STAT_CHANOP)

#define CONF_ILLEGAL          0
/* #define CONF_SKIPME           1 /* not used any more --msa */
#define CONF_CLIENT           2
#define CONF_CONNECT_SERVER   4
#define CONF_NOCONNECT_SERVER 8
/* #define CONF_UPHOST           16 /* not used either --msa */
#define CONF_OPERATOR         32
#define CONF_ME               64
#define CONF_KILL             128
#define CONF_ADMIN            256

#define MATCH_SERVER  1
#define MATCH_HOST    2

#define DEBUG_FATAL  0
#define DEBUG_ERROR  1
#define DEBUG_NOTICE 2
#define DEBUG_DEBUG  3

#define IGNORE_TOTAL    3
#define IGNORE_PRIVATE  1
#define IGNORE_PUBLIC   2

#define FLAGS_PINGSENT   1	/* Unreplied ping sent */
#define FLAGS_DEADSOCKET 2	/* Local socket is dead--Exiting soon */
#define FLAGS_KILLED     4	/* Prevents "QUIT" from being sent for this */

#define FLUSH_BUFFER   -2
#define MAXFD        32
#define UTMP         "/etc/utmp"

#define DUMMY_TERM     0
#define CURSES_TERM    1
#define TERMCAP_TERM   2


typedef struct Confitem
    {
	int status;	/* If CONF_ILLEGAL, delete when no clients */
	int clients;	/* Number of *LOCAL* clients using this */
	char *host;
	char *passwd;
	char *name;
	int port;
	long hold;	/* Hold action until this time (calendar time) */
	struct Confitem *next;
    } aConfItem;

typedef struct User
    {
	char username[USERLEN+1];
	char host[HOSTLEN+1];
	char server[HOSTLEN+1];	/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
	struct Channel *channel;
	struct Channel *invited;
	int refcnt;		/* Number of times this block is referenced */
	char *away;
    } anUser;

typedef struct Client
    {
	struct Client *next;
	short status;		/* Client type */
	char name[HOSTLEN+1];	/* Unique name of the client, nick or host */
	char info[REALLEN+1];	/* Free form additional client information */
	struct User *user;	/* ...defined, if this is a User */
	long lasttime;		/* ...should be only LOCAL clients? --msa */
	long since;		/* When this client entry was created */
	short flags;
	char *history;		/* (controlled by whowas--module) */
	struct Client *from;	/* == self, if Local Client, *NEVER* NULL! */
	int fd;			/* >= 0, for local clients */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int count;		/* Amount of data in buffer */
	char buffer[512];	/* Incoming message buffer */
	dbuf sendQ;		/* Outgoing message queue--if socket full */
	long sendM;		/* Statistics: protocol messages send */
	long sendB;		/* Statistics: total bytes send */
	long receiveM;		/* Statistics: protocol messages received */
	long receiveB;		/* Statistics: total bytes received */
	/*
	*/
	struct Confitem *conf;	/* Configuration record associated */
	char sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char passwd[PASSWDLEN+1];
    } aClient;

#define CLIENT_LOCAL_SIZE sizeof(aClient)
#define CLIENT_REMOTE_SIZE offsetof(aClient,count)

typedef struct Ignore
    {
	char user[NICKLEN+1];
	int flags;
	struct Ignore *next;
    } anIgnore;

typedef struct SMode {
  unsigned char mode;
  int limit;
} Mode;

typedef struct SLink {
  struct SLink *next;
  aClient *user;
} Link;

typedef struct SInvites {
  struct SInvites *next;
  aClient *user;
} Invites;

typedef struct Channel
    {
	struct Channel *nextch;
	Mode mode;
	char topic[CHANNELLEN+1];
	int users;
	Link *members;
	Invites *invites;
	char chname[1];
    } aChannel;

extern char *version, *infotext[];
extern char *generation, *creation;
extern aClient me;   /* ...a bit squeamish about this... --msa */

/* function declarations */

extern aChannel *GetChannel();

extern struct Client *make_client();
extern long getlongtime();

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do { strncpy(x, y, N); x[N-1] = '\0'; } while (0)
#define StrEq(x,y) (!strcmp((x),(y)))

/* Channel Visibility macros */

#define MODE_PRIVATE    0x1
#define MODE_SECRET     0x2
#define MODE_ANONYMOUS  0x4
#define MODE_MODERATED  0x8
#define MODE_TOPICLIMIT 0x10
#define MODE_INVITEONLY 0x20
#define MODE_NOPRIVMSGS 0x40

   /* channel visible */
#define PubChannel(x) ((!x) || ((x)->mode.mode &\
			       (MODE_PRIVATE | MODE_SECRET)) == 0)

  /* name invisible */
#define SecretChannel(x) ((x) && ((x)->mode.mode & MODE_SECRET))

  /* chan num invis */
#define HiddenChannel(x) ((x) && ((x)->mode.mode & MODE_PRIVATE))

#define HoldChannel(x) (!(x))
#define ShowChannel(v,c) (PubChannel(c) ||\
			  (v)->user && (v)->user->channel == (c))

/* #define UnlimChannel(x) ((x) > 0 && (x) < 10)  /* unlim # of users */

/* Misc macros */

#define BadPtr(x) (!(x) || (*(x) == '\0'))

#define MyClient(x) ((x)->fd >= 0)
#define MyConnect(x) ((x)->fd >= 0)
#define IsMagicLink(x) ((x)->fd == -20)
#define SetMagicLink(x) ((x)->fd = -20)

#define LOCAL static

#if HPUX
#define random rand
#define srandom srand
#endif
