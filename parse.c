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
extern aClient *client;
char sender[20];

int
myncmp(str1, str2, n)
char *str1, *str2;
int n;
{
  while (SameLetter(*str1, *str2)) {
    str1++; str2++; n--;
    if (n == 0 || (*str1 == '\0' && *str2 == '\0'))
      return(0);
  }
  return(1);
}

aClient *
find_client(ch, cptr)
char *ch;
aClient *cptr;
{
  aClient *c2ptr=client;
  int len;
  char *ch2 = index(ch,' ');
  if (ch2)
    len = ch2 - ch;
  else
    len = strlen(ch);
  if (len > NICKLEN)
    len = NICKLEN;
  for ( ; c2ptr; c2ptr = c2ptr->next) 
    if (strlen(c2ptr->nickname) == len &&
	MyEqual(ch, c2ptr->nickname, len))
      break;
  
  return ((c2ptr) ? c2ptr : cptr);
}

aClient *
find_server(ch, cptr)
char *ch;
aClient *cptr;
{
  aClient *c2ptr=client;
  int len;
  char *ch2;
  if (ch == NULL)
    return(cptr);
  ch2 = index(ch,' ');
  if (ch2)
    len = ch2 - ch;
  else
    len = strlen(ch);
  for ( ; c2ptr ; c2ptr = c2ptr->next)
    if ((ISSERVER(c2ptr) || ISME(c2ptr)) &&
        strlen(c2ptr->host) == len && MyEqual(ch, c2ptr->host, len))
      break;
  
  return ((c2ptr) ? c2ptr : cptr);
}

#define Range(x) ((x) < 32 || (x) > 126)

#define SkipBlanks(x) while(*(x) == ' ')  (x)++;

parse(cptr, buffer, length, mptr)
aClient *cptr;
char *buffer;
int length;
struct Message *mptr;
{
  aClient *from = cptr;
  char *ch, *ch2, *para[MAXPARA+1];
  int len, i, numeric, paramcount;
  int (*func)();
  *sender = '\0';
  for (i=0; i<length; i++)
    if (buffer[i] && buffer[i] != BellChar && Range(buffer[i]))
      buffer[i] = ' '; 
  ch = buffer;
  SkipBlanks(ch);
    
  if (*ch == ':') {
    ch++;
    if (cptr->status != STAT_SERVER)
      from = cptr;
    else {
      from = find_client(ch, cptr);
      if (!MyEqual(from->fromhost, cptr->fromhost, HOSTLEN)) {
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
  if (!ch) {
    debug(DEBUG_NOTICE, "Empty message from %s:%s (host %s)",
	  cptr->nickname,from->nickname, cptr->host);
    return(-1);
  }
  SkipBlanks(ch); 
  ch2 = index(ch, ' ');
  len = (ch2) ? (ch2 - ch) : strlen(ch);
  if (len == 3 && isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2))) {
    mptr = (struct Message *) 0;
    numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10 + (*(ch + 2) - '0');
    paramcount = 11;
  } else {
    for (; mptr->cmd; mptr++) 
      if (MyEqual(mptr->cmd, ch, len) && strlen(mptr->cmd) == len)
	break;
    
    if (!mptr->cmd) {
      if (buffer[0] != '0')
	sendto_one(from, ":%s %d %s %s Unknown command", myhostname,
		   ERR_UNKNOWNCOMMAND, from->nickname, ch);
      return(-1);
    }
    paramcount = mptr->parameters;
  }
  i = 0;
  while (ch2 && *ch2 && i < MAXPARA-1 && i < paramcount) {
    SkipBlanks(ch2);
      
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
  for (; i <= MAXPARA; i++) 
    para[i] = NULL;

  if (!mptr) 
    return (do_numeric(numeric, cptr, from, sender, para[0], para[1], para[2],
		       para[3], para[4], para[5], para[6], para[7], para[8]));
    func = mptr->func;
    mptr->count++;
    return (*func)(cptr, from, sender,
		   para[0], para[1], para[2], para[3], para[4], para[5],
		   para[6], para[7], para[8]);
}
