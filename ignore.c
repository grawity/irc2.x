/*************************************************************************
 ** ignore.c  Beta v2.01.6    (09 May 1989)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988, 1989 Jarkko Oikarinen
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 **
 *************************************************************************/

char ignore_id[]="ignore.c v2.0 (c) 1988, 1989 Jarkko Oikarinen";

#include "struct.h"
#include "sys.h"

aIgnore *ignore = (aIgnore *) 0;
char ibuf[80];
extern aIgnore *find_ignore();

do_ignore(user, temp)
char *user, *temp;
{
  char *ch;
  aIgnore *iptr;
  char *apu = user;
  int status;
  if (!user || (*user == '\0')) {
    putline("*** Current ignore list entries:");
    for (iptr = ignore; iptr; iptr = iptr->next) {
      sprintf(ibuf,"    Ignoring %s messages from user %s", 
	      (ISIGNORE_TOTAL(iptr)) ? "all" :
	      (ISIGNORE_PRIVATE(iptr)) ? "private" : "public", 
	      iptr->user);
      putline(ibuf);
    }
    putline("*** End of ignore list entries");
    return (0);
  }
  while (apu && *apu) {
    ch = apu;
    if (*ch == '+') {
      ch++;
      status = IGNORE_PUBLIC;
    }
    else if (*ch == '-') {
      ch++;
      status = IGNORE_PRIVATE;
    }
    else 
      status = IGNORE_TOTAL;
    if (apu = index(ch, ','))
      *(apu++) = '\0';
    if (iptr = find_ignore(ch, (aIgnore *) 0)) {
      sprintf(ibuf,"*** Ignore removed: user %s", iptr->user);
      putline(ibuf);
      kill_ignore(iptr);
    } else {
      if (strlen(ch) > NICKLEN)
	ch[NICKLEN] = '\0';
      if (add_ignore(ch, status) >= 0) {
	sprintf(ibuf,"*** Ignore %s messages from user %s", 
		(status == IGNORE_TOTAL) ? "all" :
		(status == IGNORE_PRIVATE) ? "private" : "public", ch);
	putline(ibuf);
      } else
	putline("Fatal Error: Cannot allocate memory for ignore buffer");
    }
  }
}    

aIgnore *
find_ignore(user, para)
char *user;
aIgnore *para;
{
  aIgnore *iptr = ignore;
  for (; iptr; iptr = iptr->next)
    if (MyEqual(iptr->user, user, NICKLEN))
      break;
  return iptr ? iptr : para;
}

int
kill_ignore(iptr)
aIgnore *iptr;
{
  aIgnore *i2ptr = ignore, *i3ptr = (aIgnore *) 0;
  for (; i2ptr; i3ptr = i2ptr, i2ptr = i2ptr->next) 
    if (i2ptr == iptr)
      break;

  if (!i2ptr)
    return -1; 

  if (i3ptr)
    i3ptr->next = i2ptr->next;
  else
    ignore = i2ptr->next;
  free(i2ptr);
  return (1);
}

int add_ignore(ch, status)
char *ch;
int status;
{
  aIgnore *iptr = (aIgnore *) malloc(sizeof (aIgnore));
  if (!iptr)
    return(-1);
  CopyFixLen(iptr->user, ch, NICKLEN-1);
  iptr->next = ignore;
  ignore = iptr;
  iptr->flags = status;
  return(1);
}
