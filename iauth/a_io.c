/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_io.c
 *   Copyright (C) 1998 Christophe Kalt
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: a_io.c,v 1.1 1998/07/19 19:37:24 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define A_IO_C
#include "a_externs.h"
#undef A_IO_C

anAuthData 	cldata[MAXCONNECTIONS]; /* index == ircd fd */
static int	cl_highest = -1;
#if defined(USE_POLL)
static int	fd2cl[MAXCONNECTIONS]; /* fd -> cl mapping */
#endif

#define IOBUFSIZE 4096
static char		iobuf[IOBUFSIZE];
static char		rbuf[IOBUFSIZE];	/* incoming ircd stream */
static int		iob_len = 0, rb_len = 0;

void
init_io()
{
    bzero((char *) &cldata, sizeof(cldata));
}

/* sendto_ircd() functions */
#if ! USE_STDARG
void
sendto_ircd(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
#else
void
vsendto_ircd(char *pattern, va_list va)
#endif
{
	char	ibuf[4096];

#if ! USE_STDARG
	sprintf(ibuf, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
	vsprintf(ibuf, pattern, va);
#endif
	DebugLog((ALOG_DSPY, 0, "To ircd: [%s]", ibuf));
	strcat(ibuf, "\n");
	if (write(0, ibuf, strlen(ibuf)) != strlen(ibuf))
	    {
		sendto_log(ALOG_DMISC, LOG_NOTICE, "Daemon exiting. [w]");
		exit(0);
	    }
}

#if USE_STDARG
void
sendto_ircd(char *pattern, ...)
{
        va_list va;
        va_start(va, pattern);
        vsendto_ircd(pattern, va);
        va_end(va);
}
#endif

/*
 * next_io
 *
 *	given an entry, look for the next module instance to start
 */
static void
next_io(cl, last)
int cl;
AnInstance *last;
{
    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): last=%s state=0x%X", cl, last,
	      (last) ? last->mod->name : "", cldata[cl].state));
    if (cldata[cl].rfd > 0 || cldata[cl].wfd > 0)
	{
	    /* last is defined here */
	    sendto_log(ALOG_IRCD|ALOG_DMISC, LOG_ERR,
		       "module \"%s\" didn't clean up fd's! (%d %d)",
		       last->mod->name, cldata[cl].rfd, cldata[cl].wfd);
	    if (cldata[cl].rfd > 0)
		    close(cldata[cl].rfd);
	    if (cldata[cl].wfd > 0 && cldata[cl].rfd != cldata[cl].wfd)
		    close(cldata[cl].wfd);
	    cldata[cl].rfd = cldata[cl].wfd = 0;
	}
		       
    cldata[cl].buflen = 0;
    cldata[cl].mod_status = 0;
    cldata[cl].instance = NULL;
    cldata[cl].timeout = 0;

    if (cldata[cl].state & A_START)
	{
	    cldata[cl].state ^= A_START;
	    if (cldata[cl].tried)
		    DebugLog((ALOG_DIO, 0,
			      "next_io(#%d, %x): Starting 2nd pass",
			      cl, last));
	    else
		    DebugLog((ALOG_DIO, 0,
			      "next_io(#%d, %x): Starting 1st pass",
			      cl, last));
	    next_io(cl, NULL); /* start from beginning */
	    return;
	}

    if (cldata[cl].authuser && cldata[cl].best == last)
	{
	    /* we got an authentication from the last module, stop here */
	    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): Stopping", cl, last));
	    if (cldata[cl].state & A_GOTH)
		    /*
		    ** we have the hostname info, so this is already the
		    ** 2nd pass we made
		    */
		    sendto_ircd("D %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
	    return;
	}

    if (last == NULL)
	    cldata[cl].instance = instances;
    else
	    cldata[cl].instance = last->nexti;

    while (cldata[cl].instance)
	{
	    int noipchk = 1;

	    if (cldata[cl].state & A_CHKALL)
		    noipchk = 0;
	    else
		{
		    if (cldata[cl].instance == cldata[cl].tried)
			    /*
			    ** we've reached the last module
			    ** tried in the 1st pass
			    */
			    cldata[cl].state |= A_CHKALL;
		    if (cldata[cl].tried == NULL &&
			!(cldata[cl].state & A_COMPLETE))
			    /* first pass */
			    noipchk = 0;
		}

	    if (conf_match(cl, cldata[cl].instance, noipchk) == 0)
		    break;
	    DebugLog((ALOG_DIO, 0,"next_io(#%d, %x): no match(%d) for %x (%s)",
		      cl, last, noipchk, cldata[cl].instance,
		      cldata[cl].instance->mod->name));
	    cldata[cl].instance = cldata[cl].instance->nexti;
	}

    if (cldata[cl].instance == NULL)
	{
	    DebugLog((ALOG_DIO, 0,
		      "next_io(#%d, %x): no more instances to try (%X)",
		      cl, last, cldata[cl].state & A_GOTH));
	    if (cldata[cl].state & A_GOTH)
		    /* this is the 2nd pass */
		    sendto_ircd("D %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
	    else
		    cldata[cl].state |= A_COMPLETE;
	    return;
	}

    cldata[cl].timeout = time(NULL) + 120; /* hmmpf */

    /* record progress of the first pass */
    if ((cldata[cl].state & A_GOTH) == 0)
	{
	    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): 1st pass: %x",
		      cl, last, cldata[cl].instance));
	    cldata[cl].tried = cldata[cl].instance;
	}
    else
	{
	    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): 2nd pass: %x",
		      cl, last, cldata[cl].instance));
	}

    /* run the module */
    if (cldata[cl].instance->mod->start(cl) != 0)
	    /* start() failed, or had nothing to do */
	    next_io(cl, cldata[cl].instance);
}

/*
 * parse_ircd
 *
 *	parses data coming from ircd (doh ;-)
 */
static void
parse_ircd()
{
	char *ch, *chp, *buf = iobuf;
	int cl = -1, ncl;

	iobuf[iob_len] = '\0';
	while (ch = index(buf, '\n'))
	    {
		*ch = '\0';
		DebugLog((ALOG_DSPY, 0, "parse_ircd(): got [%s]", buf));

		cl = atoi(chp = buf);
		while (*chp++ != ' ');
		switch (chp[0])
		    {
		case 'C': /* new connection */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
			   "Entry %d [R] is already active (fatal)!", cl);
				exit(1);
			    }
			if (cldata[cl].instance || cldata[cl].rfd > 0 ||
			    cldata[cl].wfd > 0)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d is already active! (fatal)",
					   cl);
				/*
				** Ok, I don't think this is actually fatal,
				** cleaning up already active entry and going
				** on should work; but this should really never
				** happen, so let's forbid it. -kalt
				*/
				exit(1);
			    }
			if (cldata[cl].authuser)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased data [%d]!", cl);
				free(cldata[cl].authuser);
				cldata[cl].authuser = NULL;
			    }
			cldata[cl].state = A_ACTIVE|A_START;
			cldata[cl].best = cldata[cl].tried = NULL;
			cldata[cl].buflen = 0;
			if (sscanf(chp+2, "%[^ ] %hu %[^ ] %hu",
				   cldata[cl].itsip, &cldata[cl].itsport,
				   cldata[cl].ourip, &cldata[cl].ourport) != 4)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
					   "Bad data from ircd [%s] (fatal)",
					   chp);
				exit(1);
			    }
			if (cl > cl_highest)
				cl_highest = cl;
			next_io(cl, NULL); /* get started */
			break;
		case 'D': /* client disconnect */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d [D] is not active! (fatal)", cl);
				exit(1);
			    }
			cldata[cl].state = 0;
			if (cldata[cl].rfd > 0 || cldata[cl].wfd > 0)
				cldata[cl].instance->mod->clean(cl);
			cldata[cl].instance = NULL;
			/* log here? hmmpf */
			if (cldata[cl].authuser)
				free(cldata[cl].authuser);
			cldata[cl].authuser = NULL;
			break;
		case 'R': /* fd remap */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
					   "Entry %d [R] is not active!", cl);
				break;
			    }
			ncl = atoi(buf+1);
			if (!(cldata[ncl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
			   "Entry %d [R] is already active (fatal)!", ncl);
				exit(1);
			    }
			if (cldata[ncl].instance || cldata[ncl].rfd > 0 ||
			    cldata[ncl].wfd > 0)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d is already active! (fatal)",
					   ncl);
				/*
				** Ok, I don't think this is actually fatal,
				** cleaning up already active entry and going
				** on should work; but this should really never
				** happen, so let's forbid it. -kalt
				*/
				exit(1);
			    }
			if (cldata[ncl].authuser)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased data [%d]!", ncl);
				free(cldata[ncl].authuser);
				cldata[ncl].authuser = NULL;
			    }
			bcopy(cldata+cl, cldata+ncl, sizeof(anAuthData));

			cldata[cl].state = 0;
			cldata[cl].rfd = cldata[cl].wfd = 0;
			cldata[cl].instance = NULL;
			cldata[cl].authuser = NULL;
			break;
		case 'N': /* hostname */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d [N] is not active! (fatal)", cl);
				exit(1);
			    }
			strcpy(cldata[cl].host, buf+2);
			cldata[cl].state |= A_GOTH|A_START;
			if (cldata[cl].instance == NULL)
				next_io(cl, NULL);
			break;
		case 'A': /* host alias */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d [A] is not active! (fatal)", cl);
				exit(1);
			    }
			/* hmmpf */
			break;
		case 'U': /* user provided username */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
                                sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d [U] is not active! (fatal)", cl);
				exit(1);
			    }
			cldata[cl].state |= A_GOTU;
			/* hmmpf */
			break;
		default:
			sendto_log(ALOG_IRCD, LOG_ERR, "Unexpected data [%s]",
				   buf);
			break;
		    }

		buf = ch+1;
	    }
	rb_len = 0; iob_len = 0;
	if (strlen(buf))
		bcopy(iobuf, rbuf, rb_len = iob_len = strlen(buf));
}

/*
 * loop_io
 *
 *	select()/poll() loop
 */
void
loop_io()
{
    /* the following is from ircd/s_bsd.c */
#if ! USE_POLL
# define SET_READ_EVENT( thisfd )       FD_SET( thisfd, &read_set)
# define SET_WRITE_EVENT( thisfd )      FD_SET( thisfd, &write_set)
# define CLR_READ_EVENT( thisfd )       FD_CLR( thisfd, &read_set)
# define CLR_WRITE_EVENT( thisfd )      FD_CLR( thisfd, &write_set)
# define TST_READ_EVENT( thisfd )       FD_ISSET( thisfd, &read_set)
# define TST_WRITE_EVENT( thisfd )      FD_ISSET( thisfd, &write_set)

        fd_set  read_set, write_set;
        int     highfd = -1;
#else
/* most of the following use pfd */
# define POLLSETREADFLAGS       (POLLIN|POLLRDNORM)
# define POLLREADFLAGS          (POLLSETREADFLAGS|POLLHUP|POLLERR)
# define POLLSETWRITEFLAGS      (POLLOUT|POLLWRNORM)
# define POLLWRITEFLAGS         (POLLOUT|POLLWRNORM|POLLHUP|POLLERR)

# define SET_READ_EVENT( thisfd ){  CHECK_PFD( thisfd );\
                                   pfd->events |= POLLSETREADFLAGS;}
# define SET_WRITE_EVENT( thisfd ){ CHECK_PFD( thisfd );\
                                   pfd->events |= POLLSETWRITEFLAGS;}

# define CLR_READ_EVENT( thisfd )       pfd->revents &= ~POLLSETREADFLAGS
# define CLR_WRITE_EVENT( thisfd )      pfd->revents &= ~POLLSETWRITEFLAGS
# define TST_READ_EVENT( thisfd )       pfd->revents & POLLREADFLAGS
# define TST_WRITE_EVENT( thisfd )      pfd->revents & POLLWRITEFLAGS

# define CHECK_PFD( thisfd )                    \
        if ( pfd->fd != thisfd ) {              \
                pfd = &poll_fdarray[nbr_pfds++];\
                pfd->fd     = thisfd;           \
                pfd->events = 0;                \
					    }

        struct pollfd   poll_fdarray[MAXCONNECTIONS];
        struct pollfd * pfd     = poll_fdarray;
        int        nbr_pfds = 0;
#endif

	int i, nfds = 0;
	struct timeval wait;
	time_t now = time(NULL);

#if !defined(USE_POLL)
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	highfd = 0;
#else
	/* set up such that CHECK_FD works */
	nbr_pfds = 0;
	pfd      = poll_fdarray;
	pfd->fd  = -1;
#endif  /* USE_POLL */

	SET_READ_EVENT(0); nfds = 1;		/* ircd stream */
#if defined(USE_POLL) && defined(IAUTH_DEBUG)
	for (i = 0; i < MAXCONNECTIONS; i++)
		fd2cl[i] = -1; /* sanity */
#endif
	for (i = 0; i <= cl_highest; i++)
	    {
		if (cldata[i].timeout && cldata[i].timeout < now)
		    {
			DebugLog((ALOG_DIO, 0,
				  "loop_io(): module %s timeout [%d]",
				  cldata[i].instance->mod->name, i));
			if (cldata[i].instance->mod->timeout(i) != 0)
				next_io(i, cldata[i].instance);
		    }
		if (cldata[i].rfd > 0)
		    {
			SET_READ_EVENT(cldata[i].rfd);
#if !defined(USE_POLL)
			if (cldata[i].rfd > highfd)
				highfd = cldata[i].rfd;
#else
			fd2cl[cldata[i].rfd] = i;
#endif
			nfds++;
		    }
		else if (cldata[i].wfd > 0)
		    {
			SET_WRITE_EVENT(cldata[i].wfd);
#if ! USE_POLL
			if (cldata[i].wfd > highfd)
				highfd = cldata[i].wfd;
#else
			fd2cl[cldata[i].wfd] = i;
#endif
			nfds++;
		    }
	    }

	DebugLog((ALOG_DIO, 0, "io_loop(): checking for %d fd's", nfds));
	wait.tv_sec = 5; wait.tv_usec = 0;
#if ! USE_POLL
	nfds = select(highfd + 1, (SELECT_FDSET_TYPE *)&read_set,
		      (SELECT_FDSET_TYPE *)&write_set, 0, &wait);
	DebugLog((ALOG_DIO, 0, "io_loop(): select() returned %d, errno = %d",
		  nfds, errno));
#else
	nfds = poll(poll_fdarray, nbr_pfds,
		    wait.tv_sec * 1000 + wait.tv_usec/1000 );
	DebugLog((ALOG_DIO, 0, "io_loop(): poll() returned %d, errno = %d",
		  nfds, errno));
	pfd = poll_fdarray;
#endif
	if (nfds == -1)
		if (errno == EINTR)
			return;
		else
		    {
			sendto_log(ALOG_IRCD, LOG_CRIT,
				   "fatal select/poll error: %s",
				   strerror(errno));
			exit(1);
		    }
	if (nfds == 0)	/* end of timeout */
		return;

	/* no matter select() or poll() this is also fd # 0 */
	if (TST_READ_EVENT(0))
	    {
		/* data from the ircd.. */
		while (1)
		    {
			if (rb_len)
				bcopy(rbuf, iobuf, iob_len = rb_len);
			if ((i=recv(0,iobuf+iob_len,IOBUFSIZE-iob_len,0)) <= 0)
			    {
				DebugLog((ALOG_DIO, 0, "io_loop(): recv(0) returned %d, errno = %d", i, errno));
				break;
			    }
			iob_len += i;
			DebugLog((ALOG_DIO, 0,
				  "io_loop(): got %d bytes from ircd [%d]", i,
				  iob_len));
			parse_ircd();
		    }
		if (i == 0)
		    {
			sendto_log(ALOG_DMISC, LOG_NOTICE,
				   "Daemon exiting. [r]");
			exit(0);
		    }
		nfds--;
	    }
#if !defined(USE_POLL)
	for (i = 0; i <= cl_highest && nfds; i++)
#else
	for (pfd = poll_fdarray+1; pfd != poll_fdarray+nbr_pfds && nfds; pfd++)
#endif
	    {
#if defined(USE_POLL)
		i = fd2cl[pfd->fd];
# if defined(IAUTH_DEBUG)
		if (i == -1)
		    {
			sendto_log(ALOG_DALL, LOG_CRIT,"loop_io(): fatal bug");
			exit(1);
		    }
# endif
		if (cldata[i].rfd <= 0 && cldata[i].wfd <= 0)
		    {
			sendto_log(ALOG_IRCD, LOG_CRIT,
			   "loop_io(): fatal data inconsistency (%d, %d)",
				   cldata[i].rfd, cldata[i].wfd);
			exit(1);
		    }
#else
		if (cldata[i].rfd <= 0)
			continue;
#endif
		if (TST_READ_EVENT(cldata[i].rfd))
		    {
			int len;
 
			len = recv(cldata[i].rfd,
				   cldata[i].inbuffer + cldata[i].buflen,
				   INBUFSIZE - cldata[i].buflen, 0);
			DebugLog((ALOG_DIO, 0, "io_loop(): i = #%d: recv(%d) returned %d, errno = %d", i, cldata[i].rfd, len, errno));
			if (len < 0)
			    {
				cldata[i].instance->mod->clean(i);
				next_io(i, cldata[i].instance);
			    }
			else
			    {
				cldata[i].buflen += len;
				if (cldata[i].instance->mod->work(i) != 0)
					next_io(i, cldata[i].instance);
				else if (len == 0)
				    {
					cldata[i].instance->mod->clean(i);
					next_io(i, cldata[i].instance);
				    }
			    }
			nfds--;
		    }
		if (TST_WRITE_EVENT(cldata[i].wfd))
		    {
			if (cldata[i].instance->mod->work(i) != 0)
				next_io(i, cldata[i].instance);
			
			nfds--;
		    }
	    }
#if defined(IAUTH_DEBUG)
	if (nfds > 0)
		sendto_log(ALOG_DIO, 0, "loop_io(): nfds = %d !!!", nfds);
# if !defined(USE_POLL)
	/* the equivalent should be written for poll() */
	if (nfds == 0)
		while (i <= cl_highest)
		    {
			if (cldata[i].rfd > 0 && TST_READ_EVENT(cldata[i].rfd))
			    {
				/* this should not happen! */
				/* hmmpf */
			    }
			i++;
		    }
# endif
#endif
}

/*
 * set_non_blocking (ripped from ircd/s_bsd.c)
 */
static void
set_non_blocking(fd, ip, port)
int fd;
char *ip;
u_short port;
{
	int     res, nonb = 0;

#if NBLOCK_POSIX
        nonb |= O_NONBLOCK;
#endif
#if NBLOCK_BSD
        nonb |= O_NDELAY;
#endif
#if NBLOCK_SYSV
        /* This portion of code might also apply to NeXT.  -LynX */
        res = 1;
 
        if (ioctl (fd, FIONBIO, &res) < 0)
                sendto_log(ALOG_IRCD, 0, "ioctl(fd,FIONBIO) failed for %s:%u",
			   ip, port);
#else   
        if ((res = fcntl(fd, F_GETFL, 0)) == -1)
                sendto_log(ALOG_IRCD, 0, "fcntl(fd, F_GETFL) failed for %s:%u",
			   ip, port);
        else if (fcntl(fd, F_SETFL, res | nonb) == -1)
                sendto_log(ALOG_IRCD, 0,
			   "fcntl(fd, F_SETL, nonb) failed for %s:%u",
			   ip, port);
#endif  
}

/*
 * tcp_connect
 *
 *	utility function for use in modules, creates a socket and connects
 *	it to an IP/port
 *
 *	Returns the fd
 */
int
tcp_connect(ourIP, theirIP, port, error)
char *ourIP, *theirIP, **error;
u_short port;
{
	int fd;
	static char errbuf[BUFSIZ];
	struct  sockaddr_in sk;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	    {
		sprintf(errbuf, "socket() failed: %s", strerror(errno));
		*error = errbuf;
		return -1;
	    }
	sk.sin_family = AF_INET;
	sk.sin_addr.s_addr = inetaddr(ourIP);
	sk.sin_port = 0;
	if (bind(fd, (SAP)&sk, sizeof(sk)) < 0)
	    {
		sprintf(errbuf, "bind() failed: %s", strerror(errno));
		*error = errbuf;
		close(fd);
		return -1;
	    }
	set_non_blocking(fd, theirIP, port);
	sk.sin_addr.s_addr = inetaddr(theirIP);
	sk.sin_port = htons(port);
	if (connect(fd, (SAP)&sk, sizeof(sk)) < 0 && errno != EINPROGRESS)
	    {
		sprintf(errbuf, "connect() to %s %u failed: %s", theirIP, port,
			strerror(errno));
		*error = errbuf;
		close(fd);
		return -1;
	    }
	*error = NULL;
	return fd;
}
