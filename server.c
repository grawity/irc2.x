/*************************************************************************
 ** server.c  Beta  v2.1    (16 Nov 1989)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen 
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 ** 
 *************************************************************************/

char conf_id[] = "conf.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <stdio.h>
#include "struct.h"
#include "sys.h"


aConfItem *conf = NULL;
extern int portnum;


char * MyMalloc(size)
int size;
{
  register char *p = (char *) malloc(size);
  if (!p) {
    perror("malloc");
    debug(DEBUG_FATAL, "Out of memory: restarting server...");
    restart();
  }
  return p;
}

#define AllocString(tmp) (MyMalloc(strlen(tmp)+1))

aConfItem *
make_conf()
{
  aConfItem *cptr = (aConfItem *) MyMalloc(sizeof(aConfItem));

  if (cptr) {
    cptr->next = conf;
    conf = cptr;
    cptr->host = cptr->passwd = cptr->name = NULL;
    cptr->status = CONF_ILLEGAL;
    cptr->port = 0;
  }
  return (cptr);
}

matches(name1, name2)
char *name1, *name2;
{
  char c1, c2;
  for (; *name1 && *name2; name1++, name2++) {
    c1 = (isupper(*name1)) ? tolower(*name1) : *name1;
    c2 = (isupper(*name2)) ? tolower(*name2) : *name2;
    if (c1 == c2) 
      continue;
    if (c1 == '?' || c2 == '?')
      continue;
    if (*name1 == '*') {
      if (*(++name1) == '\0')
	return(0);
      for (; *name2 && matches(name1, name2); name2++);
      return *name2 ? 0 : 1;
    }
#ifdef old
    if (*name2 == '*') {
      if (*(++name2) == '\0')
	return(0);
      for (; *name1 && matches(name1, name2); name1++);
      if (*name1)
	return(0);
      else
	return(1);
   }
#endif 
    break;
  }
  return (*name1 != '\0' || *name2 != '\0');
}

aConfItem *
find_admin()
{
  aConfItem *aconf = conf;
  for (; aconf; aconf = aconf->next) 
    if (aconf->status & CONF_ADMIN)
      break;
  
  return (aconf);
}

aConfItem *
find_me()
{
  aConfItem *aconf = conf;
  for (; aconf; aconf = aconf->next) 
    if (aconf->status & CONF_ME)
      break;
  
  return (aconf);
}

aConfItem *
find_conf(host, aconf, name, statmask)
char *host, *name;
aConfItem *aconf;
int statmask;
{
  aConfItem *tmp = conf;
  int len = strlen(host);
  int namelen;
  if (name)
    namelen = strlen(name);
  for (; tmp; tmp = tmp->next) 
    if ((tmp->status & statmask) && len < HOSTLEN && !matches(tmp->host, host)) 
      if (name == NULL || (!matches(tmp->name, name) && namelen < HOSTLEN))
      break;

  return (tmp) ? tmp : aconf;
}



rehash()
{
  aConfItem *tmp = conf, *tmp2;
  for (; tmp; tmp = tmp2) {
    tmp2 = tmp->next;
    free(tmp);
  }
  conf = (aConfItem *) 0;
  initconf();
}

extern char * getfield();

initconf()
{
  FILE *fd;
  char line[256], *tmp;
  aConfItem *aconf = (aConfItem *) 0;
  if (!(fd = fopen(CONFIGFILE,"r")))
    return(-1);

  while (fgets(line,255,fd)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == ' ' || line[0] == '\t')
      continue;
    if (!aconf || !ISSKIPME(aconf) || !ISILLEGAL(aconf)) {
      aconf = make_conf();
    }
    aconf->status = CONF_ILLEGAL;
    switch (*getfield(line)) {
    case 'C':   /* Server where I should try to connect */
    case 'c':   /* in case of link failures             */
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
      aconf->status = CONF_NOCONNECT_SERVER;
      break;
    case 'U':   /* Uphost, ie. host where client reading */
    case 'u':   /* this should connect.                  */
                /* This is for client only, I must ignore this */
      aconf->status = CONF_SKIPME;
      break;    
    case 'O':   /* Operator. Line should contain at least */
    case 'o':   /* password and host where connection is  */
      aconf->status = CONF_OPERATOR;      /* allowed from */
      break;
    case 'M':   /* Me. Host field is name used for this host */
    case 'm':   /* and port number is the number of the port */
      aconf->status = CONF_ME;
      break;
    case 'A':   /* Name, e-mail address of administrator of this */
    case 'a':   /* server. */
      aconf->status = CONF_ADMIN;
      break;
    default:
      debug(DEBUG_ERROR, "Error in config file: %s", line);
      break;
    }
    if (ISSKIPME(aconf) || ISILLEGAL(aconf)) {
      conf = conf->next;
      free(aconf);
      aconf = (aConfItem *) 0;
      continue;
    }
    if (tmp = getfield(NULL)) {
      aconf->host = AllocString(tmp); 
      strcpy(aconf->host, tmp);
      if (tmp = getfield(NULL)) {
        aconf->passwd = MyMalloc(strlen(tmp)+1);
	strcpy(aconf->passwd, tmp);
	if (tmp = getfield(NULL)) {
          aconf->name = AllocString(tmp); 
	  strcpy(aconf->name, tmp);
	  if (tmp = getfield(NULL)) {
	    if ((aconf->port = atoi(tmp)) == 0)
	      debug(DEBUG_ERROR, "Error in config file, illegal port field");
	  }
	}
      }
    }
    if (ISMECONF(aconf)) {
      if (aconf->port > 0)
	portnum = aconf->port;
      if (aconf->host[0]) 
	CopyFixLen(myhostname,aconf->host,HOSTLEN+1);
    }
    debug(DEBUG_NOTICE, "Read Init: (%d) (%s) (%s) (%s) (%d)",
	  aconf->status, aconf->host, aconf->passwd,
	  aconf->name, aconf->port);
  }
}

