/*
 * irc.c: a new irc client.  I like it.  I hope you will too 
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#ifdef HPUX
#include <curses.h>
#endif
#include <stdio.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>
#include "irc.h"
#include "term.h"

#define IRCRC_NAME "/.ircrc"

extern char *getenv();

int server_des = -1,		/* the descriptor of the server */
    current_channel = 0;	/* current channel user is on */
int irc_port = IRC_PORT;	/* port of ircd */
char operator = 0;		/* true is user is oper */
char server_paused = 0;		/* true is server output is stopped */
char buffer[BUFFER_SIZE + 1];	/* generic, multipurpose buffer */
char my_path[PATH_LEN + 1];	/* path to users home dir */
char nickname[NICKNAME_LEN + 1],/* users nickname */
     current_server[NAME_LEN + 1],	/* name of current server */
     hostname[NAME_LEN + 1],	/* name of current host */
     realname[REALNAME_LEN + 1],/* real name of user */
     username[NAME_LEN + 1],	/* usernameof user */
    *away_message = null(char *);

char dumb = 0;

/* Flags, counters, descriptors, for use with /EXEC'd processes */
int process_stdout = -1,	/* stdout descriptor for process */
    process_stderr = -1,	/* stderr descriptor for process */
    process_redirect = 0,	/* true, process output goes to channel or
				 * queried user */
    process_counter = 0;	/* count of number of lines of process output */
char *process_who = null(char *);	/* If non-null, user to whom process
					 * output will go */


char who_mask = 0;		/* keeps track of which /who switchs are set */
char *who_info = null(char *);	/* extra /who switch info */
char *who_nick = null(char *);	/* extra /who switch info */

char irc_version[] = "v1.75";

char cntl_c_hit = 0;

/* put_file: uses put_it() to display the contents of a file to the display */
void put_file(filename)
char *filename;
{
    FILE *fp;
    char line[256];		/* too big?  too small?  who cares? */
    int len;

    if ((fp = fopen(filename, "r")) != null(FILE *)) {
	while (fgets(line, 256, fp)) {
	    len = strlen(line);
	    if (*(line + len - 1) == '\n')
		*(line + len - 1) = null(char);
	    put_it(line);
	}
	fclose(fp);
    }
}

/*
 * flush_server: eats all output from server, until there is at least a
 * second delay between bits of servers crap... useful to abort a /links. 
 */
void flush_server()
{
    fd_set rd;
    struct timeval timeout;
    char flushing = 1;

    if (server_des == -1)
	return;
    timeout.tv_usec = 0;
    timeout.tv_sec = 1;
    while (flushing) {
	FD_ZERO(&rd);
	FD_SET(server_des, &rd);
	switch (select(32, &rd, 0, 0, &timeout)) {
	    case -1:
	    case 0:
		flushing = 0;
		break;
	    default:
		if (FD_ISSET(server_des, &rd)) {
		    if (read(server_des, buffer, BUFFER_SIZE) == 0)
			flushing = 0;
		}
		break;
	}
    }
    /* make sure we've read a full line from server */
    FD_ZERO(&rd);
    FD_SET(server_des, &rd);
    if (select(32, &rd, 0, 0, &timeout) > 0)
	dgets(buffer, BUFFER_SIZE, server_des);
}

/* irc_exit: cleans up and leaves */
void irc_exit()
{
         close(server_des);
    logger(0);
    if (!dumb)
	term_reset();
#ifdef HPUX
    endwin();			/* Added for curses */
    system("tset -Q");
    system("reset");
#endif
    exit(0);
}

/*
 * cntl_c: emergency exit.... if somehow everything else freezes up, hitting
 * ^C five times should kill the program. 
 */
void cntl_c()
{
    if   (cntl_c_hit++ >= 4)
	     irc_exit();
}

/*
 * connect_to_server: handles the tcp connection to the server and the
 * initial set user setup, setting nickname, user info, and channel. Returns
 * 0 if no error occurred (setting server_des to the descriptor for the
 * server connection), or returns the error returned by connect_by_number,
 * setting server_des to -1 
 */
int connect_to_server(server_name, port)
char *server_name;
int port;
{
    int new_des;

    put_it("*** Connecting to port %d of server %s", port, server_name);
    if ((new_des = connect_by_number(port, server_name)) < 0) {
	put_it("*** Unable to connect to port %d of server %s.", port, server_name);
	if (server_des != -1)
	    put_it("*** Connection to server %s resumed...", current_server);
	return (new_des);
    }
    operator = 0;
    make_status();
    if (server_des != -1) {
	send_to_server("QUIT");
	put_it("*** Closing connection to %s", current_server);
	sleep(2);		/* a little time for the quit to get around */
	close(server_des);
    }
    strncpy(current_server, server_name, NAME_LEN);
    server_des = new_des;

    send_to_server("NICK %s", nickname);
    send_to_server("USER %s %s %s %s", username, hostname, current_server, realname);
    send_to_server("CHANNEL %d", current_channel);
    if (away_message)
	send_to_server("AWAY %s", away_message);
    return (0);
}

/*
 * start_process: starts the named process with the given args, sets stdin to
 * the processes stdout, and stdout to the processes stdin. 
 */
void start_process(name, args)
char *name,
   **args;
{
    int p1[2],
        p2[2];

    pipe(p1);
    pipe(p2);
    switch (fork()) {
	case -1:
	    put_it("*** Couldn't start new process!");
	    break;
	case 0:
	    dup2(p1[1], 1);
	    dup2(p2[0], 0);
	    close(p1[0]);
	    close(p2[1]);
	    execv(name, args);
	    put_it("*** Error execing process!");
	    _exit(0);
	    break;
	default:
	    dup2(p1[0], 0);
	    dup2(p2[1], 1);
	    close(p1[1]);
	    close(p2[0]);
	    if (fork())
		exit(0);
	    break;
    }
}

/* get_arg: used by parse args to get an argument after a switch */
static char *get_arg(arg, next_arg, ac)
char *arg;
char *next_arg;
int *ac;
{
    (*ac)++;
    if (*arg)
	return (arg);
    else {
	if (next_arg)
	    return (next_arg);
	else {
	    fprintf(stderr, "Missing parameter\n");
	    exit(1);
	}
    }
}

/*
 * parse_args: parse command line arguments for irc, and sets all initial
 * flags, etc. 
 */
static void parse_args(argv, argc)
char *argv[];
int argc;
{
    char *arg,
        *ptr;
    int ac;
    struct passwd *entry;
    char *prog_name = null(char *);

    *nickname = null(char);
    *current_server = null(char);
    *realname = null(char);
    ac = 1;
    while (arg = argv[ac++]) {
	if (*(arg++) == '-') {
	    while (*arg) {
		switch (*(arg++)) {
		    case 'c':
			current_channel = atoi(get_arg(arg, argv[ac], &ac));
			break;
		    case 'p':
			irc_port = atoi(get_arg(arg, argv[ac], &ac));
			break;
		    case 'd':
			dumb = 1;
			break;
		    case 'e':
			dumb = 1;
			prog_name = get_arg(arg, argv[ac], &ac);
			start_process(prog_name, &(argv[ac - 1]));
			ac = argc;
			break;
		    default:
			fprintf(stderr, "Unknown flag: -%c\nUsage: irc [-c channel] [-p port] [nickname [server]] [-e program args]\n", *(arg - 1));
			exit(1);
		}
	    }
	} else {
	    if (*nickname) {
		if (*current_server)
		    fprintf(stderr, "Extra stuff on command line ignored\n");
		else
		    strncpy(current_server, arg - 1, NAME_LEN);
	    } else
		strncpy(nickname, arg - 1, NAME_LEN);

	}
    }
    if ((*nickname == null(char)) && (ptr = getenv("IRCNICK")))
	strncpy(nickname, ptr, NICKNAME_LEN);
    if ((*current_server == null(char)) && (ptr = getenv("IRCSERVER")))
	strncpy(current_server, ptr, NAME_LEN);
    if (ptr = getenv("IRCNAME"))
	strncpy(realname, ptr, REALNAME_LEN);
    if (*current_server == null(char))
	strncpy(current_server, DEFAULT_SERVER, NAME_LEN);
    if (entry = getpwuid(getuid())) {
	if (*realname == null(char)) {
#ifdef GECOS_DELIMITER
	    char *ptr;

	    if (ptr = index(entry->pw_gecos, GECOS_DELIMITER))
		*ptr = null(char);
#endif GECOS_DELIMITER
	    strncpy(realname, entry->pw_gecos, REALNAME_LEN);
	}
	strncpy(username, entry->pw_name, NAME_LEN);
	strncpy(my_path, entry->pw_dir, PATH_LEN);
	if (*nickname == null(char))
	    strncpy(nickname, username, NICKNAME_LEN);
    } else {
	char *ptr;

	if (*realname == null(char))
	    strncpy(realname, "Unknown", REALNAME_LEN);
	if (ptr = getenv("USER"))
	    strncpy(username, ptr, NAME_LEN);
	else
	    strncpy(username, "Unknown", NAME_LEN);
	if (ptr = getenv("HOME"))
	    strncpy(my_path, ptr, PATH_LEN);
	else
	    strncpy(my_path, "/", PATH_LEN);
	if (*nickname == null(char))
	    strncpy(nickname, username, NICKNAME_LEN);
    }
}

main(argc, argv)
char *argv[];
int argc;
{
    fd_set rd;
    char buffer[BUFFER_SIZE + 1];
    union wait status;
    struct timeval timeout,
           *timeptr;

    timeout.tv_usec = 0L;
    timeout.tv_sec = 1L;
#ifdef HPUX
    /* Curses code added for HP-UX use */
    initscr();
    noecho();
    cbreak();
#endif
    init_variables();
    parse_args(argv, argc);
    if (gethostname(hostname, NAME_LEN)) {
	fprintf(stderr, "Couldn't figure out the name of your machine!\n");
	exit(1);
    }
    if (!dumb) {
	/* signal(SIGTSTP, term_pause); */
	signal(SIGINT, cntl_c);
	init_screen();
    }
#ifdef HPUX
    system("stty opost");
#endif
#ifdef MOTD
    /* display the motd */
    put_file(MOTD);
#endif MOTD
    if (check_nickname(nickname)) {
	if (connect_to_server(current_server, irc_port))
	    put_it("*** Use /SERVER to connect to server");
    } else {
	put_it("*** Bad nickname");
	put_it("*** Choose a new nickname and use /SERVER to connect to a server");
    }
#ifdef GLOBAL_IRCRC
    /* read global.ircrc */
    if (access(GLOBAL_IRCRC, R_OK) == 0)
	load("", GLOBAL_IRCRC);
#endif GLOBAL_IRCRC
    /* read the .ircrc file */
    strncpy(buffer, my_path, BUFFER_SIZE);
    strmcat(buffer, IRCRC_NAME, BUFFER_SIZE);
    if (access(buffer, R_OK) == 0)
	load("", buffer);
    while (1) {
	if (get_int_var(SHELL_LIMIT_VAR) &&
	    process_counter >= get_int_var(SHELL_LIMIT_VAR)) {
	    close(process_stdout);
	    close(process_stderr);
	    process_stdout = -1;
	    process_stderr = -1;
	}
	wait3(&status, WNOHANG, 0);
	FD_ZERO(&rd);
	FD_SET(0, &rd);
	if (!server_paused && (server_des != -1))
	    FD_SET(server_des, &rd);
	if (process_stdout != -1)
	    FD_SET(process_stdout, &rd);
	if (process_stderr != -1)
	    FD_SET(process_stderr, &rd);
	if (term_reset_flag) {
	    refresh_screen();
	    term_reset_flag = 0;
	}
	switch (select(32, &rd, 0, 0, timeptr)) {
	    case -1:
	    case 0:
		if (cntl_c_hit) {
		    edit_char('\003');
		    cntl_c_hit = 0;
		}
		cursor_to_input();
		fflush(stdout);
		timeptr = null(struct timeval *);
		break;
	    default:
		if (term_reset_flag) {
		    refresh_screen();
		    term_reset_flag = 0;
		}
		/*
		 * must check server before stdin, cause this will hang on a
		 * /FLUSH otherwise 
		 */
		if (server_des != -1) {
		    if (FD_ISSET(server_des, &rd)) {
			switch (dgets(buffer, BUFFER_SIZE, server_des)) {
			    case -1:
				close(server_des);
				break;
			    case 0:
				put_it("*** EOF from server, reconnecting.");
				close(server_des);
				server_des = -1;
				if (connect_to_server(current_server, irc_port))
				    put_it("*** Use /SERVER to connect to a server");
				break;
			    default:
				parse_server(buffer);
				break;
			}

		    }
		}
		if (FD_ISSET(0, &rd)) {
		    if (dumb) {
			if (fgets(buffer, BUFFER_SIZE, stdin)) {
			    *(buffer + strlen(buffer) - 1) = null(char);
			    parse_command(buffer);
			} else {
			    put_it("*** Irc exit on EOF from sdtin!");
			    irc_exit();
			}
		    } else {
			if (read(0, buffer, 1))
			    edit_char(*buffer);
			cntl_c_hit = 0;
		    }
		}
		if (process_stdout != -1) {
		    if (FD_ISSET(process_stdout, &rd)) {
			switch (dgets(buffer, 255, process_stdout)) {
			    case -1:
				break;
			    case 0:
				close(process_stdout);
				process_stdout = -1;
				break;
			    default:
				process_counter++;
				if (*(buffer + strlen(buffer) - 1) == '\n')
				    *(buffer + strlen(buffer) - 1) = null(char);
				if (process_redirect) {
				    if (process_who)
					send_text(process_who, buffer);
				    else
					send_text(query_nick, buffer);
				} else
				    put_it(buffer);
				break;
			}
		    }
		}
		if (process_stderr != -1) {
		    if (FD_ISSET(process_stderr, &rd)) {
			switch (dgets(buffer, 255, process_stderr)) {
			    case -1:
				break;
			    case 0:
				close(process_stderr);
				process_stderr = -1;
				break;
			    default:
				process_counter++;
				if (*(buffer + strlen(buffer) - 1) == '\n')
				    *(buffer + strlen(buffer) - 1) = null(char);
				if (process_redirect) {
				    if (process_who)
					send_text(process_who, buffer);
				    else
					send_text(query_nick, buffer);
				} else
				    put_it(buffer);
				break;
			}
		    }
		}
		break;
	}
	if ((timeptr == null(struct timeval *)) && cursor_in_display)
	    timeptr = &timeout;
    }
}
