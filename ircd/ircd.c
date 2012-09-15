/************************************************************************
 *   IRC - Internet Relay Chat, ircd/ircd.c
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

char ircd_id[] = "ircd.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/file.h>
#include <stdio.h>
#include <signal.h>

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */
extern	aConfItem *conf, *find_me();
extern	char	*get_client_name PROTO((aClient *, int));
extern	aClient *find_name PROTO((char *, aClient *));
extern	aClient	*add_connection PROTO((aClient *, int));
extern	aClient	*local[];
extern	int find_kill PROTO((aClient *));
#if defined(R_LINES) && defined(R_LINES_OFTEN)
extern	int find_restrict PROTO((aClient *));
#endif
extern	void clear_channel_hash_table();
extern	void clear_client_hash_table();
extern	void write_pidfile ();

char	**myargv;
int	portnum = -1;               /* Server port number, listening this */
char	*configfile = CONFIGFILE;	/* Server configuration file */
int	debuglevel = -1;		/* Server debug level */
int	debugtty = 0;
int	autodie = 0;
static	int dorehash = 0;
char	*debugmode = "";		/*  -"-    -"-   -"-  */

int	maxusersperchannel = MAXUSERSPERCHANNEL;

long	nextconnect = -1;		/* time for next try_connections call */
long	nextping = -1;		/* same as above for check_pings() */

VOIDSIG terminate()
{
  flush_connections(me.fd);
  exit(-1);
}

static VOIDSIG catch_hup()
{
    dorehash = 1;
    signal(SIGHUP, catch_hup);	/* sysV -argv */
}

VOIDSIG restart()
    {
	static int restarting = 0;
	int i;

	if (restarting == 0)
	    {
		/* Send (or attempt to) a dying scream to oper if present */

		restarting = 1;
		sendto_ops("Aieeeee!!!  Restarting server...");
	    }
	debug(DEBUG_NOTICE,"Restarting server...");
	flush_connections(me.fd);
	/*
	** fd 0 must be 'preserved' if either the -d or -i options have
	** been passed to us before restarting.
	*/
#ifdef USE_SYSLOG
	closelog();
#endif
	for (i = (debugtty == 0 && debuglevel < 0) ? 0:3; i < 255; i++)
		close(i);
	close(1);	/* not used for anything but skipped above */
	if (debugtty >= 0)
		close(0);	/* close fd 0 */
	execv(MYNAME, myargv);
#ifdef USE_SYSLOG
	/* Have to reopen since it has been closed above */

	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
	syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", MYNAME, myargv[0]);
	closelog();
#endif
	debug(DEBUG_FATAL,"Couldn't restart server !!!!!!!!");
	exit(-1);
    }


/*
** try_connections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
static long try_connections(currenttime)
long currenttime;
{
  Reg1 aConfItem *aconf;
  Reg2 aClient *cptr;
  aConfItem **pconf;
  int connecting, confrq;
  long next = 0;
  aClass *cltmp;
  aConfItem *con_conf;
  int con_class = 0;
  
  connecting = FALSE;
  debug(DEBUG_NOTICE,"Connection check at   : %s", myctime(currenttime));
  for (aconf = conf; aconf; aconf = aconf->next )
    { /* Also when already connecting! (update holdtimes) --SRB */
      if (!(aconf->status & CONF_CONNECT_SERVER) || aconf->port <= 0)
	continue;
      cltmp = Class(aconf);
      /*
       ** Skip this entry if the use of it is still on hold until
       ** future. Otherwise handle this entry (and set it on hold
       ** until next time). Will reset only hold times, if already
       ** made one successfull connection... [this algorithm is
       ** a bit fuzzy... -- msa >;) ]
       */
      
      if ((aconf->hold > currenttime)) {
	if ((next > aconf->hold) || (next == 0))
	  next = aconf->hold;
	continue;
      }

      confrq = get_con_freq(cltmp);
      aconf->hold = currenttime + confrq;
      /*
       ** Found a CONNECT config with port specified,
       ** scan clients and see if this server is already
       ** connected?
       */
      cptr = find_name(aconf->name, (aClient *)NULL);
      
      if ((cptr == NULL) && 
	  (!(connecting) || (Class(cltmp) > con_class)) &&
	  (Links(cltmp) < MaxLinks(cltmp)))
	{
	  con_class = Class(cltmp);
	  con_conf = aconf;
	  connecting = TRUE; /* We connect only one at time... */
	}
      if ((next > aconf->hold) || (next == 0))
	next = aconf->hold;
    }
  if (connecting) {
    if (con_conf->next) {  /* are we already last? */
      for (pconf = &conf; (aconf = *pconf) != NULL; pconf = &(aconf->next))
	if (aconf == con_conf) /* put the current one at the end */
	  *pconf = aconf->next; /* makes sure we try all connections */
      (*pconf = con_conf)->next = 0;
    }
    if (connect_server(con_conf) == 0)
      sendto_ops("Connection to %s[%s] activated.",
		 con_conf->name, con_conf->host);
  }
  debug(DEBUG_NOTICE,"Next connection check : %s", myctime(next));
  return (next);
}

static long check_pings(currenttime)
long currenttime;
{		
  Reg1	aClient	*cptr;
  Reg2	int	killflag;
  int	ping = 0, i, rflag = 0;
  long	oldest = 0, timeout;

  for (i = 0; i < MAXCONNECTIONS; i++)
    {
      if (!(cptr = local[i]) || IsMe(cptr))
	continue;

      if (cptr->flags & FLAGS_DEADSOCKET) {
	/*
	 ** Note: No need to notify opers here. It's
	 ** already done when "FLAGS_DEADSOCKET" is set.
	 */
	exit_client((aClient *)NULL, cptr, &me, "Dead socket");
	i = 0;
	continue;
      }
      
      killflag = IsPerson(cptr) ? find_kill(cptr) : 0;
#ifdef R_LINES_OFTEN
      rflag = IsPerson(cptr) ? find_restrict(cptr) : 0;
#endif
      ping = get_client_ping(cptr);
      if (killflag || ((currenttime - cptr->lasttime) >= ping) || rflag)
       {
	/*
	 * If the server hasnt talked to us in 2*ping seconds
	 * and it has a ping time, then close its connection.
	 * If the client is a user and a KILL line was found
	 * to be active, close this connection too.
	 */
	if (((currenttime - cptr->lasttime) >= (2 * ping) &&
	     (cptr->flags & FLAGS_PINGSENT)) ||
	    (IsUnknown(cptr) && (currenttime - cptr->since) >= ping) ||
	    killflag || rflag)
	 {
	  if (IsServer(cptr))
	    sendto_ops("No response from %s, closing link",
		       get_client_name(cptr,FALSE));
	  /*
	   * this is used for KILL lines with time
	   * restrictions on them - send a messgae to the
	   * user being killed first.
	   */
	  if (killflag && IsPerson(cptr))
	    sendto_ops("Kill line active for %s, closing link",
		       get_client_name(cptr, FALSE));

#if defined(R_LINES) && defined(R_LINES_OFTEN)
	  if (IsPerson(cptr) && rflag)
	      sendto_ops("Restricting %s, closing link.",
			 get_client_name(cptr,FALSE));
#endif
	  exit_client((aClient *)NULL, cptr, &me, "Ping timeout");
	  /*
	   * need to start loop over because the close can affect the ordering
	   * of the local[] array.- avalon
	   */
	  i = 0;
	  continue;
	 } else if ((cptr->flags & FLAGS_PINGSENT) == 0) {
	  /*
	   * if we havent PINGed the connection and we havent
	   * heard from it in a while, PING it to make sure
	   * it is still alive.
	   */
	  cptr->flags |= FLAGS_PINGSENT;
	  cptr->lasttime = currenttime - ping;	/* not nice but does the job */
	  sendto_one(cptr, "PING %s", me.name);
	}
      }
      timeout = cptr->lasttime + ping;
      while (timeout <= currenttime)
	  timeout += ping;
      if (timeout < oldest || !oldest)
	      oldest = timeout;
    }
    if (!oldest || oldest < currenttime)
      oldest = currenttime + PINGFREQUENCY;
    debug(DEBUG_NOTICE,"Next check_ping() call at: %s, %d %d %d",
	  myctime(oldest),ping,oldest,currenttime);
  return (oldest);
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static int bad_command()
{
  printf(
	 "Usage: ircd %s[-h servername] [-p portnumber] [-x loglevel] [-t]\n",
#ifdef CMDLINE_CONFIG
	 "[-f config] "
#else
	 ""
#endif
	 );
  printf("Server not started\n\n");
  exit(-1);
  return -1;
}

int	main(argc, argv)
int	argc;
char	*argv[];
{
	aClient *cptr;
	int	bootopt = 0, foo;
	char	buffer[BUFSIZE];
	long	delay = 0, now;
	static	int open_log();

	myargv = argv;
	umask(077);                /* better safe than sorry --SRB */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, catch_hup);
	signal(SIGALRM, dummy);   
	signal(SIGTERM, terminate); 
	signal(SIGINT, restart);
#ifndef	DEBUGMODE
	signal(SIGQUIT, SIG_IGN);
#endif
#ifdef RESTARTING_SYSTEMCALLS
	/*
	** At least on Apollo sr10.1 it seems continuing system calls
	** after signal is the default. The following 'siginterrupt'
	** should change that default to interrupting calls.
	*/
	siginterrupt(SIGALRM, 1);
#endif
	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	while (--argc > 0 && (*++argv)[0] == '-')
	    {
		char *p = argv[0]+1;
		int flag = *p++;

		if (flag == '\0' || *p == '\0')
			if (argc > 1 && argv[1][0] != '-')
			    {
				p = *++argv;
				argc -= 1;
			    }
			else
				p = "";

		switch (flag)
		    {
                    case 'a':
			autodie = 1;
			break;
		    case 'c':
			bootopt |= 1;
			break;
		    case 'q':
			bootopt |= 2;
			break;
		    case 'd': /* Per user local daemon... */
                        setuid(getuid());
			autodie = 1;
                        debugtty = -1;  /* mark fd 0 to be oper connection */
		        break;
#ifdef CMDLINE_CONFIG
		    case 'f':
                        setuid(getuid());
			configfile = p;
			break;
#endif
		    case 'h':
			strncpyzt(me.name, p, sizeof(me.name));
			break;
		    case 'i':
                        debugtty = -2;
		        break;
		    case 'p':
			if ((foo = atoi(p)) > 0 )
				portnum = foo;
			break;
		    case 't':
                        setuid(getuid());
			debugtty = 1;
			break;
		    case 'x':
                        setuid(getuid());
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			break;
		    default:
			bad_command();
			break;
		    }
	    }

	setuid(geteuid());
	if (getuid() == 0)
	    {

#if defined(IRC_UID) && defined(IRC_GID)

		/* run as a specified user */
		fprintf(stderr,"WARNING: running ircd with uid = %d\n", IRC_UID
			);
		fprintf(stderr,"         changing to gid %d.\n",IRC_GID);
		setuid(IRC_UID);
		setgid(IRC_GID);
#else
	/* check for setuid root as usual */
		fprintf(stderr,
			"ERROR: do not run ircd setuid root. Make it setuid a\
 normal user.\n"
			);
		exit(-1);
#endif	
	    } 

	if (debuglevel == -1)  /* didn't set debuglevel */
           if (debugtty > 0) {  /* but asked for debugging output to tty */
	     fprintf( stderr, "you specified -t without -x. use -x 9...\n" );
	     exit(-1);
	   }

	if (argc > 0)
	  bad_command(); /* Should never return from here... */

	clear_client_hash_table();
	clear_channel_hash_table();
	initclass();
	init_sys(bootopt);
	if (portnum < 0)
	  portnum = PORTNUM;
	me.port = portnum;
	get_my_name(me.name, me.sockhost,sizeof(me.sockhost)-1);
	if (debugtty == -2)
	    {
		me.fd = 0;
		local[0] = &me;
		me.flags |= FLAGS_LISTEN;
	    }

	open_log();
#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	if (initconf(bootopt & 2) == -1)
	    {
		debug(DEBUG_FATAL,
			"Failed in reading configuration file %s", configfile);
		printf("Couldn't open configuration file %s\n", configfile);
		exit(-1);
	    }
	if (debugtty != -2)
	    {
		aConfItem	*aconf;

		if (aconf = find_me())
			portnum = aconf->port;
		if (inetport(&me, "*", portnum))
			exit(1);
	    }
	/*
	** If neither command line nor configuration defined any, use
	** compiled default port and sockect hostname.
	*/
	if (me.name[0] == '\0')
		strncpyzt(me.name,me.sockhost,sizeof(me.name));
	me.hopcount = 0;
	me.confs = NULL;
	me.next = NULL;
	me.user = NULL;
	me.from = &me;
	SetMe(&me);

	me.lasttime = me.since = me.firsttime = time(NULL);
	add_to_client_hash_table(me.name, &me);

	check_class();
	if (debugtty == -1) {
	  aClient *tmp = add_connection(&me, 0);
	  if (tmp == (aClient *)NULL)
	    exit(1);
	  SetMaster(tmp);
	}
	else
	  write_pidfile();

	debug(DEBUG_NOTICE,"Server ready...");
	for (;;)
	    {
		now = time(NULL);
		/*
		** We only want to connect if a connection is due,
		** not every time through.  Note, if there are no
		** active C lines, this call to Tryconnections is
		** made once only; it will return 0. - avalon
		*/
		if (nextconnect && now >= nextconnect)
			nextconnect = try_connections(now);

		/*
		** take the smaller of the two 'timed' event times as
		** the time of next event (stops us being late :) - avalon
		** WARNING - nextconnect can return 0!
		*/
		if (nextconnect)
			delay = MIN(nextping, nextconnect) - now;
		else
			delay = nextping - now;
		/*
		** Adjust delay to something reasonable [ad hoc values]
		** (one might think something more clever here... --msa)
		** We don't really need to check that often and as long
		** as we don't delay too long, everything should be ok.
		** waiting too long can cause things to timeout...
		** i.e. PINGS -> a disconnection :(
		** - avalon
		*/
		if (delay < 1)
			delay = 1;
		else delay = MIN(delay, TIMESEC);
		read_message(buffer, BUFSIZE, &cptr, delay);
		
		debug(DEBUG_DEBUG,"Got message(s)");
		
		now = time(NULL);
		/*
		** ...perhaps should not do these loops every time,
		** but only if there is some chance of something
		** happening (but, note that conf->hold times may
		** be changed elsewhere--so precomputed next event
		** time might be too far away... (similarly with
		** ping times) --msa
		*/
		if (now >= nextping)
			nextping = check_pings(now);

		if (dorehash)
		    {
		    sendto_ops(
		       "Got signal SIGHUP, rehashing ircd configuration file.");
		    rehash();
		    dorehash = 0;
		    }
		/*
		** Flush output buffers on all connections now if they
		** have data in them (or at least try to flush)
		** -avalon
		*/
		flush_connections(me.fd);
	    }
    }

static int open_log()
    {
	int fd;
	if (debuglevel >= 0)
	    {
	      if (debugtty != 1) /* leave debugging output on stderr */
		{
		  if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
		    if ((fd = open("/dev/null", O_WRONLY)) < 0)
		      exit(-1);
		  if (fd != 2)
		    {
		      dup2(fd, 2);
		      close(fd); 
		    }
		  lseek(2, 0L, 2);	/* SEEK_END == 2 */
		}
	    }
	else
	    {
		if ((fd = open("/dev/null", O_WRONLY)) < 0) 
			exit(-1);
		if (fd != 2)
		    {
			dup2(fd, 2);
			close(fd);
		    }
	    }
	return 0;
    }

