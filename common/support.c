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

#include "config.h"
#include "common.h"
#include "sys.h"
#include <sys/errno.h>

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
	*pos++ = '\0';		/* remove first sep after the token */
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

#ifdef USE_OUR_CTYPE

char	tolowertab[] =
		{ -1, 0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
		  0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
		  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
		  0x1e, 0x1f,
		  ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
		  '*', '+', ',', '-', '.', '/',
		  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		  ':', ';', '<', '=', '>', '?',
		  '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
		  'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
		  't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
		  '_',
		  '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
		  'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
		  't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
		  0x7f,
		  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		  0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
		  0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
		  0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
		  0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
		  0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
		  0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		  0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
		  0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

char	touppertab[] =
		{ -1, 0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
		  0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
		  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
		  0x1e, 0x1f,
		  ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
		  '*', '+', ',', '-', '.', '/',
		  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		  ':', ';', '<', '=', '>', '?',
		  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
		  'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
		  'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
		  0x5f,
		  '`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
		  'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
		  'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
		  0x7f,
		  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
		  0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
		  0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
		  0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
		  0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
		  0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
		  0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		  0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
		  0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

char char_atribs[] = {
/* EOF */       0,
/* 0-7 */	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 8-12 */	CNTRL, CNTRL|SPACE, CNTRL|SPACE, CNTRL|SPACE, CNTRL|SPACE,
/* 13-15 */	CNTRL|SPACE, CNTRL, CNTRL,
/* 16-23 */	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* 24-31 */	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
/* space */	PRINT|SPACE,
/* !"#$%&'( */	PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
/* )*+,-./ */	PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
/* 0123 */	PRINT|DIGIT, PRINT|DIGIT, PRINT|DIGIT, PRINT|DIGIT,
/* 4567 */	PRINT|DIGIT, PRINT|DIGIT, PRINT|DIGIT, PRINT|DIGIT,
/* 89:; */	PRINT|DIGIT, PRINT|DIGIT, PRINT, PRINT,
/* <=>? */	PRINT, PRINT, PRINT, PRINT,
/* @ */		PRINT,
/* ABC */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* DEF */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* GHI */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* JKL */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* MNO */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* PQR */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* STU */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* VWX */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* YZ[ */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* \]^ */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* _   */	PRINT,
/* abc */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* def */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* ghi */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* jkl */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* mno */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* pqr */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* stu */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* vwx */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* yz{ */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* \}~ */	PRINT|ALPHA, PRINT|ALPHA, PRINT|ALPHA,
/* del */	0,
/* 80-8f */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90-9f */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* a0-af */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* b0-bf */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* c0-cf */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* d0-df */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* e0-ef */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* f0-ff */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};

#endif
