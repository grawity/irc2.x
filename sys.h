/*************************************************************************
 ** bsd.h  Beta  v2.0    (22 Aug 1988)
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

#if HPUX | VMS
#include <string.h>
#define index         strchr
#define rindex        strrchr
#define bcopy(a,b,s)  memcpy(b,a,s)
#define bzero(a,s)    memset(a,0,s)
extern char *strchr(), *strrchr();
extern char *inet_ntoa();
#else 
#include <strings.h>
#endif
#include <ctype.h>
#include <pwd.h>
