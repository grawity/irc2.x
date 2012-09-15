/************************************************************************
 *   IRC - Internet Relay Chat, ircd/chkconf.c
 *   Copyright (C) 1993 Darren Reed
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

#ifndef lint
static  char sccsid[] = "@(#)chkconf.c	1.3 6/18/93 (C) 1993 Darren Reed";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/socket.h>
#include <fcntl.h>
#ifdef __hpux
#include "inet.h"
#endif
#ifdef PCS
#include <time.h>
#endif
#ifdef	R_LINES
#include <signal.h>
#endif

#ifdef DYNIXPTX
#include <sys/types.h>
#include <time.h>
#endif

#define	MyMalloc(x)	malloc(x)

static	void	new_class();
static	char	*getfield();
static	int	openconf(), initconf();
static	aClass	*get_class();

static	int	numclasses = 0, *classarr = (int *)NULL;
static	char	*configfile = CONFIGFILE;
static	char	nullfield[] = "";
static	char	maxsendq[12];

main(argc, argv)
int	argc;
char	*argv[];
{
	new_class(0);
	(void)sprintf(maxsendq, "%d", MAXSENDQLENGTH);

	if (chdir(DPATH))
	    {
		perror("chdir");
		exit(-1);
	    }
	if (argc > 1)
		configfile = argv[1];
	(void)initconf();
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
static	int	openconf()
{
#ifdef	M4_PREPROC
	int	pi[2];

	if (pipe(pi) == -1)
		return -1;
	switch(fork())
	{
	case -1 :
		return -1;
	case 0 :
		(void)close(pi[0]);
		if (pi[1] != 1)
		    {
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		(void)dup2(1,2);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp("m4", "m4", "ircd.m4", configfile, 0);
		perror("m4");
		exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	return open(configfile, O_RDONLY);
#endif
}

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

static	int 	initconf(opt)
int	opt;
{
	FILE	*fp;
	int	fd;
	char	line[512], *tmp, c[80];
	int	ccount = 0, ncount = 0;
	aConfItem conf, *aconf = &conf;

	(void)fprintf(stderr, "initconf(): ircd.conf = %s\n", configfile);
	if ((fd = openconf()) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return -1;
	    }
	fp = fdopen(fd, "r");

	aconf->host = (char *)NULL;
	aconf->passwd = (char *)NULL;
	aconf->name = (char *)NULL;

	while (fgets(line, sizeof(line) - 1, fp))
	    {
		if (aconf->host)
			(void)free(aconf->host);
		if (aconf->passwd)
			(void)free(aconf->passwd);
		if (aconf->name)
			(void)free(aconf->name);
		aconf->host = (char *)NULL;
		aconf->passwd = (char *)NULL;
		aconf->name = (char *)NULL;
		aconf->class = (aClass *)NULL;

		if (line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		if (tmp = (char *)index(line, '\n'))
			*tmp = 0;
		else while(fgets(c, sizeof(c) - 1, fp))
			if (tmp = (char *)index(c, '\n'))
			    {
				*tmp = 0;
				break;
			    }

		/* Could we test if it's conf line at all?	-Vesa */
		if (line[1] != ':')
		    {
                        (void)fprintf(stderr, "ERROR: Bad config line (%s)\n",
				line);
                        continue;
                    }

		(void)printf("\n%s\n",line);

		tmp = getfield(line);
		if (!tmp)
		    {
                        (void)fprintf(stderr, "\tERROR: no fields found\n");
			continue;
		    }

		aconf->status = CONF_ILLEGAL;

		switch (*tmp)
		{
			case 'A': /* Name, e-mail address of administrator */
			case 'a': /* of this server. */
				aconf->status = CONF_ADMIN;
				break;
			case 'C': /* Server where I should try to connect */
			case 'c': /* in case of lp failures             */
				ccount++;
				aconf->status = CONF_CONNECT_SERVER;
				break;
			case 'H': /* Hub server line */
			case 'h':
				aconf->status = CONF_HUB;
				break;
			case 'I': /* Just plain normal irc client trying  */
			case 'i': /* to connect me */
				aconf->status = CONF_CLIENT;
				break;
			case 'K': /* Kill user line on irc.conf           */
			case 'k':
				aconf->status = CONF_KILL;
				break;
			/* Operator. Line should contain at least */
			/* password and host where connection is  */
			case 'L': /* guaranteed leaf server */
			case 'l':
				aconf->status = CONF_LEAF;
				break;
			/* Me. Host field is name used for this host */
			/* and port number is the number of the port */
			case 'M':
			case 'm':
				aconf->status = CONF_ME;
				break;
			case 'N': /* Server where I should NOT try to     */
			case 'n': /* connect in case of lp failures     */
				  /* but which tries to connect ME        */
				++ncount;
				aconf->status = CONF_NOCONNECT_SERVER;
				break;
			case 'O':
				aconf->status = CONF_OPERATOR;
				break;
			/* Local Operator, (limited privs --SRB) */
			case 'o':
				aconf->status = CONF_LOCOP;
				break;
			case 'P': /* listen port line */
			case 'p':
				aconf->status = CONF_LISTEN_PORT;
				break;
			case 'Q': /* a server that you don't want in your */
			case 'q': /* network. USE WITH CAUTION! */
				aconf->status = CONF_QUARANTINED_SERVER;
				break;
#ifdef R_LINES
			case 'R': /* extended K line */
			case 'r': /* Offers more options of how to restrict */
				aconf->status = CONF_RESTRICT;
				break;
#endif
			case 'S': /* Service. Same semantics as   */
			case 's': /* CONF_OPERATOR                */
				aconf->status = CONF_SERVICE;
				break;
			case 'U': /* Uphost, ie. host where client reading */
			case 'u': /* this should connect.                  */
			/* This is for client only, I must ignore this */
			/* ...U-line should be removed... --msa */
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
		    default:
			(void)fprintf(stderr,
				"\tERROR: unknown conf line letter (%c)\n",
				*tmp);
			break;
		    }

		if (IsIllegal(aconf))
			continue;

		for (;;) /* Fake loop, that I can use break here --msa */
		    {
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (!(aconf->status & CONF_CLASS))
				aconf->class = get_class(atoi(tmp));
			break;
		    }
		if (!aconf->class && (aconf->status & (CONF_CONNECT_SERVER|
		     CONF_NOCONNECT_SERVER|CONF_OPS|CONF_CLIENT)))
		    {
			(void)fprintf(stderr,
				"\tWARNING: No class.  Default 0\n");
			aconf->class = get_class(0);
		    }
		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS)
		    {
			if (!aconf->host)
			    {
				(void)fprintf(stderr,"\tERROR: no class #\n");
				continue;
			    }
			if (!aconf->port)
			    {
				(void)fprintf(stderr,
					"\tWARNING: missing sendq field\n");
				(void)fprintf(stderr, "\t\t default: %d\n",
					MAXSENDQLENGTH);
				aconf->port = MAXSENDQLENGTH;
			    }
			new_class(atoi(aconf->host));
			aconf->class = get_class(atoi(aconf->host));
			goto print_confline;
		    }

		if (aconf->status & CONF_LISTEN_PORT)
		    {
			aconf->class = get_class(0);
			goto print_confline;
		    }

		if (aconf->status & CONF_SERVER_MASK &&
		    (!aconf->host || index(aconf->host, '*') ||
		     index(aconf->host, '?')))
		    {
			(void)fprintf(stderr, "\tERROR: bad host field\n");
			continue;
		    }

		if (aconf->status & CONF_SERVER_MASK && BadPtr(aconf->passwd))
		    {
			(void)fprintf(stderr,
					"\tERROR: empty/no password field\n");
			continue;
		    }

		if (aconf->status & CONF_SERVER_MASK && !aconf->name)
		    {
			(void)fprintf(stderr, "\tERROR: bad name field\n");
			continue;
		    }

		if (aconf->status & (CONF_SERVER_MASK|CONF_OPS))
			if (!index(aconf->host, '@'))
			    {
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)sprintf(newhost, "*@%s", aconf->host);
				(void)free(aconf->host);
				aconf->host = newhost;
			    }

		if (!aconf->class)
			aconf->class = get_class(0);

		if (!aconf->name)
			aconf->name = nullfield;
		if (!aconf->passwd)
			aconf->passwd = nullfield;
		if (!aconf->host)
			aconf->host = nullfield;
print_confline:
		(void)printf("(%d) (%s) (%s) (%s) (%d) (%d)\n",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port, aconf->class->class);
	    }
	(void)fclose(fp);
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	return 0;
}

static	aClass	*get_class(cn)
int	cn;
{
	static	aClass	cls;
	int	i = numclasses - 1;

	cls.class = -1;
	for (; i >= 0; i--)
		if (classarr[i] == cn)
		    {
			cls.class = cn;
			break;
		    }
	if (i == -1)
		(void)fprintf(stderr,"\tWARNING: class %d not found\n", cn);
	return &cls;
}

static	void	new_class(cn)
int	cn;
{
	numclasses++;
	if (classarr)
		classarr = (int *)realloc(classarr, sizeof(int) * numclasses);
	else
		classarr = (int *)malloc(sizeof(int));
	classarr[numclasses-1] = cn;
}

/*
 * field breakup for ircd.conf file.
 */
static	char	*getfield(newline)
char	*newline;
{
	static	char *line = NULL;
	char	*end, *field;
	
	if (newline)
		line = newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if ((end = (char *)index(line,':')) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}
