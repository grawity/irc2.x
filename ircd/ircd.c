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

/* -- Jto -- 07 Jul 1990
 * Added jlp@hamblin.byu.edu's debugtty fixes
 * Added init_messages() -call (for mail.c)
 */

/* -- Jto -- 03 Jun 1990
 * Added Kill fixes from gruner@lan.informatik.tu-muenchen.de
 * Changed some subroutine call order to get error messages...
 */

/* -- Jto -- 13 May 1990
 * Added fixes from msa:
 *   Fixed some messages to readable ones
 *   Added initial configuration file read success checking.
 */

char ircd_id[] = "ircd.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <signal.h>
#include "struct.h"
#include "numeric.h"

#define TRUE 1
#define FALSE 0

extern int dummy();
aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */
extern aConfItem *conf;
extern aConfItem *find_me();
extern char *GetClientName();

char **myargv;
int portnum;			/* Server port number, listening this */
char *configfile;		/* Server configuration file */
int debuglevel = -1;		/* Server debug level */
int debugtty = 0;
char *debugmode = "";		/*  -"-    -"-   -"-  */

int maxusersperchannel = MAXUSERSPERCHANNEL;

static long lasttime;		/* ...actually kind of current event time */

#ifndef TTYON
#define START_FD 0
#else
#define START_FD 3
#endif

restart()
    {
	static int restarting = 0;
	int i;

	if (restarting == 0)
	    {
		/* Send (or attempt to) a dying scream to oper if present */

		restarting = 1;
		sendto_ops("Aieeeee!!!  Restarting server...");
#ifdef MSG_MAIL
		save_messages();
#endif
	    }
	debug(DEBUG_NOTICE,"Restarting server...");
	
	for (i=START_FD; i<MAXFD; i++) 
		close(i);
	execv(MYNAME, myargv);
	debug(DEBUG_FATAL,"Couldn't restart server !!!!!!!!");
	exit(-1);
    }


/*
** TryConnections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
static long TryConnections(currenttime)
long currenttime;
    {
	aConfItem *aconf;
	aClient *cptr;
	int connected = FALSE;
	long nexttime = currenttime + CONNECTFREQUENCY;

	for (aconf = conf; aconf; aconf = aconf->next )
	    {
		if (aconf->status != CONF_CONNECT_SERVER || aconf->port <= 0)
			continue;
		/*
		** Skip this entry if the use of it is still on hold until
		** future. Otherwise handle this entry (and set it on hold
		** until next time). Will reset only hold times, if already
		** made one successfull connection... [this algorithm is
		** a bit fuzzy... -- msa >;) ]
		*/
		if (lasttime < aconf->hold)
		    {
			if (aconf->hold < nexttime)
				nexttime = aconf->hold;
			continue;
		    }
		aconf->hold = lasttime + CONNECTFREQUENCY;
		if (connected)
			continue;
		/*
		** Found a CONNECT config with port specified,
		** scan clients and see if this server is already
		** connected?
		*/
		for (cptr = client; cptr; cptr = cptr->next) 
			if (!IsPerson(cptr) &&
			    mycmp(cptr->name, aconf->name) == 0)
				break;
		
		if (cptr == NULL && connect_server(aconf) == 0)
		    {
			sendto_ops("Connecting to %s activated.",
				   aconf->name);
			connected = TRUE; /* We connect only one at time... */
		    }
	    }
	return nexttime;
    }

static int check_pings()
    {		
	register aClient *cptr;
	register int flag;
	extern int find_kill();
	char reply[128];

	for (cptr = client; cptr; )
	    {
		if (cptr->flags & FLAGS_DEADSOCKET)
		    {
			/*
			** Note: No need to notify opers here. It's
			** already done when "FLAGS_DEADSOCKET" is set.
			*/
			ExitClient((aClient *)NULL, cptr);
			cptr = client; /* NOTICE THIS! */
			continue;
		    }


		if (!IsMe(cptr) && cptr->fd >= 0 &&
		    ((flag = IsPerson(cptr) ? find_kill(cptr->user->host,
					cptr->user->username, reply) : 0) > 0
		     ||  (lasttime - cptr->lasttime) > PINGFREQUENCY))
		  {
		    if ((lasttime - cptr->lasttime) > 2 * PINGFREQUENCY ||
			flag > 0)
		      {
			if (IsServer(cptr))
			  sendto_ops("No response from %s, closing link",
				     GetClientName(cptr,FALSE));
			if (flag && IsPerson(cptr))
			  sendto_ops("Kill line active for %s, closing link",
				     GetClientName(cptr, FALSE));
			if (flag && IsPerson(cptr))
			  sendto_one(cptr, reply, me.name,
				     ERR_YOUREBANNEDCREEP, cptr->name);
			ExitClient((aClient *)NULL, cptr);
			cptr = client; /* NOTICE THIS! */
			continue;
		      }
		    else if ((cptr->flags & FLAGS_PINGSENT) == 0)
		      {
			cptr->flags |= FLAGS_PINGSENT;
			sendto_one(cptr, "PING %s", me.name);
		      }
		    if ((flag == -1) && IsPerson(cptr))
		      sendto_one(cptr, reply, me.name,
				 ERR_YOUWILLBEBANNED, cptr->name);
		  }
		cptr = cptr->next; /* This must be here, see "NOTICE THIS!" */
	    }
    }

/*
** BadCommand
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static int BadCommand()
    {
	printf(
"Usage: ircd [-f config] [-h servername] [-p portnumber] [-x loglevel] [-t]\n");
	printf("Server not started\n\n");
	exit(-1);
    }

main(argc, argv)
int argc;
char *argv[];
    {
	aClient *cptr;
	aConfItem *aconf;
	int length;		/* Length of message received from client */
	char buffer[BUFSIZE];
	myargv = argv;
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, restart);
	signal(SIGALRM, dummy);
#if RESTARTING_SYSTEMCALLS
	/*
	** At least on Apollo sr10.1 it seems continuing system calls
	** after signal is the default. The following 'siginterrupt'
	** should change that default to interrupting calls.
	*/
	siginterrupt(SIGALRM, 1);
#endif
	setuid(geteuid());
	if (getuid() == 0)
	    {
		setgid(-2);
		setuid(-2);
	    } 
	getmyname(me.sockhost,sizeof(me.sockhost)-1);
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
		    case 'f':
			configfile = p;
			break;
		    case 'h':
			strncpyzt(me.name, p, sizeof(me.name));
			break;
		    case 'p':
			if ((length = atoi(p)) > 0 )
				portnum = length;
			break;
		    case 'x':
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			break;
		    case 't':
			debugtty = 1;
			break;
	    /*      case 'c':
			maxusersperchannel = atoi(p));
			break; */
		    default:
			BadCommand();
			break;
		    }
	    }

	if (debuglevel == -1)  /* didn't set debuglevel */
           if (debugtty)       /* but asked for debugging output to tty */
               debugtty = 0;   /* turn it off */


	if (argc > 0)
		BadCommand(); /* Should never return from here... */
	if (BadPtr(configfile))
		configfile = CONFIGFILE;
	if (initconf() == -1)
	  {
	    debug(DEBUG_FATAL,
		  "Failed in reading configuration file %s", configfile);
	    printf("Couldn't open configurayion file %s\n", configfile);
	    exit(-1);
	  }
	init_sys(); 
	OpenLog();
	/*
	** If neither command line nor configuration defined any, use
	** compiled default port and sockect hostname.
	*/
	if (me.name[0] == '\0')
		strncpyzt(me.name,me.sockhost,sizeof(me.name));
	if (portnum <= 0)
		portnum = PORTNUM;
	me.next = NULL;
	me.from = &me;
	me.user = NULL;
	me.fd = open_port(portnum);
	me.status = STAT_ME;
	me.lasttime = me.since = getlongtime();

#ifdef MSG_MAIL
	init_messages();
#endif
	
	debug(DEBUG_DEBUG,"Server ready...");
	for (;;)
	    {
		long delay = TryConnections(getlongtime());

		delay -= getlongtime();
		/*
		** Adjust delay to something reasonable [ad hoc values]
		** (one might think something more clever here... --msa)
		*/
		if (delay <= 0)
			delay = 1;
		else if (delay > TIMESEC)
			delay = TIMESEC;
		if ((length = ReadMessage(buffer, BUFSIZE, &cptr, delay)) > 0)
		    {
			cptr->lasttime = getlongtime();
			cptr->flags &= ~FLAGS_PINGSENT;
			dopacket(cptr, buffer, length);
		    } 
		
		debug(DEBUG_DEBUG,"Got message");
		
		lasttime = getlongtime();
		/*
		** ...perhaps should not do these loops every time,
		** but only if there is some chance of something
		** happening (but, note that conf->hold times may
		** be changed elsewhere--so precomputed next event
		** time might be too far away... (similarly with
		** ping times) --msa
		*/
		check_pings();
	    }
    }

static int OpenLog()
    {
	int fd;
#ifndef TTYON
	if (debuglevel >= 0)
	    {
	      if (!debugtty) /* leave debugging output on stderr */
		{
		  if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
		    if ((fd = open("/dev/null", O_WRONLY)) < 0)
		      exit(-1);
		  if (fd != 2)
		    {
		      dup2(fd, 2);
		      close(fd); 
		    }
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
#endif
    }

