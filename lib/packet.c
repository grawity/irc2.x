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

dopacket(cptr, buffer, length)
struct Client *cptr;
char *buffer;
int length;
{
  char *ch1, *ch2, *ch3;
  strncat(cptr->buffer, buffer, length);
  ch1 = ch2 = cptr->buffer;
  while (*ch2) {
    if (*ch2 == '\r' || *ch2 == '\n') {
      if (ch2 != ch1) {
	ch1[ch2-ch1] = '\0';
	if (parse(cptr, ch1, ch2 - ch1, msgtab) == FLUSH_BUFFER) 
	  return(0);
      }
      if (*(ch2+1) == '\r' || *(ch2+1) == '\n')
	ch2++;
      ch1 = ch2+1;
    }
    ch2++;
  }
  for (ch3 = cptr->buffer; *ch1; ch1++, ch3++)
    *ch3 = *ch1;
  *ch3 = '\0';
}
