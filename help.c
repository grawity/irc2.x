/*************************************************************************
 ** help.c  Beta v2.0    (22 Aug 1988)
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

char help_id[]="help.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "sys.h"
#include "help.h"

char helpbuf[80];

do_help(ptr, temp)
char *ptr, *temp;
{
  struct Help *hptr;
  int count;

  if (ptr == NULL || *ptr == '\0') {
    sprintf(helpbuf, "*** Help: Internet Relay Chat v%s Commands:", version);
    putline(helpbuf);
    count = 0;
    for (hptr = helplist; hptr->command; hptr++) {
      sprintf(&helpbuf[count*10], "%10s", hptr->command);
      if (++count >= 6) {
	count = 0;
	putline(helpbuf);
      }
    }
    if (count)
      putline(helpbuf);
    putline("Type /HELP <command> to get help about a particular command.");
    putline("For example \"/HELP signoff\" gives you help about the");
    putline("/SIGNOFF command. To use a command you must prefix it with a");
    putline("slash or whatever your current command character is (see");
    putline("\"/HELP cmdch\"");
    putline("*** End Help");
  } else {
#ifdef never
    for (ch = ptr; *ch; ch++)
      if (islower(*ch))
	*ch = toupper(*ch);
#endif 
    hptr = helplist;
    while (hptr->command) {
      if (mycncmp(ptr, hptr->command)) 
	break;
      hptr++;
    }
    if (hptr->command == (char *) 0) {
      putline("*** There is no help information for that command.");
      putline("*** Type \"/HELP\" to get a list of commands.");
      return(0);
    }
    sprintf(helpbuf, "*** Help: %s", hptr->syntax);
    putline(helpbuf);
    for (count = 0; count < 5; count++)
      if (hptr->explanation[count] && *(hptr->explanation[count])) {
	sprintf(helpbuf, "    %s", hptr->explanation[count]);
	putline(helpbuf);
      }
    putline("*** End Help");
  }
  return(0);
}
