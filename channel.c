/*************************************************************************
 ** channel.c  Beta  v2.1    (27 Apr 1989)
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

char channel_id[] = "channel.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
extern aChannel *channel;


aChannel *
find_channel(chname, para)
char *chname;
aChannel *para;
{
  aChannel *ch2ptr = channel;
  char *ch = chname;
  int chan;
  while (*ch) {
    if (*ch < '0' || *ch > '9')
      break;
    ch++;
  }
  if (*ch)
    return (para);
  if ((chan = atoi(chname)) == 0)
    return (para);
  while (ch2ptr) {
    if (ch2ptr->channo == chan)
      break;
    ch2ptr = ch2ptr->nextch;
  }
  if (ch2ptr)
    return (ch2ptr);
  else
    return (para);
}
