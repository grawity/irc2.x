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

/*
 * $Id: s_bsd.c,v 6.1 1991/07/04 21:05:25 gruner stable gruner $
 *
 * $Log: s_bsd.c,v $
 * Revision 6.1  1991/07/04  21:05:25  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:43  gruner
 * frozen beta revision 2.6.1
 *
 */

/* -- Jto -- 07 Jul 1990
 * Added jlp@hamblin.byu.edu's debugtty fix
 */

/* -- Armin -- Jun 18 1990
 * Added setdtablesize() for more socket connections
 * (sequent OS Dynix only) -- maybe select()-call must be changed ...
 */

/* -- Jto -- 13 May 1990
 * Added several fixes from msa:
 *   Better error messages
 *   Changes in check_access
 * Added SO_REUSEADDR fix from zessel@informatik.uni-kl.de
 */

char s_bsd_id[] = "s_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
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
#include "common.h"
#include "sys.h"
#include "numeric.h"
#ifdef NeXT
#include <arpa/nameser.h>
#include <resolv.h>
#endif
#include "sock.h"	/* If FD_ZERO isn't define up to this point,  */
			/* define it (BSD4.2 needs this) */

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif

/*  ...should really start using *.h -files instead of these :-( --msa */
extern aClient me;
extern aClient *client;
extern aConfItem *FindConf(), *attach_confs(), *FindConfName();
extern char *GetClientName();
extern char *MyNameForLink();
extern int portnum;
extern int debugtty;
extern long nextconnect;

extern int errno;

aClient	*local[MAXCONNECTIONS];
int	highest_fd = 0;

/*
** AddLocalDomain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

static char *AddLocalDomain(hname, size)
char *hname;
int size;
{
#ifdef RES_INIT
  /* try to fix up unqualified names */
  if (!index(hname, '.'))
    {
      if (!(_res.options & RES_INIT))
	res_init();
      if (_res.defdname[0])
	{
	  strncat(hname, ".", size);
	  strncat(hname, _res.defdname, size-2);
	}
    }
#endif
  return(hname);
}

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
	Reg1 int errtmp = errno; /* debug may change 'errno' */
	Reg2 char *host = cptr == NULL ? "" : GetClientName(cptr,FALSE);
	extern char *strerror();

	debug(DEBUG_ERROR,text,host,strerror(errtmp));
	sendto_ops(text,host,strerror(errtmp));
	return 0;
    }

int open_port(portnum)
int portnum;
    {
	int sock, length;
	int opt = 1;
	static struct sockaddr_in server;

	/* At first, open a new socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	    {
		ReportError("opening stream socket %s:%s",(aClient *)NULL);
		exit(-1);
	    }

#ifdef SO_REUSEADDR
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	  {
	    ReportError("setsockopt %s:%s", (aClient *) 0);
	    exit(-1);
	  }
#endif

	/* Bind a port to listen for new connections */
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(portnum);
	for (length = 0; length < 10; length++)
	    {
		if (bind(sock, (struct sockaddr *) &server, sizeof(server)))
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
	{ /* KLUDGE to get it work... */
	  char buf[1024];
	  int len = sizeof(server);
	  if (getsockname(sock, &server, &len)) {
	    exit(1);
	  }
	  sprintf(buf, ":%s %d %d :Port to local server is\n",
		  me.name, RPL_MYPORTIS, server.sin_port);
	  write(0, buf, strlen(buf));
	}
	highest_fd = sock;
	local[sock] = &me;
	listen(sock, 3);
	return(sock);
    }

init_sys()
    {
	int fd;
#ifdef sequent
	setdtablesize(SEQ_NOFILE);
#endif
#if !HPUX
	setlinebuf(stdout);
	setlinebuf(stderr);
#endif

        if (debugtty > 0)           /* don't close anything or fork if */
           return 0;                /* debugging is going to a tty */


	close(1); close(2);

	if (debugtty < 0)
	  return 0;

	if (isatty(0))
	    {
		close(0);
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
	else close (0);
	for (fd = 0; fd < MAXCONNECTIONS; fd++)
		local[fd] = (aClient *)NULL;
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
  if (aconf = attach_confs(cptr, hname, flags))
    {
      /*
       ** This leaves the name that was actually used in
       ** accepting the connection into 'sockhost'
       */
      if (cptr->sockhost[0] == '\0' ||
	  index(hname, '.') && !index(cptr->sockhost, '.'))
	strncpyzt(cptr->sockhost, hname, elementsof(cptr->sockhost));
      debug(DEBUG_DEBUG,"Access ok, host (%s)", hname);
      return 1;
    }
  debug(DEBUG_DEBUG,"Access checked");
  return 0;
}

/**
 ** check_access()
 ** Check if the other end of the connection is allowed to connect to
 ** this server.
 **
 ** returns -2, if no connection is open (closed before access check)
 **         -1, if no access allowed
 **          0, if access OK
 **/

static int check_access(cptr, flags)
aClient *cptr;
int flags;
{
  struct sockaddr_in name;
  struct hostent *host;
  char *hname;
  int len = sizeof(name);
  
  if (isatty(cptr->fd)) /* If descriptor is a tty, special checking... */
    {
      char hostbuf[HOSTLEN + 1];
      getmyname(hostbuf, HOSTLEN);
      hostbuf[HOSTLEN] = '\0';
      check_name(cptr, hostbuf, flags);
      if (cptr->confs)
	return 0;
      else
	return -1;
    }
  if (getpeername(cptr->fd, (struct sockaddr *) &name, &len) == -1)
    {
      ReportError("Failed in connecting to %s :%s", cptr);
      return -2;
    }
  /*
   ** If IP-address is localhost, try with the servers sockhost
   ** (as requested by EPS).
   **
   ** ...if ULTRIX doesn't have inet_netof(), then just make
   ** that function or #if this out for ULTRIX..  --msa
   */

  if (inet_netof(name.sin_addr) == IN_LOOPBACKNET)
    check_name(cptr,me.sockhost,flags);

  host = gethostbyaddr((char *)&(name.sin_addr),
		       sizeof(struct in_addr),AF_INET);
  if (host != NULL)
    {
      char tmp[HOSTLEN];
      int i = 0;
      
      for (hname = host->h_name; hname; hname = host->h_aliases[i++])
	{
	  strncpy(tmp, hname, sizeof(tmp));
	  AddLocalDomain(tmp, sizeof(tmp) - strlen(tmp));
	  check_name(cptr, tmp, flags);
	}
    }

  /*
   ** Last ditch attempt: try with numeric IP-address
   */

  check_name(cptr,inet_ntoa(name.sin_addr),flags);

  /*
   ** Copy some name (first) to 'sockhost' just in case this will
   ** not be accepted. Then we have some valid name to put into the
   ** error message...
   */
  if (cptr->sockhost[0] == '\0')
    strncpyzt(cptr->sockhost, inet_ntoa(name.sin_addr),
	      sizeof(cptr->sockhost));
  if (cptr->confs)
    return 0;
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
  aConfItem *aconf;
  cptr->status = STAT_HANDSHAKE;
	
  switch (check_access(cptr, CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER))
    {
    case -1:
      sendto_ops("Host %s is not enabled for connecting (N-line)",
		 GetClientName(cptr,FALSE));
      return FALSE;
    case 0:
      aconf = FindConf(cptr->confs, (char *) 0, CONF_CONNECT_SERVER);
      if (!BadPtr(aconf->passwd))
	sendto_one(cptr, "PASS %s", aconf->passwd);

      if (!(aconf = FindConf(cptr->confs, (char *) 0, CONF_NOCONNECT_SERVER)))
	return FALSE;

      sendto_one(cptr, "SERVER %s %s",
		 MyNameForLink(me.name, aconf), me.info);
      return (cptr->flags & FLAGS_DEADSOCKET) == 0;
    default:
      return FALSE;
    }
}

/*
** CloseConnection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
CloseConnection(cptr)
aClient *cptr;
    {
        Reg1 aConfItem *aconf;
	Reg2 int i,j;

        if (IsServer(cptr))
		{
                if (aconf = FindConfName(GetClientName(cptr,FALSE),
                                     CONF_CONNECT_SERVER))
               		aconf->hold = MIN(aconf->hold,
					  time(NULL) + ConfConFreq(aconf));
                }
	if (cptr->fd >= 0) {
		local[cptr->fd] = (aClient *)NULL;
		close(cptr->fd);
		if (cptr->fd >= highest_fd)
			for (; highest_fd > 0; highest_fd--)
				if (local[highest_fd])
					break;
		cptr->fd = -1;
		dbuf_clear(&cptr->sendQ);
	}
	while (cptr->confs)
		detach_conf(cptr, cptr->confs->value);
	cptr->from = NULL; /* ...this should catch them! >:) --msa */
	AcceptNewConnections = TRUE; /* ...perhaps new ones now succeed? */

	/*
	 * fd remap to keep local[i] filled at the bottom.
	 */
	for (i = 0, j = highest_fd; i < j; i++)
		if (!local[i]) {
			int	arg;

			if (fcntl(i, F_GETFL, &arg) != -1)
				continue;
			if (dup2(j,i) == -1)
				continue;
			local[i] = local[j];
			local[i]->fd = i;
			local[j] = (aClient *)NULL;
			close(j);
			for (; j > i && !local[j]; j--)
				;
		}
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
	return 0;
    }

aClient *AddConnection(fd)
int fd;
{
  aClient *cptr;
  cptr = make_client((aClient *)NULL);
  cptr->fd = fd;
  if (check_access(cptr, CONF_CLIENT | CONF_NOCONNECT_SERVER | CONF_LOCOP
		   | CONF_OPERATOR | CONF_CONNECT_SERVER) < 0) {
    free(cptr);
    return (aClient *) 0;
  }
  if (fd > highest_fd)
     highest_fd = fd;
  local[fd] = cptr;
  add_client_to_list(cptr);
  SetNonBlocking(cptr);
  return cptr;
}

ReadMessage(buffer, buflen, from, delay)
     char *buffer;
     int buflen;
     aClient **from;
     long delay; /* Don't ever use ZERO here, unless you mean to poll and then
		    you have to have sleep/wait somewhere else in the code. --msa */
{
  Reg1 aClient *cptr;
  Reg2 int nfds;
  int res, length, fd, i;
  struct timeval wait;
  fd_set read_set, write_set;
  aConfItem *aconf;
  long tval;
  
  for (res = 0;;)
    {
      tval = time (NULL) + 10; /* Allow bursts of 5 msgs (2 * 5 = 10) -SRB */
      FD_ZERO(&read_set);
      FD_ZERO(&write_set);
      if (AcceptNewConnections)
	FD_SET(me.fd, &read_set);
      for (i = 0; i <= highest_fd; i++)
	if (cptr = local[i])
	  {
	    if (!IsMe(cptr))
	      if (cptr->since > tval) /* To allow blocking an fd this 
					 is a shorter delay than usually */
		delay = 1;
	      else
		FD_SET(i, &read_set);
	    if (dbuf_length(&cptr->sendQ) > 0 ||
		IsConnecting(cptr))
	      FD_SET(i, &write_set);
	  }
      wait.tv_sec = delay;
      wait.tv_usec = 0;
      nfds = select(FD_SETSIZE, &read_set, &write_set,
		    (fd_set *) 0, &wait);
      if (nfds == 0 || (nfds == -1 && errno == EINTR))
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
      if (fd >= MAXCONNECTIONS) {
	close(fd);
	AcceptNewConnections = FALSE;
	break;
      }
      cptr = make_client((aClient *)NULL);
      cptr->fd = fd;
      if (check_access(cptr, CONF_CLIENT | CONF_NOCONNECT_SERVER | CONF_LOCOP
		       | CONF_OPERATOR | CONF_CONNECT_SERVER) >= 0)
	{
	  add_client_to_list(cptr);
	  if (fd > highest_fd)
	    highest_fd = fd;
	  local[fd] = cptr;
	  SetNonBlocking(cptr);
	  break;
	}
      /* ... if socket was closed (-2 from check_access), then
	 this message is also misleading, but it shouldn't
	 really happen... --msa */
      sendto_ops("Received unauthorized connection from %s.",
		 GetClientName(cptr,FALSE));
      close(fd);
      free(cptr);
      break; /* Really! This was not a loop, just faked "if" --msa */
    }	/* WARNING IS NORMAL HERE! */

  tval = time(NULL);

  for (i = 0; (i <= highest_fd) && nfds; i++)
    {
      if (!(cptr = local[i]) || IsMe(cptr))
	continue;
      if (FD_ISSET(i, &write_set))
	{
	  if (IsConnecting(cptr) && !CompletedConnection(cptr))
	    {
	      ExitClient(cptr,cptr,&me,"Failed connect?");
	      return 0; /* Cannot continue loop after exit */
	    }
	  /*
	   ** ...room for writing, empty some queue then...
	   */
	  SendQueued(cptr);
	  nfds--;
	}
      if (!FD_ISSET(i, &read_set))
	continue;
      nfds--;
      if ((length = read(i, buffer, buflen)) > 0) {
	*from = cptr;
	if (cptr->since < tval)
	  cptr->since = tval;
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
      if (cptr->confs &&
	  (aconf = (aConfItem *) cptr->confs->value) != NULL &&
	  aconf->status & CONF_CONNECT_SERVER)
	{
	  aconf->hold = time(NULL);
	  aconf->hold +=
	    (aconf->hold - cptr->since > HANGONGOODLINK) ?
	      HANGONRETRYDELAY : ConfConFreq(aconf);
	  if (nextconnect > aconf->hold)
	    nextconnect = aconf->hold;
	}
      ExitClient(cptr, cptr, &me, "Bad link?");
      continue;
      /* return 0; /* ...cannot continue loop, blocks freed! --msa */
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

	if (isdigit(*(aconf->host))) 
		server.sin_addr.s_addr = inet_addr(aconf->host);
	else 
	    {
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
	    }
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
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	local[cptr->fd] = cptr;
	cptr->status = STAT_CONNECTING;
	add_client_to_list(cptr);
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
	sprintf(line,"/dev/%s", linebuf);
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
	sprintf(line, "ircd: Channel %s: %s@%s (%s) %s\n\r",
		(who->user->channel) ? who->user->channel->chname : "0",
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
  if (!index(name, '.'))
    {
      if (hp = gethostbyname(name))
	{
	strncpy(name, hp->h_name, len);
	AddLocalDomain(name, len - strlen(name));
      	}
    }
  
  return (0);
}
