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

#include "config.h"

#define MAXUSERSPERCHANNEL 10 /* 10 is currently recommended. If this is */
                              /* zero or negative, no restrictions exist */
                              /* If you are connected to other ircds, do */
                              /* NOT change this from default without    */
                              /* asking from other irc administrators    */
                              /* first !                                 */

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
#define CONF_SKIPME           1
#define CONF_CLIENT           2
#define CONF_CONNECT_SERVER   4
#define CONF_NOCONNECT_SERVER 8
#define CONF_UPHOST           16
#define CONF_OPERATOR         32
#define CONF_ME               64
#define CONF_KILL             128
#define CONF_ADMIN            256

#define DEBUG_FATAL  0
#define DEBUG_ERROR  1
#define DEBUG_NOTICE 2
#define DEBUG_DEBUG  3

#define IGNORE_TOTAL    3
#define IGNORE_PRIVATE  1
#define IGNORE_PUBLIC   2

#define FLAGS_PINGSENT 1

#define FLUSH_BUFFER   -2
#define MAXFD        32
#define UTMP         "/etc/utmp"

#define DUMMY_TERM     0
#define CURSES_TERM    1
#define TERMCAP_TERM   2

struct Client {
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
};

struct Ignore {
  char user[NICKLEN+1];
  int flags;
  struct Ignore *next;
};

struct Channel {
  struct Channel *nextch;
  int channo;
  char name[CHANNELLEN+1];
  int users;
};

struct Confitem {
  int status;
  char *host;
  char *passwd;
  char *name;
  int port;
  struct Confitem *next;
};


extern char *version, *info1, *info2, *info3; 
extern char myhostname[], *HEADER, *welcome1;
extern char *intro;

extern struct Client *make_client();
extern long getlongtime();


