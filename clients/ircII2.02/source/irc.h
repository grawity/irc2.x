/*
 * irc.h: header file for all of irc! 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#define BUFFER_SIZE 1024
#define DAEMON_UID 1

#define TRUE 1
#define FALSE 0

#define NICKNAME_LEN 9
#define NAME_LEN 80
#define REALNAME_LEN 30
#define PATH_LEN 1024

/* stuff that exists on 4.3 machines, so I stuck it in here */
#ifndef NBBY
#define	NBBY	8		/* number of bits in a byte */
#endif NBBY

#ifndef NFDBITS
#define NFDBITS	(sizeof(long) * NBBY)	/* bits per mask */
#endif NFDBITS

#ifndef FD_SET
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#endif FD_SET

#ifndef FD_CLR
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#endif FD_CLR

#ifndef FD_ISSET
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#endif FD_ISSET

#ifndef FD_ZERO
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif FD_ZERO

#define null(type) (type) 0L

#ifdef ISC22
extern char *strchr();
#define index strchr
#else
extern char *index();
#endif
extern int errno;
extern char *sys_errlist[];

/* flags used by who() and whoreply() for who_mask */
#define WHO_OPS 1
#define WHO_NAME 2
#define WHO_HOST 4
#define WHO_SERVER 8
#define WHO_ZERO 16
#define WHO_NICK 32
#define WHO_CHOPS 64
#define WHO_ALL (WHO_OPS | WHO_NAME | WHO_HOST | WHO_SERVER | WHO_NICK | WHO_ZERO | WHO_CHOPS)

/*
 * declared in irc.c 
 */
extern int irc2_6;
extern char operator;
extern char oper_command;
extern int irc_port;
extern int send_text_flag;
extern char irc_io_loop;
extern char use_flow_control;
extern int irc_io();
extern char *joined_nick;
extern char *public_nick;
extern char empty_string[];

extern char irc_version[];
extern char buffer[];
extern char nickname[],
     old_nickname[],
    *ircrc_file,
     hostname[],
     realname[],
     username[],
    *away_message;
extern char my_path[];
extern char *current_channel();
extern char *invite_channel;
extern void connect_to_server();
extern char who_mask;
extern char *who_nick;
extern char *who_host;
extern char *who_name;
extern char *who_server;

/* the "dumb" variable */
extern char dumb;

extern void help();
