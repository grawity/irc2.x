/*************************************************************************
 ** str.c  Beta  v2.0    (22 Aug 1988)
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

char str_id[] = "str.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "sys.h"

char *
center(buf,str,len)
char *buf, *str;
int len;
{
  char i,j,k;
  if ((i = strlen(str)) > len) {
    buf[len-1] = '\0';
    for(len--; len > 0; len--) buf[len-1] = str[len-1];
    return(buf);
  }
  j = (len-i)/2;
  for (k=0; k<j; k++) buf[k] = ' ';
  buf[k] = '\0';
  strcat(buf,str);
  for (k=j+i; k<len; k++) buf[k] = ' ';
  buf[len] = '\0';
  return (buf);
}

/* William Wisner <wisner@b.cc.umich.edu>, 16 March 1989 */
char *
real_name(user)
     struct passwd *user;
{
  char *bp, *cp;
  static char name[REALLEN+1];

  bp = user->pw_gecos;
  cp = name;

  name[REALLEN] = '\0';
  do {
    switch(*bp) {
    case '&':
      *cp = '\0';
      strncat(name, user->pw_name, REALLEN-strlen(name));
      name[REALLEN] = '\0';
      if (islower(*cp))
	*cp = toupper(*cp);
      cp = index(name, '\0');
      bp++;
      break;
    case ',':
      *bp = *cp = '\0';
      break;
    case '\0':
      *cp = *bp;
      break;
    default:
      *cp++ = *bp++;
    }
  } while (*bp != '\0' && strlen(name) < REALLEN);
  return(name);
}

