/*************************************************************************
 ** struct.h  Beta  v2.0    (22 Aug 1988)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen 
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 ** 
 *************************************************************************/

/* Type of host... currently only BSD and similar are supported */
#ifdef sun
#define BSD42      1          /* 4.2 BSD, SunOS 3.x */
#endif
#define BSD43      0          /* 4.3 BSD, SunOS 4.x, Apollo */
#ifdef hpux
#define HPUX       1          /* HP-UX */
#endif

#define ULTRIX     0          /* Vax Ultrix. */
#define SYSV        0         /* Does not work yet. Under construction */


/* #define VMS       0        */ 	/* Might even work... */

/* #define MAIL50    1        */ 	/* If you're running VMS 5.0 */

/* WARNING: DOCURSES and DOTERMCAP are under testing. Currently define
   only DOCURSES and undefine DOTERMCAP                                  */

#define DOCURSES    1         /* Undefine this if you do not want to     */
                              /* use curses (or you do not have it)      */
/* #define DOTERMCAP   1 */   /* Undefine this if you do not want to     */
                              /* use termcap (or you do not have it)     */
                              /* You can have both DOCURSES and          */
                              /* DOTERMCAP defined, but you cannot       */
                              /* undefine both of them !                 */
                              /* Remember to check libraries in Makefile */
                              /* too (-ltermcap, -lcurses)               */

/* define NOTTY if you want to run ircd as a daemon. */
/* (That's the normal way to run it)                 */
#define NOTTY    1 

/* Full path name of the server executable */
#define MYNAME     "/usr/local/bin/ircd"

/* Configurationfile seems to work... */
#define CONFIGFILE "/usr/local/lib/irc/irc.conf"

/* Only for irc client: default name for host to connect */
/* Leave this undefined if client runs on same host than server */
/* or you want to use irc.conf to define uphost... */
#define UPHOST     "coho.ee.ubc.ca" 

/* Path for Message of Today file. Leave this undefined if you don't */
/* have one                                                          */
#define MOTD "/usr/local/lib/irc/irc.motd"

/* Port where ircd resides. NOTE: This *MUST* be greater than 1024, */
/* if you plan to run ircd under any other uid than root. Command line */
/* parameter port DOES NOT  affect this port, but only the port, where ircd */
/* tries to connect, ie. the port another server uses, not the one this uses */
#define PORTNUM 6667

/* Logfile is not in use unless you specifically say sop when starting  */
/* ircd. It might take lots of disk space so I recommend using logfile  */
/* only when you need it for debugging purposes                         */
/* Use #define LOGFILE "NLA0:" if you have VMS and do not want to use   */
/* logging */

#define LOGFILE "/dev/null"

#define TIMESEC  60           /* Time interval to wait and if no        */
                              /* messages has been received, check      */
                              /* for PINGFREQUENCY and CONNECTFREQUENCY */

#define PINGFREQUENCY    120  /* If daemon doesn't receive anything */
                              /* from some daemon/client within     */
                              /* PINGFREQUENCY seconds, it tries to */
                              /* wake it up with PING message       */
                              /* If no reply is received within     */
                              /* 2 * PINGFREQUENCY seconds,         */
                              /* connection will be closed          */

#define CONNECTFREQUENCY 1200 /* if connection to to uphost is down,  */
                              /* try to reconnect about every         */
                              /* CONNECTFREQUENCY  seconds            */

#define MAXUSERSPERCHANNEL 10 /* 10 is currently recommended. If this is */
                              /* zero or negative, no restrictions exist */
                              /* If you are connected to other ircds, do */
                              /* NOT change this from default without    */
                              /* asking from other irc administrators    */
                              /* first !                                 */

/* Define OLDWHOREPLY if you want to user old version of who in irc client */
/* #define OLDWHOREPLY        1 */

#define WRITEWAITDELAY     15  /* Number of seconds to wait for write     */
                              /* to complete if stuck...                 */

/* You probably shouldn't touch constants after this line... */

#define HOSTLEN   50  /* Length of hostname... this should be changed */
                      /* longer due to domain system... */
#define NICKLEN   10
#define USERLEN   10
#define REALLEN   30
#define HEADERLEN 200
#define PASSWDLEN 20

#define CHANNELLEN 50
#define BUFSIZE  256 
#define MAXBUFLEN 512

#define STAT_HANDSHAKE -3
#define STAT_ME        -2
#define STAT_UNKNOWN   -1
#define STAT_SERVER     0
#define STAT_CLIENT     1
#define STAT_LOG        2
#define STAT_SERVICE    3      /* Services not implemented yet */
#define STAT_OPER       4      /* Operator */

#define CONF_ILLEGAL          0
#define CONF_SKIPME           (1 << 0)
#define CONF_CLIENT           (1 << 1)
#define CONF_CONNECT_SERVER   (1 << 2)
#define CONF_NOCONNECT_SERVER (1 << 3)
#define CONF_UPHOST           (1 << 4)
#define CONF_OPERATOR         (1 << 5)
#define CONF_ME               (1 << 6)
#define CONF_KILL             (1 << 7)
#define CONF_ADMIN            (1 << 8)

#define DEBUG_FATAL  0
#define DEBUG_ERROR  1
#define DEBUG_NOTICE 2
#define DEBUG_DEBUG  3

#define IGNORE_PRIVATE  (1 << 1)
#define IGNORE_PUBLIC   (1 << 2)
#define IGNORE_TOTAL (IGNORE_PRIVATE | IGNORE_PUBLIC)

#define FLAGS_PINGSENT (1 << 0)


#define FLUSH_BUFFER   -2
#define MAXFD        32
#define UTMP         "/etc/utmp"

#define DUMMY_TERM     0
#define CURSES_TERM    1
#define TERMCAP_TERM   2

typedef struct Client {
  struct Client *next;
  char host[HOSTLEN+1];
  char nickname[NICKLEN+1];
  char username[USERLEN+1];
  char realname[REALLEN+1];
  char server[HOSTLEN+1];
  char fromhost[HOSTLEN+1];
  char buffer[MAXBUFLEN+1];
  char sockhost[HOSTLEN+1];
  char passwd[PASSWDLEN+1];
  char *away;
  short status;
  int fd;
  int channel;
  long lasttime;
  short flags;
} aClient;

typedef struct Ignore {
  char user[NICKLEN+1];
  int flags;
  struct Ignore *next;
} aIgnore;

typedef struct Channel {
  struct Channel *nextch;
  int channo;
  char name[CHANNELLEN+1];
  int users;
} aChannel;

typedef struct Confitem {
  int status;
  char *host;
  char *passwd;
  char *name;
  int port;
  struct Confitem *next;
} aConfItem;

extern char *version, *info1, *info2, *info3; 
extern char myhostname[], *HEADER, *welcome1;

extern struct Client *make_client();
extern long getlongtime();

/* various macros added by Mike */

#define DigitORMinus(x) (isdigit(x) || ((x) == '-'))
#define ISIGNORE_PRIVATE(x) ((x)->flags & IGNORE_PRIVATE)
#define ISIGNORE_PUBLIC(x) ((x)->flags & IGNORE_PUBLIC)
#define ISIGNORE_TOTAL(x) ((x)->flags & IGNORE_TOTAL)

#define PrivChannel(ch) ((ch)> 999 || (ch) < 1)

#define CopyFixLen(d, s, l) \
       strncpy((d), (s), (l) - 1), (d)[(l)-1] = '\0'

extern char *MyMalloc();

#define UnLimChannel(x) (((x) > 0) && ((x) < 10))
#define PublicChannel(x) (!chan_isprivate(x))

#define VisibleChannel(x, y) \
   (!PrivChannel(x) || ((x) == (y) && (x) != 0))


#define SameLetter(x,y) \
  ((islower(x) ? toupper(x) : x) == (islower(y) ? toupper(y) : y))

#define BellChar ('\007')

#define ISOPER(x) ((x)->status == STAT_OPER)
#define ISCLIENT(x) ((x)->status == STAT_CLIENT)
#define ISSERVER(x) ((x)->status == STAT_SERVER)
#define ISME(x) ((x)->status == STAT_ME)

#define chan_isprivate(ch) ((ch)->channo > 999 || (ch)->channo < 1)
#define chan_conv(name)  (atoi(name))
#define chan_match(ch, x) ((ch)->channo == (x))
#define chan_issecret(ch) ((ch)->channo < 0)
#define Equal(x,y) (!strcmp((x), (y)))
#define MyEqual(x,y,z) (!myncmp((x), (y), (z)))
#define ErrorExit(m,n) { perror(m); exit(n); }

#define is_full(ch, us) (!UnLimChannel((ch)) &&  ((us) >= maxusersperchannel))

#define ISPINGSENT(x) ((x)->flags & FLAGS_PINGSENT)
#define SETPINGSENT(x) ((x)->flags |=  FLAGS_PINGSENT)
#define CLRPINGSENT(x) ((x)->flags &=  ~FLAGS_PINGSENT)


#define ISSKIPME(x)           ((x)->status == CONF_SKIPME)
#define ISMECONF(x)           ((x)->status == CONF_ME) 
#define ISCLIENTCONF(x)         ((x)->status == CONF_CLIENT) 
#define ISILLEGAL(x)         ((x)->status == CONF_ILLEGAL) 

