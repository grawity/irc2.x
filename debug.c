/*************************************************************************
 ** debug.c  Beta  v2.0    (22 Aug 1988)
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
 
char debug_id[] = "debug.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include "struct.h"
#if HPUX
#include <fcntl.h>
#endif

extern int debuglevel;

debug(level, form, para1, para2, para3, para4, para5, para6)
int level;
char *form, *para1, *para2, *para3, *para4, *para5, *para6;
{
  if (debuglevel >= 0) 
    if (level <= debuglevel) { 
      fprintf(stderr, form, para1, para2, para3, para4, para5, para6);
      fprintf(stderr, "\n");
    } 
}

OpenLog()
{
  int fd;
#ifdef NOTTY
  if (debuglevel >= 0) {
    if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
      if ((fd = open("/dev/null", O_WRONLY)) < 0)
        exit(-1);
    if (fd != 2) {
      dup2(fd, 2);
      close(fd); 
    }
  } else {
    if ((fd = open("/dev/null", O_WRONLY)) < 0) 
      exit(-1);
    if (fd != 2) {
      dup2(fd, 2);
      close(fd);
    }
  }
#endif
}
