/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

char smisc_id[]="s_misc.c v2.0 (c) 1988 University of Oulu, Computing Centers\
 and Jarkko Oikarinen";

#include <stdio.h>
#include <sys/time.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/stat.h>
#ifdef GETRUSAGE_2
#include <sys/resource.h>
#else
#  ifdef TIMES_2
#include <sys/times.h>
#  endif
#endif

extern	aClient	*client, *local[];
extern	int	highest_fd, errno;
#ifdef DEBUGMODE
extern	int	readcalls, writecalls, writeb[];
#endif
extern	int	dbufalloc, dbufblocks;

static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static char *weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

char	*date(clock) 
long	clock;
{
	static	char	buf[80];
	Reg1	struct	tm *ltbuf;
	struct	timeval	tp;
	struct	timezone tzp;
	Reg2	char	*timezonename;
#if !(HPUX || AIX)
	extern char *timezone();
#endif

	if (!clock) 
		time(&clock);
	ltbuf = localtime(&clock);
	gettimeofday(&tp, &tzp);

#if HPUX || AIX
	tzset();
	timezonename = tzname[ltbuf->tm_isdst];
#else
	timezonename = timezone(tzp.tz_minuteswest, ltbuf->tm_isdst);
#endif

	sprintf(buf, "%s %s %d 19%02d -- %02d:%02d %s",
		weekdays[ltbuf->tm_wday], months[ltbuf->tm_mon],ltbuf->tm_mday,
		ltbuf->tm_year, ltbuf->tm_hour,	ltbuf->tm_min, timezonename);

	return buf;
}

/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time value as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there and never 'refreshes' it -- zapping that
 **   might break things in other places...)
 **
 **/

char *myctime(value)
long value;
{
	static	char	buf[28];
	Reg1	char	*p;

	strcpy(buf, ctime(&value));
	if ((p = index(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}
/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int check_registered_user(sptr)
aClient	*sptr;
{
	if (!IsRegisteredUser(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d * :You have not registered as a user",
			   me.name, ERR_NOTREGISTERED);
		return -1;
	    }
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int	check_registered(sptr)
aClient	*sptr;
{
	if (!IsRegistered(sptr))
	    {
		sendto_one(sptr,
			   ":%s %d * :You have not registered yourself yet",
			   me.name, ERR_NOTREGISTERED);
		return -1;
	    }
	return 0;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "servername", if remote server or sockethost == servername
**	  "servername[sockethost]", for local servers otherwise
**	  "nickname", for remote users (or sockethost == nickname, never!)
**	  "nickname[sockethost]", for local users (non-servers)
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char *get_client_name(sptr, showip)
aClient *sptr;
int	showip;
{
	static char nbuf[sizeof(sptr->name)+sizeof(sptr->sockhost)+3];

	if (MyConnect(sptr) && mycmp(sptr->name,sptr->sockhost))
	    {
		if (showip)
			sprintf(nbuf, "%s[%s]",
				sptr->name, inet_ntoa(sptr->ip));
		else
			sprintf(nbuf, "%s[%s]", sptr->name, sptr->sockhost);
		return nbuf;
	    }
	return sptr->name;
}


/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */
char *my_name_for_link(name, conf)
char *name;
aConfItem *conf;
{
	static char namebuf[HOSTLEN];
	register int count = conf->port;
	register char *start = name;

	if (count <= 0 || count > 5)
		return start;

	while (count-- && name)
	    {
		name++;
		name = index(name, '.');
	    }
	if (!name)
		return start;

	namebuf[0] = '*';
	strncpy(&namebuf[1], name, HOSTLEN - 1);
	namebuf[HOSTLEN - 1] = '\0';

	return namebuf;
}

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
*/
int exit_client(cptr, sptr, from, comment)
aClient *cptr;	/*
		** The local client originating the exit or NULL, if this
		** exit is generated by this server for internal reasons.
		** This will not get any of the generated messages.
		*/
aClient *sptr;	/* Client exiting */
aClient *from;	/* Client firing off this Exit, never NULL! */
char *comment;	/* Reason for the exit */
    {
	Reg1 aClient *acptr;
	Reg2 aClient *next;
	static int exit_one_client();

	if (MyConnect(sptr))
	    {
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
#ifdef FNAME_USERLOG
# if defined(USE_SYSLOG) && defined(SYSLOG_USERS)
			syslog(LOG_DEBUG, "%s (%3d:%02d:%02d): %s@%s\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->user->username, sptr->user->host);
# else
	    {
		FILE *userlogfile;
		struct stat stbuf;
		long	on_for;

		/*
 		 * This conditional makes the logfile active only after
		 * it's been created - thus logging can be turned off by
		 * removing the file.
		 */
		/*
		 * stop NFS hangs...most systems should be able to open a
		 * file in 3 seconds. -avalon (curtesy of wumpus)
		 */
		alarm(3);
		if (IsPerson(sptr) && !stat(FNAME_USERLOG, &stbuf) &&
		    (userlogfile = fopen(FNAME_USERLOG, "a")))
		    {
			alarm(0);
			on_for = time(NULL) - sptr->firsttime;
			fprintf(userlogfile, "%s (%3d:%02d:%02d): %s@%s\n",
				myctime(sptr->firsttime),
				on_for / 3600, (on_for % 3600)/60,
				on_for % 60,
				sptr->user->username, sptr->user->host);
			fclose(userlogfile);
		    }
		alarm(0);
		/* Modification by stealth@caen.engin.umich.edu */
	    }
# endif
#endif
		close_connection(sptr);
		/*
		** First QUIT all NON-servers which are behind this link
		**
		** Note	There is no danger of 'cptr' being exited in
		**	the following loops. 'cptr' is a *local* client,
		**	all dependants are *remote* clients.
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (!IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, me.name);
		    }
		/*
		** Second SQUIT all servers behind this link
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, me.name);
		    }
	    }

	exit_one_client(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static exit_one_client(cptr, sptr, from, comment)
aClient *sptr;
aClient *cptr;
aClient *from;
char *comment;
    {
	aClient *acptr;
	Reg1 int i;
	Reg2 Link *link;

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
		return 0;	/* ...must *never* exit self!! */
	else if (IsServer(sptr)) {
	  /*
	   ** Old sendto_serv_but_one() call removed because we now
	   ** need to send different names to different servers
	   ** (domain name matching)
	   */
	  for (i = 0; i <= highest_fd; i++)
	    {
	      if (!(acptr = local[i]) || !IsServer(acptr) ||
		  acptr == cptr || IsMe(acptr))
		continue;
	      if (matches(my_name_for_link(me.name, acptr->confs->value.aconf),
			  sptr->name) == 0)
		continue;
	      if (sptr->from == acptr)
		/*
		** SQUIT going "upstream". This is the remote
		** squit still hunting for the target. Use prefixed
		** form. "from" will be either the oper that issued
		** the squit or some server along the path that didn't
		** have this fix installed. --msa
		*/
		sendto_one(acptr, ":%s SQUIT %s :%s",
			   from->name, sptr->name, comment);
	      else
		sendto_one(acptr, "SQUIT %s :%s", sptr->name, comment);
	    }
	} else if (!(IsPerson(sptr) || IsService(sptr)))
				    /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty hole in the server... --msa
				    */
		 ; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
			sendto_serv_butone(cptr,":%s QUIT :%s",
					   sptr->name, comment);
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user) {
		  Link *tmp;
		  sendto_common_channels(sptr, ":%s QUIT :%s",
					 sptr->name, comment);
		  for (link = sptr->user->channel; link; link = tmp) {
		    tmp = link->next;
		    remove_user_from_channel(sptr, link->value.chptr);
		    /* link is freed and sptr->user->channel updated */
		  }
		  /* Clean up invitefield */
		  for (link = sptr->user->invited; link; link = tmp) {
		    tmp = link->next;
		    del_invite(sptr, link->value.chptr);
		    /* no need to free link, del_invite does that */
		  }
		}
	    }
	/* Remove sptr from the client list */
	if (sptr->name[0])
	    del_from_client_hash_table(sptr->name, sptr);
	remove_client_from_list(sptr);
	return (0);
    }

#ifdef DEBUGMODE
/*
 * This is part of the STATS replies. There is no offical numeric for this
 * since this isnt an official command, in much the same way as HASH isnt.
 * It is also possible that some systems wont support this call or have
 * different field names for "struct rusage".
 * -avalon
 */
send_usage(cptr, nick)
aClient *cptr;
char *nick;
{
#ifdef GETRUSAGE_2
	struct rusage rus;
	u_long secs;

	if (getrusage(RUSAGE_SELF, &rus) == -1)
	    {
		extern char *sys_errlist[];
		sendto_one(cptr,":%s NOTICE %s :Getruseage error: %s.",
			   me.name, nick, sys_errlist[errno]);
		return 0;
	    }
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	if (secs == 0)
		secs = 1;

	sendto_one(cptr,
		   ":%s NOTICE %s :CPU Secs %d:%d User %d:%d System %d:%d",
		   me.name, nick, secs/60, secs%60,
		   rus.ru_utime.tv_sec/60, rus.ru_utime.tv_sec%60,
		   rus.ru_stime.tv_sec/60, rus.ru_stime.tv_sec%60);

	sendto_one(cptr, ":%s NOTICE %s :RSS %d ShMem %d Data %d Stack %d",
		   me.name, nick, rus.ru_maxrss,
		   rus.ru_ixrss/secs, rus.ru_idrss/secs, rus.ru_isrss/secs);

	sendto_one(cptr, ":%s NOTICE %s :Swaps %d Reclaims %d Faults %d",
		   me.name, nick, rus.ru_nswap, rus.ru_minflt, rus.ru_majflt);

	sendto_one(cptr, ":%s NOTICE %s :Block in %d out %d",
		   me.name, nick, rus.ru_inblock, rus.ru_oublock);

	sendto_one(cptr, ":%s NOTICE %s :Msg Rcv %d Send %d",
		   me.name, nick, rus.ru_msgrcv, rus.ru_msgsnd);

	sendto_one(cptr, ":%s NOTICE %s :Signals %d Context Vol. %d Invol %d",
		   me.name,nick,rus.ru_nsignals,rus.ru_nvcsw,rus.ru_nivcsw);
#else
#  ifdef TIMES_2
	struct	tms	tmsbuf;
	u_long	secs;

	if (times(&tmsbuf) == -1)
	    {
		extern char *sys_errlist[];
		sendto_one(cptr,":%s NOTICE %s :times(2) error: %s.",
			   me.name, nick, strerror(errno));
		return 0;
	    }
	secs = tms.tms_utime + tms.tms_stime;

	sendto_one(cptr,
		   ":%s NOTICE %s :CPU Secs %d:%d User %d:%d System %d:%d",
		   me.name, nick, secs/60, secs%60,
		   tms.tms_utime/60, tms.tms_utime%60,
		   tms.tms_stime/60, tms.tms_stime%60);
#  endif
#endif
	sendto_one(cptr, ":%s NOTICE %s :Reads %d Writes %d",
		   me.name, nick, readcalls, writecalls);
	sendto_one(cptr, ":%s NOTICE %s :DBUF alloc %d blocks %d",
		   me.name, nick, dbufalloc, dbufblocks);
	sendto_one(cptr,
		   ":%s NOTICE %s :Writes:  <0 %d 0 %d <16 %d <32 %d <64 %d",
		   me.name, nick,
		   writeb[0], writeb[1], writeb[2], writeb[3], writeb[4]);
	sendto_one(cptr,
		   ":%s NOTICE %s :<128 %d <256 %d <512 %d <1024 %d >1024 %d",
		   me.name, nick,
		   writeb[5], writeb[6], writeb[7], writeb[8], writeb[9]);
}
#endif
