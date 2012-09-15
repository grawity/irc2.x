/*
 * ircserv.c: A quaint little program to make irc life PING free 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

/* #define NON_BLOCKING */

#define null(foo) (foo) 0

extern char *index();
extern int errno;
extern int errno;
extern char *sys_errlist[];

char *new_malloc(size)
int size;
{
    char *ptr;

    if ((ptr = (char *) malloc(size)) == null(char *)) {
	printf("-1 0\n");
	exit(1);
    }
    return (ptr);
}

/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and
 * there 
 */
void new_free(ptr)
char **ptr;
{
    if (*ptr) {
	free(*ptr);
	*ptr = null(char *);
    }
}

/*
 * Connect_By_Number Performs a connecting to socket 'service' on host
 * 'host'.  Host can be a hostname or ip-address.  If 'host' is null, the
 * local host is assumed.   The parameter full_hostname will, on return,
 * contain the expanded hostname (if possible).  Note that full_hostname is a
 * pointer to a char *, and is allocated by connect_by_numbers() 
 *
 * Errors: 
 *
 * -1 get service failed 
 *
 * -2 get host failed 
 *
 * -3 socket call failed 
 *
 * -4 connect call failed 
 */
int connect_by_number(service, host)
int service;
char *host;
{
    int s;
    char buf[100];
    struct sockaddr_in server;
    struct hostent *hp;

    if (host == null(char *)) {
	gethostname(buf, sizeof(buf));
	host = buf;
    }
    if ((server.sin_addr.s_addr = inet_addr(host)) == -1) {
	if (hp = gethostbyname(host)) {
	    bzero((char *) &server, sizeof(server));
	    bcopy(hp->h_addr, (char *) &server.sin_addr, hp->h_length);
	    server.sin_family = hp->h_addrtype;
	} else
	    return (-2);
    } else
	server.sin_family = AF_INET;
    server.sin_port = (unsigned short) htons(service);
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return (-3);
    setsockopt(s, SOL_SOCKET, ~SO_LINGER, 0, 0);
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, 0, 0);
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, 0, 0);
    if (connect(s, &server, sizeof(server)) < 0) {
	close(s);
	return (-4);
    }
    return (s);
}

/*
 * ircserv: This little program connects to the server (given as arg 1) on
 * the given port (given as arg 2).  It then accepts input from stdin and
 * sends it to that sever. Likewise, it reads stuff sent from the server and
 * sends it to stdout.  Simple?  Yes, it is.  But wait!  There's more!  It
 * also intercepts server PINGs and automatically responds to them.   This
 * frees up the process that starts ircserv (such as IRCII) to pause without
 * fear of being pooted off the net. 
 *
 * Future enhancements: No-blocking io.  It will either discard or dynamically
 * buffer anything that would block. 
 */
main(argc, argv)
int argc;
char **argv;
{
    int des;
    fd_set rd;
    int done = 0,
        c;
    char *ptr,
         buffer[BUFSIZ + 1],
         pong[BUFSIZ + 1];
#ifdef NON_BLOCKING
    char block_buffer[BUFSIZ + 1];
    fd_set *wd_ptr = null(fd_set *),
           wd;
    int wrote;
#endif NON_BLOCKING

    if (argc < 3) {
	printf("-1 0\n");
	exit(1);
    }
    if ((des = connect_by_number(atoi(argv[2]), argv[1])) < 0) {
	printf("%d %d\n", des, errno);
	exit(des);
    }
    printf("0\n");
    fflush(stdout);

#ifdef NON_BLOCKING
    if (fcntl(1, F_SETFL, FNDELAY)) {
	printf("-1 %d\n", errno);
	exit(1);
    }
#endif NON_BLOCKING
    while (!done) {
	fflush(stderr);
	FD_ZERO(&rd);
	FD_SET(0, &rd);
	FD_SET(des, &rd);
#ifdef NON_BLOCKING
	if (wd_ptr) {
	    FD_ZERO(wd_ptr);
	    FD_SET(1, wd_ptr);
	}
	switch (new_select(&rd, wd_ptr, NULL)) {
#else NON_BLOCKING
	switch (new_select(&rd, null(fd_set *), NULL)) {
#endif NON_BLOCKING
	    case -1:
	    case 0:
		break;
	    default:
#ifdef NON_BLOCKING
		if (wd_ptr) {
		    if (FD_ISSET(1, wd_ptr)) {
			c = strlen(block_buffer);
			if ((wrote = write(1, block_buffer, c)) == -1) {
			    wd_ptr = &wd;
			} else if (wrote < c) {
			    strcpy(block_buffer, &(block_buffer[wrote]));
			    wd_ptr = &wd;
			} else
			    wd_ptr = null(fd_set *);
		    }
		}
#endif NON_BLOCKING
		if (FD_ISSET(0, &rd)) {
		    if (c = dgets(buffer, BUFSIZ, 0))
			write(des, buffer, c);
		    else
			done = 1;
		}
		if (FD_ISSET(des, &rd)) {
		    if (c = dgets(buffer, BUFSIZ, des)) {
			if (strncmp(buffer, "PING ", 5) == 0) {
			    if (ptr = index(buffer, ' ')) {
				sprintf(pong, "PONG user@host %s\n", ptr + 1);
				write(des, pong, strlen(pong));
			    }
			} else {
#ifdef NON_BLOCKING
			    if ((wrote = write(1, buffer, c)) == -1)
				wd_ptr = &wd;
			    else if (wrote < c) {
				strcpy(block_buffer, &(buffer[wrote]));
				wd_ptr = &wd;
			    }
#else
			    write(1, buffer, c);
#endif NON_BLOCKING
			}
		    } else
			done = 1;
		}
	}
    }
}
