/*************************************************************************
 ** packet.c  Beta  v2.0    (22 Aug 1988)
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
 
char packet_id[]="packet.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "msg.h"
#include "sys.h"

#define IsEOL(x) ((x) == '\r' || (x) == '\n')

dopacket(cptr, buffer, length)
aClient *cptr;
char *buffer;
int length;
{
  char *ch1, *ch2, *ch3;

  strncat(cptr->buffer, buffer, length);
  ch1 = ch2 = cptr->buffer;
  for (; *ch2; ch2++) {
    if (!IsEOL(*ch2))
      continue; 
    if (ch2 != ch1) {
      ch1[ch2-ch1] = '\0';
      if (parse(cptr, ch1, ch2 - ch1, msgtab) == FLUSH_BUFFER) 
	return(0);
    }
    if (IsEOL(ch2[1]))
      ch2++;
    ch1 = ch2+1;
  }
  for (ch3 = cptr->buffer; *ch1; )
    *ch3++ = *ch1++;
  *ch3 = '\0';
}


#ifndef NULL
#define NULL  ((char *) 0)
#endif

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

