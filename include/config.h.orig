/************************************************************************
 *   IRC - Internet Relay Chat, include/config.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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
 * -- Jto -- 14 Jul 1990
 * Added Wumpus's MAXIMUM_LINKS fix
 *
 * -- Jto -- 16 Jun 1990
 * Added new MSG_BASE (now MAIL) fixes from Wizible...
 *
 * -- Jto -- 03 Jun 1990
 * Added MSG_BASE defines to irc code
 *
 * -- Jto -- 10 May 1990
 * Added UPHOST back into config.h (Who removed it ?)
 *
 * Revision 2.4  90/05/04  23:48:36  chelsea
 * New macros, new options, a bit more tidy and more comments.
 * 
 * Revision 1.2  90/01/04  22:27:02  poohbear
 * Cleaned up to make configurations easier, fixed a few typos.
 * 
 * Revision 1.1  90/01/01  17:03:17  poohbear
 * Initial revision
 */


/* Type of host... currently only BSD and similar are supported */

#define BSD42		0	/* 4.2 BSD, SunOS 3.x */
#define BSD43		1	/* 4.3 BSD, SunOS 4.x, Apollo */
#define HPUX		0	/* HP-UX */
#define ULTRIX		0	/* Vax Ultrix. */
#define SYSV		0	/* Does not work yet. Under construction */
#define VMS		0	/* Should work for IRC client, not server */
#define MAIL50		0	/* If you're running VMS 5.0 */
#define RSUMMON		0	/* Remote summon feature --does not work */

/*
 * The following are additional symbols to define. If you are not
 * sure about them, leave them all defined as 0.
 */
#define HAVE_ANSI_INCLUDES	0 /* You have ANSI standard include files */
#define HAVE_SYSV_INCLUDES	0 /* You have SYSV style include files */

#define HAVE_RELIABLE_SIGNALS	0
#define	RESTARTING_SYSTEMCALLS	0 /* read/write are restarted after signals
				     defining this 1, gets siginterrupt call
				     compiled, which attempts to remove this
				     behaviour (apollo sr10.1/bsd4.3 needs
				     this) */
/*
 * Logfile is not in use unless you specifically say so when starting ircd. It
 * will use a lot of diskspace at higher debugging levels ie: above 4, so it's
 * recommended to use logging only for debugging purposes.
 */

#define DEBUGMODE	1	/* define DEBUGMODE to enable debugging mode. */


/*
 * If you have curses, and wish to use it, then define HAVECURSES. This is the
 * default mode. If you do not have termcap, then undefine HAVETERMCAP. This is
 * the default mode. You can use both HAVECURSES and HAVETERMCAP, but you must
 * define one of the two at least. Remember to check LIBFLAGS in the Makefiles.
 *
 * NOTICE: HAVECURSES and HAVETERMCAP are still under testing. Currently, use
 *  only HAVECURSES and not HAVETERMCAP. This is a temporary condition only.
 */

#define HAVECURSES	1	/* If you have curses, and want to use it.  */
#define HAVETERMCAP	0	/* If you have termcap, and want to use it. */

  /* Define this if you want to use MSGBASE system                */
  /* MSGBASE is a system which allows you to leave messages       */
  /* for users and make them be sent to recipient when he/she     */
  /* signs on IRC. This does not work on all systems !            */
/* #define MSG_MAIL     "MAIL" */

/*
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the reccommened names and paths. Change as needed.
 */

#define SPATH "/home/kick/etc/irc2.5.1/ircd/ircd" /* Where the server lives.  */
#define CPATH "/home/kick/etc/irc2.5.1/ircd/ircd.cf"	 /* IRC Configuration file.  */
#define MPATH "/home/kick/etc/irc2.5.1/ircd/ircd.motd"     /* Message Of The Day file. */
#define LPATH "/home/kick/etc/irc2.5.1/ircd/ircd.log"      /* Where the Logfile lives. */
#ifdef MSG_MAIL
/* Where MSGBASE saves its messages from time to time... */
#define MAIL_SAVE_FILENAME "/home/kick/etc/irc2.5.1/ircd/.ircdmail"
#endif

#define UPHOST "choshi.kaba.or.jp"           /* Default UPHOST for irc */
                                             /* standard client        */

/* MAXIMUM LINKS
 *
 * This define is useful for leaf nodes and gateways. It keeps you from
 * connecting to too many places. It works by keeping you from
 * connecting to more than "n" nodes which you have C:blah::blah:6667
 * lines for.
 *
 * Note that any number of nodes can still connect to you. This only
 * limits the number that you actively reach out to connect to.
 *
 * Leaf nodes are nodes which are on the edge of the tree. If you want
 * to have a backup link, then sometimes you end up connected to both
 * your primary and backup, routing traffic between them. To prevent
 * this, #define MAXIMUM_LINKS 1 and set up both primary and
 * secondary with C:blah::blah:6667 lines. THEY SHOULD NOT TRY TO
 * CONNECT TO YOU, YOU SHOULD CONNECT TO THEM.
 *
 * Gateways such as the server which connects Australia to the US can
 * do a similar thing. Put the American nodes you want to connect to
 * in with C:blah::blah:6667 lines, and the Australian nodes with
 * C:blah::blah lines. Have the Americans put you in with C:blah::blah
 * lines. Then you will only connect to one of the Americans.
 *
 * If you don't want this feature, leave MAXIMUM_LINKS undefined. Setting
 * it to zero just wastes CPU time.
 */

/* #define MAXIMUM_LINKS 1 */

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

#define AUTOTOPIC 1	/* Automatic topic notify upon joining a channel  */ 

/*
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other uid than root.
 */

#define PORTNUM 6667		/* Recommended values: 6667 or 6666 */


/*
 * Time interval to wait and if no messages have been received, then check for
 * PINGFREQUENCY and CONNECTFREQUENCY 
 */

#define TIMESEC  60		/* Recommended value: 60 */


/*
 * If daemon doesn't receive anything from any of its links within
 * PINGFREQUENCY seconds, then the server will attempt to check for
 * an active link with a PING message. If no reply is received within
 * (PINGFREQUENCY * 2) seconds, then the connection will be closed.
 */

#define PINGFREQUENCY    90	/* Recommended value: 120 */


/*
 * If the connection to to uphost is down, then attempt to reconnect every 
 * CONNECTFREQUENCY  seconds.
 */

#define CONNECTFREQUENCY 1200	/* Recommended value: 1200 */

/*
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 */
#define HANGONRETRYDELAY 10	/* Recommended value: 10 seconds */
#define HANGONGOODLINK 300	/* Recommended value: 10 minutes */

/*
 * Number of seconds to wait for write to complete if stuck.
 */

#define WRITEWAITDELAY     15	/* Recommended value: 15 */

/*
 * Max time from the nickname change that still causes KILL
 * automaticly to switch for the current nick of that user. (seconds)
 */

#define KILLCHASETIMELIMIT 90   /* Recommended value: 90 */

/*
 * Max amount of internal send buffering when socket is stuck (bytes)
 */

#define MAXSENDQLENGTH 24000


#if LOG
# define TTYON
#else
# undef TTYON
#endif

#if HAVECURSES
# define DOCURSES
#else
# undef DOCURSES
#endif

#if HAVETERMCAP
# define DOTERMCAP
#else
# undef DOTERMCAP
#endif

#define MOTD MPATH
#define MYNAME SPATH
#define CONFIGFILE CPATH

#if DEBUGMODE
# define LOGFILE LPATH
#else
# if VMS
#	define LOGFILE "NLA0:"
# else
#	define LOGFILE "/dev/null"
# endif
#endif

#ifdef mips
#undef SYSV
#endif

#ifdef sequent                   /* Dynix (sequent OS) */
#define SEQ_NOFILE    128        /* set to your current kernel impl, */
#endif                           /* max number of socket connections */

#undef GETPASS                  /* Whether password non-echoing query */
                                /* should work or not..               */
