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

struct Ignore *ignore = (struct Ignore *) 0;
char ibuf[80];
extern struct Ignore *find_ignore();

do_ignore(user, temp)
char *user, *temp;
{
  char *ch;
  struct Ignore *iptr;
  char *apu = user;
  int status;
  if ((user == (char *) 0) || (*user == '\0')) {
    iptr = ignore;
    putline("*** Current ignore list entries:");
    while (iptr) {
      sprintf(ibuf,"    Ignoring %s messages from user %s", 
	      (iptr->flags == IGNORE_TOTAL) ? "all" :
	      (iptr->flags == IGNORE_PRIVATE) ? "private" : "public", 
	      iptr->user);
      putline(ibuf);
      iptr = iptr->next;
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
    if (iptr = find_ignore(ch, (struct Ignore *) 0)) {
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

struct Ignore *
find_ignore(user, para)
char *user;
struct Ignore *para;
{
  struct Ignore *iptr = ignore;
  while (iptr) {
    if (myncmp(iptr->user, user, NICKLEN) == 0)
      break;
    iptr = iptr->next;
  }
  if (iptr)
    return iptr;
  else
    return para;
}

int
kill_ignore(iptr)
struct Ignore *iptr;
{
  struct Ignore *i2ptr = ignore, *i3ptr = (struct Ignore *) 0;
  while (i2ptr) {
    if (i2ptr == iptr)
      break;
    i3ptr = i2ptr;
    i2ptr = i2ptr->next;
  }
  if (i2ptr) {
    if (i3ptr)
      i3ptr->next = i2ptr->next;
    else
      ignore = i2ptr->next;
    free(i2ptr);
    return (1);
  }
  return (-1);
}

int add_ignore(ch, status)
char *ch;
int status;
{
  struct Ignore *iptr;
  iptr = (struct Ignore *) malloc(sizeof (struct Ignore));
  if (iptr == (struct Ignore *) 0)
    return(-1);
  strncpy(iptr->user, ch, NICKLEN);
  iptr->user[NICKLEN] = '\0';
  iptr->next = ignore;
  ignore = iptr;
  iptr->flags = status;
  return(1);
}
