/*************************************************************************
 ** parse.c  Beta  v2.0    (22 Aug 1988)
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
 
char parse_id[] = "parse.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <ctype.h>
#define MSGTAB
#include "msg.h"
#undef MSGTAB
#include "struct.h"
#include "sys.h"
#include "numeric.h"

#define NULL ((char *) 0)

int msgcount[40];
extern struct Client *client;
char sender[20];

int
myncmp(str1, str2, n)
char *str1, *str2;
int n;
{
  while ((islower(*str1) ? toupper(*str1) : *str1) ==
	 (islower(*str2) ? toupper(*str2) : *str2)) {
    str1++; str2++; n--;
    if (n == 0 || (*str1 == '\0' && *str2 == '\0'))
      return(0);
  }
  return(1);
}

struct Client *
find_client(ch, cptr)
char *ch;
struct Client *cptr;
{
  struct Client *c2ptr=client;
  int len;
  char *ch2 = index(ch,' ');
  if (ch2)
    len = ch2 - ch;
  else
    len = strlen(ch);
  if (len > NICKLEN)
    len = NICKLEN;
  while (c2ptr) {
    if (strlen(c2ptr->nickname) == len &&
	myncmp(ch, c2ptr->nickname, len) == 0)
      break;
    c2ptr = c2ptr->next;
  }
  return ((c2ptr) ? c2ptr : cptr);
}

struct Client *
find_server(ch, cptr)
char *ch;
struct Client *cptr;
{
  struct Client *c2ptr=client;
  int len;
  char *ch2;
  if (ch == NULL)
    return(cptr);
  ch2 = index(ch,' ');
  if (ch2)
    len = ch2 - ch;
  else
    len = strlen(ch);
  while (c2ptr) {
    if ((c2ptr->status == STAT_SERVER || c2ptr->status == STAT_ME) &&
        strlen(c2ptr->host) == len && myncmp(ch, c2ptr->host, len) == 0)
      break;
    c2ptr = c2ptr->next;
  }
  return ((c2ptr) ? c2ptr : cptr);
}

parse(cptr, buffer, length, mptr)
struct Client *cptr;
char *buffer;
int length;
struct Message *mptr;
{
  struct Client *from = cptr;
  char *ch, *ch2, *para[MAXPARA+1];
  int len, i, numeric, paramcount;
  int (*func)();
  *sender = '\0';
  for (i=0; i<length; i++)
    if (buffer[i] && buffer[i] != '\007' &&
	(buffer[i] < 32 || buffer[i] > 126))
      buffer[i] = ' '; 
  for (ch = buffer; *ch == ' '; ch++);
  if (*ch == ':') {
    ch++;
    if (cptr->status != STAT_SERVER)
      from = cptr;
    else {
      from = find_client(ch, cptr);
      if (myncmp(from->fromhost, cptr->fromhost, HOSTLEN)) {
	debug(DEBUG_FATAL, "Message (%s) coming from (%s)",
	      buffer, (cptr->status == STAT_SERVER) ?
	      cptr->host : cptr->nickname);
	return (-1);
      }
    }
    strncpy(sender, ch, 15);
    for (ch2 = sender; (ch2 < &sender[15]) && *ch2 > 32 && *ch2 < 126; ch2++);
    *ch2 = '\0';
    ch = index(ch, ' ');
  }
  if (ch == NULL) {
    debug(DEBUG_NOTICE, "Empty message from %s:%s (host %s)",
	  cptr->nickname,from->nickname, cptr->host);
    return(-1);
  }
  while (*ch == ' ') ch++;
  ch2 = index(ch, ' ');
  len = (ch2) ? (ch2 - ch) : strlen(ch);
  if (len == 3 && isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2))) {
    mptr = (struct Message *) 0;
    numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10 + (*(ch + 2) - '0');
    paramcount = 11;
  } else {
    while (mptr->cmd) {
      if (myncmp(mptr->cmd, ch, len) == 0 && strlen(mptr->cmd) == len)
	break;
      mptr++;
    }
    if (mptr->cmd == NULL) {
      if (buffer[0] != '0')
	sendto_one(from, ":%s %d %s %s Unknown command", myhostname,
		   ERR_UNKNOWNCOMMAND, from->nickname, ch);
      return(-1);
    }
    paramcount = mptr->parameters;
  }
  i = 0;
  while (ch2 && *ch2 && i < MAXPARA-1 && i < paramcount) {
    for (; *ch2 == ' '; ch2++);
    *(ch2-1) = '\0';
    if (*ch2 == '\0')
      para[i++] = NULL;
    else {
      para[i] = ch2;
      if (*ch2 == ':') {
	para[i] = ch2 + 1;
	ch2 = NULL;
      }
      else 
	for (; *ch2 != ' ' && *ch2; ch2++);
      i++;
    }
  }
  for (; i <= MAXPARA; i++) para[i] = NULL;

  if (mptr == (struct Message *) 0) {
    return (do_numeric(numeric, cptr, from, sender, para[0], para[1], para[2],
		       para[3], para[4], para[5], para[6], para[7], para[8]));
  } else {
    func = mptr->func;
    mptr->count++;
    return (*func)(cptr, from, sender,
		   para[0], para[1], para[2], para[3], para[4], para[5],
		   para[6], para[7], para[8]);
  }
}


char *
getfield(newline)
char *newline;
{
  static char *line = NULL;
  char *end, *field;

  if (newline)
    line = newline;
  if (line == NULL)
    return(NULL);
  field = line;
  if ((end = index(line,':')) == NULL) {
    line = NULL;
    if ((end = index(field,'\n')) == NULL)
      end = field + strlen(field);
  } else
    line = end + 1;
  *end = '\0';
  return(field);
}
