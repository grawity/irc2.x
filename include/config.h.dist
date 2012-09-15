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

/* Type of host. These should be made redundant somehow. -avalon */

#define	BSD			/* 4.2 BSD, 4.3 BSD, SunOS 3.x, 4.x, Apollo */
#undef	HPUX			/* HP-UX */
#undef	ULTRIX			/* Vax Ultrix. */
#undef	AIX			/* IBM ugly so-called Unix, AIX */
#undef	MIPS			/* MIPS Unix */
#undef	SGI			/* SGI Irix */
#undef	SYSV			/* SYSV stuff - being worked on where poss. */

/* Do these work? I dunno... */

#undef	VMS			/* Should work for IRC client, not server */
#undef	MAIL50			/* If you're running VMS 5.0 */
#undef	PCS			/* PCS Cadmus MUNIX, use with BSD flag! */

/*
 * getrusage(2) is a BSDism used in s_misc.c. If you have both of these
 * use GETRUSEAGE_2 unless you get some compile time errors in which case
 * try TIMES_2. If both fail, #undef them both. -avalon
 */
#define	GETRUSAGE_2             /* if your system supports getrusage(2) */
#undef	TIMES_2                 /* or if it supports this. yuck. */
 
/*
 * The following are additional symbols to define. If you are not
 * sure about them, leave them all defined as they are.
 *
 * NOTE: *BEFORE* undefining NEED_STRERROR, check to see if your library
 *	 version returns NULL on an unknown error. If so, then I strongly
 *	 recommend you use this version as provided.
 *
 * If you get loader errors about unreferenced function calls, you must
 * define the following accordingly:
 */
#if defined(NEXT) || defined(HPUX)
#undef	NEED_STRERROR
#else
#define	NEED_STRERROR		/* Your libc.a not ANSI-compatible and has */
				/* no strerror() */
#endif

#define	NEED_STRTOKEN		/* Your libc.a does not have strtoken(3) */
#undef	NEED_STRTOK		/* Your libc.a does not have strtok(3) */
#undef	NEED_INET_ADDR  	/* You need inet_addr(3)	*/
#undef	NEED_INET_NTOA  	/* You need inet_ntoa(3)	*/
#undef	NEED_INET_NETOF 	/* You need inet_netof(3)	*/
#undef	NEED_STRCASECMP		/* You need strcasecmp(3s)	*/
/*
 * NOTE: On some systems, valloc() causes many problems.
 */
#undef	VALLOC			/* Define this if you have valloc(3) */

/*
 * The following is fairly system dependent and is important that you
 * get it right. Use *ONE* of these as your #define and *ONE ONLY*
 *
 * define this if your signal() calls DONT get reset back to the default
 * action when a signal is trapped. BSD signals are by reliable.
 */
#define	BSD_RELIABLE_SIGNALS

/*
 * if you are on a sysv-ish system, your signals arent reliable.
 */
#undef	SYSV_UNRELIABLE_SIGNALS

/*
 * define POSIX_SIGNALS if your system has the POSIX signal library.
 * POSIX_SIGNALS are RELIABLE. NOTE: these may *NOT* be used automatically
 * by your system when you compile so define here to make sure.
 */
#undef	POSIX_SIGNALS

#ifdef APOLLO
#define	RESTARTING_SYSTEMCALLS
#endif 				  /* read/write are restarted after signals
				     defining this 1, gets siginterrupt call
				     compiled, which attempts to remove this
				     behaviour (apollo sr10.1/bsd4.3 needs
				     this) */

#define	DEBUGMODE		/* define DEBUGMODE to enable debugging mode.*/

/*
 * If you have curses, and wish to use it, then define HAVECURSES. This is the
 * default mode. If you do not have termcap, then undefine HAVETERMCAP. This is
 * the default mode. You can use both HAVECURSES and HAVETERMCAP, but you must
 * define one of the two at least. Remember to check LIBFLAGS in the Makefiles.
 *
 * NOTICE: HAVECURSES and HAVETERMCAP are still under testing. Currently, use
 *  only HAVECURSES and not HAVETERMCAP. This is a temporary condition only.
 *
 * NOTE: These *ONLY* apply to the irc client in this package
 */

#define	HAVECURSES		/* If you have curses, and want to use it.  */
#undef	HAVETERMCAP		/* If you have termcap, and want to use it. */

/* Define NPATH if you want to run NOTE system. Be sure that this file is
 * either not present or non empty (result of previous size). If it is empty,
 * then remove it before starting the server.
 * The file is for request save/backup.
 */
/* #define NPATH "/usr/lib/irc/.ircdnote" /* */

/*
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the recommened names and paths. Change as needed.
 * You must define these to something, even if you don't really want them.
 */

#define	SPATH "/usr/local/bin/ircd" /* Where the server lives.  */
#define	CPATH "/usr/local/lib/ircd.conf" /* IRC configuration file.  */
#define	MPATH "/usr/local/lib/ircd.motd" /* message of the day file. */
#define	LPATH "/tmp/ircd.log" /* Where the debug file lives, if DEBUGMODE */
#define	PPATH "/usr/local/lib/ircd.pid" /* where the server's pid is */

/* ENABLE_SUMMON
 *
 * The SUMMON command requires the ircd to be run as group tty in order
 * to work properly in many cases.  If you are on a machine where it
 * won't work, or simply don't want local users to be summoned, undefine
 * this.
 */

#undef	ENABLE_SUMMON	/* local summon */
#define	ENABLE_USERS	/* enables local /users (same as who/finger output) */

/* SHOW_INVISIBLE_LUSERS
 *
 * As defined this will show the correct invisible count for anyone who does
 * LUSERS on your server. On a large net this doesnt mean much, but on a
 * small net it might be an advantage to undefine it.
 */
#define	SHOW_INVISIBLE_LUSERS

/* NO_DEFAULT_INVISIBLE
 *
 * When defined, your users will not automatically be attributed with user
 * mode "i" (i == invisible). Invisibility means people dont showup in
 * WHO or NAMES unless they are on the same channel as you.
 */
#undef	NO_DEFAULT_INVISIBLE

/* OPER_KILL
 *
 * If you dont believe operators should be allowed to use the /KILL command
 * or believe it is uncessary for them to use it, then leave OPER_KILL
 * undefined. This will not affect other operators or servers issuing KILL
 * commands however.
 */
#undef	OPER_KILL

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
 * This value is only used if you don't have server classes defined, and
 * a server is in class 0 (the default class if none is set).
 *
 */

#define MAXIMUM_LINKS 1

/*
 * If your server is running as a a HUB Server then define this.
 * A HUB Server has many servers connect to it at the same as opposed
 * to a leaf which just has 1 server (typically the uplink). Define this
 * correctly for performance reasons.
 */
#undef	HUB

/* R_LINES:  The conf file now allows the existence of R lines, or
 * restrict lines.  These allow more freedom in the ability to restrict
 * who is to sign on and when.  What the R line does is call an outside
 * program which returns a reply indicating whether to let the person on.
 * Because there is another program involved, Delays and overhead could
 * result. It is for this reason that there is a line in config.h to
 * decide whether it is something you want or need. -Hoppie
 *
 * The default is no R_LINES as most people probably don't need it. --Jto
 */

#undef	R_LINES

#ifdef	R_LINES
/* Also, even if you have R lines defined, you might not want them to be 
   checked everywhere, since it could cost lots of time and delay.  Therefore, 
   The following two options are also offered:  R_LINES_REHASH rechecks for 
   R lines after a rehash, and R_LINES_OFTEN, which rechecks it as often
   as it does K lines.  Note that R_LINES_OFTEN is *very* likely to cause 
   a resource drain, use at your own risk.  R_LINES_REHASH shouldn't be too
   bad, assuming the programs are fairly short. */
#define R_LINES_REHASH
#define R_LINES_OFTEN
#endif

/*
 * NOTE: defining CMDLINE_CONFIG and installing ircd SUID or SGID is a MAJOR
 *       security problem - they can use the "-f" option to read any files
 *       that the 'new' access lets them. Note also that defining this is
 *       a major security hole if your ircd goes down and some other user
 *       starts up the server with a new conf file that has some extra
 *       O-lines. So don't use this unless you're debugging.
 */
#undef	CMDLINE_CONFIG /* allow conf-file to be specified on command line */

/*
 * Define this filename to maintain a list of persons who log
 * into this server. Logging will stop when the file does not exist.
 * Logging will be disable also if you do not define this.
 */

/*#define FNAME_USERLOG "/usr/local/lib/ircd/userlog" /* */

/*
 * If you wish to have the server send 'vital' messages about server
 * through syslog, define USE_SYSLOG. Only system errors and events critical
 * to the server are logged although if this is defined with FNAME_USERLOG,
 * syslog() is used instead of the above file. It is not recommended that
 * this option is used unless you tell the system administrator beforehand
 * and obtain their permission to send messages to the system log files.
 */

#undef	USE_SYSLOG

/*
 * If you use syslog above, you may want to turn some (none) of the
 * spurious log messages for KILL/SQUIT off.
 */
#undef	SYSLOG_KILL	/* log all operator kills to syslog */
#undef	SYSLOG_SQUIT	/* log all remote squits for all servers to syslog */
#undef	SYSLOG_CONNECT	/* log remote connect messages for other all servs */
#undef	SYSLOG_USERS	/* send userlog stuff to syslog */
#undef	SYSLOG_OPER	/* log all users who successfully become an Op */

/*
 * If you want to log to a different facility than DAEMON, change
 * this define.
 */
#define LOG_FACILITY LOG_DAEMON

/*
 * If you want your server to be paranoid about IP# lookup, define GETHOST
 * so that a gethostbyname() is done for each gethostbyaddr() which returns
 * a hostname. If your system already has paranoid calls (SunOS 4.1.1 or
 * later) #undef this. The resolv and resolv+ libraries from Berkeley are
 * *NOT* paranoid about hostname lookups in this way.
 */
#undef	GETHOST

/*
 * define this if you want to use crypted passwords for operators in your
 * ircd.conf file. See ircd/crypt/README for more details on this.
 */
#define	CRYPT_OPER_PASSWORD

/*
 * define this if you enable summon and if you want summon to look for the
 * least idle tty a user is logged in on.
 */
#undef	LEAST_IDLE

/*
 * Max amount of internal send buffering when socket is stuck (bytes)
 */
#define MAXSENDQLENGTH 100000    /* Recommended value: 100000 for leaves */
                                 /*                    200000 for backbones */

/*
 * Use our "ctype.h" defines (islower(),isupper(),tolower(),toupper(), etc).
 * These are all table referencing macros and are 'quicker' than on some
 * systems, and they provide consistant ANSI behavior for the Finnish character
 * set no mater how broken the host OS is or what language things are.
 * IRC expects []{}\| and A-Za-z to behave properly as upper/lower case,
 * and if this isn't true bad things can happen.
 *
 * THE CODE ASSUMES THAT tolower AND toupper DO NOT DESTROY CHARACTERS
 * THEY ARE NOT CHANGING. This is ANSI, but not old-style K&R. In particular,
 * you cannot just use the standard SunOS 4.1.1 ctype macros and have
 * the server work.
 *
 * Our code assumes EOF is -1. If that isn't true on your system, don't
 * use our ctype, but make sure yours understands { is lower case [, and
 * so on for }] and |\.
 */

#define USE_OUR_CTYPE

/*
 * DNS.
 *
 * It is *very* important that if you are running an IRC server that the
 * DNS (Domain Name Service) libraries work properly and that it be correctly
 * setup on your host. However, on many hosts, its run with improper
 * configurations. A simple test is to run the "hostname" command at any
 * shell prompt. If it returns your FULL hostname (ie has your domainname in
 * it) then it should be working and setup correctly. If not, then you *MUST*
 * #define the following. If while running your server, you get clients which
 * connect as just "host" with no ".local.domain" then you should also define
 * this. There is nothing wrong with defining it just to be safe except that
 * some operating systems may have compile time errors.
 */
#define	BAD_DNS

/*
 * use these to setup a Unix domain socket to connect clients/servers to.
 */
#undef	UNIXPORT

/*
 * define IDENT to use the RFC931 identification server to verify usernames
 * note that you also need to edit ircd/Makefile to link (or not link) in
 * the authuser library code.
 *
 * AUTHTIMEOUT is the number of seconds to time out a connection that neither
 * accepts or refuses.  3s is good.  Most sites will either accept within a
 * short time frame, or refuse immediately.
 *
 * RFC931 servers are available on ftp.lysator.liu.se (for Suns, HPs, some
 * others -- pidentd-1.7 is the most recent version as of 920519) and also
 * ftp.uu.net (networking/authd/rfc931-authd.3.01.shar.Z -- this is far more
 * portable than pidentd, but slower)
 *
 * #defining IDENT adds an additional measure of security by making it harder
 * to spoof usernames with hacked clients and/or telnet-to-the-port.
 *
 * I recommend defining IDENT and leaving AUTHTIMEOUT at 3.  -ckd@eff.org
 */
#undef	IDENT
#ifdef	IDENT
#define	AUTHTIMEOUT 3
#endif /* IDENT */

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

/*
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other uid than root.
 */
#define PORTNUM 6667		/* Recommended values: 6667 or 6666 */

/*
 * Maximum number of network connections your server will allow.  This should
 * never exceed max. number of open file descrpitors and wont increase this.
 * Should remain LOW as possible. Most sites will usually have under 30 or so
 * connections. A busy hub or server may need this to be as high as 50 or 60.
 * Making it over 100 decreases any performance boost gained from it being low.
 * if you have a lot of server connections, it may be worth splitting the load
 * over 2 or more servers.
 * 1 server = 1 connection, 1 user = 1 connection.
 */
#define MAXCONNECTIONS	50

/*
 * this defines the length of the nickname history.  each time a user changes
 * nickname or signs off, their old nickname is added to the top of the list.
 * The following sizes are recommended:
 * 8MB or less  core memory : 200
 * 8MB-16MB     core memory : 200-300
 * 16MB-32MB    core memory : 300-500
 * 32MB or more core memory : 500+
 * (100 nicks/users ~ 25k)
 * NOTE: this is directly related to the amount of memory ircd will use whilst
 *	 resident and running - it hardly ever gets swapped to disk! You can
 *	 ignore these recommendations- they only are meant to serve as a guide
 */
#define NICKNAMEHISTORYLENGTH 500

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
#define PINGFREQUENCY    120	/* Recommended value: 120 */

/*
 * If the connection to to uphost is down, then attempt to reconnect every 
 * CONNECTFREQUENCY  seconds.
 */
#define CONNECTFREQUENCY 600	/* Recommended value: 600 */

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
 * Max number of channels a user is allowed to join.
 */
#define MAXCHANNELSPERUSER  10	/* Recommended value: 10 */

/*
 * SendQ-Always causes the server to put all outbound data into the sendq and
 * flushing the sendq at the end of input processing. This should cause more
 * efficient write's to be made to the network.
 * There *shouldn't* be any problems with this method.
 * -avalon
 */
#define	SENDQ_ALWAYS

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define	MYNAME SPATH
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PPATH

#ifdef DEBUGMODE
# define LOGFILE LPATH
#else
# if VMS
#	define LOGFILE "NLA0:"
# else
#	define LOGFILE "/dev/null"
# endif
#endif

#ifndef ENABLE_SUMMON
#  undef LEAST_IDLE
#endif

#ifndef DEBUGMODE
#undef	GETRUSAGE_2
#undef	TIMES_2
#endif

#ifdef	CLIENT_COMPILE
#undef	SENDQ_ALWAYS
#endif

#if defined(mips) || defined(PCS)
#undef SYSV
#endif

#ifdef MIPS
#undef BSD
#define BSD             1       /* mips only works in bsd43 environment */
#endif

#ifdef sequent                   /* Dynix (sequent OS) */
#define SEQ_NOFILE    128        /* set to your current kernel impl, */
#endif                           /* max number of socket connections */

#ifdef	BSD_RELIABLE_SIGNALS
#if defined(SYSV_UNRELIABLE_SIGNALS) || defined(POSIX_SIGNALS)
error You stuffed up config.h signals #defines use only one.
#endif
#define	HAVE_RELIABLE_SIGNALS
#endif

#ifdef	SYSYV_UNRELIABLE_SIGNALS
#ifdef	POSIX_SIGNALS
error You stuffed up config.h signals #defines use only one.
#endif
#undef	HAVE_RELIABLE_SIGNALS
#endif

#ifdef POSIX_SIGNALS
#define	HAVE_RELIABLE_SIGNALS
#endif

#ifndef	HUB
#define	MAXIMUM_LINKS	1
#endif

#ifdef HAVECURSES
# define DOCURSES
#else
# undef DOCURSES
#endif

#ifdef HAVETERMCAP
# define DOTERMCAP
#else
# undef DOTERMCAP
#endif

#ifndef	UNIXPORT
#undef	UNIXPORTPATH
#endif

#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7 register
#define Reg8 register
#define Reg9 register
#define Reg10 register
#define Reg11 
#define Reg12 
#define Reg13 
#define Reg14 
#define Reg15 
#define Reg16 
