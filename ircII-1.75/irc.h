/*
 * irc.h: header file for all of irc! 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include "config.h"

#define BUFFER_SIZE 1024

#define NICKNAME_LEN 9
#define NAME_LEN 80
#define REALNAME_LEN 30
#define PATH_LEN 1024

/* number of entries in irc_command array */
#define NUMBER_OF_COMMANDS 49

/* IrcCommand: structure for each command in the command table */
typedef struct {
    char *name;		    /* what the user types */
    char *server_func;	    /* what gets sent to the server (if anything) */
    char oper;		    /* true if this is an operator command */
    void (*func) ();	    /* function that is the command */
}      IrcCommand;

/* IrcVariable: structure for each variable in the variable table */
typedef struct {
    char *name;		    /* what the user types */
    char type;		    /* variable types, see below */
    int integer;	    /* int value of variable */
    char *string;	    /* string value of variable */
    void (*func) ();	    /* function to do every time variable is set */
}      IrcVariable;

/* KeyMap: the structure of the irc keymaps */
typedef struct {
    int index;
    char *stuff;
}      KeyMap;

/* KeyMapNames: the structure of the keymap to realname array */
typedef struct {
    char *name;
    void (*func) ();
}      KeyMapNames;

/* the default character used to indicate irc commands */
#define DEFAULT_CMDCHAR '/'

/* the types of IrcVariables */
#define BOOL_TYPE_VAR 0
#define CHAR_TYPE_VAR 1
#define INT_TYPE_VAR 2
#define FILE_TYPE_VAR 3

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

#define IRC_PORT 9000

extern char *index();
extern int errno;
extern char *sys_errlist[];

/* flags used by who() and whoreply() for who_mask */
#define WHO_OPS 1
#define WHO_NAME 2
#define WHO_HOST 4
#define WHO_SERVER 8
#define WHO_ZERO 16
#define WHO_NICK 32
#define WHO_ALL (WHO_NAME | WHO_HOST | WHO_SERVER | WHO_NICK)

/* declared in irc.c
*/
extern int process_stdout,
    process_stderr,
    process_redirect,
    process_counter;
extern char *process_who;    
extern char operator;
extern char server_paused;
extern int irc_port;

extern char irc_version[];
extern char buffer[];
extern char nickname[],
     current_server[],
     hostname[],
     realname[],
     username[],
    *away_message;
extern char my_path[];
extern int server_des,
    current_channel;
extern int connect_to_server();
extern char who_mask;
extern char *who_info;
extern char *who_nick;

extern IrcCommand irc_command[];

/* declared in ircaux.c */
extern char *check_nickname();
extern char *next_arg();
extern char *expand_twiddle();
extern char *upper();
extern char *sindex();

/* used by get_from_history */
#define NEXT 0
#define PREV 1

/* defined in history.c */
extern void set_history_size();
extern void add_to_history();
extern char *get_from_history();

/* defined in vars.c */
extern IrcVariable *find_variable();
extern void set_variable();
extern int get_int_var();
extern char *get_string_var();
extern void init_variables();

/* indexes for the irc_variable array */
#define CMDCHAR_VAR 0
#define DISPLAY_VAR 1
#define COMMAND_HISTORY_VAR 2
#define INLINE_ALIASES 3
#define INSERT_MODE_VAR 4
#define LASTLOG_VAR 5
#define LASTLOG_CONVERSATION_VAR 6
#define LASTLOG_MSG_ONLY_VAR 7
#define LOG_VAR 8
#define LOGFILE_VAR 9
#define SCROLL_VAR 10
#define SHELL_VAR 11
#define SHELL_FLAGS_VAR 12
#define SHELL_LIMIT_VAR 13
#define WARN_OF_IGNORES_VAR 14

/* defined in help.c */
extern void help();
extern void help_line();

/* defined in lastlog.c */
extern void set_lastlog_size();
extern void lastlog();
extern void add_to_lastlog();

/* declared in edit.c */
extern char *sent_nick,
    *recv_nick,
    *query_nick;
extern void load();
extern void send_text();
extern IrcCommand *find_command();

/* declared in alias.c */
extern void add_alias();
extern char *get_alias();
extern void list_aliases();
extern char mark_alias();
extern void delete_alias();
extern char *inline_aliases();

/* declared in output.c */
extern void make_status();
extern void put_it();
extern void send_to_server();
extern void refresh_screen();
extern void init_screen();

/* declared in log.c */
extern void logger();
extern void set_log_file();
extern void add_to_log();

/* declared in ignore.c */
extern void ignore_nickname();
extern int remove_ignore();
extern int is_ignored();
extern void ignore_list();
/* Type of ignored nicks */
#define IGNORE_ALL 0
#define IGNORE_PRIV 1
#define IGNORE_PUB 2

/* declared in input.c */
extern void move_cursor();
extern void forward_word();
extern void backward_word();
extern void delete();
extern void backspace();
extern void beginning_of_line();
extern void end_of_line();
extern void delete_previous_word();
extern void delete_next_word();
extern void add_ch();
extern void set_input();
extern char *get_input();
extern void clear_to_eol();
extern void clear_to_bol();
extern void init_input();
/* used by update_input */
#define NO_UPDATE 0
#define UPDATE_ALL 1
#define UPDATE_FROM_CURSOR 2
#define UPDATE_JUST_CURSOR 3

/* declared in display.c */
extern void erase_display();
extern void resize_display();
extern void redraw_display();
extern void add_to_display();
extern char cursor_in_display;


/* the "dumb" variable */
extern char dumb;


/* defined in reg.c */
extern int reg_exec();
extern void reg_comp();

/* declared in keys.c */

extern KeyMap keys[], meta1_keys[], meta2_keys[];
extern KeyMapNames key_names[];
