/*************************************************************************
 ** bsd.c  Beta  v2.0    (22 Aug 1988)
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

char bsd_id[] = "bsd.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"

dummy()
{
  return(0);
}

deliver_it(fd, str, len)
int fd, len;
char *str;
{
  int retval;
  alarm(WRITEWAITDELAY);
#if VMS
  retval = netwrite(fd, str, len);
#else
  retval = write(fd, str, len);
#endif
  alarm(0);
  return(retval);
}

