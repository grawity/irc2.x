/************************************************************************
 *   IRC - Internet Relay Chat, irc/termcap.h
 *   Copyright (C) 1990
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

/*	termcap.c	880204	NHA	*/
/* My implementation of the termcap(3X) library routines.
 * All specs lifted straight from 4.3bsd Programmers Reference Manual.
 * These functions extract and use capabilities from the terminal
 * capability database termcap(5).  These are low level routines;
 * see curses(3X) for a higher level package.
 ** You'll find it looks a lot nicer if you use a tab interval of 4.
 */

#define	DEBUG	0

#if DEBUG
#define	MAJOR	'L'							/* major module identifier */
#define	MINOR	'T'							/* minor module identifier */
#include	<gen.h>							/* my all-purpose include file */
#else DEBUG
#include	<stdio.h>
#include	<file.h>
#include	"common.h"
#include	"struct.h"
#define		export
#define		import		extern
#define		local		static
#define		bool		int
#define		abs(x)		( (x < 0) ? (-(x)) : (x) )
#define		YES			1
#define		NO			0
#define		error(s)	{perror(s);  exit(99);}
#define		initdebug(pac, pav, confile, listfile, initstring)
static		printd(lvl, fmt) {}
#endif DEBUG

#define	BUFSIZE	1024

/* external variables (supplied by user) required by this package */
import char		PC;	/* pad char, default ^@ */
import char		BC;	/* backspace char if not ^H */
import char		UP;	/* char for Upline (cursor up) */
import char		ospeed;	/* output speed, see stty(3) */

#ifdef __STDC__
import char		*getenv(char *id);
import int		open(char *name, unsigned mode);
import unsigned	strlen(char *str);
import unsigned	strspn(char *str1, char *str2);
import int		strcmp(char *str1, char *str2);
import int		strncmp(char *str1, char *str2, unsigned n);
import char		*strncpy(char *buf, char *str, unsigned n);
import char		*strchr(char *string, char ch);
import char		*strpbrk(char *string, char *delimiters);
#else __STDC__
import char		*getenv();
import int		open();
import unsigned	strlen();
import unsigned	strspn();
import int		strcmp();
import int		strncmp();
import char		*strncpy();
import char		*strchr();
import char		*strpbrk();
#endif __STDC__

/* milliseconds per character, for each of the possible baud rates of ospeed */
/* here multiplied by 10 for computational convenience */
local unsigned	delayFactor[] = {
   0,		/* B0 */	/* hang up dataphone */
1920,		/* B50 */
1280,		/* B75 */
 872,		/* B110 */
 730,		/* B134 */
 640,		/* B150 */
 480,		/* B200 */
 320,		/* B300 */
 160,		/* B600 */
  80,		/* B1200 */
  50,		/* B1800 */
  40,		/* B2400 */
  20,		/* B4800 */
  10,		/* B9600 */
   5,		/* EXTA (19200 here) */
   2,		/* EXTB (34800 here) */
};
/* remember where user's terminal entry buffer is */
local char		*ebuf = NULL;/* pointer to entry buffer */



/*+		f i n d C a p
 * Returns pointing to the character immediately following the capability id.
 * Returns NULL if tgetent() has not yet been called successfully.
 * Returns NULL if capability not found.
 */
local char *findCap(id)
char	*id;/* name of the capability to find */
	{
	char	*p;/* pointer into the entry buffer */

	printd(5, "findCap(\"%s\"), ebuf=%p\n", id, ebuf);
	if (ebuf == NULL)
		return NULL;
	for (p = ebuf   ;   *p   ;   ++p)
		{
		printd(9, " %02x", *p);
		if ( (p[0] == ':')  &&  (p[1] == id[0])  &&  (p[2] == id[1]) )
			{
			printd(7, "findCap(): SUCCESS, p=%.15s...\n", p);
			p = &p[3];
			break;
			}
		}
	if ( ! *p)
		p = NULL;
	printd(5, "findCap(): returning %p (%.11s...)\n", p, p);
	return p;
	}



/*+		g e t E n t r y
 * Gets the named entry from the already-opened termcap file into the buffer.
 * The size of the buffer is BUFSIZE, and it is considered to be an
 * error if the size of the entry equals or exceeds this.
 * We place a terminating NULL character at the end of the entry.
 * Call error() on any irregularities.
 * Return 0 if the named entry not found, else 1.
 * Removes terminal names and all newlines from the entry.
 **If this is called for a 2nd time from tgetent(), then the length checking
 **is useless.
 */
local int getEntry(fd, outbuf, name)
int		fd;	/* FileDescriptor for termcap file*/
char	*outbuf;			/* where we put the entry */
char	*name;/* terminal type name we seek */
	{
	unsigned	namlen;		/* # chars in name */
	int			cnt;		/* # unprocessed chars in inbuf[] */
	char		*ip;		/* pointer into input buffer */
	char		*op;		/* pointer into output buffer */
	char		*ptmp;		/* temporary pointer */
	int			stat;		/* status of read(), etc */
	char		inbuf[BUFSIZE];/* termcap file is read into here */

	printd(5, "getEntry(%d, %p, \"%s\")\n", fd, inbuf, name);
	op = outbuf;
	namlen = strlen(name);
	cnt = read(fd, inbuf, BUFSIZE-1);
	if (cnt == -1)
		error("getEntry(): file is empty\n");
	inbuf[cnt] = '\0';		/* maintain inbuf[] as a string */
	for (ip = inbuf   ;   0 < cnt   ;   ++ip, --cnt)
		{
		printd(7, "cnt=%d, ip='%.55s...'\n", cnt, ip);
		stat = strspn(ip, "\r\n \t\b\f");
		if (0 < stat)
			{
			printd(8, "skipping %d whitespace characters\n", stat);
			ip = &ip[--stat];
			cnt -= stat;
			}
		else if (*ip == '#')
			{
			printd(6, "comment line '%.11s...'\n", ip);
			ptmp = ip;
			ip = strchr(ip, (char)'\n');
			cnt  -=  (ip == NULL) ? cnt : (int)(ip - ptmp);
			}
		else if (strncmp(name, ip, namlen) == 0)
			{
			printd(6, "getEntry(): SUCCESS, ip = '%.22s...', cnt=%u\n", ip,cnt);
			ip = strchr(ip, (char)':');		/* skip over namelist */
			printd(7, "getEntry(): raw entry is: '%s'\n", ip);
			/* copy entry into output buffer */
			/* eliminate non-space whitespace and continuation \ */
			for (op = outbuf   ;   ip != NULL  &&  *ip != '\0'   ;   ++ip)
{
printd(9, " %02x", *ip);
if (ip[0] == '\\'   &&   ip[1] == '\r'   &&   ip[2] == '\n')
	ip = &ip[2];
else if (ip[0] == '\\'   &&   ip[1] == '\n')
	++ip;
else if (strchr("\t\r\b\f", *ip) != NULL)
	continue;
else if (*ip == '\n')
	break;
else
	*op++  =  *ip;
}
			if (*ip != '\n')
error("getEntry(): entry too long\n");
			*op = '\0';
			printd(6, "getEntry(): outbuf='%s'\n", outbuf);
			printd(5, "getEntry(): returning 1  [SUCCESS]\n");
			return 1;
			}
		else
			{/* advance to next name in list */
			ptmp = ip;
			ip = strpbrk(ip, "|:");			/* find name delimiter */
			if (ip == NULL)
error("getEntry(): bad format\n");
			cnt -= ip - ptmp;
			if (*ip != '|')
{			/* at end of namelist for entry */
/* dispose of entire entry */
printd(8, "end of namelist, cnt=%d\n", cnt);
for (++ip, --cnt   ;   0 < cnt   ;   ++ip, --cnt)
	if ( ip[0] == '\n'   && 
		  ( (ip[-1] == '\r'  &&   ip[-2] != '\\')
		  	||
		    (ip[-1] != '\r'  &&   ip[-1] != '\\') )
	   )
		{/* skip to next entry in file */
		/* delete this entry from inbuf */
		for (ptmp = inbuf   ;   *ip != '\0'   ;   ++ptmp, ++ip)
			*ptmp = *ip;
		*ptmp = *ip;		/* string stopper character */
		ip = inbuf;
		if (strlen(ip) != cnt)
			error("getEntry(): bad strlen(ip)\n");
		/* fill inbuf with more characters */
		stat = read(fd, ptmp, BUFSIZE - cnt - 1);
		if (0 < stat)
			{
			cnt += stat;
			inbuf[cnt] = '\0';
			}
		break;
		}
if (cnt <= 0)
	error("getEntry(): entry too long!\n");
}
			}
		}
	outbuf[0] = '\0';		/* not found */
	printd(6, "getEntry(): outbuf='%s'\n", outbuf);
	printd(5, "getEntry(): returning 0  [FAILURE]\n");
	return 0;
	}


	
/*+		t g e t e n t
 * Extracts the entry for terminal name into the buffer at bp.
 * Bp should be a character array of size 1024 and must be retained through
 * all subsequent calls to tgetnum(), tgetflag(), and tgetstr().
 * Returns -1 if it cannot open the termcap file, 0 if the terminal name
 * given does not have an entry, and 1 if all goes well.
 * Looks in the environment for a TERMCAP variable.  If found, and the value
 * does not begin with a slash, and the terminal type name is the same as
 * the environment string TERM, the TERMCAP string is used instead of reading
 * the termcap file.  If it does begin with a slash, the string is used
 * as a pathname rather than /etc/termcap.  This can speed up entry into
 * programs that call tgetent(), as well as to help debug new terminal
 * descriptions  or to make one for your terminal if you can't write the
 * file /etc/termcap.
 */
export int tgetent(bp, name)
char	*bp;/* pointer to user's buffer */
char	*name;/* terminal type name */
	{
	char	*termcap;		/* pointer to $TERMCAP string */
	int		fd;/* File Descriptor, termcap file */
	int		retval;			/* return value */
	
	printd(3, "tgetent(%p, \"%s\")\n", bp, name);
	termcap = getenv("TERMCAP");
	if (termcap != NULL   &&   termcap[0] != '/'   &&
	    strcmp(name, getenv("TERM")) == 0)
		{	/* use $TERMCAP as the entry */
		printd(6, "tgetent(): using contents of $EXINIT\n");
		strncpynt(bp, termcap, (BUFSIZE-1));
		termcap = "/etc/termcap";			/* in case :tc capability found */
		retval = 1;
		}
	else
		{	/* look for entry in termcap file */
		if (termcap[0] != '/')
			termcap = "sys$login:termcap";		/* use default termcap file */
		printd(6, "tgetent(): opening file %s\n", termcap);
		fd = open(termcap, O_RDONLY);
		if (fd == -1)
			retval = -1;
		else
			{
			retval = getEntry(fd, bp, name);
			close(fd);
			}
		}
	if (retval == 1)
		ebuf = bp;			/* for our use in future pkg calls*/
	
	/* deal with the :tc= capability */
	bp = findCap("tc");
	if (bp != NULL)
		{
		char	newname[88];
		
		printd(6, "tgetent(): :tc found at %p, is '%s'\n", &bp[-3], &bp[-3]);
		strncpy(newname, &bp[1], sizeof newname);
		if (strchr(newname, (char)':') != NULL)
			*(strchr(newname, (char)':'))  =  '\0';
		fd = open(termcap, O_RDONLY);
		if (fd == -1)
			{
			printd(2, "tgetent(%s): can't open :tc file '%s'\n", name, newname);
			retval = -1;
			}
		else
			{
			retval = getEntry(fd, &bp[-2], newname);
			close(fd);
			}
		}
		
	printd(3, "tgetent(): returning %d\n", retval);
	return retval;
	}


	
/*+		t g e t n u m
 * Gets the numeric value of capability id, returning -1 if it is not given
 * for the terminal.
 */
export int tgetnum(id)
char	*id;/* capability name */
	{
	int		retval;
	char	*p;

	printd(3, "tgetnum(\"%s\")\n", id);
	p = findCap(id);
	if (p == NULL   ||   *p != '#')
		retval = -1;		/* not found, or not numeric */
	else
		{
		retval = 0;
		for (++p   ;   *p != ':'   ;   ++p)
			retval  =  (retval * 10) + (*p - '0');
		}
	printd(3, "tgetnum(): returning %d\n", retval);
	return retval;
	}



/*+		t g e t f l a g
 * Returns 1 if the specified capability is present in the terminal's entry,
 * 0 if it is not.
 **This implementation requires that the capability be a boolean one.
 */
export int tgetflag(id)
char	*id;/* capability name */
	{
	int		retval;
	char	*p;

	printd(3, "tgetflag(\"%s\")\n", id);
	p = findCap(id);
	retval  =  (p != NULL  &&  *p == ':');
	printd(3, "tgetflag(): returning %d\n", retval);
	return retval;
	}



/*+		t g e t s t r
 * Returns the string value of the capability id, places it in the buffer
 * at area, and advances the area pointer [past the terminating '\0' char].
 * It decodes the abbreviations for this field described in termcap(5),
 * except for cursor addressing and padding information.
 * Returns NULL if the capability was not found.
 */
export char *tgetstr(id, area)
char	*id;/* capability name */
char	**area;/* pointer to output pointer */
	{
	char		*retval;	/* return value */
	char		*p;			/* pointer into capability string */
	unsigned	sum;		/* for chars given in octal */

	printd(3, "tgetstr(\"%s\", %p): *area=%p\n", id, area, *area);
	p = findCap(id);
	if (p == NULL   ||   *p != '=')
		retval = NULL;
	else
		{
		retval = *area;
		for (++p   ;   *p != ':'   ;   ++p)
			{
			printd(9, "p=%p,  *p=%02x\n", p, *p);
			if (*p == '\\')
switch (*++p)
	{		/* special */
case '0': case '1': case '2': case '3':
case '4': case '5': case '6': case '7':
	sum = (p[0] - '0') << 6  +
		  (p[1] - '0') << 3  +
		  (p[2] - '0');
	++p;
	++p;
	*(*area)++  =  (char)(sum & 0377);
	/** will \200 really end up as \000 like it should ? **/
	/** see termcap(5), top of page 6 **/
	break;
case '^':	*(*area)++  =  '^';		break;
case '\\':	*(*area)++  =  '\\';	break;
case 'E':	*(*area)++  =  '\033';	break;		/* escape */
case 'b':	*(*area)++  =  '\b';	break;
case 'f':	*(*area)++  =  '\f';	break;
case 'n':	*(*area)++  =  '\n';	break;
case 'r':	*(*area)++  =  '\r';	break;
case 't':	*(*area)++  =  '\t';	break;
default:	*(*area)++  =  *p;		break;
	}
			else if (*p == '^')
*(*area)++  =  *++p - '@';	/* control */
			else
*(*area)++  =  *p;			/* normal */
			}
		*(*area)++ = '\0';	/* NULL-terminate the string */
		}
	printd(3, "tgetstr(): returning ");
	if (retval == NULL)
		{	/* these must be here for print() */
		printd(3, "NULL");
		}	/* these must be here for print() */
	else
		{
		printd(3, "%p  [", retval);
		for (p = retval   ;   p != *area   ;   ++p)
			printd(3, " %02x", (unsigned)*p);
		printd(3, "]");
		}
	printd(3, ",  *area=%p\n",  *area);
	return retval;
	}


	
/*+		t g o t o
 * Returns a cursor addressing string decoded from cm to go to column destcol
 * in line destline. It uses the external variables UP (from the up capability)
 * and BC (if bc is given rather than bs) if necessary to avoid placing
 * \n, ^D, or ^@ in the returned string.  (Programs which call tgoto() should
 * be sure to turn off the XTABS bit(s), since tgoto() may not output a tab.
 * Note that programs using termcap should in general turn off XTABS anyway
 * since some terminals use control I for other functions, such as non-
 * destructive space.)  If a % sequence is given which is not understood,
 * then tgoto() returns "OOPS".
 **Output buffer is local, so don't try any recursion.
 **No error checking here.
 */
export char *tgoto(cm, destcol, destline)
char	*cm;/* cm capability string */
int		destcol;			/* destination column (left is 0) */
int		destline;			/* destination line (top is 0) */
	{
	char		*outp;		/* pointer into answer[] */
	local char	answer[88];	/* result stashed here */
	bool		reversed;	/* YES when should send col 1st */
	int			value;		/* next value to output */

	printd(3, "tgoto(\"%s\", %d, %d)\n", cm, destcol, destline);
	reversed = NO;
	value = destline;
	outp = answer;
	for (   ;   *cm   ;   ++cm)
		{
		printd(9, " %02x", *cm);
		if (*cm == '%')
			{
			switch (*++cm)
{
			case '%':	*outp++ = '%';
		break;
			case 'd':	sprintf(outp, "%d", value);
		if (value < 0)
			++outp;
		++outp;
		if (9 < abs(value))
			++outp;
		if (99 < abs(value))
			++outp;
		value = (reversed) ? destline : destcol;
		break;
			case '2':	sprintf(outp, "%02d", value);
		outp += 2;
		value = (reversed) ? destline : destcol;
		break;
			case '3':	sprintf(outp, "%03d", value);
		outp += 3;
		value = (reversed) ? destline : destcol;
		break;
			case '.':	*outp++ = value;
		value = (reversed) ? destline : destcol;
		break;
			case '+':	*outp++ = value + *++cm;
		value = (reversed) ? destline : destcol;
		break;
			case '>':	if (value > *++cm)
			value += *++cm;
		else
			++cm;
		break;
			case 'r':	value = (reversed) ? destline : destcol;
		reversed ^= YES;
		break;
			case 'i':	++value;
		break;
			case 'n':	destcol  ^= 0140;
		destline ^= 0140;
		break;
			case 'B':	value = (16 * (value / 10))  +  (value % 10);
		break;
			case 'D':	value = (value - (2 * (value % 16)));
		break;
			default:	sprintf(outp, "OOPS");
		outp += 4;
		break;
}
			printd(8, "reversed=%b, value=%d\n", reversed, value);
			}
		else
			*outp++ = *cm;
		}
	*outp = '\0';
	printd(3, "tgoto(): returning '%s'\n", answer);
	return answer;
	}



/*+		t p u t s
 * Decodes the leading pad information of the string cp; affcnt gives the
 * number of lines affected by the operation, or 1 if this is not applicable.
 * Outc is a routine which is called with each character in turn.
 * The external variable ospeed should contain the output speed of
 * the terminal as encoded by stty(3).
 * The external variable PC should contain a pad character to be used
 * (from the pc capability) if a null (^@) is inappropriate.
 */
export void tputs(cp, affcnt, outc)
char	*cp;/* leading pad information string */
int		affcnt;/* number lines affected, or 1 */
int		(*outc)();			/*output function to call per char*/
	{
	char		*p;
	bool		decimalFlag;/* delay had a decimal point */
	unsigned	delay;
	unsigned	cnt;
	
	printd(3, "tputs(\"%s\", %d, %p):  ospeed=%u\n", cp, affcnt, outc, ospeed);

	/* calculate delay, if any */
	/* currently no guard against having more than 1 digit after decimal point*/
	decimalFlag = NO;
	delay = 0;
	for (p = cp   ;   strchr("0123456789.", *p)   ;   ++p)
		if (*p == '.')
			decimalFlag = YES;
		else
			delay  =  (delay * 10) + (*p - '0');
	if ( ! decimalFlag)
		delay *= 10;		/* units are really 10ms */
	if (*p == '*')
		delay *= affcnt;
	printd(6, "tputs(): delay = %u.%u milliseconds\n", delay/10, delay%10);
	delay += (delayFactor[ospeed] + 1) / 2;	/* round up */
	delay /= delayFactor[ospeed];
	printd(5, "tputs(): delay = %u characters,  [delayFactor is %u]\n",
	  delay, delayFactor[ospeed]);

	for (   ;   *cp != '\0'   ;   ++cp)
		(*outc)(*cp);			/* output string */
	for (cnt = delay   ;   cnt   ;   --cnt)
		(*outc)(PC);			/* output delay characters */
	printd(3, "tputs(): returning\n");
	}



#if		TEST

export char		PC;			/* pad char, default ^@ */
export char		BC;			/* backspace char if not ^H */
export char		UP;			/* char for Upline (cursor up) */
export char		ospeed = 13;/* output speed, see stty(3) */

local char	buf[1024];		/* holds termcap entry */
local char	strbuf[512];	/* for output of tgetstr() */
local char	*strptr;		/* ptr to strbuf[] */


/*+		o c
 * Tiny test routine to simulate putting out a character.
 */
local void oc(c)
char	c;
	{
	putc(c, stdout);
	}


/*+		m a i n
 * Test program for the termcap routines.
 * Command line parameters:
 *	1st is terminal name, defaulted to "mono".
 *	2nd is name of numeric capability, default "co".
 *	3rd is name of boolean capability, default "bs".
 *	4th is name of string capability, default "so".
 *	5th is test string for tgoto(), default is "6\\E&%r%2c%2Y".
 *	6th is test string for tputs(), default is "3.5*123".
 */
export int main(ac, av)
int		ac;
char	**av;
	{
	int		stat;			/* integer return value */
	char	*instr;			/* input string value */
	char	*outstr;		/* string return value */
	char	*ttype;			/* terminal type string */
	char	*capability;	/* capability name string */
	
	/* setup */
	initdebug(&ac, &av, "/dev/con", "debug.out", "LB3LT8");
	PC = '@';
	BC = 'H';
	UP = 'B';
	
	/* test tgetent() */
	ttype = (ac < 2) ? "mono" : av[1];
	stat = tgetent(buf, ttype);
	printf("main: tgetent(buf, \"%s\") returned %d\n", ttype, stat);
	if (stat != 1)
		exit (99);
		
	/* test tgetnum() */
	capability = (ac < 3) ? "co" : av[2];
	stat = tgetnum(capability);
	printf("main: tgetnum(%s) returned %d\n", capability, stat);

	/* test tgetflag() */
	capability = (ac < 4) ? "bs" : av[3];
	stat = tgetflag(capability);
	printf("main: tgetflag(%s) returned %d\n", capability, stat);

	/* test tgetstr() */
	capability = (ac < 5) ? "so" : av[4];
	strptr = strbuf;
	outstr = tgetstr(capability, &strptr);
	printf("main: tgetstr(%s, 0x%lx) returned '%s'  [strbuf=0x%lx, strptr=0x%lx]\n",
	  capability, &strptr, outstr, strbuf, strptr);
	if (strcmp(capability, "so") == 0)
		{
		strptr = strbuf;
		tgetstr("se", &strptr);
		printf(strbuf);
		}

	/* test tgoto() */
	instr = (ac < 6) ? "6\\E&%r%2c%2Y" : av[5];
	outstr = tgoto(instr, 12, 3);
	printf("main: tgoto(\"%s\", 12, 3) returned '%s'\n", instr, outstr);
	
	/* test tputs() */
	instr = (ac < 7) ? "3.5*123" : av[6];
	printf("main: tputs(\"%s\", 3, oc) returned '", instr);
	tputs(instr, 3, oc);
	printf("'\n");
	
	return 0;
	}

#endif	TEST
