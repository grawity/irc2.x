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

/* The amount of time to sleep between checks?				*/
#define	SLEEPTIME	30

/* An obnoxious BEEP BEEP BEEP to output with every line of output	*/
#define PROMPT		"\007\007\007"

/* What to say when someone is sighted, or not sighted?
 * Previously, I had
 *		"has just got on IRC"
 *		"has just left IRC"
 *		"has probably left IRC" (in the who  searches, this
 *					 possibility of doubt exists)
 */
#define	LOGONPROMPT	"SIGNON"
#define	LOGOFFPROMPT	"SIGNOFF"
#define	PROBLOGOFF	"SIGNOFF (probably)"

/*
 * Do we run in continous mode, or exit after all argumentators
 * have been located once?
 *
 * It can be triggerred by a [-|+]cont command line option, so
 * choose whatever, without too much panicing...;-)
 */
#define	CONT_FLAG	FALSE	/* Means Do not run in continous mode	*/

/* Obvious, the default irc port number at your site....		*/
#define	IRCPORT		6667

/* Obvious, the location of the server you want to connect to...	*/
/* Not so obvious, use either the hostname, or the internet number for	*/
/* this option, just ensure that you leave it as a string		*/
#define	IRCSERVERHOST	"128.189.102.10"

/* Do you want your messages timestamped? just for the heck of it? :-)	*/
#define	TIMESTAMP

/* Options to displaying the clock				*/
#define	TIMER_FLAG	X_24HR_CLK	/* Kind of timestamp to use	*/
#define	X_24HR_CLK	0		/* Leave well enough alone	*/
#define	X_12HR_CLK	1		/* Leave well enough alone	*/
#define	X_NONE_CLK	2		/* Leave well enough alone	*/

/* The current whois format is due to change.  When you get a new	*/
/* server, define the statement below, and you get the new one.		*/
/* Hopefully, this won't happen too much....sigh			*/
/* In any case, We currently don't think we are going to worry about	*/
/* this sequence beyond this, sorry.  As and when this happens, modify	*/
/* the string routines routine `format_whois()' in ircwatch.c and	*/
/* recompile	*sigh*							*/
/* The current format changes with experimental versions and 2.1.1	*/
#define	NEWSERVER

#undef NEVERINCLUDE	/* Again Obviously :-)	*/

#ifdef	EBUG
#define	DEBUG
#endif

/* STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP */
/*
 * The rest of the stuff is buffer management, don't tweak arbitrarily,
 * unless you know what you are doing, i.e. you have looked at the code :-)
 */
#define	STRSIZE		 64	/* length of a user supplied argument	*/
#define	WKSIZE		128	/* length of a string we will print out	*/
#define	BUFSIZE	1024	/* Size of buffer used for ircd list	*/

#define	FALSE	0
#define	TRUE	! FALSE

#define	SEND(fd,string)		write (fd, string, Strlen(string))
#define	ERROR(string)			\
		{			\
		perror (string);	\
		exit (0);		\
		}


#define	BLANK	' '
#define	COLON	':'
#define	NEWLINE	'\n'
#define	NIL	'\0'	/* NOT NULL, NIL, as in NIL_terminated string	*/
#define	PAREN	')'

#define	ISON_IRC	42
#define	ISNOTON_IRC	19
#define	ISA_HOSTNAME	84
#define	OOOPS		96	/* Any 2 damn values, see :-)	*/
#define	GOTCHA		69

#define	QUIT	0
#define	STAY	-1


#ifdef	DEBUG
#define	DEBUGOUT(FMT,VAR)	fprintf (stderr, FMT, VAR)
#else
#define	DEBUGOUT(FMT,VAR)
#endif

#define	SIGNOFF		"quit\n"
#define	WHO		"who\n"
#define	WHOIS		"whois %s\n"

#define HOSTLEN         80
