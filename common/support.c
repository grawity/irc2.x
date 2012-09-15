/************************************************************************
 *   IRC - Internet Relay Chat, common/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * $Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
 *
 * $Log: support.c,v $
 * Revision 6.1  1991/07/04  21:04:01  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:04:55  gruner
 * frozen beta revision 2.6.1
 *
 */

#include <sys/errno.h>
#include "config.h"
#include "common.h"
#include "sys.h"

extern int errno; /* ...seems that errno.h doesn't define this everywhere */

#if NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
**
**	$Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
*/

char *strtoken(save, str, fs)
char **save;
char *str, *fs;
{
    char *pos = *save;	/* keep last position across calls */
    Reg1 char *tmp;

    if (str)
	pos = str;		/* new string scan */

    while (pos && *pos && index(fs, *pos) != NULL)
	pos++; 		 	/* skip leading separators */

    if (!pos || !*pos)
	return (pos = *save = NULL); 	/* string contains only sep's */

    tmp = pos; 			/* now, keep position of the token */

    while (*pos && index(fs, *pos) == NULL)
	pos++; 			/* skip content of the token */

    if (*pos)
	*pos++ = NULL;		/* remove first sep after the token */
    else
	pos = NULL;		/* end of string */

    *save = pos;
    return(tmp);
}

/*
** NOT encouraged to use!
*/

char *strtok(str, fs)
char *str, *fs;
{
    static char *pos;

    return strtoken(&pos, str, fs);
}

#endif /* NEED_STRTOKEN */

#if NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   argv 11/90
**	$Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
*/

char *strerror(err_no)
int err_no;
{
    extern char *sys_errlist[];	 /* Sigh... hopefully on all systems */
    extern int sys_nerr;

    return(err_no > sys_nerr ? 
	"Undefined system error" : sys_errlist[err_no]);
}

#endif /* NEED_STRERROR */

#if NEED_INET_NTOA
/*
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX don't have this)
**			argv 11/90
**	$Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
*/

char *inet_ntoa(in)
struct in_addr in;
{
    static char buf[16];

    (void) sprintf(buf, "%d.%d.%d.%d",
		    (int)  in.s_net, (int)  in.s_host,
		    (int)  in.s_lh,  (int)  in.s_impno);

    return buf;
}
#endif /* NEED_INET_NTOA */

#if NEED_INET_ADDR
/*
**	inet_addr --	convert a character string
**			to the internet number
**		   	argv 11/90
**	$Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
**
*/

/* netinet/in.h and sys/types.h already included from common.h */

unsigned long inet_addr(host)
char *host;
{
    Reg1 char *tmp;
    Reg2 int i = 0;
    char hosttmp[16];
    struct in_addr addr;
    extern char *strtok();
    extern int atoi();

    if (host == NULL)
	return -1;

    bzero((char *)&addr, sizeof(addr));
    strncpy(hosttmp, host, sizeof(hosttmp));
    host = hosttmp;

    for (; tmp = strtok(host, "."); host = NULL) 
	switch(i++)
	    {
	    case 0:	addr.s_net   = (unsigned char) atoi(tmp); break;
	    case 1:	addr.s_host  = (unsigned char) atoi(tmp); break;
	    case 2:	addr.s_lh    = (unsigned char) atoi(tmp); break;
	    case 3:	addr.s_impno = (unsigned char) atoi(tmp); break;
	    }

    return(addr.s_addr ? addr.s_addr : -1);
}
#endif /* NEED_INET_ADDR */

#if NEED_INET_NETOF
/*
**	inet_netof --	return the net portion of an internet number
**			argv 11/90
**	$Id: support.c,v 6.1 1991/07/04 21:04:01 gruner stable gruner $
**
*/

int inet_netof(in)
struct in_addr in;
{
    int addr = in.s_net;

    if (addr & 0x80 == 0)
	return ((int) in.s_net);

    if (addr & 0x40 == 0)
	return ((int) in.s_net * 256 + in.s_host);

    return ((int) in.s_net * 256 + in.s_host * 256 + in.s_lh);
}
#endif /* NEED_INET_NETOF */
