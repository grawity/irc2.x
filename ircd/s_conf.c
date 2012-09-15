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

/*
 * $Id: s_conf.c,v 6.1 1991/07/04 21:05:27 gruner stable gruner $
 *
 * $Log: s_conf.c,v $
 * Revision 6.1  1991/07/04  21:05:27  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:44  gruner
 * frozen beta revision 2.6.1
 *
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

char conf_id[] = "conf.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <stdio.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"

aConfItem *conf = NULL;

extern int portnum;
extern char *configfile;
extern long nextconnect;

char *MyMalloc(x)
int x;
    {
	char *ret = (char *) malloc(x);

	if (!ret)
	    {
		debug(DEBUG_FATAL, "Out of memory: restarting server...");
		restart();
	    }
	return ret;
    }

char *MyRealloc(p,x)
char *p; int x;
{
  char *ret=(char*)realloc(p,x);
 
  if (!ret)
    {    
      debug(DEBUG_FATAL, "Out of memory: restarting server...");
      restart();
    }
  return ret;
}  

static aConfItem *make_conf()
    {
	aConfItem *cptr =(struct ConfItem *) MyMalloc(sizeof(struct ConfItem));
	
	cptr->next = NULL;
	cptr->host = cptr->passwd = cptr->name = NULL;
	cptr->status = CONF_ILLEGAL;
	cptr->clients = 0;
	cptr->port = 0;
	cptr->hold = 0;
	Class(cptr) = 0;
	return (cptr);
    }

static free_conf(aconf)
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
det_confs_butone(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
  Reg1 Link *tmp;
  tmp = cptr->confs;
  while (tmp && tmp->value == (char *) aconf)
    tmp = tmp->next;
  while (tmp) {
    detach_conf(cptr, tmp->value);
    tmp = cptr->confs;
    while (tmp && tmp->value == (char *) aconf)
      tmp = tmp->next;
  }
}

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
det_confs_butmask(cptr, mask)
aClient *cptr;
int mask;
{
  Reg1 Link *tmp, *tmp2;
  tmp = cptr->confs;
  while (tmp && tmp->value) {
    tmp2 = tmp->next;
    if ( ( ((aConfItem *)tmp->value)->status & mask) == 0)
      detach_conf(cptr, tmp->value);
    tmp = tmp2;
  }
}
/*
 * remove all attached I lines except for the first one.
 */
det_I_lines_butfirst(cptr)
aClient *cptr;
{
  Reg1 Link *tmp, *tmp2;
  Link *first = (Link *)NULL;
  tmp = cptr->confs;
  while (tmp && tmp->value) {
    tmp2 = tmp->next;
    if ( ((aConfItem *)tmp->value)->status == CONF_CLIENT)
      if (first)
	detach_conf(cptr, tmp->value);
      else
	first = tmp;
    tmp = tmp2;
  }
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
detach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
  Reg1 Link **link, *tmp;
  int status;

  link = &(cptr->confs);

  while (*link) {
    if ((*link)->value == (char *) aconf) {
      if ((aconf) && (Class(aconf))) {
	status = aconf->status;
	if (IsIllegal(aconf))
	  aconf->status *= CONF_ILLEGAL;
	if (aconf->status & (CONF_CLIENT | CONF_CONNECT_SERVER |
	    CONF_NOCONNECT_SERVER))
	  if (ConfLinks(aconf) > 0)
	     --ConfLinks(aconf);
        if (ConfMaxLinks(aconf) == -1 && ConfLinks(aconf) == 0)
	  free(Class(aconf));
      }
      aconf->status = status;
      if (aconf != NULL && --aconf->clients == 0 && IsIllegal(aconf))
	free_conf(aconf);
      tmp = *link;
      *link = (*link)->next;
      free(tmp);
    } else
      link = &((*link)->next);
  }
}

static int
IsAttached(aconf, cptr)
aConfItem *aconf;
aClient *cptr;
{
  Reg1 Link *link = cptr->confs;
  while (link) {
    if (link->value == (char *) aconf)
      break;
    link = link->next;
  }
  return (link) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
attach_conf(aconf,cptr)
aConfItem *aconf;
aClient *cptr;
{
  Reg1 Link *link;
  if (IsAttached(aconf, cptr))
    return 1;
  link = (Link *) MyMalloc(sizeof(Link));
  link->next = cptr->confs;
  link->value = (char *) aconf;
  cptr->confs = link;
  aconf->clients += 1;
  if (aconf->status & (CONF_CLIENT | CONF_CONNECT_SERVER |
      CONF_NOCONNECT_SERVER))
    ConfLinks(aconf)++;
  return 1;
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

aConfItem *attach_confs(cptr, host, statmask)
aClient *cptr;
char *host;
int statmask;
{
  Reg1 aConfItem *tmp;
  aConfItem *first = (aConfItem *) 0;
  int len = strlen(host);
  
  if (len > HOSTLEN)
    return (aConfItem *) 0;

  for (tmp = conf; tmp; tmp = tmp->next) {
    if ((tmp->status & statmask) &&
	(tmp->status & (CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)) == 0 &&
	(matches(tmp->host, host) == 0)) {
      if (!first)
	first = tmp;
      attach_conf(tmp, cptr);
    } else if ((tmp->status & statmask) &&
	       (tmp->status & (CONF_CONNECT_SERVER|CONF_NOCONNECT_SERVER)) &&
	       (mycmp(tmp->host, host) == 0)) {
      if (!first)
	first = tmp;
      attach_conf(tmp, cptr);
    }
  }
  return(first);
}

aConfItem *FindConfName(name, statmask)
char *name;
int statmask;
{
  Reg1 aConfItem *tmp;
  
  for (tmp = conf; tmp; tmp = tmp->next) {
    /*
     ** Accept if the *real* hostname (usually sockecthost)
     ** matches *either* host or name field of the configuration.
     */
    if ((tmp->status & statmask) &&
	(matches(tmp->name, name) == 0)) {
      break;
    }
  }
  return(tmp);
}

aConfItem *FindConf(link, name, statmask)
char *name;
Link *link;
int statmask;
{
  Reg1 aConfItem *tmp;
  int namelen = name ? strlen(name) : 0;
  
  if (namelen > HOSTLEN)
    return (aConfItem *) 0;

  for (; link; link = link->next) {
    tmp = (aConfItem *) link->value;
    if ((tmp->status & statmask) &&
	(((tmp->status & (CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER)) == 0
	 && (!name || matches(tmp->name, name) == 0)) ||
	((tmp->status & (CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER)
	 && (!name || mycmp(tmp->name, name) == 0)))))
      break;
  }
  return(link ? tmp : (aConfItem *) 0);
}

rehash()
    {
	Reg1 aConfItem *tmp = conf, *tmp2;
	Reg2 aClass *cltmp, *cltmp2;

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
		tmp->status *= CONF_ILLEGAL;
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

	conf = (aConfItem *) 0;
	initconf();
    }

extern char *getfield();

/**
 ** initconf() 
 **    Read configuration file.
 **
 **    returns -1, if file cannot be opened
 **             0, if file opened
 **/

#define MAXCONFLINKS 150

initconf()
    {
	FILE *fd;
	char line[256], *tmp, type, c[80];
	int ccount = 0, ncount = 0, illegal;
	aConfItem *aconf;
	if (!(fd = fopen(configfile,"r")))
		return(-1);
	while (fgets(line,255,fd))
	    {
		if (line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		aconf = make_conf();
		illegal = 0;

		switch (*getfield(line))
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
		  AddClass(atoi(aconf->host), atoi(aconf->passwd),
			   atoi(aconf->name), aconf->port);
		}

		if (aconf->status & (CONF_CONNECT_SERVER |
		    CONF_NOCONNECT_SERVER | CONF_CLASS)) {
		  if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
		      aconf->host && index(aconf->host, '*')) {
		    if (aconf->host)
		      free(aconf->host);
		    if (aconf->passwd)
		      free(aconf->passwd);
		    if (aconf->name)
		      free(aconf->name);
		    conf = aconf->next;
		    free(aconf);
		    continue;
		  }
		}

		/*
                ** associate each conf line with a class by using a pointer
                ** to the correct class record. -avalon
                */
		if (aconf->status & (CONF_CONNECT_SERVER | CONF_CLIENT |
		    CONF_NOCONNECT_SERVER | CONF_OPERATOR | CONF_LOCOP)) {
		  if (Class(aconf) == 0)
		    Class(aconf) = find_class(0);
		  if (MaxLinks(Class(aconf)) < 0)
		    Class(aconf) = find_class(0);
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
	nextconnect = time(NULL);
	return (0);
    }

int find_kill(host, name, reply)
char *host, *name, *reply;
    {
	aConfItem *tmp;
	static int check_time_interval();
	int rc = ERR_YOUREBANNEDCREEP;

	if (strlen(host)  > HOSTLEN || (name ? strlen(name) : 0) > HOSTLEN)
		return (0);
	strcpy(reply, ":%s %d %s :*** Ghosts are not allowed on IRC.");
	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((matches(tmp->host, host) == 0) &&
		    tmp->status == CONF_KILL &&
 		    (name == NULL || matches(tmp->name, name) == 0))
 			if (BadPtr(tmp->passwd) ||
 			   (rc = check_time_interval(tmp->passwd, reply)))
 			break;
 		return (tmp ? rc : 0);
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
  
int find_restrict(host, name, reply)
char *host, *name, *reply;
    {
	aConfItem *tmp;
	char cmdline[132],*rplhold=reply,temprpl[80],rplchar='Y';
	FILE *fp;
	int rc = 0;

	for (tmp = conf; tmp; tmp = tmp->next)
	  if (tmp->status == CONF_RESTRICT &&
	      (host == NULL || (matches(tmp->host, host) == 0)) &&
	      (name == NULL || matches(tmp->name, name) == 0))
	    {
	      bzero(rplhold,5);
	      if (BadPtr(tmp->passwd))
		sendto_ops("Program missing on R-line %s/%s, ignoring.",
			   name,host);
	      else
		{
		  sprintf(cmdline,"%s %s %s",tmp->passwd,name,host);
		  if ((fp=popen(cmdline,"r"))==NULL)
		    sendto_ops("Couldn't run '%s' for R-line %s/%s, ignoring.",
			       tmp->passwd,name,host);
		  else
		    while (fscanf(fp,"%[^\n]\n",temprpl)!=EOF)
		      if (strlen(temprpl)+strlen(reply) < 131)
			sprintf(rplhold,"%s %s",rplhold,temprpl);
		      else
			sendto_ops("R-line %s/%s: reply too long, truncating",
				   name,host);
		  pclose(fp);
		  while (*rplhold == ' ') rplhold++;
		  rplchar = *rplhold; /* Pull out the yes or no */
		  while (*rplhold != ' ') rplhold++;
		  while (*rplhold == ' ') rplhold++;
		  strcpy(reply,rplhold);
		}
	      if (rc=(rplchar == 'n' || rplchar == 'N'))
		break;
	    }
	return (rc);
      }
#endif


/*
** check against a set of time intervals
*/

static int check_time_interval(interval, reply)
char	*interval, *reply;
{
	struct tm *tptr;
 	long	tick;
 	char	*p, *oldp;
 	int	perm_min_hours, perm_min_minutes,
 		perm_max_hours, perm_max_minutes;
 	int	now, perm_min, perm_max;

 	tick = time(NULL);
	tptr = localtime(&tick);
 	now = tptr->tm_hour * 60 + tptr->tm_min;

	while (interval)
	  {
	    p = index(interval, ',');
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
