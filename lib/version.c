/*************************************************************************
 ** version.c  Beta  v2.0    (22 Aug 1988)
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

#include "struct.h"

char *intro = "Internet Relay Chat v%s";
char *version = "2.2PL0";
char *info1 = "Programmed by Jarkko Oikarinen";
char *info2 = "(c)  1988,1989   University of Oulu,   Computing Center";
char *info3 = "INTERNET: jto@tolsun.oulu.fi    BITNET: toljto at finou";
char myhostname[HOSTLEN+1];
char *HEADER = 
" *** Internet Relay Chat *** Type /help to get help *** Client v%s ***      ";
char *welcome1 = "Welcome to Internet Relay Server v";
