/*
 * This program has been written by Kannan Varadhan and Jeff Trim.
 * You are welcome to use, copy, modify, or circulate as you please,
 * provided you do not charge any fee for any of it, and you do not
 * remove these header comments from any of these files.
 *
 * Jeff Trim wrote the socket code
 *
 * Kannan Varadhan wrote the string manipulation code
 *
 *		KANNAN		kannan@cis.ohio-state.edu
 *		Jeff		jtrim@orion.cair.du.edu
 *		Wed Aug 23 22:44:39 EDT 1989
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "config.h"
#include "strings.h"


typedef	struct	nameslist	{
	char	lcased[STRSIZE]	/* lower cased version of above	*/;
	char	build[WKSIZE]	/* build a prompt string	*/;
	char	dfsa		/* State of the search		*/;
	char	notnick		/* do a 'who' instead		*/;
	char	substate	/* On a who search, track info	*/;
	}	nstruct;

nstruct	**narray;	/* malloc(2) and treat this as an array of structures */
int	cont_flag;
int	timer_flag;
int	s;
int	ppid;
int	sleeptime;

char	sendstr[BUFSIZE], recvstr[BUFSIZE];

main (argc, argv, envp)
int	argc;
char	**argv, **envp;

{
extern	int	dumpargs(), re_eval_args(), quit(), abend();
nstruct	**process_args();

	
	narray = (nstruct **) process_args (argc, argv);

	if (! narray)
	  {
	  usage ();
	  exit (0);
	  }

	ppid = getppid ();
#ifndef	DEBUG
	if (fork()) exit ();
#endif

	s = open_irc_socket (IRCSERVERHOST, IRCPORT);
	signal (SIGUSR1, dumpargs);
	signal (SIGUSR2, dumpargs); /* well, shit happens :-)	*/

	signal (SIGFPE, re_eval_args);

/*
 * Now we set some bizzare signal settings here...
 * The first set are for a clean exit....they are received for
 * any number of reasons, too numerous to enumerate here...
 * The context in which they occur is obvious from the specific signal...
 */
	signal (SIGHUP, quit);
	signal (SIGINT, quit);
	signal (SIGQUIT, quit);
	signal (SIGTERM, quit);
	signal (SIGPIPE, quit);
/*
 * THe following signals are error flags...We believe the code is
 * provably correct, given the correctness of the IRC daemon ;-)
 * (reminds you of Floyd-Hoare, dunnit? :-)
 * Given the above assertions, then the following signals indicate a
 * disaster situation, requiring debugging, catch a guru for help if any
 * of these occur...
 *
 * But then, we have been proven wrong :-/...The next to last bug has
 * just been found...;-)	Sun Oct 22 13:50:53 EDT 1989
 */
	signal (SIGBUS, abend);
	signal (SIGSEGV, abend);
	signal (SIGSYS, abend);
	signal (SIGURG, abend);

	writeout ("ircw:", "Up, up and away...");
	while (kill (ppid, 0) == 0)
	  {
	  DEBUGOUT ("debug: %s\n", "loop");
	  chk_whois ();
	  if (dowho()) chk_who();
	  if (exitcode() == QUIT) break;
	  sleep (sleeptime);
	  }
	quit ();
/*NOTREACHED*/
}


copy_lower (string1, string2)
char	*string1, *string2;

{
char	boo;

	while (boo = *string2)
	  {
	  if (isupper (boo))
	    boo = tolower (boo);
	  *string1 = boo;
	  string1++; string2++;
	  }
	return 0;
}


chk_whois ()

{
char	command[100];
char	buffer[BUFSIZE];
nstruct	**ntemp, *nuser;

/*
 * Output format of the whois command....
ERROR :No such nickname
NOTICE  :jto is jto at tolsun (Jarkko Oikarinen) on channel 0
NOTICE  :On IRC via server tolsun.oulu.fi (Experimental IRC Server at tolsun.oulu.fi, Univers)
 */
	for (ntemp = narray, nuser = *ntemp; nuser; nuser = *++ntemp)
	  {
	  if (nuser->notnick) continue;
	  sprintf (command, WHOIS, nuser->lcased);
	  clbuf (s);
	  Strcpy (sendstr, command);
	  DEBUGOUT ("debug: sending %s\n", command);
	  SEND (s, command);
	  bzero (buffer, BUFSIZE);
	  while (! isready (s)) ;
	  read (s, buffer, BUFSIZE);
	  Strcpy (recvstr, buffer);
	  DEBUGOUT ("debug: received\n%s\n", buffer);
	  if (match (buffer, "No such nickname"))
	    {
	    if (cont_flag && (nuser->dfsa == ISON_IRC))
	      writeout (nuser->build, LOGOFFPROMPT);
	    nuser->dfsa = ISNOTON_IRC;
	    }
	  else /* match (buffer, "NOTICE") */
	    if (nuser->dfsa == ISNOTON_IRC)
	      {
	      format_whois (nuser->build, buffer);
	      writeout (nuser->build, LOGONPROMPT);
	      nuser->dfsa = ISON_IRC;
	      }
	  /* DRATS...we may have residual data...clear TFB	*/
	  clbuf (s);
	  }
	return 0;
}


chk_who ()

{
/*
 * The format of the who output is...
WHOREPLY * User Host Server Nickname S :Name
WHOREPLY 0 t36332e puukko.hut.fi taltta.hut.fi Alf H  :Teemu 'Alf' Rantanen
WHOREPLY 1 kts otax.tky.hut.fi otax.tky.hut.fi kts H  :Syd{nmaanlakka Kari Tapani
WHOREPLY 0 jto tolsun tolsun.oulu.fi jto H  :Jarkko Oikarinen
WHOREPLY 123 gl8f astsun2.astro.Virginia.EDU astsun2.acc.Virginia.EDU gm H  :The Wumpus GameMaster
 *
 * Note the there will always be atleast one line...
 *
 * OK, a few notes on what I am trying here....
 *	A string of the form `user@host' gets transformed to `user host',
 *	and we directly do a caseless comparison to the who output.
 *	This is not the same as strcasecmp, which gets mentioneds in
 *	X3J11 (does it?) or POSIX (huh?) or wherever, because a egrep
 *	like match is attempted.  We assume that WHOstuff is of the form
#define HDR	"WHOREPLY * User Host Server Nickname S :Name\n"
 *	If the WiZ Changes this, tough luck :-(
 *	If he does, the matching properties may fail.  Additionally,
 *	one has to chjange the scanf statement and the FMT define below
 *	appropriately.  Have fun, whomsoever...;-)
 */
#define HDR	"WHOREPLY * User Host Server Nickname S :Name\n"
#define	FMT	"%*s %*d %s %s %*s %s"
/*			 ^^ ^^     ^^	*/
/*			 usr host  nick	*/
/* Did you know that scanf() gets confused when given specific search
 * patterns?	*sigh*
#define	FMT	"%*s %*d %s %s %*s %s %s :%[a-zA-Z0-9]\n"
 */
nstruct	**ntemp, *nuser;
char	buffer[BUFSIZE], iobuf[BUFSIZE], lbuf[100], excess[100];
char	*line, *eoln;
char	user[10], host[50], nick[10], *name;
char	done;

	/* ok, so prime the b'stard :-) */
	clbuf (s);
	Strcpy (sendstr, WHO);
	SEND (s, WHO);
	for (ntemp = narray; *ntemp; *++ntemp)
	  (*ntemp)->substate = OOOPS;
	DEBUGOUT ("debug: wholist%c", '\n');
	while (! isready(s));
	read (s, buffer, strlen(HDR))	/* skip the Header	*/;
	DEBUGOUT ("debug: header\t%s", buffer);
	Strcpy (excess, "");
	while (isready (s))
	  {
	  Strcpy (buffer, excess);
	  bzero (iobuf, BUFSIZE);
	  read (s, iobuf, BUFSIZE - 101);
	  Strcat (buffer, iobuf);
	  Strcpy (recvstr, buffer);
	  line = eoln = buffer;
	  while (eoln = Index (line, NEWLINE))
	    {
	    *eoln = NIL;
	    bzero (lbuf, 100);
	    copy_lower (lbuf, line);
	    done = FALSE;
	    for (ntemp = narray, nuser = *ntemp;
			nuser && ! done;
			nuser = *++ntemp)
	      if (match (lbuf, nuser->lcased))
	        {
		  sscanf (line, FMT, user, host, nick);
		  name = Index (line, COLON) + 1;
	          Sprintf (nuser->build, "%s %s@%s (%s)",
			nick, user, host, name);
		  nuser->substate = GOTCHA;
/*
		done = TRUE;
 * What happens if I try "ircw -w username username"
 * ie...search for username among who, and as a nick via whois?
 * An endless loop...bah
 */
	        }
	    line = eoln + 1;
	    }
	  if (! eoln && line)
	    /* We have more to go, and have only received half some line */
	    Strcpy (excess, line);
	  }

	/* now user the dfsa and the substate to print messages	*/
	for (ntemp = narray, nuser = *ntemp; nuser; nuser = *++ntemp)
	  {
	  if ((nuser->dfsa == ISON_IRC) && (nuser->substate == OOOPS))
	    {
	    writeout (nuser->build, PROBLOGOFF);
	    nuser->dfsa = ISNOTON_IRC;
	    }
	  else if ((nuser->dfsa == ISNOTON_IRC) && (nuser->substate == GOTCHA))
	    {
	    writeout (nuser->build, LOGONPROMPT);
	    nuser->dfsa = ISON_IRC;
	    }
	  nuser->substate = OOOPS;
	  }
	return 0;
}


isready (fd)
int	fd;

{
struct	timeval	poll;
int	rmask[2];
	 
	poll.tv_sec = 1 /* just a few secs, dude */;
	poll.tv_usec = 0;
	rmask[0] = 1<<fd;
	rmask[1] = 0;
	return select (32, rmask, 0, 0, &poll);
}


match (string1, string2)
char	*string1, *string2;

{
char	*midway;
char	lchr;
int	len2;

	len2 = Strlen (string2);
	lchr = *string2;
	midway = string1;

	while ((midway = Index (midway, lchr)) != (char *) NULL)
	  {
	  if (Strncmp (midway, string2, len2) == 0)
	    return TRUE;
	  midway++;
	  }
	return FALSE;
}


nstruct	**
process_args (argc, argv)
int	argc;
char	**argv;

{
#define	SAME	0
int	whoflag, atleast1;
char	*avptr;
char	*atsign;
nstruct	**mynarray, **ntemp, *user;

	if (argc <= 1)
	  return (nstruct **) NULL;

     /*
      * mynarray is a rough measure...we return it anyhow
      * But yes, we would like it zeroed out, hence calloc() :-)
      */
	ntemp = mynarray = (nstruct **) calloc (argc, sizeof (nstruct *));
	if (ntemp == (nstruct **) NULL)
	  {
	  perror ("calloc failed: probably too many arguments");
	  exit (0);
	  }
	atleast1 = FALSE;
	cont_flag = CONT_FLAG;
	timer_flag = TIMER_FLAG;
	whoflag = FALSE;
	sleeptime = SLEEPTIME;
	for (avptr = *++argv; avptr ; avptr = *++argv)
	  {
	  switch (*avptr)
	    {
	    case '-':
	      switch (*(avptr + 1))
	        {
		case 'c':
			cont_flag = TRUE;
			break;
		case 'h':
			goto BLEAH;
		case 's':
			avptr = *++argv;	/* A Fake switcheroo */
			if (avptr == (char *) NULL) goto BLEAH;
			sleeptime = atoi (avptr);
			break;
		case 't':
			avptr = *++argv;	/* A Fake switcheroo */
			if (avptr == (char *) NULL) goto BLEAH;
			if (Strcmp (avptr, "24hr") == SAME)
			  timer_flag = X_24HR_CLK;
			else if (Strcmp (avptr, "12hr") == SAME)
			  timer_flag = X_12HR_CLK;
			else if (Strcmp (avptr, "none") == SAME)
			  timer_flag = X_NONE_CLK;
			else
			  timer_flag = TIMER_FLAG;
			break;
	        case 'w':
			whoflag = TRUE;
			break;
	        default:  goto BLEAH ; 
	        }
	      break;
	    case '+':
	      switch (*(avptr + 1))
		{
		case 'c':
			cont_flag = FALSE;
			break;
		default:  goto BLEAH;
		}
	      break;
	    default:
              user = (nstruct *) calloc (1, sizeof (nstruct));
	      if (user == (nstruct *) NULL)
		{
		perror ("calloc failed: probably too many arguments");
		goto BLEAH;
		}
	      atleast1 = TRUE;
	      copy_lower (user->lcased, avptr);
	      if (atsign = Index (user->lcased, '@'))
		*atsign = BLANK;
	      strcpy (user->build, "");
              user->dfsa = ISNOTON_IRC;
              user->notnick = whoflag;
	      user->substate = OOOPS;
	      whoflag = FALSE;
	      *ntemp = user;
	      ntemp++;
	      break;
            }
	  }

	if ((! atleast1) || (whoflag)) goto BLEAH;
	return mynarray;

BLEAH:
/* This is just to clean up and quit peacefully...
 * returning a NULL pointer flags the main routine to invoke usage(),
 * and exit() respectively...
 * Software engineers, purists and the like would probably say, why not
 * have used a function call, and structured code, but then, BLEAH to 
 * such people ;-)
 */
	for (ntemp = mynarray; (*ntemp); *++ntemp)
	  free (*ntemp);
	free (mynarray);
	return (nstruct **) NULL;
}


usage ()

{
	fprintf (stderr, "usage ircw [options] {<nick>| -w <string>}s\n");
	fprintf (stderr, "Legal Options are:\n");
	fprintf (stderr, "\t-c\t\t\trun in continous mode\n");
	fprintf (stderr, "\t+c\t\t\tturn off continous mode\n");
	fprintf (stderr, "\t-t {24hr|12hr|none}\tKind of timestamp you prefer\n");
	fprintf (stderr, "\t-h\t\t\tprint this help menu\n");
	fprintf (stderr, "\t<nick(s)>\t\ttrack given nick(s)\n");
	fprintf (stderr, "\t-w <string>\t\ttrack a given generic string\n");
	return 0;
}


exitcode ()

{
nstruct	**ntemp;

	if (cont_flag)
	  return STAY;
	
	for (ntemp = narray; *ntemp; *++ntemp)
	  if ((*ntemp)->dfsa == ISNOTON_IRC)
	    return STAY;
	
	return QUIT;
}


dowho ()

{
nstruct	**ntemp;

	for (ntemp = narray; *ntemp; *++ntemp)
	  if ((*ntemp)->notnick)
	    return TRUE;
	return FALSE;
}


open_irc_socket (host, port)
char	*host;
int	port;

{
struct	sockaddr_in	sin;
struct	hostent		*hp, *gethostbyname();
int	s;
char	error[100];

	bzero ((char *) &sin, sizeof sin);

	if (isdigit(*host))
	  sin.sin_addr.s_addr = inet_addr (host);
	else
	  {
	  if ((hp = gethostbyname (host)) == (struct hostent *) NULL)
	    {
	    Sprintf (error, "ircw: gethostbyname (%s) failed", host);
	    perror (error);
	    exit (0);
	    }
	  bcopy (hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
	  }
	
	sin.sin_family = AF_INET;
	sin.sin_port = htons (port);

	if ((s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror ("ircw: socket");
	  exit (0);
	  }
	
	if (connect (s, &sin, sizeof sin) < 0) {
	  perror ("ircw: connect");
	  exit (0);
	  }
	
	return s;
}


writeout (build, message)
char	*build, *message;

{
char	tmstamp[9];
#ifdef TIMESTAMP
struct	tm	*tstamp, *localtime();
long	clock, time();
int	hour;

	clock = time ((long *) 0);
	tstamp = localtime (&clock);
	switch (timer_flag) {
	  case X_12HR_CLK:
          	if ((hour = tstamp->tm_hour) > 11) hour -= 12;
          	if (hour == 0) hour = 12;
          	Sprintf (tmstamp, "%2d:%02d%s",
                        hour, tstamp->tm_min,
			(tstamp->tm_hour > 11) ? "pm" : "am");
		break;
	  case X_24HR_CLK:
	  	Sprintf (tmstamp, "%02d:%02d:%02d",
			tstamp->tm_hour, tstamp->tm_min, tstamp->tm_sec);
		break;
	  case X_NONE_CLK:
		Strcpy (tmstamp, "");
		break;
	  }
#else
	Strcpy (tmstamp, "");
#endif	TIMESTAMP
	fprintf (stderr, "%s%s%s %s%s\n",
				tmstamp, 
				(*tmstamp ? "  " : ""),
				build, message, PROMPT);
        fflush(stderr);
	return 0;
}


/*
 * signal code comes here
 */

abend (sig, code, scp)
int	sig, code;
struct	sigcontext	*scp;

{
	signal (SIGPIPE, SIG_DFL);
	if (kill (ppid, 0) != -1)
	  {
	  fprintf (stderr, "ircw: invalid signal %2d received .. aborting\n", sig);
	  fprintf (stderr, "Last command sent: %s\n", sendstr);
	  fprintf (stderr, "Recieved response: %s\n", recvstr);
	  }
	SEND (s, SIGNOFF);
	(void) wait (0);
	exit (0);
}


dumpargs()
/*
 * Invoke this routine on a USR1 or USR2 signal to dump the current
 * state of the structure `narray'.
 */

{
nstruct	**ntemp;

	if (kill (ppid, 0) == -1) quit ();
	fprintf (stderr, "Dumping intermediate states:\n");
	for (ntemp = narray; *ntemp; *++ntemp)
	  if ((*ntemp)->dfsa == ISNOTON_IRC)
	    fprintf (stderr, "\t%s%s\t\tis not on IRC now\n", 
			(*ntemp)->lcased,
			((*ntemp)->notnick ? "(W)" : ""));
	  else
	    fprintf (stderr, "\t%s%s\n",
			(*ntemp)->build,
			((*ntemp)->notnick ? " (W)" : ""));
	return 0;
}


quit ()

{
	signal (SIGPIPE, SIG_DFL);
	if (kill(ppid, 0) != -1) 
	  writeout ("ircw:", "exiting");
	SEND (s, SIGNOFF);
	(void) wait (0);
	exit (0);
}


re_eval_args ()
/*
 * Invoke this routine on a FPE signal, to re-check the consistency
 * of the stored structure `narray'
 */

{
	if (kill (ppid, 0) == -1) quit ();
	chk_whois ();
	if (dowho()) chk_who();
	return 0;
}


clbuf (s)
int	s;

{
char	buffer[BUFSIZE];

	  while (isready (s)) read (s, buffer, BUFSIZE);
	  return 0;
}



/*
 * The formatting command for the who and whois commands come here...
 */

format_whois (outstr, inbuf)
char	*outstr, *inbuf;

/*
 * Output format of the whois command....
ERROR :No such nickname
NOTICE  :jto is jto at tolsun (Jarkko Oikarinen) on channel 0
NOTICE  :On IRC via server tolsun.oulu.fi (Experimental IRC Server at tolsun.oulu.fi, Univers)
 */

{
#ifndef	NEWSERVER
char	*nick, *user, *host;
#else
char	nick[50], user[50],
	host[69] /* Doesn't the HR-RFC have something about this? */;
#endif
char	*name, *temp;

#ifndef NEWSERVER
        nick = Index (inbuf, COLON) + 1;
        temp = Index (nick, BLANK); *temp = NIL; temp++;
        user = Index (temp, BLANK) + 1;
        temp = Index (user, BLANK); *temp = NIL; temp++;
        host = Index (temp, BLANK) + 1;
        temp = Index (host, PAREN) + 1; *temp = NIL;
	name = (char *) NULL;
	Sprintf (outstr, "%s %s@%s", nick, user, host);
#else
/*
 * Output format of the new whois command....
whois kannan
:tolsun.oulu.fi 311  kannan kannan giza.cis.ohio-state.edu * :Kannan Varadhan
:tolsun.oulu.fi 312  giza.cis.ohio-state.edu :Ohio State University, Columbus
 */
#define	WHOIS_FMT	"%*s %*d %s %s %s"
	
	Sscanf (inbuf, WHOIS_FMT, nick, user, host);
	name = Index (inbuf+1, COLON) + 1;
	temp = Index (name, NEWLINE);
	*temp = NIL;
	Sprintf (outstr, "%s %s@%s (%s)", nick, user, host, name);
#endif
	return 0;
}


#ifdef NEVERINCLUDE
format_who()

{
fprintf (stderr, "READ THE COMMENTS ASSOCIATED WITH THE CHK_WHO ROUTINE\n");
}
#endif

#ifdef DEBUG
dbxhalt() { return write (2, "cont\n", 5); }
#endif
