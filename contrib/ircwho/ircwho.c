/*
 * This program has been written by Kannan Varadhan and Jeff Trim.
 * You are welcome to use, copy, modify, or circulate as you please,
 * provided you do not charge any fee for any of it, and you do not
 * remove these header comments from any of these files.
 *
 * Jeff Trim wrote the socket code
 *
 * Kannan Varadhan wrote the string manipulation code
 *
 *		KANNAN		kannan@cis.ohio-state.edu
 *		Jeff		jtrim@orion.cair.du.edu
 *		Wed Aug 23 22:44:39 EDT 1989
 */

/*
 * considering that there is no string manipulation code in this, and
 * considering that this is essentially a raw tool interface, Jeff
 * wrote this, and Kannan wrote the ircwho.sh awk routines.
 */
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>

#include "config.h"
#include "strings.h"

#define	COMMAND(s,string)	write (s, string, Strlen(string))

main(argc, argv)
int argc;
char **argv;
{
     struct sockaddr_in sin;
     struct hostent *hp;
     int s, port;
     char	error[100], host[HOSTLEN];

     port=IRCPORT;
     Strncpy(host, IRCSERVERHOST, HOSTLEN); /* .. kannan using 13 character */
     host[HOSTLEN -1] = '\0';               /* #define names ... ;-) - geez */

     bzero((char *)&sin, sizeof(sin));

     if ( isdigit(*host))
	sin.sin_addr.s_addr = inet_addr(host);
     else
     {
	hp = gethostbyname(host);
	if (hp == NULL) 
	{
	   Sprintf (error, "gethostbyname (%s) failed", host);
	   perror (error);
                exit(0);
	}
	bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
     }

     sin.sin_family = AF_INET;
     sin.sin_port = htons(port);

     if ( (s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror ("Error creating socket");
            exit(0);
     }

     if (connect (s, &sin, sizeof(sin) ) < 0) {
	  perror ("connect");
          exit(0);
     }


     if (argc <= 1)
       do_io (s, "who\n");
     else
       dowhois (s, argc, argv);
     COMMAND (s, "quit\n");
     close(s);
}


do_io (fd, string)
int	fd;
char	*string;

{
#ifdef BUFSIZE
#undef BUFSIZE
#endif
#define	BUFSIZE 1024
char	buffer[BUFSIZE];
int	cc;

	COMMAND (fd, string);
	while (isready(fd))
	  {
	  bzero (buffer, BUFSIZE);
	  cc = read (fd, buffer, BUFSIZE);
	  write (fileno(stdout), buffer, cc);
	  }
	return 0;
}


isready (fd)
int	fd;

{
struct	timeval	poll;
int	rmask[2];
	 
	poll.tv_sec = 1 /* just a sec, dude */;
	poll.tv_usec = 0;
	rmask[0] = 1<<fd;
	rmask[1] = 0;
	return select (32, rmask, 0, 0, &poll);
}


dowhois (s, argc, argv)
int	s, argc;
char	**argv;

{
int	i, cc;
char	command[100], buffer[512];


	for (i = 1; i < argc; i++)
	  {
	  Sprintf (command, "whois %s\n", argv[i]);
	  COMMAND (s, command);
	  while (isready(s))
	    {
	    bzero (buffer, 512);
	    cc = read (s, buffer, 512);
	    if (Strncmp (buffer, "ERROR", 5) == 0)
	      {
	      fprintf (stdout, "%s\t", argv[i]);
	      fflush (stdout);
	      }
	    write (fileno(stdout), buffer, cc);
	    }
	  }
	return 0;
}
