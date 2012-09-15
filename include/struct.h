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

#include "config.h"

#include <sys/types.h>
#include <netinet/in.h>
#ifdef USE_SYSLOG
#include <sys/syslog.h>
#endif

typedef struct ConfItem aConfItem;
typedef struct Client aClient;
typedef struct Channel aChannel;
typedef struct User anUser;

#ifndef VMSP
#include "class.h"
#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

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
#define USERLEN   	10
#define REALLEN   	50
#define CHANNELLEN   	50
#define HEADERLEN 	200
#define PASSWDLEN 	20

#define USERHOST_REPLYLEN       (NICKLEN+HOSTLEN+USERLEN+5)

#define MAXRECIPIENTS 	20

#ifdef USE_SERVICES
#include "service.h"
#endif

#define STAT_MASTER     -5    /* Local ircd master before identification */
#define STAT_CONNECTING -4
#define STAT_HANDSHAKE -3
#define STAT_ME        -2
#define STAT_UNKNOWN   -1
#define STAT_SERVER     0
#define STAT_CLIENT     1
#define STAT_LOG        2
#define STAT_SERVICE    4      /* Services not implemented yet */

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

#define IsRegisteredUser(x) ((x)->status == STAT_CLIENT)
#define IsRegistered(x) ((x)->status >= STAT_SERVER)
#define IsConnecting(x)	((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)	((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)		((x)->status == STAT_ME)
#define	IsUnknown(x)	((x)->status == STAT_UNKNOWN || (x)->status == STAT_MASTER)
#define	IsServer(x)	((x)->status == STAT_SERVER)
#define	IsClient(x)	((x)->status == STAT_CLIENT)
#define	IsLog(x)	((x)->status == STAT_LOG)
#define	IsService(x)	((x)->status == STAT_SERVICE)

#define	IsOper(x)	((x)->flags & FLAGS_OPER)
#define IsLocOp(x)      ((x)->flags & FLAGS_LOCOP)
#define IsInvisible(x)  ((x)->flags & FLAGS_INVISIBLE)
#define IsAnOper(x)     ((x)->flags & (FLAGS_OPER|FLAGS_LOCOP))
#define IsPerson(x)	IsClient(x)
#define IsPrivileged(x)	(IsOper(x) || IsServer(x))
#define SendWallops(x)  ((x)->flags & FLAGS_WALLOP)
#define SendServNotice(x) ((x)->flags & FLAGS_SERVNOTICE)
#define	IsUnixSocket(x)	((x)->flags & FLAGS_UNIX)

#define SetMaster(x)    ((x)->status = STAT_MASTER)
#define SetConnecting(x) ((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)	((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)	((x)->status = STAT_ME)
#define	SetUnknown(x)	((x)->status = STAT_UNKNOWN)
#define	SetServer(x)	((x)->status = STAT_SERVER)
#define	SetClient(x)	((x)->status = STAT_CLIENT)
#define	SetLog(x)	((x)->status = STAT_LOG)
#define	SetService(x)	((x)->status = STAT_SERVICE)

#define	SetOper(x)	((x)->flags |= FLAGS_OPER)
#define SetLocOp(x)     ((x)->flags |= FLAGS_LOCOP)
#define SetInvisible(x) ((x)->flags |= FLAGS_INVISIBLE)
#define SetWallops(x)   ((x)->flags |= FLAGS_WALLOP)
#define	SetUnixSock(x)	((x)->flags |= FLAGS_UNIX)

#define ClearOper(x)    ((x)->flags &= ~FLAGS_OPER)
#define ClearInvisible(x) ((x)->flags &= ~FLAGS_INVISIBLE)
#define ClearWallops(x) ((x)->flags &= ~FLAGS_WALLOP)

#define CONF_ILLEGAL            0x80000000
#define CONF_QUARANTINED_SERVER 0x0001
#define CONF_CLIENT             0x0002
#define CONF_CONNECT_SERVER     0x0004
#define CONF_NOCONNECT_SERVER   0x0008
#define CONF_LOCOP              0x0010
#define CONF_OPERATOR           0x0020
#define CONF_ME                 0x0040
#define CONF_KILL               0x0080
#define CONF_ADMIN              0x0100
#ifdef R_LINES
#define CONF_RESTRICT           0x0200
#endif
#define CONF_CLASS              0x0400
#define CONF_SERVICE            0x0800
#define CONF_LEAF		0x1000
#define CONF_LISTEN_PORT	0x2000

#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_CONNECT_SERVER | \
				 CONF_LOCOP | CONF_NOCONNECT_SERVER | \
				 CONF_OPERATOR | CONF_SERVICE)


#define MATCH_SERVER  1
#define MATCH_HOST    2

#define DEBUG_FATAL  0
#define DEBUG_ERROR  1
#define DEBUG_DNS    3
#define DEBUG_NOTICE 4
#define DEBUG_SEND   8
#define DEBUG_DEBUG  9

#define IGNORE_TOTAL    3
#define IGNORE_PRIVATE  1
#define IGNORE_PUBLIC   2

#define FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_OPER       0x0008	/* Operator */
#define FLAGS_CHANOP     0x0010 /* Channel operator */
#define FLAGS_LOCOP      0x0020 /* Local operator -- SRB */
#define FLAGS_INVISIBLE  0x0040 /* makes user invisible */
#define FLAGS_WALLOP     0x0080 /* send wallops to them */
#define FLAGS_SERVNOTICE 0x0100 /* server notices such as kill */
#define	FLAGS_BLOCKED    0x0200	/* socket is in a blocked condition */
#define	FLAGS_UNIX	 0x0400	/* socket is in the unix domain, not inet */
#define	FLAGS_CLOSING    0x0800	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x1000 /* used to mark clients which we listen() on */

#define FLUSH_BUFFER   -2
#define BUFSIZE		512
#define UTMP         "/etc/utmp"

#define DUMMY_TERM     0
#define CURSES_TERM    1
#define TERMCAP_TERM   2

struct ConfItem
    {
	int	status;		/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	struct	in_addr ipnum;	/* ip number of host field */
	char	*host;
	char	*passwd;
	char	*name;
	int	port;
	long	hold;	/* Hold action until this time (calendar time) */
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
    };

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)

struct User
    {
	char username[USERLEN+1];
	char host[HOSTLEN+1];
        char server[HOSTLEN+1]; /*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
	struct SLink *channel;
	struct SLink *invited;
	int refcnt;		/* Number of times this block is referenced */
	long last;
	char *away;
    };

struct Client
    {
	struct	Client *next,*prev, *hnext;
	short	status;		/* Client type */
	char name[HOSTLEN+1];	/* Unique name of the client, nick or host */
	char info[REALLEN+1];	/* Free form additional client information */
	anUser	*user;		/* ...defined, if this is a User */
#ifdef USE_SERVICES
	aService *service;
#endif
	long	lasttime;	/* ...should be only LOCAL clients? --msa */
	long	firsttime;
	long	since;		/* When this client entry was created */
	int	flags;
	struct	Client *from;	/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	char	buffer[512];	/* Incoming message buffer */
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
#ifndef VMSP
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
#endif
	long	sendM;		/* Statistics: protocol messages send */
	long	sendB;		/* Statistics: total bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
	long	receiveB;	/* Statistics: total bytes received */
	aClient	*acpt;
	struct	SLink *confs;	/* Configuration record associated */
	struct	in_addr	ip;	/* keep real ip# too */
	int	port;		/* and the remote port# too :-) */
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char	passwd[PASSWDLEN+1];
    };

#define CLIENT_LOCAL_SIZE sizeof(aClient)
#define CLIENT_REMOTE_SIZE offsetof(aClient,count)

/* mode structure for channels */
typedef struct SMode {
  unsigned char mode;
  int limit;
} Mode;

/* general link structure used for chains */

typedef struct SLink {
  struct SLink *next;
  union {
	aClient *cptr;
	aChannel *chptr;
	aConfItem *aconf;
	char *cp;
  } value;
  unsigned char flags;
} Link;

/* channel structure */

struct Channel
    {
	struct Channel *nextch, *prevch, *hnextch;
	Mode mode;
	char topic[CHANNELLEN+1];
	int users;
	Link *members;
	Link *invites;
	Link *banlist;
	char chname[1];
    };

/* ignore structure */

typedef struct Ignore
    {
	char user[NICKLEN+1];
	char from[USERLEN+HOSTLEN+2];
	int flags;
	struct Ignore *next;
    } anIgnore;

extern char *version, *infotext[];
extern char *generation, *creation;
extern aClient me;   /* ...a bit squeamish about this... --msa */

/* function declarations */

extern struct Client *make_client();

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do { strncpy(x, y, N); x[N-1] = '\0';} while(0)
#define StrEq(x,y) (!strcmp((x),(y)))

/*
 * Channel Related macros follow
 */

/* Channel Visibility macros */

#define MODE_PRIVATE	0x1
#define MODE_SECRET	0x2
#define MODE_MODERATED  0x8
#define MODE_TOPICLIMIT 0x10
#define MODE_INVITEONLY 0x20
#define MODE_NOPRIVMSGS 0x40
#define MODE_VOICE	0x80


  /* name invisible */
#define SecretChannel(x) ((x) && ((x)->mode.mode & MODE_SECRET))
  /* channel not shown but names are */
#define HiddenChannel(x) ((x) && ((x)->mode.mode & MODE_PRIVATE))
#define HoldChannel(x) (!(x))
   /* channel visible */
#define ShowChannel(v,c) (PubChannel(c) ||\
			  (v)->user && IsMember((v),(c)))
#define PubChannel(x) ((!x) || ((x)->mode.mode &\
			       (MODE_PRIVATE | MODE_SECRET)) == 0)

#define IsMember(user,chan) (find_user_link((chan)->members,user) ? 1 : 0)
#define IsChannelName(name) (name && *name == '#')

/* Misc macros */

#define BadPtr(x) (!(x) || (*(x) == '\0'))

#define isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define MyConnect(x) ((x)->fd >= 0)
#define MyClient(x)  (MyConnect(x) && IsClient(x))
#define IsMagicLink(x) ((x)->fd == -20)
#define SetMagicLink(x) ((x)->fd = -20)

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define MODE_NULL      0
#define MODE_ADD       1
#define MODE_DEL       2
