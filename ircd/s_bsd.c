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

char s_bsd_id[] = "s_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef __hpux
#include <arpa/inet.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/errno.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "sock.h"	/* If FD_ZERO isn't define up to this point,  */
			/* define it (BSD4.2 needs this) */

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif

/*  ...should really start using *.h -files instead of these :-( --msa */
extern aClient me;
extern aClient *client;
extern aConfItem *find_conf(), *find_conf_host();
extern aConfItem *attach_confs(), *attach_confs_host(), *find_conf_name();
extern char *get_client_name();
extern char *my_name_for_link();
extern int portnum;
extern int debugtty, debuglevel;
extern long nextconnect;

extern int errno;

aClient	*local[MAXCONNECTIONS];
int	highest_fd = 0;

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

static char *add_local_domain(hname, size)
char *hname;
int size;
{
#ifdef RES_INIT
  /* try to fix up unqualified names */
  if (!index(hname, '.'))
    {
      if (!(_res.options & RES_INIT))
	{
	  debug(DEBUG_DNS,"res_init()");
	  res_init();
	}
      if (_res.defdname[0])
	{
	  strncat(hname, ".", size-1);
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
** report_error
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
static int report_error(text,cptr)
char *text;
aClient *cptr;
    {
	Reg1 int errtmp = errno; /* debug may change 'errno' */
	Reg2 char *host = cptr == NULL ? "" : get_client_name(cptr,FALSE);
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
		report_error("opening stream socket %s:%s",(aClient *)NULL);
		exit(-1);
	    }

#ifdef SO_REUSEADDR
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	  {
	    report_error("setsockopt %s:%s", (aClient *) 0);
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
			report_error("binding stream socket %s:%s",
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
	if (sock > highest_fd)
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

	for (fd = 0; fd < MAXCONNECTIONS; fd++)
		local[fd] = (aClient *)NULL;

        if (debugtty == 1)	/* don't close anything or fork if */
           return 0;		/* debugging is going to a tty */


	close(1); close(2);

	if (debugtty < 0)	/* fd 0 opened by inetd */
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
	return 0;
    }

void	write_pidfile()
    {
#ifdef IRCD_PIDFILE
	int fd;
	char buff[20];
	if ((fd = open(IRCD_PIDFILE, O_CREAT|O_WRONLY, 0600))>=0)
	    {
		bzero(buff, sizeof(buff));
		sprintf(buff,"%5d\n",getpid());
		if (write(fd, buff, strlen(buff)) == -1)
			debug(DEBUG_NOTICE,"Error writing to pid file %s",
			      IRCD_PIDFILE);
		close(fd);
		return;
	    }
	else
		debug(DEBUG_NOTICE,"Error opening pid file %s", IRCD_PIDFILE);
#endif
    }
		
/*
** non_ip_addresses
**             Check if any of the config lines contains
**             a non numeric IP-address.
*/
non_ip_addresses(confs)
     Link *confs;
{
  Link *tmp;

  for (tmp=confs; tmp; tmp=tmp->next)
    if (inet_addr(tmp->value.aconf->host) == -1)
      return 1;

  return 0;
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
  aConfItem *aconf = NULL;
  debug(DEBUG_NOTICE,"Checking access for host: %s", hname);
  if (cptr->confs)
      det_confs_butmask(cptr, 0);
  aconf = attach_confs_host(cptr, hname, flags);
  if (aconf)
    {
      /*
       ** This leaves the name that was actually used in
       ** accepting the connection into 'sockhost'
       */
      strncpyzt(cptr->sockhost, hname, elementsof(cptr->sockhost));
      if (!find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER |
		     CONF_NOCONNECT_SERVER))
	{
	  det_confs_butmask(cptr, 0);
	  aconf = (aConfItem *)NULL;
	}
      debug(DEBUG_NOTICE,"Access ok, host: %s", hname);
    }
  if (aconf == (aConfItem *)NULL)
      debug(DEBUG_NOTICE,"Access check failed, host: %s", hname);
  return (aconf) ? 0 : -1;
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

int check_access(cptr, flags, do_server)
aClient *cptr;
int flags, do_server;
{
  struct sockaddr_in addr;
  struct hostent *host = (struct hostent *)NULL;
  char *hname = (char *)NULL;
  unsigned char *t;
  int len = sizeof(struct sockaddr_in);
  int i = 0, usenameserver = 1;

  bzero(&addr, len);

  if (isatty(cptr->fd)) /* If descriptor is a tty, special checking... */
      return check_name(cptr, me.sockhost, flags)-1;
  if (getpeername(cptr->fd, &addr, &len) == -1)
    {
      report_error("Failed in connecting to %s :%s", cptr);
      return -2;
    }
  if (do_server)
    {
      if (IsUnknown(cptr) && !index(cptr->name, '*'))
	{
	  if (attach_confs(cptr, cptr->name, flags)==(aConfItem *)NULL)
	    {
	      strncpy(cptr->sockhost, inet_ntoa(addr.sin_addr),
			sizeof(cptr->sockhost));
	      return -1;
	    }
	}
      else if (IsConnecting(cptr) || IsHandshake(cptr))
	return (cptr->confs) ? 0 : -1;
    /*
     * Check if there is any meaning with doing nameserver lookups.
     * If all config lines only contain numeric IP-addresses there isn't
     * much meaning doing name server lookups that can cost long lags.
     */
      usenameserver = non_ip_addresses(cptr->confs);
    }
  *cptr->sockhost = '\0';
  /*
  ** If IP-address is localhost, try with the servers sockhost
  ** (as requested by EPS).
  **
  ** ...if ULTRIX doesn't have inet_netof(), then just make
  ** that function or #if this out for ULTRIX..  --msa
  */
  if ( (inet_netof(addr.sin_addr) == IN_LOOPBACKNET) &&
	check_name(cptr, me.sockhost, flags))
    return 0;

  /* To be able to fully utilize the benefits of having an special alias
   * for the host running ircd at a site, we do first a gethostbyname and
   * check if the socket address is among the returned. In that case
   * all is ok. This check should be the normal to pass...
   */
  if (do_server && find_conf_host(cptr->confs, cptr->name, flags))
    {
      if (host = gethostbyname(cptr->name))
        for(i = 0; host->h_addr_list[i]; i++)
	 {
	  debug(DEBUG_DNS,"Found host %s for %s",
		inet_ntoa(host->h_addr_list[i]), cptr->name);
	  if(!bcmp(host->h_addr_list[i], &(addr.sin_addr.s_addr),
		   sizeof(struct in_addr)))
	    {
	      strncpyzt(cptr->sockhost, cptr->name, sizeof(cptr->sockhost));
	      return 0;
	    }
	 }
    }

  if (usenameserver || cptr->confs == (Link *)NULL)
    {
      host = gethostbyaddr(&(addr.sin_addr), sizeof(struct in_addr),
			   addr.sin_family);
      if (host != NULL)
	{
	  char tmp[HOSTLEN];

	  i = 0;
	  for (hname = host->h_name; hname; hname = host->h_aliases[i++])
	    {
	      strncpyzt(tmp, hname, sizeof(tmp));
	      add_local_domain(tmp, sizeof(tmp) - strlen(tmp));
	      debug(DEBUG_DNS,"Got Name %s[%s] for %s",
		    hname, tmp, inet_ntoa(addr.sin_addr));
	      if (!check_name(cptr, tmp, flags))
		return 0;
	      if (!do_server && attach_confs(cptr, tmp, flags))
		{
		  strncpyzt(cptr->sockhost, tmp, sizeof(cptr->sockhost));
		  return 0;
		}
	    }
	}
    }
  /*
   ** Copy some name (first) to 'sockhost' just in case this will
   ** not be accepted. Then we have some valid name to put into the
   ** error message...
   */
  if (cptr->sockhost[0] == '\0')
      if (host)
	  strncpyzt(cptr->sockhost, host->h_name, sizeof(cptr->sockhost));
      else
	  strncpyzt(cptr->sockhost, inet_ntoa(addr.sin_addr),
		    sizeof(cptr->sockhost));
  /*
   ** Last ditch attempt: try with numeric IP-address
   */
  debug(DEBUG_DNS,"Last Check For Host %s", cptr->sockhost);
  if (!check_name(cptr,cptr->sockhost,flags))
    return 0;
  if (!isdigit(*cptr->sockhost) && do_server)
   {
     strncpyzt(cptr->sockhost, inet_ntoa(addr.sin_addr),
	       sizeof(cptr->sockhost));
     if (!check_name(cptr,cptr->sockhost,flags))
       return 0;
   }
  if (!do_server)
    attach_confs(cptr,cptr->sockhost,flags);
  else
    det_confs_butmask(cptr,0);
  return (cptr->confs) ? 0 : -1;
}

/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
static int completed_connection(cptr)
aClient *cptr;
{
  aConfItem *aconf;
  SetHandshake(cptr);
	
  /* Lots of code moved to connect_server.
   * This routine "can't" fail anymore...   meLazy
   */

  aconf = find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER);
  if (!BadPtr(aconf->passwd))
    sendto_one(cptr, "PASS %s", aconf->passwd);

  aconf = find_conf(cptr->confs, cptr->name, CONF_NOCONNECT_SERVER);
  sendto_one(cptr, ":%s SERVER %s 1 :%s",
		me.name, my_name_for_link(me.name, aconf), me.info);

  return (cptr->flags & FLAGS_DEADSOCKET) == 0;
}

/*
** close_connection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
close_connection(cptr)
aClient *cptr;
    {
        Reg1 aConfItem *aconf;
	Reg2 int i,j;

        if (IsServer(cptr))
		{
                if (aconf = find_conf_name(get_client_name(cptr,FALSE),
                                     CONF_CONNECT_SERVER))
               		aconf->hold = MIN(aconf->hold,
					  time(NULL) + ConfConFreq(aconf));
                }
	if (cptr->fd >= 0) {
		flush_connections(cptr->fd);
		local[cptr->fd] = (aClient *)NULL;
		close(cptr->fd);
		if (cptr->fd >= highest_fd)
			for (; highest_fd > 0; highest_fd--)
				if (local[highest_fd])
					break;
		cptr->fd = -1;
		DBufClear(&cptr->sendQ);
	}
	while (cptr->confs)
		detach_conf(cptr, cptr->confs->value.aconf);
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
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
static set_non_blocking(cptr)
aClient *cptr;
    {
	int res;

	/*
	** Do fcntl's return "-1" with F_GETFL in case of error?
	** Or, are the following error tests bogus??? --msa
	*/
	if ((res = fcntl(cptr->fd, F_GETFL, 0)) == -1)
		report_error("fcntl(fd,F_GETFL) failed for %s:%s",cptr);
	else if (fcntl(cptr->fd, F_SETFL, res | FNDELAY) == -1)
		report_error("fcntl(fd,F_SETL,FNDELAY) failed for %s:%s",cptr);
	return 0;
    }

aClient *add_connection(fd)
int fd;
{
  aClient *cptr;
  cptr = make_client((aClient *)NULL);

  /* Removed preliminary access check. Full check is performed in
   * m_server and m_user instead. Also connection time out help to
   * get rid of unwanted connections.
   */
  
  if (isatty(fd)) /* If descriptor is a tty, special checking... */
    strncpyzt(cptr->sockhost, me.sockhost, sizeof(cptr->sockhost));
  else
    {
      struct sockaddr_in addr;
      int len = sizeof(struct sockaddr_in);

      if (getpeername(fd, (struct sockaddr *) &addr, &len) == -1)
	{
	  report_error("Failed in connecting to %s :%s", cptr);
	  free(cptr);
	  return 0;
	}
      /* Copy ascii address to 'sockhost' just in case. Then we have
       * something valid to put into error messages...
       */
      strncpyzt(cptr->sockhost, inet_ntoa(addr.sin_addr),
		sizeof(cptr->sockhost));
    }

  cptr->fd = fd;
  if (fd > highest_fd)
     highest_fd = fd;
  local[fd] = cptr;
  add_client_to_list(cptr);
  set_non_blocking(cptr);
  return cptr;
}

read_message(buffer, buflen, from, delay)
     char *buffer;
     int buflen;
     aClient **from;
     long delay; /* Don't ever use ZERO here, unless you mean to poll and then
		    you have to have sleep/wait somewhere else in the code.
		    --msa */
{
  static long lastaccept = 0;
  Reg1 aClient *cptr;
  Reg2 int nfds;
  int res, length, fd, i;
  struct timeval wait;
  fd_set read_set, write_set;
  aConfItem *aconf;
  long tval, delay2 = delay, now;

  now = time(NULL);
  for (res = 0;;)
    {
      tval = now + 5; /* +x - # of messages allowed in bursts */
      FD_ZERO(&read_set);
      FD_ZERO(&write_set);
      if (AcceptNewConnections && (now > lastaccept))
	FD_SET(me.fd, &read_set);
      for (i = 0; i <= highest_fd; i++)
	if (cptr = local[i])
	  {
	    if (!IsMe(cptr)) {
	      if (cptr->since > tval) /* find time till we can next read
					 from this connection. -avalon */
		delay2 = cptr->since - tval;
	      else
		FD_SET(i, &read_set);
	    }
	    if (DBufLength(&cptr->sendQ) > 0 || IsConnecting(cptr))
	      FD_SET(i, &write_set);
	  }
      wait.tv_sec = MIN(delay2, delay);
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
      report_error("select %s:%s",(aClient *)NULL);
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
      lastaccept = now;
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
	  report_error("Cannot accept new connections %s:%s",
		      (aClient *)NULL);
	  AcceptNewConnections = FALSE;
	  break;
	}
      if (fd >= MAXCONNECTIONS) {
	close(fd);
	AcceptNewConnections = FALSE;
	break;
      }
      /* Reuse of add_connection (which do never fail)  meLazy
       */
      add_connection(fd);
      break; /* Really! This was not a loop, just faked "if" --msa */
    }	/* WARNING IS NORMAL HERE! */

  tval = now;

  for (i = 0; (i <= highest_fd) && nfds > 0; i++)
    {
      if (!(cptr = local[i]) || IsMe(cptr))
	continue;
      if (FD_ISSET(i, &write_set))
	{
	  nfds--;
	  if (IsConnecting(cptr) && !completed_connection(cptr))
	    {
	      exit_client(cptr,cptr,&me,"Failed connect?");
	      return 0; /* Cannot continue loop after exit */
	    }
	  /*
	   ** ...room for writing, empty some queue then...
	   */
	  send_queued(cptr);
	}
      if (!FD_ISSET(i, &read_set))
	continue;
      nfds--;
      if ((length = read(i, buffer, buflen)) > 0) {
	*from = cptr;
	cptr->since = cptr->lasttime = now;
	cptr->flags &= ~FLAGS_PINGSENT;
	dopacket(cptr, buffer, length);
	continue;
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
		       get_client_name(cptr,FALSE));
	  else
	    report_error("Lost server connection to %s:%s",
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
      if (cptr->confs && (aconf = cptr->confs->value.aconf) != NULL &&
	  aconf->status & CONF_CONNECT_SERVER)
	{
	  aconf->hold = now;
	  aconf->hold +=
	    (aconf->hold - cptr->since > HANGONGOODLINK) ?
	      HANGONRETRYDELAY : ConfConFreq(aconf);
	  if (nextconnect > aconf->hold)
	    nextconnect = aconf->hold;
	}
      exit_client(cptr, cptr, &me, "Bad link?");
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
	aClient *cptr = make_client((aClient *)NULL), *c2ptr;
	int errtmp;

	if (c2ptr = (aClient *)find_server(aconf->name, NULL))
	    {
		sendto_ops("Server %s already present from %s",
			   aconf->name, c2ptr->name);
		return -1;
	    }
	/* might as well get sockhost from here, the connection is attempted
	** with it so if it fails its useless.
	*/
	strncpy(cptr->sockhost, aconf->host, sizeof(cptr->sockhost));
	cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (cptr->fd < 0)
	    {
		report_error("opening stream socket to server %s:%s",cptr);
		free(cptr);
		return -1;
	    }
	server.sin_family = AF_INET;
	
	/* MY FIX -- jtrim@duorion.cair.du.edu  (2/10/89) */

	if (isdigit(*(aconf->host)) &&
	    (aconf->ipnum.s_addr == -1))  {
		aconf->ipnum.s_addr = inet_addr(aconf->host);
	} else if (aconf->ipnum.s_addr == -1) {
		hp = gethostbyname(aconf->host);
		if (hp == 0)
		    {
			close(cptr->fd);
			free(cptr);
			debug(DEBUG_FATAL, "%s: unknown host", aconf->host);
			return(-2);
		    }
		bcopy(hp->h_addr, &aconf->ipnum, sizeof(struct in_addr));
	    }
	server.sin_addr.s_addr = aconf->ipnum.s_addr;
	server.sin_port = htons((aconf->port > 0) ? aconf->port : portnum);
	
	/* End FIX */

	set_non_blocking(cptr);
	signal(SIGALRM, dummy);
	alarm(4);
	if (connect(cptr->fd, (struct sockaddr *)&server,
		    sizeof(server)) < 0 && errno != EINPROGRESS)
	    {
		alarm(0);
		errtmp = errno; /* sendto_ops may eat errno */
		report_error("Connect to host %s failed: %s",cptr);
		close(cptr->fd);
		free(cptr);
		if (errtmp == EINTR)
			errtmp = ETIMEDOUT;
		return(errtmp);
	    }
	alarm(0);

        /* Attach config entries to client here rather than in
         * completed_connection. This to avoid null pointer references
         * when name returned by gethostbyaddr matches no C lines
         * (could happen in 2.6.1a when host and servername differ).
         * No need to check access and do gethostbyaddr calls.
         * There must at least be one as we got here C line...  meLazy
         */
        attach_confs_host(cptr, aconf->host,
		       CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER);

	if (!find_conf_host(cptr->confs, aconf->host, CONF_NOCONNECT_SERVER))
	    {
      		sendto_ops("Host %s is not enabled for connecting (N-line)",
			   aconf->host);
		det_confs_butmask(cptr, 0);
		close(cptr->fd);
		free(cptr);
                return(-1);
	    }
	/*
	** The socket has been connected or connect is in progress.
	*/
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	local[cptr->fd] = cptr;
	SetConnecting(cptr);

	strncpyzt(cptr->sockhost, aconf->host, sizeof(cptr->sockhost));
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));
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
	sprintf(line, "ircd: Channel *: %s@%s (%s) %s\n\r",
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

get_my_name(conf_name, name, len)
char *conf_name, *name;
int len;
{
  struct hostent *hp;

  if (gethostname(name,len) < 0)
    return -1;
  name[len] = '\0';

  /* assume that a name containing '.' is a fully qualified domain name */
  if (!index(name,'.'))
    add_local_domain(name, len-strlen(name));

  /* If hostname gives another name than conf_name, then check if there is
   * a CNAME record for conf_name pointing to hostname. If so accept conf_name
   * as our name.   meLazy
   */
  if (!BadPtr(conf_name) && mycmp(conf_name, name))
    {
      if (hp = gethostbyname(conf_name))
	{
	  char tmp[HOSTLEN];
	  char *hname;
	  int i=0;

	  for (hname = hp->h_name; hname; hname = hp->h_aliases[i++])
	    {
	      strncpy(tmp, hname, sizeof(tmp));
	      add_local_domain(tmp, sizeof(tmp) - strlen(tmp));
	      if (!mycmp(tmp, conf_name))
		{
		  strncpy(name, conf_name, len);
		  break;
		}
	    }
	}
    }

  return (0);
}
