/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_bsd.c
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


char s_bsd_id[] = "s_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#if HPUX
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#if !HPUX
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/errno.h>
#if BSD42 || ULTRIX || HPUX
#include "sock.h"
#endif
#include "sys.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif

#define TRUE 1
#define FALSE 0

/*  ...should really start using *.h -files instead of these :-( --msa */
extern aClient me;
extern aClient *client;
extern aConfItem *find_conf();
extern attach_conf();
extern char *GetClientName();
extern int portnum;
extern int dummy();

/*
** AcceptNewConnections
**	If TRUE, select() will be enabled for the listening socket
**	(me.fd). If FALSE, it's not enabled and new connections are
**	not accepted. This is needed, because select() seems to
**	return "may do accept" even if the process has run out of
**	available file descriptors (fd's). Thus, the server goes
**	into a loop: select() OK -> accept() FAILS :(
*/
static int AcceptNewConnections = TRUE;

/*
** Cannot use perror() within daemon. stderr is closed in
** ircd and cannot be used. And, worse yet, it might have
** been reassigned to a normal connection...
*/

/* These definitions would probably be in <stdio.h>? --msa */

extern char *sys_errlist[]; /* Hopefull this exists on all systems --msa */
extern int errno;

/*
** ReportError
**	This a replacement for perror(). Record error to log and
**	also send a copy to all *LOCAL* opers online.
**
**	text	is a *format* string for outputting error. It must
**		contain only two '%s', the first will be replaced
**		by the sockhost from the cptr, and the latter will
**		be taken from sys_errlist[errno].
**
**	cptr	if not NULL, is the *LOCAL* client associated with
**		the error.
*/
static int ReportError(text,cptr)
char *text;
aClient *cptr;
    {
	register int errtmp = errno; /* debug may change 'errno' */
	register char *host = cptr == NULL ? "" : GetClientName(cptr,FALSE);

	debug(DEBUG_ERROR,text,host,sys_errlist[errtmp]);
	sendto_ops(text,host,sys_errlist[errtmp]);
	return 0;
    }

int open_port(portnum)
int portnum;
    {
	int sock, length;
	static struct sockaddr_in server;

	/* At first, open a new socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	    {
		ReportError("opening stream socket %s:%s",(aClient *)NULL);
		exit(-1);
	    }
	
	/* Bind a port to listen for new connections */
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(portnum);
	for (length = 0; length < 10; length++)
	    {
		if (bind(sock, &server, sizeof(server)))
		    {
			ReportError("binding stream socket %s:%s",
				    (aClient *)NULL);
			if (length >= 9)
				exit(-1);
			sleep(20);
		    }
		else
			break;
	    }
	listen(sock, 3);
	return(sock);
    }

init_sys()
    {
	int fd;
#ifndef TTYON
#if !HPUX
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif
	if (isatty(0))
	    {
		close (0); close(1); close(2);
		if (fork())
			exit(0);
#ifdef TIOCNOTTY
		if ((fd = open("/dev/tty", O_RDWR)) >= 0)
		    {
			ioctl(fd, TIOCNOTTY, (char *) 0);
			close(fd);
		    }
#else
		setpgrp(0,getpid());
#endif
	    }
	else
	    {
		close (0); close(1); close(2);
	    }
#endif
    }

/*
** check_name
**	Check whether any clients/daemons/operators
**	are allowed to enter from this host.
*/
static int check_name(cptr,hname,flags) /* Local for "check_access" */
aClient *cptr;
char *hname;
int flags;
    {
	aConfItem *aconf;

	debug(DEBUG_DEBUG,"Checking access for (%s) host", hname);
	if (aconf = find_conf(hname, NULL, NULL, flags))
	    {
		/*
		** This leaves the name that was actually used in
		** accepting the connection into 'sockhost'
		*/
		strncpyzt(cptr->sockhost, hname, elementsof(cptr->sockhost));
		debug(DEBUG_DEBUG,"Access ok, host (%s)", hname);
		return 1;
	    }
	/*
	** Copy some name (first) to 'sockhost' just in case this will
	** not be accepted. Then we have some valid name to put into the
	** error message...
	*/
	if (cptr->sockhost[0] == '\0')
		strncpyzt(cptr->sockhost, hname, sizeof(cptr->sockhost));
	debug(DEBUG_DEBUG,"Access checked");
	return 0;
    }

static int check_access(cptr, flags)
aClient *cptr;
int flags;
    {
	struct sockaddr_in name;
	struct hostent *host;
	char *hname;
	int len = sizeof(name);

	if (getpeername(cptr->fd, &name, &len) == -1)
	    {
		ReportError("check_access (getpeername) %s:%s",cptr);
		return -1;
	    }
	/*
	** If IP-address is localhost, try with the servers sockhost
	** (as requested by EPS).
	**
	** ...if ULTRIX doesn't have inet_netof(), then just make
	** that function or #if this out for ULTRIX..  --msa
	*/
#ifdef IN_LOOPBACKNET
	if (inet_netof(name.sin_addr) == IN_LOOPBACKNET &&
	    check_name(cptr,me.sockhost,flags))
		return 0;
#endif
	host = gethostbyaddr((char *)&(name.sin_addr),
			     sizeof(struct in_addr),AF_INET);
	if (host != NULL)
	    {
		int i = 0;

		for (hname = host->h_name; hname; hname = host->h_aliases[i++])
			if (check_name(cptr,hname,flags))
				return 0;
	    }
#if !ULTRIX
	/*
	** This funny ULTRIX side step here could be removed, if
	** someone finds 'inet_ntoa' for ULTRIX--I hear it's not
	** present there...  --msa
	*/

	/*
	** Last ditch attempt: try with numeric IP-address
	*/
	if (check_name(cptr,inet_ntoa(name.sin_addr),flags))
		return 0;
#endif
	return -1;
    }

/*
** CompletedConnection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
static int CompletedConnection(cptr)
aClient *cptr;
    {
	cptr->status = STAT_HANDSHAKE;
	if (check_access(cptr, CONF_NOCONNECT_SERVER) < 0)
	    {
		sendto_ops("Host %s is not enabled for connecting (N-line)",
			   GetClientName(cptr,FALSE));
		return FALSE;
	    }
	if (cptr->conf && !BadPtr(cptr->conf->passwd))
		sendto_one(cptr, "PASS %s",cptr->conf->passwd);
	sendto_one(cptr, "SERVER %s %s", me.name, me.info);
	return TRUE;
    }

/*
** CloseConnection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
CloseConnection(cptr)
aClient *cptr;
    {
	close(cptr->fd);
	cptr->fd = -1;
	dbuf_clear(&cptr->sendQ);
	detach_conf(cptr);
	cptr->from = NULL; /* ...this should catch them! >:) --msa */
	AcceptNewConnections = TRUE; /* ...perhaps new ones now succeed? */
    }

/*
** SetNonBlocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
static SetNonBlocking(cptr)
aClient *cptr;
    {
	int res;

	/*
	** Do fcntl's return "-1" with F_GETFL in case of error?
	** Or, are the following error tests bogus??? --msa
	*/
	if ((res = fcntl(cptr->fd, F_GETFL, 0)) == -1)
		ReportError("fcntl(fd,F_GETFL) failed for %s:%s",cptr);
	else if (fcntl(cptr->fd, F_SETFL, res | FNDELAY) == -1)
		ReportError("fcntl(fd,F_SETL,FNDELAY) failed for %s:%s",cptr);
    }

ReadMessage(buffer, buflen, from, delay)
char *buffer;
int buflen;
aClient **from;
long delay; /* Don't ever use ZERO here, unless you mean to poll and then
	       you have to have sleep/wait somewhere else in the code. --msa */
    {
	struct timeval wait;
	int nfds, res, length, fd;
	fd_set read_set, write_set;
	aClient *cptr;
	aConfItem *aconf;

	for (res = 0;;)
	    {
		wait.tv_sec = delay;
		wait.tv_usec = 0;
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		for (cptr = client; cptr; cptr = cptr->next) 
			if (cptr->fd >= 0)
			    {
				if (!IsMe(cptr) || AcceptNewConnections)
					FD_SET(cptr->fd, &read_set);
				if (dbuf_length(&cptr->sendQ) > 0 ||
				    IsConnecting(cptr))
					FD_SET(cptr->fd, &write_set);
			    }
		nfds = select(FD_SETSIZE, &read_set, &write_set,
			      (fd_set *) 0, &wait);
		if (nfds == 0)
			return 0;
		else if (nfds > 0)
			break;
		/*
		** ...it comes here also, when SIGNAL's have
		** occurred. Perhaps should actually check the
		** error code...? ---msa
		*/
		ReportError("select %s:%s",(aClient *)NULL);
		res++;
		if (res > 5)
			restart();
		sleep(10);
	    }

	/* This actually "if ()..", but now I can use "break" within
	   the body. Not happy with this, but... --msa */

	while (FD_ISSET(me.fd, &read_set))
	    {
		nfds--;
		if ((fd = accept(me.fd, (struct sockaddr *)0, (int *)0)) < 0)
		    {
			/*
			** There may be many reasons for error return, but
			** in otherwise correctly working environment the
			** probable cause is running out of file descriptors
			** (EMFILE, ENFILE or others?). The man pages for
			** accept don't seem to list these as possible,
			** although it's obvious that it may happen here.
			** Thus no specific errors are tested at this
			** point, just assume that connections cannot
			** be accepted until some old is closed first.
			*/
			ReportError("Cannot accept new connections %s:%s",
				    (aClient *)NULL);
			AcceptNewConnections = FALSE;
			break;
		    }
		cptr = make_client((aClient *)NULL); /* Local client */
		cptr->fd = fd;
		if (check_access(cptr,
				 CONF_CLIENT | CONF_NOCONNECT_SERVER) >= 0)
		    {
			cptr->next = client;
			client = cptr;
			SetNonBlocking(cptr);
			break;
		    }
		sendto_ops("Received unauthorized connection from %s.",
			   GetClientName(cptr,FALSE));
		close(fd);
		free(cptr);
		break; /* Really! This was not a loop, just faked "if" --msa */
	    }

	for (cptr = client; cptr && nfds; cptr = cptr->next)
	    {
		if (cptr->fd<0 || IsMe(cptr))
			continue;
		if (FD_ISSET(cptr->fd, &write_set))
		    {
			if (IsConnecting(cptr) && !CompletedConnection(cptr))
			    {
				ExitClient(cptr,cptr);
				return 0; /* Cannot continue loop after exit */
			    }
			/*
			** ...room for writing, empty some queue then...
			*/
			SendQueued(cptr);
			nfds--;
		    }
		if (!FD_ISSET(cptr->fd, &read_set))
			continue;
		nfds--;
		if ((length = read(cptr->fd, buffer, buflen)) > 0)
		    {
			*from = cptr;
			return (length);
		    }
		/*
		** ...hmm, with non-blocking sockets we might get
		** here from quite valid reasons, although.. why
		** would select report "data available" when there
		** wasn't... so, this must be an error anyway...  --msa
		*/
		if (IsServer(cptr) || IsHandshake(cptr))
		    {
			if (length == 0)
				sendto_ops("Server %s closed the connection",
					   GetClientName(cptr,FALSE));
			else
				ReportError("Lost server connection to %s:%s",
					    cptr);
		    }
		else
			debug(DEBUG_DEBUG,"READ ERROR: fd = %d", cptr->fd);

		/*
		** Reschedule a faster reconnect, if this was a automaticly
		** connected configuration entry. (Note that if we have had
		** a rehash in between, the status has been changed to
		** CONF_ILLEGAL). But only do this if it was a "good" link.
		*/
		if ((aconf = cptr->conf) != NULL &&
		    aconf->status == CONF_CONNECT_SERVER)
		    {
			aconf->hold = getlongtime();
			aconf->hold +=
				(aconf->hold - cptr->since > HANGONGOODLINK) ?
					HANGONRETRYDELAY : CONNECTFREQUENCY;
		    }
		ExitClient(cptr, cptr);
		return 0; /* ...cannot continue loop, blocks freed! --msa */
		/*
		** Another solution would be to restart the loop from the
		** beginning. Then we would have to issue FD_CLR for sockets
		** which have been already read... --msa
		*/
	    }
	return -1;
    }    


connect_server(aconf)
aConfItem *aconf;
    {
	struct sockaddr_in server;
	struct hostent *hp;
	aClient *cptr = make_client((aClient *)NULL);
	int errtmp;
	char *tmp;
	extern int errno;

	strncpyzt(cptr->sockhost, aconf->host, sizeof(cptr->sockhost));
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));

	cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (cptr->fd < 0)
	    {
		ReportError("opening stream socket to server %s:%s",cptr);
		free(cptr);
		return -1;
	    }
	server.sin_family = AF_INET;
	
	/* MY FIX -- jtrim@duorion.cair.du.edu  (2/10/89) */
#if !ULTRIX
	if (isdigit(*(aconf->host))) 
		server.sin_addr.s_addr = inet_addr(aconf->host);
	else 
	    {
#endif
		tmp = aconf->host;
		hp = gethostbyname(tmp);
		if (hp == 0)
		    {
			close(cptr->fd);
			free(cptr);
			debug(DEBUG_FATAL, "%s: unknown host", aconf->host);
			return(-2);
		    }
		bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
#if !ULTRIX
	    }
#endif
	server.sin_port = htons((aconf->port > 0) ? aconf->port : portnum);
	
	/* End FIX */

	SetNonBlocking(cptr);
	signal(SIGALRM, dummy);
	alarm(4);
	if (connect(cptr->fd, (struct sockaddr *)&server,
		    sizeof(server)) < 0 && errno != EINPROGRESS)
	    {
		alarm(0);
		errtmp = errno; /* sendto_ops may eat errno */
		ReportError("Connect to host %s failed: %s",cptr);
		close(cptr->fd);
		free(cptr);
		if (errtmp == EINTR)
			errtmp = ETIMEDOUT;
		return(errtmp);
	    }
	alarm(0);
	/*
	** The socket has been connected or connect is in progress.
	*/
	cptr->status = STAT_CONNECTING;
	cptr->next = client;
	client = cptr;
	attach_conf(aconf,cptr);
	return 0;
    }

int utmp_open()
{
  return (open(UTMP,O_RDONLY));
}

int utmp_read(fd, name, line, host)
int fd;
char *name, *line, *host;
    {
	struct utmp ut;
	while (read(fd, &ut, sizeof (struct utmp)) == sizeof (struct utmp))
	    {
		strncpyzt(name,ut.ut_name,9);
		strncpyzt(line,ut.ut_line,10);
#ifdef USER_PROCESS
#	if HPUX
		strncpyzt(host,(ut.ut_host[0]) ? (ut.ut_host) : me.name, 16);
#	else
		strncpyzt(host, me.name, 9);
#	endif
		if (ut.ut_type == USER_PROCESS)
			return 0;
#else
		strncpy(host,(ut.ut_host[0]) ? (ut.ut_host) : me.name, 16);
		if (ut.ut_name[0])
			return 0;
#endif
	    }
	return -1;
    }

int utmp_close(fd)
int fd;
    {
	return(close(fd));
    }

summon(who, namebuf, linebuf)
aClient *who;
char *namebuf, *linebuf;
    {
	int fd;
	char line[120], *wrerr = "NOTICE %s :Write error. Couldn't summon.";
	if (strlen(linebuf) > 9)
	    {
		sendto_one(who,"NOTICE %s :Serious fault in SUMMON.",
			   who->name);
		sendto_one(who,
			   "NOTICE %s :linebuf too long. Inform Administrator",
			   who->name);
		return -1;
	    }
	/* Following line added to prevent cracking to e.g. /dev/kmem if */
	/* UTMP is for some silly reason writable to everyone... */
	if ((linebuf[0] != 't' || linebuf[1] != 't' || linebuf[2] != 'y')
	    && (linebuf[0] != 'c' || linebuf[1] != 'o' || linebuf[2] != 'n')
#if HPUX
	    && (linebuf[0] != 'p' || linebuf[1] != 't' || linebuf[2] != 'y' ||
		linebuf[3] != '/')
#endif
	    )
	    {
		sendto_one(who,
		      "NOTICE %s :Looks like mere mortal souls are trying to",
			   who->name);
		sendto_one(who,"NOTICE %s :enter the twilight zone... ",
			   who->name);
		debug(0, "%s (%s@%s, nick %s, %s)",
		      "FATAL: major security hack. Notify Administrator !",
		      who->user->username, who->user->host,
		      who->name, who->info);
		return -1;
	    }
	strcpy(line,"/dev/");
	strcat(line,linebuf);
	alarm(5);
	if ((fd = open(line, O_WRONLY | O_NDELAY)) == -1)
	    {
		alarm(0);
		sendto_one(who,
			   "NOTICE %s :%s seems to have disabled summoning...",
			   who->name, namebuf);
		return -1;
	    }
	alarm(0);
	strcpy(line,
	       "\n\r\007ircd: You are being summoned to Internet Relay Chat by\n\r");
	alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		alarm(0);
		close(fd);
		sendto_one(who,wrerr,who->name);
		return -1;
	    }
	alarm(0);
	sprintf(line, "ircd: Channel %d: %s@%s (%s) %s\n\r",
		who->user->channel,
		who->user->username, who->user->host, who->name, who->info);
	alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		alarm(0);
		close(fd);
		sendto_one(who,wrerr,who->name);
		return -1;
	    }
	alarm(0);
	strcpy(line,"ircd: Respond with irc\n\r");
	alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		alarm(0);
		close(fd);
		sendto_one(who,wrerr,who->name);
		return -1;
	    }
	close(fd);
	alarm(0);
	sendto_one(who, "NOTICE %s :Summoning user %s to irc",
		   who->name, namebuf);
	return 0;
    }

getmyname(name, len)
char *name;
int len;
    {
	struct hostent *hp;
	
	if (gethostname(name,len) < 0)
		return -1;
	name[len] = '\0';
	
	/* assume that a name containing '.' is a fully qualified domain name */
	if (index(name, '.') == (char *) 0)
	    {
		if ((hp = gethostbyname(name)) != (struct hostent *) 0)
			strncpy(name, hp->h_name, len);
	    }
	
	return (0);
    }

long getlongtime()
    {
	return(time(0));
    }
