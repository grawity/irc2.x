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
#include <sys/un.h>
#if defined(__hpux)
#include "inet.h"
#endif
#include "netdb.h"
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include "nameser.h"
#include "resolv.h"
#include "sock.h"	/* If FD_ZERO isn't define up to this point,  */
			/* define it (BSD4.2 needs this) */

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif
#ifdef RLIMIT_FDMAX
#define	RLIMIT_FD_MAX	RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  undef RLIMIT_FD_MAX
# endif
#endif

/*  ...should really start using *.h -files instead of these :-( --msa */
extern	aClient me, *client;
extern	aClient *find_server PROTO((char *, aClient *));
extern	aConfItem *find_conf PROTO((Link *, char*, int));
extern	aConfItem *find_conf_host PROTO((Link *, char *, int));
extern	aConfItem *attach_confs PROTO((aClient*, char *, int));
extern	aConfItem *attach_confs_host PROTO((aClient*, char *, int));
extern	aConfItem *find_conf_name PROTO((char *, int));
extern	char	*get_client_name PROTO((aClient *, int));
extern	char	*my_name_for_link PROTO((char *, aConfItem *));
extern	int	portnum, debugtty, debuglevel;
extern	long	nextconnect;

extern	int errno;

aClient	*local[MAXCONNECTIONS];
int	highest_fd = 0, readcalls = 0;

struct	sockaddr *connect_unix_server PROTO((aConfItem *, aClient *, int *));
struct	sockaddr *connect_inet_server PROTO((aConfItem *, aClient *, int *));
static	int	completed_connection PROTO((aClient *));

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

static	char	*add_local_domain(hname, size)
char	*hname;
int	size;
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
static	int	AcceptNewConnections = TRUE;

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
static	int	report_error(text,cptr)
char	*text;
aClient *cptr;
    {
	Reg1 int errtmp = errno; /* debug may change 'errno' */
	Reg2 char *host = cptr == NULL ? "" : get_client_name(cptr,FALSE);
	extern char *strerror();

	debug(DEBUG_ERROR,text,host,strerror(errtmp));
	sendto_ops(text,host,strerror(errtmp));
#ifdef USE_SYSLOG
	syslog(LOG_WARNING,text,host,strerror(errtmp));
#endif
	return 0;
    }

int	open_port(portnum)
int	portnum;
    {
	int	sock, length, sock2;
	int	opt = 1;
	static	struct sockaddr_in server;

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
	  if (getsockname(sock, &server, &len))
	  	exit(1);

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

#ifdef	UNIXPORT
int	unixport(port, cptr)
int	port;
aClient	*cptr;
{
	struct sockaddr_un un;
	char	path[80];

	cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	sprintf(path, "%s/%d", UNIXPORTPATH, port);
	unlink(path);
	strcpy(un.sun_path, path);
	if (bind(cptr->fd, (struct sockaddr *) &un, strlen(path) + 2))
	    {
		perror("bind unix");
		return -1;
	    }
	else
	    {
		if (cptr->fd > highest_fd)
			highest_fd = cptr->fd;
		listen(cptr->fd, 3);
		SetMe(cptr);
		cptr->flags |= FLAGS_UNIX;
		cptr->from = cptr;
		strcpy(cptr->sockhost, me.sockhost);
		strcpy(cptr->name, me.name);
		local[cptr->fd] = cptr;
	    }
	return 0;
}
#endif

int	init_sys(bootopt)
int	bootopt;
    {
	struct rlimit limit;
	int fd;
#ifdef sequent
	setdtablesize(SEQ_NOFILE);
#endif
#ifdef PCS
	char logbuf[BUFSIZ];

	setvbuf(stdout,logbuf,_IOLBF,sizeof(logbuf));
	setvbuf(stderr,logbuf,_IOLBF,sizeof(logbuf));
#else
#ifndef HPUX
	setlinebuf(stdout);
	setlinebuf(stderr);
# endif
#endif

#ifdef RLIMIT_FD_MAX
	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	    {
		if (limit.rlim_max < MAXCONNECTIONS)
		    {
			fprintf(stderr,"ircd fd table too big\n");
			fprintf(stderr,"Hard Limit: %d IRC max: %d\n",
				limit.rlim_max, MAXCONNECTIONS);
			fprintf(stderr,"Fix MAXCONNECTIONS in config.h\n");
			exit(-1);
		    }
		limit.rlim_cur = MAXCONNECTIONS;
		setrlimit(RLIMIT_FD_MAX, &limit);
	    }
#endif

	for (fd = 0; fd < MAXCONNECTIONS; fd++)
		local[fd] = (aClient *)NULL;

        if (debugtty == 1)	/* don't close anything or fork if */
           return 0;		/* debugging is going to a tty */


	close(1); close(2);

	if (debugtty < 0)	/* fd 0 opened by inetd */
	  return 0;

	if (isatty(0) || bootopt & 1)
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
int non_ip_addresses(confs)
     Link *confs;
{
	Link *tmp;

	for (tmp=confs; tmp; tmp=tmp->next)
		if (inet_addr(tmp->value.aconf->host) == -1)
			return 1;

  return 0;
}

/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static	int	check_init(cptr, sockn, full, socka)
Reg1	aClient	*cptr;
Reg2	char	*full, *sockn;
Reg3	struct	sockaddr_in *socka;
{
	int	len = sizeof(struct sockaddr_in);

#ifdef	UNIXPORT
	if (cptr->flags & FLAGS_UNIX)
	    {
		strcpy(full, me.sockhost);
		strcpy(sockn, me.sockhost);
		strcpy(cptr->sockhost, me.sockhost);
		bcopy(&cptr->ip, &socka->sin_addr, sizeof(struct in_addr));
		socka->sin_family = AF_INET;

		return 0;
	    }
#endif

	/* If descriptor is a tty, special checking... */
	if (isatty(cptr->fd))
	    {
		strncpy(full, me.sockhost, HOSTLEN);
		bzero(socka, sizeof(struct sockaddr_in));
	    }
	else if (getpeername(cptr->fd, socka, &len) == -1)
	    {
		report_error("connect failure: %s :%s", cptr);
		return -1;
	    }
	strcpy(sockn, (char *)inet_ntoa(socka->sin_addr));
	if (inet_netof(socka->sin_addr) == IN_LOOPBACKNET)
		strncpy(full, me.sockhost, HOSTLEN);
	else
		strcpy(full, sockn);
	bcopy(&socka->sin_addr, &cptr->ip, sizeof(struct in_addr));

	return 0;
}

#ifdef	CHECK_GETHOST
struct	hostent	*gethost(sp, len, fam)
void	*sp;
int	len, fam;
{
	struct	hostent	*hp = NULL, hps, *hp2 = NULL;
	int	stop = 0;

	hp = gethostbyaddr(sp, len, fam);
	if (!hp)
		return NULL;

	hp2 = gethostbyname(hp->h_name);
	if (!hp2)
		return NULL;

	for (i = 0; !stop && hp->h_addr_list[i]; i++)
		for (j = 0; !stop && hp2->h_addr_list[j]; j++)
			if (!bcmp(hp->h_addr_list[i],
				  hp2->h_addr_list[j], sizeof(long))
				stop = 1;
	if (stop)
		return hp;
	return NULL;
}
/* quick hack so furhter #ifdef's arent needed */
#define	gethostbyaddr	gethost
#endif
/*
 * Ordinary client access check. Look for conf lines which have the same
 * status as the flags passed.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int	check_client(cptr, flags)
aClient	*cptr;
int	flags;
{
	Reg1	char	*name;
	Reg2	aConfItem *aconf;
	char	sockname[16], fullname[HOSTLEN+1];
	struct	hostent *hp;
	struct	sockaddr_in sk;
	int	i = 0;
 
	debug(DEBUG_DNS, "ch_cl: check access for %s[%s]",
		cptr->name, inet_ntoa(cptr->ip));

	if (check_init(cptr, sockname, fullname, &sk))
		return -2;

	hp = gethostbyaddr(&(sk.sin_addr), sizeof(struct in_addr),
			   sk.sin_family);

	/* attach I-lines using hostname first, then check ip#'s. */
	if (hp)
		for (name = hp->h_name; name ; name = hp->h_aliases[i++])
		    {
			strncpy(fullname, name, sizeof(fullname)-1);
			add_local_domain(fullname,HOSTLEN-strlen(fullname));
			debug(DEBUG_DNS, "ch_cl: gethostbyaddr: %s->%s",
				sockname, fullname);

			if (!attach_confs_host(cptr, fullname, flags))
				attach_confs(cptr, fullname, flags);
			if (cptr->confs)
				break;
		    }

	if (!cptr->confs)
	    {
		strcpy(fullname, sockname);
		if (!attach_confs_host(cptr, fullname, flags))
			attach_confs(cptr, fullname, flags);
	    }

	strcpy(cptr->sockhost, fullname);
	if (!cptr->confs)
	    {
		debug(DEBUG_DNS,"ch_cl: access denied: %s[%s]",
			cptr->name, fullname);
		return -1;
	    }
	aconf = cptr->confs->value.aconf;
	det_confs_butmask(cptr, 0);
	attach_conf(cptr, aconf);
	debug(DEBUG_DNS, "ch_cl: access ok: %s[%s]", cptr->name, fullname);

	return 0;
}

#define	CFLAG	CONF_CONNECT_SERVER
#define	NFLAG	CONF_NOCONNECT_SERVER
/*
 * check_server
 *	check access for a server given its name (passed in cptr struct).
 *	Must check for all C/N lines which have a name which matches the
 *	name given and a host which matches. A host alias which is the
 *	same as the server name is also acceptable in the host field of a
 *	C/N line.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int	check_server(cptr)
aClient	*cptr;
{
	Reg1	char	*name;
	char	sockname[16], fullname[HOSTLEN+1];
	Reg2	aConfItem *c_conf = NULL, *n_conf = NULL;
	Reg3	struct hostent *hp = NULL;
	struct sockaddr_in sk;
	Reg4	int i = 0;
	Link	*links;

	if (check_init(cptr, sockname, fullname, &sk))
		return -2;

	name = cptr->name;
	strcpy(cptr->sockhost, fullname);
	debug(DEBUG_DNS, "sv_cl: check access for %s[%s]", name, fullname);

	if (IsUnknown(cptr) && !attach_confs(cptr, name, CFLAG|NFLAG))
	    {
		debug(DEBUG_DNS,"No C/N lines for %s", name);
		return -1;
	    }
	links = cptr->confs;
	/*
	 * We initiated this connection so the client should have a C and N
	 * line already attached after passing through the connec_server()
	 * function earlier.
	 */
	if (IsConnecting(cptr) || IsHandshake(cptr))
	    {
		c_conf = find_conf(links, name, CFLAG);
		n_conf = find_conf(links, name, NFLAG);
		if (!c_conf || !n_conf)
		    {
			sendto_ops("Connecting Error: %s[%s]", name, fullname);
			det_confs_butmask(cptr, 0);
			return -1;
		    }
	    }
#ifdef	UNIXPORT
	if (cptr->flags & FLAGS_UNIX)
	    {
		if (!c_conf)
			c_conf = find_conf(links, name, CFLAG);
		if (!n_conf)
			n_conf = find_conf(links, name, NFLAG);
	    }
#endif

	/*
	** If the servername is a hostname, either an alias (CNAME) or
	** real name, then check with it as the host. Use gethostbyname()
	** to check for servername as hostname.
	*/
	if ((!c_conf || !n_conf) && gethostbyname(name))
	    {
		strncpyzt(cptr->sockhost, name, sizeof(cptr->sockhost));
		if (!c_conf)
			c_conf = find_conf_host(links, name, CFLAG);
		if (!n_conf)
			n_conf = find_conf_host(links, name, NFLAG);
	    }

	if (!c_conf || !n_conf)
		hp = gethostbyaddr(&(sk.sin_addr), sizeof(struct in_addr),
				   sk.sin_family);
	if (hp)
		/*
		 * if we are missing a C or N line from above, search for
		 * it under all known hostnames we have for this ip#.
		 */
		for (name = hp->h_name; name ; name = hp->h_aliases[i++])
		    {
			strncpyzt(fullname, name, sizeof(fullname));
			add_local_domain(fullname,HOSTLEN-strlen(fullname));
			debug(DEBUG_DNS, "sv_cl: gethostbyaddr: %s->%s",
				sockname, fullname);
			if (!c_conf)
				c_conf = find_conf_host(links,fullname,CFLAG);
			if (!n_conf)
				n_conf = find_conf_host(links,fullname,NFLAG);
			if (c_conf && n_conf)
			    {
				strncpyzt(cptr->sockhost, fullname,
					  sizeof(cptr->sockhost));
				break;
			    }
		    }
	/*
	 * Check for C and N lines with the hostname portion the ip number
	 * of the host the server runs on. This also checks the case where
	 * there is a server connecting from 'localhost'.
	 */
	if (IsUnknown(cptr))
	    {
		if (!c_conf)
			c_conf = find_conf_host(links, sockname, CFLAG);
		if (!n_conf)
			n_conf = find_conf_host(links, sockname, NFLAG);
	    }
	/* detach all conf lines that got attached by attach_confs() */
	det_confs_butmask(cptr, 0);

	/* if no C or no N lines, then deny access */
	if (!c_conf || !n_conf)
	    {
		strncpyzt(cptr->sockhost, sockname, sizeof(cptr->sockhost));
		debug(DEBUG_DNS, "sv_cl: access denied: %s[%s] c %x n %x",
			cptr->name, cptr->sockhost, c_conf, n_conf);
		return -1;
	    }
	/*
	 * attach the C and N lines to the client structure for later use.
	 */
	attach_conf(cptr, n_conf);
	attach_conf(cptr, c_conf);

	if ((c_conf->ipnum.s_addr == -1) && !(cptr->flags & FLAGS_UNIX))
		bcopy(&(sk.sin_addr),&(c_conf->ipnum),sizeof(struct in_addr));
	if (!(cptr->flags & FLAGS_UNIX))
		strncpy(cptr->sockhost, c_conf->host, HOSTLEN);

	debug(DEBUG_DNS,"sv_cl: access ok: %s[%s]",
		cptr->name, cptr->sockhost);
	return 0;
}
#undef	CFLAG
#undef	NFLAG
/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
static	int completed_connection(cptr)
aClient	*cptr;
{
	aConfItem *aconf;
	SetHandshake(cptr);
	
	aconf = find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER);
	if (aconf == (aConfItem *)NULL)
	    {
		sendto_ops("Lost C-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	if (!BadPtr(aconf->passwd))
		sendto_one(cptr, "PASS %s", aconf->passwd);

	aconf = find_conf(cptr->confs, cptr->name, CONF_NOCONNECT_SERVER);
	if (aconf == (aConfItem *)NULL)
	    {
		sendto_ops("Lost N-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	sendto_one(cptr, ":%s SERVER %s 1 :%s",
		   me.name, my_name_for_link(me.name, aconf), me.info);

	return (cptr->flags & FLAGS_DEADSOCKET) ? -1 : 0;
}

/*
** close_connection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
int	close_connection(cptr)
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
		cptr->fd = -1;
		DBufClear(&cptr->sendQ);
	}
	for (; highest_fd > 0; highest_fd--)
		if (local[highest_fd])
			break;

	det_confs_butmask(cptr, 0);
	cptr->from = NULL; /* ...this should catch them! >:) --msa */
	AcceptNewConnections = TRUE; /* ...perhaps new ones now succeed? */

	/*
	 * fd remap to keep local[i] filled at the bottom.
	 */
	for (i = 0, j = highest_fd; i < j; i++)
		if (!local[i]) {
			if (fcntl(i, F_GETFL, 0) != -1)
				continue;
			if (local[j]) {
				if (dup2(j,i) == -1)
					continue;
				local[i] = local[j];
				local[i]->fd = i;
				local[j] = (aClient *)NULL;
				close(j);
			}
			for (; j > i && !local[j]; j--)
				;
		}
	return 0;
    }

/*
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
static int	set_non_blocking(cptr)
aClient *cptr;
    {
	int res;

#ifdef PCS
	/* This portion of code might also apply to NeXT and sun.  -LynX */
	res = 1;

	if (ioctl (cptr->fd, FIONBIO, &res) < 0)
		report_error("ioctl(fd,FIONBIO) failed for %s:%s",cptr);
#else
	if ((res = fcntl(cptr->fd, F_GETFL, 0)) == -1)
		report_error("fcntl(fd,F_GETFL) failed for %s:%s",cptr);
	else if (fcntl(cptr->fd, F_SETFL, res | FNDELAY) == -1)
		report_error("fcntl(fd,F_SETL,FNDELAY) failed for %s:%s",cptr);
#endif
	return 0;
    }

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yuet since it doesnt have a name.
 */
aClient *add_connection(fd)
int	fd;
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
		struct	sockaddr_in addr;
		int	len = sizeof(struct sockaddr_in);

		if (getpeername(fd, (struct sockaddr *) &addr, &len) == -1)
		    {
			report_error("Failed in connecting to %s :%s", cptr);
			free(cptr);
			return 0;
		    }
		/* Copy ascii address to 'sockhost' just in case. Then we
		 * have something valid to put into error messages...
		 */
		strncpyzt(cptr->sockhost, (char *)inet_ntoa(addr.sin_addr),
			  sizeof(cptr->sockhost));
		bcopy (&cptr->ip, &addr.sin_addr, sizeof(struct in_addr));
		bcopy (&addr.sin_addr, &cptr->ip, sizeof(struct in_addr));
	    }

	cptr->fd = fd;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = cptr;
	add_client_to_list(cptr);
	set_non_blocking(cptr);
	return cptr;
}

#ifdef	UNIXPORT
aClient *add_unixconnection(fd)
int	fd;
{
	aClient *cptr;
	struct	sockaddr_un addr;
	struct	hostent	*hp;
	int	len = sizeof(struct sockaddr_un);

	cptr = make_client((aClient *)NULL);

	/* Copy ascii address to 'sockhost' just in case. Then we
	 * have something valid to put into error messages...
	 */
	strncpyzt(cptr->sockhost, me.sockhost, sizeof(cptr->sockhost));

	cptr->fd = fd;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = cptr;
	cptr->flags |= FLAGS_UNIX;
	hp = gethostbyname(me.sockhost);
	if (hp)
		bcopy(hp->h_addr_list[0], &cptr->ip, sizeof(struct in_addr));
	else
		cptr->ip.s_addr = inet_addr("127.0.0.1");

	add_client_to_list(cptr);
	set_non_blocking(cptr);
	return cptr;
}
#endif

/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */
int	read_message(buffer, buflen, from, delay)
char	*buffer;
int	buflen;
aClient	**from;
long	delay; /* Don't ever use ZERO here, unless you mean to poll and then
		* you have to have sleep/wait somewhere else in the code.--msa
		*/
{
  static long lastaccept = 0;
  Reg1 aClient *cptr;
  Reg2 int nfds;
  int res, length, fd, i;
  struct timeval wait;
  fd_set read_set, write_set;
  aConfItem *aconf = NULL;
  long tval, delay2 = delay, now;

  now = time(NULL);
  for (res = 0;;)
    {
      tval = now + 5; /* +x - # of messages allowed in bursts */
      FD_ZERO(&read_set);
      FD_ZERO(&write_set);
      for (i = 0; i <= highest_fd; i++)
	if ((cptr = local[i]) && IsMe(cptr) && AcceptNewConnections &&
	    (now > lastaccept))
	{
debug(DEBUG_DEBUG,"listen on fd %d",i);
	  FD_SET(i, &read_set);
}
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
      nfds = select(FD_SETSIZE, &read_set, &write_set, (fd_set *) 0, &wait);
      if (nfds == 0 || (nfds == -1 && errno == EINTR))
	return -1;
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
  
  for (i = 0; i <= highest_fd; i++)
   if ((cptr = local[i]) && IsMe(cptr) && FD_ISSET(i, &read_set))
    {
debug(DEBUG_DEBUG,"connect to fd %d\n",i);
      nfds--;
      lastaccept = now;
      if ((fd = accept(i, (struct sockaddr *)0, (int *)0)) < 0)
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
#ifdef	UNIXPORT
      if (i == me.fd)
	add_connection(fd);
      else
	add_unixconnection(fd);
#else
      add_connection(fd);
#endif

      break; /* Really! This was not a loop, just faked "if" --msa */
    }	/* WARNING IS NORMAL HERE! */

  for (i = 0; (i <= highest_fd) && nfds > 0; i++)
    {
      if (!(cptr = local[i]) || IsMe(cptr))
	continue;
      if (FD_ISSET(i, &write_set))
	{
	  int writeerr = 0;
	  nfds--;
	  if (IsConnecting(cptr) && completed_connection(cptr))
	    writeerr = 1;
	  /*
	   ** ...room for writing, empty some queue then...
	   */
	  if (!writeerr)
	    send_queued(cptr);
	  if (cptr->flags & FLAGS_DEADSOCKET || writeerr)
	   {
	    if (FD_ISSET(i, &read_set))
	     {
	      nfds--;
	      FD_CLR(i, &read_set);
	     }
	    exit_client(cptr,cptr,&me,"write error");
	    continue;
	   }
	}
      if (!FD_ISSET(i, &read_set))
	continue;
      nfds--;
      readcalls++;
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
	    report_error("Lost server connection to %s:%s", cptr);
	}
      else
	debug(DEBUG_DEBUG,"READ ERROR: fd = %d", cptr->fd);
      
      /*
       ** Reschedule a faster reconnect, if this was a automaticly
       ** connected configuration entry. (Note that if we have had
       ** a rehash in between, the status has been changed to
       ** CONF_ILLEGAL). But only do this if it was a "good" link.
       */
      if (cptr->confs)
	aconf = cptr->confs->value.aconf;
      if (aconf && aconf->status == CONF_CONNECT_SERVER)
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
    }
  return -1;
}    


int	connect_server(aconf)
aConfItem *aconf;
    {
	struct	sockaddr *svp;
	aClient *cptr, *c2ptr;
	int	errtmp, len;

	if (c2ptr = find_server(aconf->name, NULL))
	    {
		sendto_ops("Server %s already present from %s",
			   aconf->name, c2ptr->name);
		return -1;
	    }
	cptr = make_client((aClient *)NULL);

#ifdef	UNIXPORT
	if (aconf->host[0] == '/')
		svp = connect_unix_server(aconf, cptr, &len);
	else
		svp = connect_inet_server(aconf, cptr, &len);
#else
	svp = connect_inet_server(aconf, cptr, &len);
#endif

	if (svp == (struct sockaddr *)NULL)
		return -1;

	set_non_blocking(cptr);
	signal(SIGALRM, dummy);
	alarm(4);
	if (connect(cptr->fd, svp, len) < 0
	    && errno != EINPROGRESS)
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

	if (!find_conf_host(cptr->confs, aconf->host, CONF_NOCONNECT_SERVER) ||
	    !find_conf_host(cptr->confs, aconf->host, CONF_CONNECT_SERVER))
	    {
      		sendto_ops("Host %s is not enabled for connecting:no C/N-line",
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

	if (!(cptr->flags & FLAGS_UNIX))
		strncpyzt(cptr->sockhost, aconf->host, sizeof(cptr->sockhost));
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));
	add_client_to_list(cptr);
	return 0;
    }

struct	sockaddr *connect_inet_server(aconf, cptr, lenp)
aConfItem	*aconf;
aClient	*cptr;
int	*lenp;
{
	static	struct	sockaddr_in	server;
	struct	hostent	*hp;

	/*
	 * might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */

	cptr->fd = socket(AF_INET, SOCK_STREAM, 0);
	strncpyzt(cptr->sockhost, aconf->host, sizeof(cptr->sockhost));
	server.sin_family = AF_INET;

	if (cptr->fd < 0)
	    {
		report_error("opening stream socket to server %s:%s",cptr);
		free(cptr);
		return (struct sockaddr *)NULL;
	    }
	
	/* MY FIX -- jtrim@duorion.cair.du.edu  (2/10/89) */

	if (isdigit(*(aconf->host)) && (aconf->ipnum.s_addr == -1))  {
		aconf->ipnum.s_addr = inet_addr(aconf->host);
	} else if (aconf->ipnum.s_addr == -1) {
		hp = gethostbyname(aconf->host);
		if (hp == (struct hostent *)NULL)
		    {
			close(cptr->fd);
			free(cptr);
			debug(DEBUG_FATAL, "%s: unknown host",
			      aconf->host);
			return (struct sockaddr *)NULL;
		    }
		bcopy(hp->h_addr_list[0], &aconf->ipnum,
		      sizeof(struct in_addr));
	   	}
	bcopy(&aconf->ipnum,&server.sin_addr,sizeof(struct in_addr));
	server.sin_port = htons((aconf->port>0)?aconf->port:portnum);

	/* End FIX */

	*lenp = sizeof(server);
	return	(struct sockaddr *)&server;
}

#ifdef	UNIXPORT
struct	sockaddr *connect_unix_server(aconf, cptr, lenp)
aConfItem	*aconf;
aClient	*cptr;
int	*lenp;
{
	static	struct	sockaddr_un	sock;

	if ((cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	    {
		report_error("Connect to host %s failed: %s", cptr);
		free(cptr);
		return (struct sockaddr *)NULL;
	    }

	strncpyzt(cptr->sockhost, me.sockhost, sizeof(cptr->sockhost));
	strncpyzt(sock.sun_path, aconf->host, sizeof(sock.sun_path));
	sock.sun_family = AF_UNIX;
	*lenp = strlen(sock.sun_path) + 2;

	cptr->flags |= FLAGS_UNIX;
	return (struct sockaddr *)&sock;
}
#endif

/*
 * The following section of code performs summoning of users to irc.
 */
#if defined(ENABLE_SUMMON) || defined(ENABLE_USERS)
int utmp_open()
{
  return (open(UTMP,O_RDONLY));
}

int utmp_read(fd, name, line, host, hlen)
int fd, hlen;
char *name, *line, *host;
    {
	struct utmp ut;
	while (read(fd, &ut, sizeof (struct utmp)) == sizeof (struct utmp))
	    {
		strncpyzt(name,ut.ut_name,9);
		strncpyzt(line,ut.ut_line,10);
#ifdef USER_PROCESS
#	ifdef HPUX
		strncpyzt(host,(ut.ut_host[0]) ? (ut.ut_host) : me.name, 16);
#	else
		strncpyzt(host, me.name, 9);
#	endif
		if (ut.ut_type == USER_PROCESS)
			return 0;
#else
		strncpy(host,(ut.ut_host[0]) ? (ut.ut_host) : me.name, hlen);
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
#  ifdef ENABLE_SUMMON
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
#ifdef HPUX
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
#  endif
#endif /* ENABLE_SUMMON */

int	get_my_name(conf_name, name, len)
char	*conf_name, *name;
int	len;
{
	struct hostent *hp;

	if (gethostname(name,len) < 0)
		return -1;
	name[len] = '\0';

	/* assume that a name containing '.' is a fully qualified domain name */
	if (!index(name,'.'))
		add_local_domain(name, len-strlen(name));

	/* If hostname gives another name than conf_name, then check if there is
	 * a CNAME record for conf_name pointing to hostname. If so accept
	 * conf_name as our name.   meLazy
	 */
	if (!BadPtr(conf_name) && mycmp(conf_name, name))
	    {
		if (hp = gethostbyname(conf_name))
		    {
			char tmp[HOSTLEN];
			char *hname;
			int i=0;

			for (hname = hp->h_name; hname;
			     hname = hp->h_aliases[i++])
	  		    {
				strncpy(tmp, hname, sizeof(tmp));
				add_local_domain(tmp,sizeof(tmp)-strlen(tmp));
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
