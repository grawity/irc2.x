/*************************************************************************
 ** conf.c  Beta  v2.0    (22 Aug 1988)
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

extern char *getfield();

initconf(host, passwd, myname, port)
char *host, *passwd, *myname;
int *port;
{
  FILE *fd;
  char line[256], *tmp;
  if ((fd = fopen(CONFIGFILE,"r")) == NULL)
    return(-1);
  while (fgets(line,255,fd)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == ' ' || line[0] == '\t')
      continue;
    switch (*getfield(line)) {
    case 'C':   /* Server where I should try to connect */
    case 'c':   /* in case of link failures             */
    case 'I':   /* Just plain normal irc client trying  */
    case 'i':   /* to connect me */
    case 'N':   /* Server where I should NOT try to     */
    case 'n':   /* connect in case of link failures     */
                /* but which tries to connect ME        */
    case 'O':   /* Operator. Line should contain at least */
    case 'o':   /* password and host where connection is  */
                /* allowed from */
    case 'M':   /* Me. Host field is name used for this host */
    case 'm':   /* and port number is the number of the port */
    case 'a':
    case 'A':
    case 'k':
    case 'K':
      break;
    case 'U':   /* Uphost, ie. host where client reading */
    case 'u':   /* this should connect.                  */
      if (tmp = getfield(NULL)) {
	strncpy(host, tmp, HOSTLEN - 1);
	host[HOSTLEN-1] = '\0';
	if (tmp = getfield(NULL)) {
	  strncpy(passwd, tmp, PASSWDLEN - 1);
	  passwd[PASSWDLEN-1] = '\0';
	  if (tmp = getfield(NULL)) {
	    strncpy(myname, tmp, HOSTLEN - 1);
	    myname[HOSTLEN-1] = '\0';
	    if (tmp = getfield(NULL)) {
	      if ((*port = atoi(tmp)) == 0)
		debug(DEBUG_ERROR, "Error in config file, illegal port field");
	    }
	  }
	}
      }
      break;    
    default:
      debug(DEBUG_ERROR, "Error in config file: %s", line);
      break;
    }
  }
}
