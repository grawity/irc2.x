/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf.c
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

/* -- avalon -- 20 Feb 1992
 * Reversed the order of the params for attach_conf().
 * detach_conf() and attach_conf() are now the same:
 * function_conf(aClient *, aConfItem *)
 */

/* -- Jto -- 20 Jun 1990
 * Added gruner's overnight fix..
 */

/* -- Jto -- 16 Jun 1990
 * Moved matches to ../common/match.c
 */

/* -- Jto -- 03 Jun 1990
 * Added Kill fixes from gruner@lan.informatik.tu-muenchen.de
 * Added jarlek's msgbase fix (I still don't understand it... -- Jto)
 */

/* -- Jto -- 13 May 1990
 * Added fixes from msa:
 * Comments and return value to init_conf()
 */

/*
 * -- Jto -- 12 May 1990
 *  Added close() into configuration file (was forgotten...)
 */

char conf_id[] = "conf.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <stdio.h>
#include "netdb.h"
#include <sys/socket.h>
#ifdef __hpux
#include "inet.h"
#endif
#ifdef PCS
#include <time.h>
#endif

aConfItem	*conf = (aConfItem *)NULL;

extern	int	portnum;
extern	char	*configfile;
extern	long	nextconnect, nextping;

static aConfItem *make_conf()
    {
	aConfItem *aconf =(struct ConfItem *) MyMalloc(sizeof(struct ConfItem));
	
	aconf->next = NULL;
	aconf->host = aconf->passwd = aconf->name = NULL;
	aconf->status = CONF_ILLEGAL;
	aconf->clients = 0;
	aconf->port = 0;
	aconf->hold = 0;
	Class(aconf) = 0;
	return (aconf);
    }

static int	free_conf(aconf)
aConfItem *aconf;
    {
	MyFree(aconf->host);
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	free(aconf);
	return 0;
    }

/*
 * det_confs_butone
 *   Removes all configuration fields except one from the client
 *
 */
aConfItem	*det_confs_butone(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg1	Link	*tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if (tmp->value.aconf != aconf)
			detach_conf(cptr, tmp->value.aconf);
	    }
	return (cptr->confs) ? cptr->confs->value.aconf : (aConfItem *)NULL;
}

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
aConfItem	*det_confs_butmask(cptr, mask)
aClient	*cptr;
int	mask;
{
	Reg1 Link *tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			detach_conf(cptr, tmp->value.aconf);
	    }
	return (cptr->confs) ? cptr->confs->value.aconf : (aConfItem *)NULL;
}
/*
 * remove all attached I lines except for the first one.
 */
aConfItem	*det_I_lines_butfirst(cptr)
aClient *cptr;
{
	Reg1	Link	*tmp, *tmp2;
	Link	*first = (Link *)NULL;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if (tmp->value.aconf->status == CONF_CLIENT)
			if (first)
				detach_conf(cptr, tmp->value.aconf);
			else
				first = tmp;
	    }
	return first ? first->value.aconf : (aConfItem *)NULL;
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int	detach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg1 Link **link, *tmp;
	int status, illegal;

	link = &(cptr->confs);

	while (*link)
	    {
		if ((*link)->value.aconf == aconf)
		    {
			if ((aconf) && (Class(aconf)))
			    {
				status = aconf->status;
				if (aconf->status & CONF_CLIENT_MASK)
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
       				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
		 		    {
					free(Class(aconf));
					Class(aconf) = (aClass *)NULL;
				    }
			     }
			aconf->status = status;
			if (aconf != NULL && --aconf->clients == 0 &&
			    IsIllegal(aconf))
				free_conf(aconf);
			tmp = *link;
			*link = tmp->next;
			free(tmp);
			return 0;
		    }
		else
			link = &((*link)->next);
	    }
	return -1;
}

static	int	is_attached(aconf, cptr)
aConfItem *aconf;
aClient *cptr;
{
	Reg1 Link *link;

	for (link = cptr->confs; link; link = link->next)
		if (link->value.aconf == aconf)
			break;

	return (link) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
int	attach_conf(cptr, aconf)
aConfItem *aconf;
aClient *cptr;
{
	Reg1 Link *link;

	if (is_attached(aconf, cptr))
		return 1;
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR)) &&
	    ConfLinks(aconf) >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
		return -1;
	link = (Link *) MyMalloc(sizeof(Link));
	link->next = cptr->confs;
	link->value.aconf = aconf;
	cptr->confs = link;
	aconf->clients += 1;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem *find_admin()
    {
	Reg1 aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
    }

aConfItem *find_me()
    {
	Reg1 aConfItem *aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;
	
	return (aconf);
    }

aConfItem *attach_confs(cptr, name, statmask)
aClient	*cptr;
char	*name;
int	statmask;
{
	Reg1 aConfItem *tmp;
	aConfItem *first = NULL;
	int len = strlen(name);
  
	if (!name || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) &&
		    (tmp->status & CONF_SERVER_MASK) &&
		    (matches(tmp->name, name) == 0))
		    {
			if (!first)
				first = tmp;
			attach_conf(cptr, tmp);
		    }
		else if ((tmp->status & statmask) &&
			 (tmp->status & CONF_SERVER_MASK) &&
			 (mycmp(tmp->name, name) == 0))
		    {
			if (!first)
				first = tmp;
			attach_conf(cptr, tmp);
		    }
	    }
	return(first);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *attach_confs_host(cptr, host, statmask)
aClient *cptr;
char	*host;
int	statmask;
{
	Reg1	aConfItem *tmp;
	aConfItem *first = NULL;
	int	len = strlen(host);
  
	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) &&
		    (tmp->status & CONF_SERVER_MASK) == 0 &&
		    (matches(tmp->host, host) == 0))
		    {
			if (!first)
				first = tmp;
			attach_conf(cptr, tmp);
		    }
		else if ((tmp->status & statmask) &&
	       	    (tmp->status & CONF_SERVER_MASK) &&
	       	    (mycmp(tmp->host, host) == 0))
		    {
			if (!first)
				first = tmp;
			attach_conf(cptr, tmp);
		    }
	    }
	return(first);
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
aConfItem *find_conf_exact(name, host, statmask)
char	*name, *host;
int	statmask;
{
	Reg1	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
		if ((tmp->status & statmask) &&
		    (mycmp(tmp->name, name) == 0) &&
		    (matches(tmp->host, host)==0))
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
			if ((tmp->status & (CONF_OPERATOR|CONF_LOCOP))) {
				if (tmp->clients < MaxLinks(Class(tmp)))
					break;
				else
					continue;
			} else
	  			break;
	return (tmp);
}

aConfItem *find_conf_name(name, statmask)
char	*name;
int	statmask;
{
	Reg1 aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (!tmp->name || (matches(tmp->name, name) == 0)))
			break;
	    }
	return(tmp);
}

aConfItem *find_conf(link, name, statmask)
char	*name;
Link	*link;
int	statmask;
{
	Reg1 aConfItem *tmp;
	int namelen = name ? strlen(name) : 0;
  
	if (namelen > HOSTLEN)
		return (aConfItem *) 0;

	for (; link; link = link->next)
	    {
		tmp = link->value.aconf;
		if ((tmp->status & statmask) &&
		    (((tmp->status & CONF_SERVER_MASK) &&
	 	     mycmp(tmp->name, name) == 0) ||
		    ((tmp->status & CONF_SERVER_MASK) == 0 &&
		     matches(tmp->name, name) == 0)))
			break;
	    }
	return(link ? tmp : (aConfItem *) 0);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *find_conf_host(link, host, statmask)
char	*host;
Link	*link;
int	statmask;
{
	Reg1 aConfItem *tmp;
	int hostlen = host ? strlen(host) : 0;
  
	if (hostlen > HOSTLEN)
		return (aConfItem *) 0;

	for (; link; link = link->next)
	    {
		tmp = link->value.aconf;
		if (tmp->status & statmask &&
		    (((tmp->status & CONF_SERVER_MASK == 0) &&
	 	     (!host || matches(tmp->host, host) == 0)) ||
		     (tmp->status & CONF_SERVER_MASK &&
	 	      (!host || mycmp(tmp->host, host) == 0) ) ) )
			break;
	    }
	return(link ? tmp : (aConfItem *) 0);
}

int rehash()
    {
	Reg1	aConfItem *tmp = conf, *tmp2;
	Reg2	aClass	*cltmp;

	while (tmp)
	    {
		tmp2 = tmp->next;
		if (tmp->clients)
		    {
			/*
			** Configuration entry is still in use by some
			** local clients, cannot delete it--mark it so
			** that it will be deleted when the last client
			** exits...
			*/
			tmp->status |= CONF_ILLEGAL;
			tmp->next = NULL;
		    }
		else
			free_conf(tmp);
	    
		tmp = tmp2;
	    }

	/*
	 * We don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	for (cltmp = NextClass(FirstClass()); cltmp; cltmp = NextClass(cltmp))
		MaxLinks(cltmp) = -1;

	close_listeners();
	conf = (aConfItem *) 0;
	return initconf(1);
    }

extern char *getfield();

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

int 	initconf(rehashing)
int rehashing;
    {
	FILE *fd;
	char line[512], *tmp, c[80];
	int ccount = 0, ncount = 0;
	aConfItem *aconf;
	struct hostent *hp;

	debug(DEBUG_DEBUG, "initconf(%d)", rehashing);
	if (!(fd = fopen(configfile,"r")))
		return(-1);
	while (fgets(line,sizeof(line)-1,fd))
	    {
		if (line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		aconf = make_conf();

		if (tmp = (char *)index(line, '\n'))
			*tmp = 0;
		else while(fgets(c, sizeof(c), fd))
		    {
			if (tmp = (char *)index(c, '\n'))
				*tmp= 0;
				break;
		    }
		tmp = getfield(line);
		if (!tmp)
			continue;
		switch (*tmp)
		    {
		    case 'C':   /* Server where I should try to connect */
		    case 'c':   /* in case of link failures             */
		      ccount++;
		      aconf->status = CONF_CONNECT_SERVER;
		      break;
		    case 'I':   /* Just plain normal irc client trying  */
		    case 'i':   /* to connect me */
		      aconf->status = CONF_CLIENT;
		      break;
		    case 'K':   /* Kill user line on irc.conf           */
		    case 'k':
		      aconf->status = CONF_KILL;
		      break;
		    case 'N':   /* Server where I should NOT try to     */
		    case 'n':   /* connect in case of link failures     */
			        /* but which tries to connect ME        */
		      ++ncount;
		      aconf->status = CONF_NOCONNECT_SERVER;
		      break;
		    case 'U':   /* Uphost, ie. host where client reading */
		    case 'u':   /* this should connect.                  */
		      /* This is for client only, I must ignore this */
		      /* ...U-line should be removed... --msa */
		      break;    
		    case 'O':   /* Operator. Line should contain at least */
		                /* password and host where connection is  */
		      aconf->status = CONF_OPERATOR;      /* allowed from */
		      break;
		    case 'o':   /* Local Operator, operator but with */
		      aconf->status = CONF_LOCOP; /* limited privs --SRB */
		      break;
		    case 'S':   /* Service. Same semantics as   */
		    case 's':   /* CONF_OPERATOR                */
		      aconf->status = CONF_SERVICE;
		      break;
		    case 'M':  /* Me. Host field is name used for this host */
		    case 'm':  /* and port number is the number of the port */
			aconf->status = CONF_ME;
			break;
		    case 'A':   /* Name, e-mail address of administrator */
		    case 'a':   /* of this server. */
			aconf->status = CONF_ADMIN;
			break;
		    case 'Y':
		    case 'y':
		        aconf->status = CONF_CLASS;
		        break;
		    case 'L':   /* guaranteed leaf server */
		    case 'l':
		      aconf->status = CONF_LEAF;
                      break;
		    case 'Q':   /* a server that you don't want in your */
		    case 'q':   /* network. USE WITH CAUTION! */
		        aconf->status = CONF_QUARANTINED_SERVER;
  		        break;
#ifdef R_LINES
		    case 'R': /* extended K line */
		    case 'r': /* Offers more options of how to restrict */
		      aconf->status = CONF_RESTRICT;
		      break;
#endif
		    case 'P': /* listen port line */
		    case 'p':
			aconf->status = CONF_LISTEN_PORT;
			break;
		    default:
			debug(DEBUG_ERROR, "Error in config file: %s", line);
			break;
		    }
		if (IsIllegal(aconf))
		    {
			free(aconf);
			continue;
		    }
		aconf->next = conf;
		conf = aconf;
		for (;;) /* Fake loop, that I can use break here --msa */
		    {
			if ((tmp = getfield(NULL)) == NULL)
			  break;
			DupString(aconf->host,tmp);
			if ((tmp = getfield(NULL)) == NULL)
			  break;
			DupString(aconf->passwd,tmp);
			if ((tmp = getfield(NULL)) == NULL)
			  break;
			DupString(aconf->name,tmp);
			if ((tmp = getfield(NULL)) == NULL)
			  break;
			aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
			  break;
			Class(aconf) = find_class(atoi(tmp));
			break;
		    }

		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS) {
		  add_class(atoi(aconf->host), atoi(aconf->passwd),
			   atoi(aconf->name), aconf->port);
		  conf = conf->next;
		  free_conf(aconf);
		  continue;
		}

		if (aconf->status & CONF_LISTEN_PORT)
		    {
			add_listener(aconf->host, aconf->port);
			conf = conf->next;
			free_conf(aconf);
			continue;
		    }
		if (aconf->status & CONF_SERVER_MASK) {
		  if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
		      aconf->host && index(aconf->host, '*')) {
		    conf = aconf->next;
		    free_conf(aconf);
		    continue;
		  }
		}

		/*
                ** associate each conf line with a class by using a pointer
                ** to the correct class record. -avalon
                */
		if (aconf->status & CONF_CLIENT_MASK) {
		  if (Class(aconf) == 0)
		    Class(aconf) = find_class(0);
		  if (MaxLinks(Class(aconf)) < 0)
		    Class(aconf) = find_class(0);
		}
		/*
		** Do name lookup now on hostnames given and store the
		** ip numbers in conf structure.
		*/
		aconf->ipnum.s_addr = -1;
		while (!rehashing && (aconf->status & CONF_SERVER_MASK)) {
		    if (BadPtr(aconf->host))
			break;
		    if (!isalpha(*aconf->host) && !isdigit(*aconf->host))
			break;
		    if (aconf->host && (hp = gethostbyname(aconf->host))) {
			bcopy(hp->h_addr, &(aconf->ipnum), hp->h_length);
			if (aconf->ipnum.s_addr)
				break;
		    }
		    if (isdigit(*aconf->host)) {
			aconf->ipnum.s_addr = inet_addr(aconf->host);
			if (aconf->ipnum.s_addr != -1)
				break;
		    }
		    if (aconf->name && (hp = gethostbyname(aconf->name))) {
			bcopy(hp->h_addr, &(aconf->ipnum), hp->h_length);
			if (aconf->ipnum.s_addr)
				break;
		    }
		    break;
		}
		/*
		** Own port and name cannot be changed after the startup.
		** (or could be allowed, but only if all links are closed
		** first).
		** Configuration info does not override the name and port
		** if previously defined. Note, that "info"-field can be
		** changed by "/rehash".
		*/
		if (aconf->status == CONF_ME)
		    {
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (me.name[0] == '\0' && aconf->host[0])
				strncpyzt(me.name, aconf->host,
					  sizeof(me.name));
			if (portnum < 0 && aconf->port >= 0)
				portnum = aconf->port;
		    }
		debug(DEBUG_NOTICE,
		      "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port, Class(aconf));
	    }
	fclose(fd);
	check_class();
	nextping = nextconnect = time(NULL);
	return (0);
    }

int	find_kill(cptr)
aClient	*cptr;
{
	char	reply[256], *host, *name;
	aConfItem *tmp;
	static	int	check_time_interval();

	if (cptr->user == NULL)
		return 0;

	host = cptr->sockhost;
	name = cptr->user->username;

	if (strlen(host)  > HOSTLEN || (name ? strlen(name) : 0) > HOSTLEN)
		return (0);

	reply[0] = '\0';

	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((matches(tmp->host, host) == 0) &&
		    tmp->status == CONF_KILL &&
 		    (name == NULL || matches(tmp->name, name) == 0))
 			if (BadPtr(tmp->passwd) ||
 			    check_time_interval(tmp->passwd, reply))
 			break;

	if (reply[0])
		sendto_one(cptr, reply,
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name);
	else if (tmp)
		sendto_one(cptr,
			   ":%s %d %s :*** Ghosts are not allowed on IRC.",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name);

 	return (tmp ? -1 : 0);
 }

#ifdef R_LINES
/* find_restrict works against host/name and calls an outside program 
   to determine whether a client is allowed to connect.  This allows 
   more freedom to determine who is legal and who isn't, for example
   machine load considerations.  The outside program is expected to 
   return a reply line where the first word is either 'Y' or 'N' meaning 
   "Yes Let them in" or "No don't let them in."  If the first word 
   begins with neither 'Y' or 'N' the default is to let the person on.
   It returns a value of 0 if the user is to be let through -Hoppie  */
  
int	find_restrict(cptr)
aClient	*cptr;
{
	aConfItem *tmp;
	char	cmdline[132], reply[80], temprpl[80];
	char	*rplhold = reply, *host, *name, *s;
	char	rplchar='Y';
	FILE	*fp;
	int	hlen, nlen, rc = 0;

	if (cptr->user == NULL)
		return 0;
	name = cptr->user->username;
	host = cptr->sockhost;
	hlen = strlen(host);
	nlen = strlen(name);

	for (tmp = conf; tmp; tmp = tmp->next)
	  if (tmp->status == CONF_RESTRICT &&
	      (host == NULL || (matches(tmp->host, host) == 0)) &&
	      (name == NULL || matches(tmp->name, name) == 0))
	    {
	      if (BadPtr(tmp->passwd))
		sendto_ops("Program missing on R-line %s/%s, ignoring.",
			   name,host);
	      else
		{
		  if (strlen(tmp->passwd) + hlen + nlen > sizeof(cmdline))
		    {
		      sendto_ops("R-line command Too Long! %s %s %s",
				tmp->passwd, name, host);
		      continue;
		    }
		  sprintf(cmdline, "%s %s %s", tmp->passwd, name, host);

		  if ((fp = popen(cmdline,"r")) == NULL)
		    {
		      sendto_ops("Couldn't run '%s' for R-line %s/%s",
				 tmp->passwd, name, host);
		      continue;
		    }
		  else
		    {
		      reply[0] = '\0';
		      while (fgets(temprpl, sizeof(temprpl)-1, fp) != EOF)
			{
			  if (s = (char *)index(temprpl, '\n'))
			      *s = '\0';
			  if (strlen(temprpl) + strlen(reply) < 80)
			      sprintf(rplhold, "%s %s", rplhold, temprpl);
			  else
			      sendto_ops("R-line %s/%s: reply too long!",
					 name,host);
			}
		    }
		  pclose(fp);

		  while (*rplhold == ' ') rplhold++;
		  rplchar = *rplhold; /* Pull out the yes or no */
		  while (*rplhold != ' ') rplhold++;
		  while (*rplhold == ' ') rplhold++;
		  strcpy(reply,rplhold);
		  rplhold = reply;

		  if (rc = (rplchar == 'n' || rplchar == 'N'))
		      break;
		}
	    }
	if (rc)
	    {
		sendto_one(cptr, ":%s %d %s :Restriction: %s",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			   reply);
		return -1;
	    }
	return 0;
      }
#endif


/*
** check against a set of time intervals
*/

static	int check_time_interval(interval, reply)
char	*interval, *reply;
{
	struct tm *tptr;
 	long	tick;
 	char	*p;
 	int	perm_min_hours, perm_min_minutes,
 		perm_max_hours, perm_max_minutes;
 	int	now, perm_min, perm_max;

 	tick = time(NULL);
	tptr = localtime(&tick);
 	now = tptr->tm_hour * 60 + tptr->tm_min;

	while (interval)
	  {
	    p = (char *)index(interval, ',');
	    if (p)
	      *p = '\0';
	    if (sscanf(interval, "%2d%2d-%2d%2d",
		       &perm_min_hours, &perm_min_minutes,
		       &perm_max_hours, &perm_max_minutes) != 4) {
	      if (p)
		*p = ',';
	      return(0);
	    }
	    if (p)
	      *(p++) = ',';

	    perm_min = 60 * perm_min_hours + perm_min_minutes;
	    perm_max = 60 * perm_max_hours + perm_max_minutes;
	    
           /*
           ** The following check allows intervals over midnight ...
           */

	    if ((perm_min < perm_max)
                ? (perm_min <= now && now <= perm_max)
                : (perm_min <= now || now <= perm_max))
	      {
		sprintf(reply, ":%%s %%d %%s :%s %d:%02d to %d:%02d.",
			"You are not allowed to connect from",
                        perm_min_hours, perm_min_minutes,
                        perm_max_hours, perm_max_minutes);
                return(ERR_YOUREBANNEDCREEP);
              }
	    if ((perm_min < perm_max)
                ? (perm_min <= now + 5 && now + 5 <= perm_max)
                : (perm_min <= now + 5 || now + 5 <= perm_max))
              {
		sprintf(reply, ":%%s %%d %%s :%d minute%s%s",
                        perm_min - now, (perm_min - now) > 1 ? "s " : " ",
                        "and you will be denied for further access");
                return(ERR_YOUWILLBEBANNED);
	      }
	    interval = p;
	  }
	return(0);
}
