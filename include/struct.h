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

#ifndef	__struct_include__
#define __struct_include__

#include "config.h"
#include "service.h"

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef STDDEFH
# include <stddef.h>
#endif
#ifndef CDEFSH
#include "cdefs.h"
#endif

#ifdef USE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#endif
#include <sys/time.h>

typedef	struct	ConfItem aConfItem;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	Service	aService;
typedef	struct	SLink	Link;
typedef	struct	SMode	Mode;
typedef	struct	fdarray	FdAry;
typedef	struct	CPing	aCPing;

#ifndef VMSP
#include "class.h"
#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		9	/* Necessary to put 9 here instead of 10
				** if s_msg.c/m_nick has been corrected.
				** This preserves compatibility with old
				** servers --msa
				*/
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	80
#define	CHANNELLEN	200
#define	PASSWDLEN 	20
#define	KEYLEN		23
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXBANS		20
#define	MAXBANLENGTH	1024
#define	BANLEN		(USERLEN + NICKLEN + HOSTLEN + 3)

/*
 * Make up some numbers which should reflect average leaf server connect
 * queue max size.
 */
#define	QUEUELEN	(((MAXCONNECTIONS / 2) * (CHANNELLEN + BANLEN + 16) + \
			  (HOSTLEN * 4 + REALLEN + NICKLEN + USERLEN + 24) * \
			  MAXCONNECTIONS) * 2)

#define	BUFFERPOOL	(QUEUELEN * MAXCONNECTIONS / 2)

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)

/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_CONSOLE	1
#define	BOOT_QUICK	2
#define	BOOT_DEBUG	4
#define	BOOT_INETD	8
#define	BOOT_TTY	16
#define	BOOT_OPER	32
#define	BOOT_AUTODIE	64

#define	STAT_RECONNECT	-7	/* Reconnect attempt for server connections */
#define	STAT_LOG	-6	/* logfile for -x */
#define	STAT_MASTER	-5	/* Local ircd master before identification */
#define	STAT_CONNECTING	-4
#define	STAT_HANDSHAKE	-3
#define	STAT_UNKNOWN	-2
#define	STAT_ME		-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1
#define	STAT_SERVICE	2

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT && (x)->user)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER || \
				 (x)->status == STAT_ME)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN || \
				 (x)->status == STAT_MASTER)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsLog(x)		((x)->status == STAT_LOG)
#define	IsService(x)		((x)->status == STAT_SERVICE && (x)->service)
#define	IsReconnect(x)		((x)->status == STAT_RECONNECT)

#define	SetMaster(x)		((x)->status = STAT_MASTER)
#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)
#define	SetService(x)		((x)->status = STAT_SERVICE)

#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define	FLAGS_BLOCKED    0x0008	/* socket is in a blocked condition */
#define	FLAGS_UNIX	 0x0010	/* socket is in the unix domain, not inet */
#define	FLAGS_CLOSING    0x0020	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x0040 /* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS  0x0080 /* ok to check clients access if set */
#define	FLAGS_DOINGDNS	 0x0100 /* client is waiting for a DNS response */
#define	FLAGS_AUTH	 0x0200 /* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	 0x0400	/* set if we havent writen to ident server */
#define	FLAGS_LOCAL	 0x0800 /* set for local clients */
#define	FLAGS_GOTID	 0x1000	/* successful ident lookup achieved */
#define	FLAGS_DOID	 0x2000	/* I-lines say must use ident return */
#define	FLAGS_NONL	 0x4000 /* No \n in buffer */
#define	FLAGS_HELD	 0x8000	/* connection held and reconnect try */
#define	FLAGS_CBURST	0x10000	/* st to mark connection being sent c. burt */

#define	FLAGS_OPER       0x0001	/* Operator */
#define	FLAGS_LOCOP      0x0002 /* Local operator -- SRB */
#define	FLAGS_WALLOP     0x0004 /* send wallops to them */
#define	FLAGS_INVISIBLE  0x0008 /* makes user invisible */

#define	SEND_UMODES	(FLAGS_INVISIBLE|FLAGS_OPER|FLAGS_WALLOP)
#define	ALL_UMODES	(SEND_UMODES|FLAGS_LOCOP)
#define	FLAGS_ID	(FLAGS_DOID|FLAGS_GOTID)

/*
 * flags macros.
 */
#define	IsOper(x)		((x)->user && (x)->user->flags & FLAGS_OPER)
#define	IsLocOp(x)		((x)->user && (x)->user->flags & FLAGS_LOCOP)
#define	IsInvisible(x)		((x)->user->flags & FLAGS_INVISIBLE)
#define	IsAnOper(x)		((x)->user && \
				 (x)->user->flags & (FLAGS_OPER|FLAGS_LOCOP))
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsServer(x) || IsAnOper(x))
#define	SendWallops(x)		((x)->user->flags & FLAGS_WALLOP)
#define	IsUnixSocket(x)		((x)->flags & FLAGS_UNIX)
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
#define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		(MyConnect(x) && (x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define	IsHeld(x)		((x)->flags & FLAGS_HELD)
#define	CBurst(x)		((x)->flags & FLAGS_CBURST)

#define	SetOper(x)		((x)->user->flags |= FLAGS_OPER)
#define	SetLocOp(x)    		((x)->user->flags |= FLAGS_LOCOP)
#define	SetInvisible(x)		((x)->user->flags |= FLAGS_INVISIBLE)
#define	SetWallops(x)  		((x)->user->flags |= FLAGS_WALLOP)
#define	SetUnixSock(x)		((x)->flags |= FLAGS_UNIX)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)

#define	ClearOper(x)		((x)->user->flags &= ~FLAGS_OPER)
#define	ClearInvisible(x)	((x)->user->flags &= ~FLAGS_INVISIBLE)
#define	ClearWallops(x)		((x)->user->flags &= ~FLAGS_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)

/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1	/* report_error() and other errors that are found */
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4	/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5	/* general usful info */
#define	DEBUG_NUM    6	/* numerics */
#define	DEBUG_SEND   7	/* everything that is sent out */
#define	DEBUG_DEBUG  8	/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9	/* malloc/free calls */
#define	DEBUG_LIST  10	/* debug list use */
#define	DEBUG_L10   10
#define	DEBUG_L11   11

/*
 * defines for curses in client
 */
#define	DUMMY_TERM	0
#define	CURSES_TERM	1
#define	TERMCAP_TERM	2

struct	CPing	{
	u_long	rtt;		/* average RTT */
	u_long	ping;
	u_long	seq;		/* sequence # of last sent */
	u_long	lseq;
	u_long	recv;		/* # received */
	u_long	lrecv;
};

struct	ConfItem	{
	u_int	status;		/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	struct	in_addr ipnum;	/* ip number of host field */
	char	*host;
	char	*passwd;
	char	*name;
	int	port,
		pref;		/* preference value */
	struct	CPing	*ping;
	time_t	hold;	/* Hold action until this time (calendar time) */
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x0001
#define	CONF_CLIENT		0x0002
#define	CONF_CONNECT_SERVER	0x0004
#define	CONF_NOCONNECT_SERVER	0x0008
#define	CONF_LOCOP		0x0010
#define	CONF_OPERATOR		0x0020
#define	CONF_ME			0x0040
#define	CONF_KILL		0x0080
#define	CONF_ADMIN		0x0100
#ifdef 	R_LINES
#define	CONF_RESTRICT		0x0200
#endif
#define	CONF_CLASS		0x0400
#define	CONF_SERVICE		0x0800
#define	CONF_LEAF		0x1000
#define	CONF_LISTEN_PORT	0x2000
#define	CONF_HUB		0x4000

#define	CONF_OPS		(CONF_OPERATOR | CONF_LOCOP)
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_SERVICE | CONF_OPS | \
				 CONF_SERVER_MASK)

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)

typedef	struct	{
	u_long	pi_id;
	u_long	pi_seq;
	struct	timeval	pi_tv;
	aConfItem *pi_cp;
} Ping;


#define	PING_REPLY	0x01
#define	PING_CPING	0x02

/*
 * Client structures
 */
struct	User	{
	Link	*channel;	/* chain of channel pointer blocks */
	Link	*invited;	/* chain of invite pointer blocks */
	char	*away;		/* pointer to away message */
	time_t	last;
	int	refcnt;		/* Number of times this block is referenced */
	int	joined;		/* number of channels joined */
	int	flags;
        struct	Server	*servp;
				/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
	struct	User	*nextu, *prevu;
	aClient	*bcptr;
	char	username[USERLEN+1];
	char	host[HOSTLEN+1];
	char	server[HOSTLEN+1];
	char	tok[5];
};

struct	Server	{
	anUser	*user;		/* who activated this connection */
	char	*up;	/* uplink for this server */
	aConfItem *nline;	/* N-line pointer for this server */
	int	version;        /* version id for local client */
	int	stok,
		ltok;
	int	refcnt;
	struct	Server	*nexts, *prevs, *shnext;
	aClient	*bcptr;
	char	by[NICKLEN+1];
	char	tok[5];
};

struct	Service	{
	int	wants;
	int	type;
	char	*server;
	aServer	*servp;
	struct	Service	*nexts, *prevs;
	aClient	*bcptr;
	char	dist[HOSTLEN+1];
	char	tok[5];
};

struct Client	{
	struct	Client *next,*prev, *hnext;
	anUser	*user;		/* ...defined, if this is a User */
	aServer	*serv;		/* ...defined, if this is a server */
	aService *service;
	int	hashv;		/* raw hash value */
	time_t	lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	long	flags;		/* client flags */
	aClient	*from;		/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	short	status;		/* Client type */
	char	name[HOSTLEN+1]; /* Unique name of the client, nick or host */
	char	username[USERLEN+1]; /* username here now for auth stuff */
	char	info[REALLEN+1]; /* Free form additional client information */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	char	buffer[BUFSIZE]; /* Incoming message buffer */
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
	dbuf	recvQ;		/* Hold for data incoming yet to be parsed */
	long	sendM;		/* Statistics: protocol messages send */
	long	sendK;		/* Statistics: total k-bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
	long	receiveK;	/* Statistics: total k-bytes received */
	u_short	sendB;		/* counters to count upto 1-k lots of bytes */
	u_short	receiveB;	/* sent and received. */
	u_int	sact;		/* could conceivably grow large...*/
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	int	authfd;		/* fd for rfc931 authentication */
	int	priority;	/* priority for slection as active */
	u_short	ract;		/* no fear about this. */
	u_short	port;	/* and the remote port# too :-) */
	struct	in_addr	ip;	/* keep real ip# too */
	struct	hostent	*hostp;
#ifdef	pyr
	struct	timeval	lw;
#endif
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char	passwd[PASSWDLEN+1];
	char	exitc;
};

#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

/*
 * statistics structures
 */
struct	stats {
	u_int	is_cl;	/* number of client connections */
	u_int	is_sv;	/* number of server connections */
	u_int	is_ni;	/* connection but no idea who it was */
	u_short	is_cbs;	/* bytes sent to clients */
	u_short	is_cbr;	/* bytes received to clients */
	u_short	is_sbs;	/* bytes sent to servers */
	u_short	is_sbr;	/* bytes received to servers */
	u_long	is_cks;	/* k-bytes sent to clients */
	u_long	is_ckr;	/* k-bytes received to clients */
	u_long	is_sks;	/* k-bytes sent to servers */
	u_long	is_skr;	/* k-bytes received to servers */
	time_t 		is_cti;	/* time spent connected by clients */
	time_t		is_sti;	/* time spent connected by servers */
	u_int	is_ac;	/* connections accepted */
	u_int	is_ref;	/* accepts refused */
	u_int	is_unco; /* unknown commands */
	u_int	is_wrdi; /* command going in wrong direction */
	u_int	is_unpf; /* unknown prefix */
	u_int	is_empt; /* empty message */
	u_int	is_num;	/* numeric message */
	u_int	is_kill; /* number of kills generated on collisions */
	u_int	is_fake; /* MODE 'fakes' */
	u_int	is_asuc; /* successful auth requests */
	u_int	is_abad; /* bad auth requests */
	u_int	is_udp;	/* packets recv'd on udp port */
	u_int	is_loc;	/* local connections made */
};

/* mode structure for channels */

struct	SMode	{
	u_int	mode;
	int	limit;
	char	key[KEYLEN+1];
};

/* Message table structure */

struct	Message	{
	char	*cmd;
	int	(* func)();
	u_int	count;
	int	parameters;
	u_int	flags;
		/* bit 0 set means that this command is allowed to be used
		 * only on the average of once per 2 seconds -SRB */
	u_long	bytes;
};

#define	MSG_LAG		0x0001
#define	MSG_NOU		0x0002	/* Not available to users */
#define	MSG_SVC		0x0004	/* Services only */
#define	MSG_NOUK	0x0008	/* Not available to unknowns */
#define	MSG_REG		0x0010	/* Must be registered */
#define	MSG_REGU	0x0020	/* Must be a registered user */
#define	MSG_PP		0x0040
#define	MSG_FRZ		0x0080
#define	MSG_OP		0x0100	/* opers only */
#define	MSG_LOP		0x0200	/* locops only */

/* fd array structure */

struct	fdarray	{
	int	fd[MAXCONNECTIONS];
	int	highest;
};

/* general link structure used for chains */

struct	SLink	{
	struct	SLink	*next;
	union {
		aClient	*cptr;
		aChannel *chptr;
		aConfItem *aconf;
		char	*cp;
	} value;
	int	flags;
};

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	int	hashv;		/* raw hash value */
	Mode	mode;
	char	topic[TOPICLEN+1];
	int	users;		/* current membership total */
	Link	*members;	/* channel members */
	Link	*invites;	/* outstanding invitations */
	Link	*banlist;
	Link	*clist;		/* list of connections which are members */
	char	chname[1];
};

/*
** Channel Related macros follow
*/

/* Channel related flags */

#define	CHFL_CHANOP     0x0001 /* Channel operator */
#define	CHFL_VOICE      0x0002 /* the power to speak */
#define	CHFL_BAN	0x0004 /* ban channel flag */

/* Channel Visibility macros */

#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_PRIVATE	0x0004
#define	MODE_SECRET	0x0008
#define	MODE_MODERATED  0x0010
#define	MODE_TOPICLIMIT 0x0020
#define	MODE_INVITEONLY 0x0040
#define	MODE_NOPRIVMSGS 0x0080
#define	MODE_KEY	0x0100
#define	MODE_BAN	0x0200
#define	MODE_LIMIT	0x0400
#define	MODE_ANONYMOUS	0x0800
#define MODE_FLAGS	0x0fff
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS	(MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)
/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define	MODE_DEL       0x40000000
#define	MODE_ADD       0x80000000
 */

#define	HoldChannel(x)		(!(x))
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	IsAnonymous(c)		((c) && ((c)->mode.mode & MODE_ANONYMOUS))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | MODE_SECRET)) == 0)

#define	IsMember(u, c)		(find_user_link((c)->members, u) ? 1 : 0)
#define	IsChannelName(n)	((n) && (*(n) == '#' || *(n) == '&' || \
					*(n) == '+'))
#define	UseModes(n)		((n) && (*(n) == '#' || *(n) == '&'))

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

#define	isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyPerson(x)			(MyConnect(x) && IsPerson(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))
#define	ME	me.name

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define	MODE_NULL      0
#define	MODE_ADD       0x40000000
#define	MODE_DEL       0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* used for sendto_serv */

#define	SV_OLD	0
#define	SV_29	1

/* used for sendto_flag */

typedef	struct	{
	int	svc_chan;
	char	*svc_chname;
	struct	Channel	*svc_ptr;
}	SChan;

#define	SCH_ERROR	1
#define	SCH_NOTICE	2
#define	SCH_KILL	3
#define	SCH_CHAN	4
#define	SCH_NUM		5
#define	SCH_SQUIT	6
#define	SCH_HASH	7
#define	SCH_MAX		7

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* misc variable externs */

extern	char	*version, *infotext[];
extern	char	*generation, *creation;

/* misc defines */

#define	FLUSH_BUFFER	-2
#define	UTMP		"/etc/utmp"
#define	COMMA		","

/* IRC client structures */

#ifdef	CLIENT_COMPILE
typedef	struct	Ignore {
	char	user[NICKLEN+1];
	char	from[USERLEN+HOSTLEN+2];
	int	flags;
	struct	Ignore	*next;
} anIgnore;

#define	IGNORE_PRIVATE	1
#define	IGNORE_PUBLIC	2
#define	IGNORE_TOTAL	3

#define	HEADERLEN	200

#endif

#endif /* __struct_include__ */
