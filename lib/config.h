/*************************************************************************
 ** config.h  Beta  v2.0    (22 Aug 1988)
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
#define BSD42      0          /* 4.2 BSD, SunOS 3.x */
#endif
#define BSD43      1          /* 4.3 BSD, SunOS 4.x, Apollo */
#ifdef hpux
#define HPUX       1          /* HP-UX */
#endif

#define ULTRIX       0          /* Vax Ultrix. */
#define SYSV         0         /* Does not work yet. Under construction */
/* #define VMS       0        /* Might even work... */
/* #define MAIL50    1        /* If you're running VMS 5.0 */

/* WARNING: DOCURSES and DOTERMCAP are under testing. Currently define
   only DOCURSES and undefine DOTERMCAP                                  */

#define DOCURSES    1         /* Undefine this if you do not want to     */
                              /* use curses (or you do not have it)      */
/* #define DOTERMCAP   1         /* Undefine this if you do not want to     */
                              /* use termcap (or you do not have it)     */
                              /* You can have both DOCURSES and          */
                              /* DOTERMCAP defined, but you cannot       */
                              /* undefine both of them !                 */
                              /* Remember to check libraries in Makefile */
                              /* too (-ltermcap, -lcurses)               */

/* define TTYON if you want to see ircd debug information. */
/* normally undefined  */
/* #define TTYON */


/* Full path name of the server executable */
#define MYNAME     "/usr/local/bin/ircd"

/* Configurationfile seems to work... */
#define CONFIGFILE "/usr/local/lib/irc/irc.conf"

/* Only for irc client: default name for host to connect */
/* Leave this undefined if client runs on same host than server */
/* or you want to use irc.conf to define uphost... */
#define UPHOST     "salmon.ee.ubc.ca"

/* Path for Message of Today file. Leave this undefined if you don't */
/* have one                                                          */
#define  MOTD      "/usr/local/lib/irc/irc.motd"

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

#define WRITEWAITDELAY     15  /* Number of seconds to wait for write     */
                              /* to complete if stuck...                 */

