/*
 * edit.c: This is really a mismash of function and such that deal with IRCII
 * commands (both normal and keybinding commands) 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include "irc.h"
#include "term.h"
#include "edit.h"
#include "crypt.h"
#include "vars.h"
#include "ircaux.h"
#include "lastlog.h"
#include "window.h"
#include "whois.h"
#include "hook.h"
#include "input.h"
#include "ignore.h"
#include "keys.h"
#include "names.h"
#include "alias.h"
#include "history.h"

/* number of entries in irc_command array */
#define NUMBER_OF_COMMANDS 64

/* used with input_move_cursor */
#define RIGHT 1
#define LEFT 0

/* The maximum number of recursive LOAD levels allowed */
#define MAX_LOAD_DEPTH 10

static char meta1_hit = 0;
static char meta2_hit = 0;
static char quote_hit = 0;

static char *temp_nick;


/* recv_nick: the nickname of the last person to send you a privmsg */
char *recv_nick = null(char *);

/* sent_nick: the nickname of the last person to whom you sent a privmsg */
char *sent_nick = null(char *);
char *sent_body = null(char *);

/* a few advance declarations */
extern void parse_command();
extern void edit_char();
extern void quit();
extern void wall();
extern void send_com();
extern void clear();
extern void quote();
extern void privmsg();
extern void flush();
extern void alias();
extern void server();
extern void away();
extern void query();
extern void oper();
extern void channel();
extern void exec();
extern void ignore();
extern void who();
extern void whois();
extern void info();
extern void nick();
extern void comment();
extern void bindcmd();
extern void history();
extern void version();
extern void on();
extern void echo();
extern void save_settings();
extern void type();
extern void notify();

/* IrcCommand: structure for each command in the command table */
typedef struct {
    char *name;			/* what the user types */
    char *server_func;		/* what gets sent to the server (if anything) */
    char oper;			/* true if this is an operator command */
    void (*func) ();		/* function that is the command */
}      IrcCommand;

/*
 * irc_command: all the availble irc commands:  Note that the first entry has
 * a zero length string name and a null server command... this little trick
 * make "/ blah blah blah" always get sent to a channel, bypassing queries,
 * etc.  Neato.  This list MUST be sorted and the NUMBER_OF_COMMANDS define
 * must represent the number of commands in this list 
 */
static IrcCommand irc_command[] =
{
 "", null(char *), 0, send_text,
 "ADMIN", "ADMIN", 0, send_com,
 "ALIAS", "ALIAS", 0, alias,
 "AWAY", "AWAY", 0, away,
 "BIND", "BIND", 0, bindcmd,
 "BYE", "QUIT", 0, quit,
 "CHANNEL", "JOIN", 0, channel,
 "CLEAR", "CLEAR", 0, clear,
 "COMMENT", "COMMENT", 0, comment,
 "CONNECT", "CONNECT", 1, send_com,
 "DATE", "TIME", 0, send_com,
 "DIE", "DIE", 1, send_com,
 "ECHO", "ECHO", 0, echo,
 "ENCRYPT", "ENCRYPT", 0, encrypt_cmd,
 "EXEC", "EXEC", 0, exec,
 "EXIT", "QUIT", 0, quit,
 "FLUSH", "FLUSH", 0, flush,
 "HELP", "HELP", 0, help,
 "HISTORY", "HISTORY", 0, history,
 "IGNORE", "IGNORE", 0, ignore,
 "INFO", "INFO", 0, info,
 "INVITE", "INVITE", 0, send_com,
 "JOIN", "JOIN", 0, channel,
 "KICK", "KICK", 0, send_com,
 "KILL", "KILL", 1, send_com,
 "LASTLOG", "LASTLOG", 0, lastlog,
 "LEAVE", "PART", 0, send_com,
 "LINKS", "LINKS", 0, send_com,
 "LIST", "LIST", 0, send_com,
 "LOAD", "LOAD", 0, load,
 "LUSERS", "LUSERS", 0, send_com,
 "MAIL", "MAIL", 0, send_com,
 "MODE", "MODE", 0, send_com,
 "MOTD", "MOTD", 0, send_com,
 "MSG", "PRIVMSG", 0, privmsg,
 "NAMES", "NAMES", 0, send_com,
 "NICK", "NICK", 0, nick,
 "NOTICE", "NOTICE", 0, privmsg,
 "NOTIFY", "NOTIFY", 0, notify,
 "ON", "ON", 0, on,
 "OPER", "OPER", 0, oper,
 "PART", "PART", 0, send_com,
 "QUERY", "QUERY", 0, query,
 "QUIT", "QUIT", 0, quit,
 "QUOTE", "QUOTE", 0, quote,
 "REHASH", "REHASH", 1, send_com,
 "SAVE", "SAVE", 0, save_settings,
 "SERVER", "SERVER", 0, server,
 "SET", "SET", 0, set_variable,
 "SIGNOFF", "QUIT", 0, quit,
 "SQUIT", "SQUIT", 1, send_com,
 "STATS", "STATS", 0, send_com,
 "SUMMON", "SUMMON", 0, send_com,
 "TIME", "TIME", 0, send_com,
 "TOPIC", "TOPIC", 0, send_com,
 "TRACE", "TRACE", 1, send_com,
 "TYPE", "TYPE", 0, type,
 "USERS", "USERS", 0, send_com,
 "VERSION", "VERSION", 0, version,
 "WALL", "WALL", 1, wall,
 "WALLOPS", "WALLOPS", 0, wall,
 "WHO", "WHO", 0, who,
 "WHOIS", "WHOIS", 0, whois,
 "WHOWAS", "WHOWAS", 0, whois,
 "WINDOW", "WINDOW", 0, window,
 null(char *), null(char *), 0, NULL,
};

/*
 * find_command: looks for the given name in the command list, returning a
 * pointer to the first match and the number of matches in cnt.  If no
 * matches are found, null is returned (as well as cnt being 0). The command
 * list is sorted, so we do a binary search.  The returned commands always
 * points to the first match in the list.  If the oper flag is set, then all
 * commands are considered in the cnt, even operator commands.  If oper is
 * false, only user commands are counted.  If the match is exact, it is
 * returned and cnt is set to the number of matches * -1.  Thus is 4 commands
 * matched, but the first was as exact match, cnt is -4. 
 */
static IrcCommand *find_command(com, cnt, oper)
char *com;
int *cnt;
char oper;
{
    int len;

    if (com && (len = strlen(com))) {
	int min,
	    max,
	    pos,
	    old_pos = -1,
	    c;

	min = 1;
	max = NUMBER_OF_COMMANDS + 1;
	while (1) {
	    pos = (max + min) / 2;
	    if (pos == old_pos) {
		*cnt = 0;
		return (null(IrcCommand *));
	    }
	    old_pos = pos;
	    c = strncmp(com, irc_command[pos].name, len);
	    if (c == 0)
		break;
	    else if (c > 0)
		min = pos;
	    else
		max = pos;
	}
	*cnt = 0;
	if (irc_command[pos].oper) {
	    if (oper)
		(*cnt)++;
	} else
	    (*cnt)++;
	min = pos - 1;
	while ((min > 0) && (strncmp(com, irc_command[min].name, len) == 0)) {
	    if (irc_command[min].oper) {
		if (oper)
		    (*cnt)++;
	    } else
		(*cnt)++;
	    min--;
	}
	min++;
	max = pos + 1;
	while ((max < NUMBER_OF_COMMANDS + 1) && (strncmp(com, irc_command[max].name, len) == 0)) {
	    if (irc_command[max].oper) {
		if (oper)
		    (*cnt)++;
	    } else
		(*cnt)++;
	    max++;
	}
	if (*cnt) {
	    if (strlen(irc_command[min].name) == len)
		*cnt *= -1;
	    return (&(irc_command[min]));
	} else
	    return (null(IrcCommand *));
    } else {
	*cnt = -1;
	return (irc_command);
    }
}

/* echo: simply displays the args to the screen */
void echo(command, args)
char *command,
    *args;
{
    int display;

    display = get_int_var(DISPLAY_VAR);
    set_int_var(DISPLAY_VAR, 1);
    put_it("%s", args);
    set_int_var(DISPLAY_VAR, display);
}

/*
 * oper_password_sendline: this routine is called by irc_io() when it is
 * called from oper() prompting the user for a password 
 */
static void oper_password_sendline()
{
    char *ptr;

    ptr = get_input();
    if (*ptr)
	send_to_server("OPER %s %s", temp_nick, ptr);
    irc_io_loop = 0;
}

/* oper: the OPER command.  */
static void oper(command, args)
char *command,
    *args;
{
    static char in_it = 0;
    char *password;

    if (!in_it) {
	in_it = 1;
	oper_command = 1;
	if ((temp_nick = next_arg(args, &args)) == null(char *))
	    temp_nick = nickname;
	if ((password = next_arg(args, &args)) == null(char *)) {
	    term_echo(0);
	    irc_io("Password:", oper_password_sendline);
	    in_it = 0;
	    term_echo(1);
	    return;
	}
	send_to_server("OPER %s %s", temp_nick, password);
	in_it = 0;
    }
}

/*
 * save_settings_sendline: What happens when you hit return after the query
 * about saving your settings. 
 */
static void save_settings_sendline()
{
    char *ptr;
    int len;

    ptr = get_input();
    irc_io_loop = 0;
    if (len = strlen(ptr)) {
	if (strnicmp(ptr, "yes", len) == 0) {
	    FILE *fp;

	    if (fp = fopen(ircrc_file, "w")) {
		fprintf(fp, "/SET DISPLAY OFF\n");
		save_bindings(fp);
		save_hooks(fp);
		save_notify(fp);
		save_variables(fp);
		save_aliases(fp);
		if ((ptr = get_string_var(CMDCHARS_VAR)) == null(char *))
		    ptr = DEFAULT_CMDCHARS;
		fprintf(fp, "%cSET DISPLAY ON\n", ptr[0]);
		fclose(fp);
		put_it("*** IRCII settings saved to %s", ircrc_file);
	    } else
		put_it("*** Error opening %s: %s", ircrc_file, sys_errlist[errno]);
	}
    }
}

/* save_settings: saves the current state of IRCII to a file */
static void save_settings(command, args)
char *command,
    *args;
{
    char *file;

    if (file = next_arg(args, &args)) {
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID) {
	    put_it("*** You may only use the default value");
	    return;
	}
#endif DAEMON_UID
	malloc_strcpy(&ircrc_file, file);
    }
    sprintf(buffer, "Really write %s? ", ircrc_file);
    irc_io(buffer, save_settings_sendline);
}

/*
 * channel: does the channel command.  I just added displaying your current
 * channel if none is given 
 */
static void channel(command, args)
char *command,
    *args;
{
    char *chan;
    int len;

    if (irc2_6)
	command = "JOIN";
    else
	command = "CHANNEL";
    message_from(null(char *), LOG_CURRENT);
    if (chan = next_arg(args, &args)) {
	len = strlen(chan);
	if (strnicmp(chan, "-invite", len) == 0) {
	    if (invite_channel)
		send_to_server("%s %s", command, invite_channel);
	    else
		put_it("*** You have not been invited to a channel!");
	} else if (strnicmp(chan, "-nick", len) == 0) {
	    if (chan = next_arg(args, &args)) {
		if (index(chan, '*'))
		    put_it("*** Wildcards not allowed here!");
		else
		    add_to_whois_queue(chan, whois_join, null(char *));
	    } else
		put_it("*** You must specify a nickname!");
	} else {
	    if (is_on_channel(chan, nickname)) {
		is_current_channel(chan, 1);
		put_it("*** You are now talking to channel %s", current_channel(chan));
		update_all_status();
	    } else
		send_to_server("%s %s", command, chan);
	}
    } else
	list_channels();
    message_from(null(char *), LOG_CRAP);
}

/* comment: does the /COMMENT command, useful in .ircrc */
static void comment(command, args)
char *command,
    *args;
{
    /* nothing to do... */
}

/*
 * nick: does the /NICK command.  Records the users current nickname and
 * sends the command on to the server 
 */
static void nick(command, args)
char *command,
    *args;
{
    char *nick;

    if ((nick = next_arg(args, &args)) == null(char *)) {
	put_it("*** Your nickname is %s", nickname);
	return;
    }
    if (strcmp(nick, nickname) == 0)
	return;
    if (check_nickname(nick)) {
	strncpy(old_nickname, nickname, NICKNAME_LEN);
	strncpy(nickname, nick, NICKNAME_LEN);
	send_to_server("NICK %s", nick);
	update_all_status();
    } else
	put_it("*** Bad nickname");
}

/* version: does the /VERSION command with some IRCII version stuff */
static void version(command, args)
char *command,
    *args;
{
    char *host;

    if (host = next_arg(args, &args))
	send_to_server("%s %s", command, host);
    else {
	put_it("*** IRCII %s", irc_version);
	send_to_server("%s", command);
    }
}

/* info: does the /INFO command.  I just added some credits */
static void info(command, args)
char *command,
    *args;
{
    put_it("*** IRCII:  Written by Michael Sandrof (ms5n@andrew.cmu.edu)");
    put_it("*** IRCII:  Copyright 1990");
    put_it("***         Do a /HELP IRCII COPYRIGHT for the full copyright");
    put_it("*** IRCII   includes software developed by the University");
    put_it("***         of California, Berkeley and its contributors");
    put_it("*** IRCII:  HP-UX modifications by Mark T. Dame (Mark.Dame@uc.edu)");
    put_it("*** IRCII:  Termio modifications by Stellan Klebom (d88-skl@nada.kth.se)");
    put_it("*** IRCII:  Other contributors:");
    put_it("***         \tCarl v. Loesch (Lynx)");
    put_it("*** Irc Server Information:");
    send_to_server("%s %s", command, args);
}

/*
 * whois: the WHOIS and WHOWAS commands.  This simply sends the information
 * to the whois handlers in whois.c 
 */
static void whois(command, args)
char *command,
    *args;
{
    char *nick,
        *ptr;

    if (strcmp(command, "WHOIS") == 0) {
	if (nick = next_arg(args, &args)) {
	    while (nick) {
		if (ptr = index(nick, ','))
		    *(ptr++) = null(char);
		if (*nick)
		    send_to_server("WHOIS %s", nick);
		nick = ptr;
	    }
	} else
	    send_to_server("WHOIS %s", nickname);
    } else
	send_to_server("%s %s", command, args);	/* thanks to lynx */

}

/*
 * who: the /WHO command. Parses the who switches and sets the who_mask and
 * whoo_stuff accordingly.  Who_mask and who_stuff are used in whoreply() in
 * parse.c 
 */
static void who(command, args)
char *command,
    *args;
{
    char *arg,
        *channel = null(char *),
         no_args = 1;
    int len;

    who_mask = 0;
    new_free(&who_nick);
    new_free(&who_host);
    new_free(&who_name);
    new_free(&who_server);
    while (arg = next_arg(args, &args)) {
	no_args = 0;
	if ((*arg == '-') && (!isdigit(*(arg + 1)))) {
	    arg++;
	    if ((len = strlen(arg)) == 0) {
		put_it("*** Unknown or missing flag");
		return;
	    }
	    if (strnicmp(arg, "operators", len) == 0)
		who_mask |= WHO_OPS;
	    else if (strnicmp(arg, "chops", len) == 0)
		who_mask |= WHO_CHOPS;
	    else if (strnicmp(arg, "name", len) == 0) {
		who_mask |= WHO_NAME;
		if (arg = next_arg(args, &args))
		    malloc_strcpy(&who_name, arg);
		else {
		    put_it("*** WHO -NAME: missing arguement");
		    return;
		}
	    } else if (strnicmp(arg, "host", len) == 0) {
		who_mask |= WHO_HOST;
		if (arg = next_arg(args, &args))
		    malloc_strcpy(&who_host, arg);
		else {
		    put_it("*** WHO -HOST: missing arguement");
		    return;
		}
	    } else if (strnicmp(arg, "server", len) == 0) {
		who_mask |= WHO_SERVER;
		if (arg = next_arg(args, &args))
		    malloc_strcpy(&who_server, arg);
		else {
		    put_it("*** WHO -SERVER: missing arguement");
		    return;
		}
	    } else if (strnicmp(arg, "all", len) == 0) {
		who_mask |= WHO_ALL;
		if (arg = next_arg(args, &args)) {
		    malloc_strcpy(&who_nick, arg);
		    malloc_strcpy(&who_host, arg);
		    malloc_strcpy(&who_name, arg);
		    malloc_strcpy(&who_server, arg);
		} else {
		    put_it("*** WHO -ALL: missing arguement");
		    return;
		}
	    } else {
		put_it("*** Unknown or missing flag");
		return;
	    }
	} else {
	    if (is_channel(arg) || (strcmp(arg, "*") == 0)) {
		if (channel == null(char *))
		    channel = arg;
	    } else {
		who_mask |= WHO_NICK;
		malloc_strcpy(&who_nick, arg);
	    }
	}
    }
    if (no_args)
	put_it("*** No argument specified");
    else {
	if (channel) {
	    if (is_number(channel) && (atoi(channel) == 0))
		who_mask |= WHO_ZERO;
	    send_com(command, channel);
	} else
	    send_com(command, empty_string);
    }
}

/*
 * query: the /QUERY command.  Works much like the /MSG, I'll let you figure
 * it out. 
 */
void query(command, args)
char *command,
    *args;
{
    char *nick,
        *rest;

    if (nick = next_arg(args, &rest)) {
	if (strcmp(nick, ".") == 0)
	    nick = sent_nick;
	else if (strcmp(nick, ",") == 0)
	    nick = recv_nick;
	if (*nick == '%') {
	    if (!is_number(nick + 1)) {
		put_it("*** You must specify a process number!");
		return;
	    }
	}
	set_query_nick(nick);
	message_from(nick, LOG_MSG);
	if (get_int_var(VERBOSE_QUERY_VAR))
	    put_it("*** Starting conversation with %s", query_nick());
    } else {
	if (query_nick()) {
	    message_from(query_nick(), LOG_MSG);
	    if (get_int_var(VERBOSE_QUERY_VAR))
		put_it("*** Ending conversation with %s", query_nick());
	    set_query_nick(null(char *));
	} else
	    put_it("*** You aren't querying anyone!");
    }
    message_from(null(char *), LOG_CRAP);
    update_all_status();
}

/*
 * away: the /AWAY command.  Keeps track of the away message locally, and
 * sends the command on to the server 
 */
static void away(command, args)
char *command,
    *args;
{
    if (*args)
	malloc_strcpy(&away_message, args);
    else
	new_free(&away_message);
    update_all_status();
    send_to_server("%s :%s", command, args);
}

/*
 * server: the /SERVER command. Closes the users connection to the current
 * server and attempt to connect to the name one.  If no conenction can be
 * made, the user is reconnected to the original.  If this fails, the user
 * must try a /SERVER again. 
 */
static void server(command, args)
char *command,
    *args;
{
    char *server,
        *port;
    int port_num,
        server_num;

    if (server = next_arg(args, &args)) {
	if (port = next_arg(args, &args))
	    port_num = atoi(port);
	else
	    port_num = irc_port;
	if (*server == '+') {
	    get_connected(current_server_index() + 1);
	    return;
	} else if (*server == '-') {
	    get_connected(current_server_index() - 1);
	    return;
	} else if (isdigit(*server) && (index(server, '.') == null(char *))) {
	    server_num = atoi(server);
	    if ((server_num >= 0) && (server_num < server_list_size()))
		get_connected(server_num);
	    else
		put_it("*** There is no server %d in the server list", server_num);
	    return;
	}
	connect_to_server(server, port_num);
    } else
	display_server_list();
}

/* quit: The /QUIT, /EXIT, etc command */
static void quit(command, args)
char *command,
    *args;
{
    irc_exit();
}

/*
 * alias: the /ALIAS command.  Calls the correct alias function depending on
 * the args 
 */
static void alias(command, args)
char *command,
    *args;
{
    char *name,
        *rest;

    if (name = next_arg(args, &rest)) {
	if (*rest)
	    add_alias(name, rest);
	else {
	    if (*name == '-') {
		if (*(name + 1))
		    delete_alias(name + 1);
		else
		    put_it("*** You must specify an alias to be removed");
	    } else
		list_aliases(name);
	}
    } else
	list_aliases(null(char *));
}

/* flush: flushes all pending stuff coming from the server */
static void flush(command, args)
char *command,
    *args;
{
    put_it("*** Standby, Flushing server output...");
    flush_server();
    put_it("*** Done");
}

/* wall: used for WALL and WALLOPS */
static void wall(command, args)
char *command,
    *args;
{
    if (strcmp(command, "WALL") == 0) {	/* I hate this */
	message_from(null(char *), LOG_WALL);
	if (!operator)
	    put_it("## %s", args);
    } else {
	message_from(null(char *), LOG_WALLOP);
	if (!operator)
	    put_it("!! %s", args);
    }
    send_to_server("%s :%s", command, args);
    message_from(null(char *), LOG_CRAP);
}

/*
 * privmsg: The MSG command, displaying a message on the screen indicating
 * the message was sent.  Also, this works for the NOTICE command. 
 */
static void privmsg(command, args)
char *command,
    *args;
{
    char *nick;

    if (nick = next_arg(args, &args)) {
	if (strnicmp(nick, "-channel", strlen(nick)) == 0) {
	    if (nick = next_arg(args, &args)) {
		if (index(nick, '*'))
		    put_it("*** Wildcards not allowed here!");
		else
		    add_to_whois_queue(nick, whois_privmsg, "%s %%s %s", command, args);
	    } else
		put_it("*** You must specify a nickname or channel!");
	} else
	    send_text(nick, args, command);
    } else
	put_it("*** You must specify a nickname or channel!");
}

/*
 * quote: handles the QUOTE command.  args are a direct server command which
 * is simply send directly to the server 
 */
static void quote(command, args)
char *command,
    *args;
{
    send_to_server(args);
}

/* clear: the CLEAR command.  Figure it out */
static void clear(command, args)
char *command,
    *args;
{
    char *arg;

    if (arg = next_arg(args, &args)) {
	if (stricmp(arg, "ALL", 3) == 0)
	    clear_all_windows();
	else
	    put_it("*** Unknown flag: %s", arg);
    } else
	clear_window(null(Window *));
    update_input(UPDATE_JUST_CURSOR);
}

/*
 * send_com: the generic command function.  Uses the full command name found
 * in 'command', combines it with the 'args', and sends it to the server 
 */
static void send_com(command, args)
char *command,
    *args;
{
    send_to_server("%s %s", command, args);
}

/*
 * send_text: Sends the line of text to whomever the user is currently
 * talking.  If they are quering someone, it goes to that user, otherwise
 * it's the current channel.  Some tricky parameter business going on. If
 * nick is null (as if you had typed "/ testing" on the command line) the
 * line is sent to your channel, and the command parameter is never used. If
 * nick is not null, and command is null, the line is sent as a PRIVMSG.  If
 * nick is not null and command is not null, the message is sent using
 * command.  Currently, only NOTICEs and PRIVMSGS work. 
 */
void send_text(nick, line, command)
char *nick;
char *line;
char *command;
{
    char *key = null(char *);
    int lastlog_level;

    if (*line) {
	if (nick) {
	    if (*nick == '%') {
		text_to_process(atoi(nick + 1), line);
		return;
	    }
	    /* Query quote -lynx */
	    if (strcmp(nick, "\"") == 0) {
		send_to_server("%s", line);
		return;
	    }
	    if (strcmp(nick, ".") == 0) {
		if ((nick = sent_nick) == null(char *))
		    nick = empty_string;
	    } else if (strcmp(nick, ",") == 0) {
		if ((nick = recv_nick) == null(char *))
		    nick = empty_string;
	    }
	    switch (send_text_flag) {
		case MSG_LIST:
		    command = "NOTICE";
		    break;
		case NOTICE_LIST:
		    echo(null(char *), "*** You cannot send a message from a NOTICE");
		    return;
	    }
	    if ((command == null(char *)) || strcmp(command, "NOTICE")) {
		lastlog_level = set_lastlog_msg_level(LOG_MSG);
		command = "PRIVMSG";
		message_from(nick, LOG_MSG);
		if (do_hook(SEND_MSG_LIST, nick, line))
		    put_it("-> *%s* %s", nick, line);
		if (key = is_crypted(nick))
		    line = crypt_msg(line, key, 1);
	    } else {
		lastlog_level = set_lastlog_msg_level(LOG_NOTICE);
		message_from(nick, LOG_NOTICE);
		if (do_hook(SEND_NOTICE_LIST, nick, line))
		    put_it("-> -%s- %s", nick, line);
	    }
	    set_lastlog_msg_level(lastlog_level);
	    if (line) {
		if (get_int_var(SEND_MULTI_NICKS_VAR) && index(nick, ','))
		    sprintf(buffer, "%s:", nick);
		else
		    *buffer = null(char);
		if (command)
		    send_to_server("%s %s :%s%s", command, nick, buffer, line);
		else
		    send_to_server("PRIVMSG %s :%s%s", nick, buffer, line);
	    } else
		return;
	    if (get_int_var(WARN_OF_IGNORES_VAR) &&
		(is_ignored(nick, IGNORE_MSGS) == IGNORED))
		put_it("*** Warning: You are ignoring private messages from %s", nick);
	    malloc_strcpy(&sent_nick, nick);
	    malloc_strcpy(&sent_body, line);
	    if (away_message && get_int_var(AUTO_UNMARK_AWAY_VAR))
		away("AWAY", empty_string);
	    message_from(null(char *), LOG_CRAP);
	    return;
	} else {
	    if (current_channel(null(char *)) == null(char *)) {
		put_it("*** You have not joined a channel!");
		return;
	    }
	    lastlog_level = set_lastlog_msg_level(LOG_PUBLIC);
	    message_from(current_channel(null(char *)), LOG_PUBLIC);
	    if (do_hook(SEND_PUBLIC_LIST, current_channel(null(char *)), line))
		put_it("> %s", line);
	    if (key = is_crypted(current_channel(null(char *))))
		line = crypt_msg(line, key, 1);
	    set_lastlog_msg_level(lastlog_level);
	    if (line) {
		char *chan;

		message_from(current_channel(null(char *)), LOG_PUBLIC);
		if (get_int_var(USE_OLD_MSG_VAR) && (chan = current_channel(null(char *))) && (*chan != '#'))
		    send_to_server("MSG :%s", line);
		else
		    send_to_server("PRIVMSG %s :%s", current_channel(null(char *)), line);
	    }
	}
	message_from(null(char *), LOG_CRAP);
    }
}

/*
 * command_completion: builds lists of commands and aliases that match the
 * given command and displays them for the user's lookseeing 
 */
void command_completion()
{
    char do_aliases;
    int cmd_cnt,
        alias_cnt,
        i,
        c,
        len;
    char **aliases = null(char **);
    char *line = null(char *),
        *com,
        *cmdchars,
        *rest;
    IrcCommand *command = null(IrcCommand *);

    malloc_strcpy(&line, get_input());
    if (com = next_arg(line, &rest)) {
	if ((cmdchars = get_string_var(CMDCHARS_VAR)) == null(char *))
	    cmdchars = DEFAULT_CMDCHARS;
	if (index(cmdchars, *com)) {
	    com++;
	    if (*com && index(cmdchars, *com)) {
		do_aliases = 0;
		alias_cnt = 0;
		com++;
	    } else
		do_aliases = 1;
	    upper(com);
	    if (do_aliases)
		aliases = match_alias(com, &alias_cnt);
	    if (command = find_command(com, &cmd_cnt, operator)) {
		if (cmd_cnt < 0)
		    cmd_cnt *= -1;
		/* special case for the empty string */
		if (*(command[0].name) == null(char)) {
		    command++;
		    cmd_cnt = NUMBER_OF_COMMANDS;
		}
	    }
	    if ((alias_cnt == 1) && (cmd_cnt == 0)) {
		sprintf(buffer, "/%s %s", aliases[0], rest);
		set_input(buffer);
		new_free(&(aliases[0]));
		new_free(aliases);
		update_input(UPDATE_ALL);
	    } else if (((cmd_cnt == 1) && (alias_cnt == 0)) ||
		       ((cmd_cnt == 1) && (alias_cnt == 1) &&
			(strcmp(aliases[0], command[0].name) == 0))) {
		sprintf(buffer, "/%s%s %s", (do_aliases ? "" : "/"), command[0].name, rest);
		set_input(buffer);
		update_input(UPDATE_ALL);
	    } else {
		*buffer = null(char);
		if (command) {
		    put_it("*** Commands:");
		    strncpy(buffer, "\t", BUFFER_SIZE);
		    c = 0;
		    for (i = 0; i < cmd_cnt; i++) {
			if ((command[i].oper == 0) ||
			    (command[i].oper && operator)) {
			    strmcat(buffer, command[i].name, BUFFER_SIZE);
			    for (len = strlen(command[i].name); len < 15; len++)
				strmcat(buffer, " ", BUFFER_SIZE);
			    if (++c == 4) {
				put_it("*** %s", buffer);
				strncpy(buffer, "\t", BUFFER_SIZE);
				c = 0;
			    }
			}
		    }
		    if (c)
			put_it("*** %s", buffer);
		}
		if (aliases) {
		    put_it("*** Aliases:");
		    strncpy(buffer, "\t", BUFFER_SIZE);
		    c = 0;
		    for (i = 0; i < alias_cnt; i++) {
			strmcat(buffer, aliases[i], BUFFER_SIZE);
			for (len = strlen(aliases[i]); len < 15; len++)
			    strmcat(buffer, " ", BUFFER_SIZE);
			if (++c == 4) {
			    put_it("*** %s", buffer);
			    strncpy(buffer, "\t", BUFFER_SIZE);
			    c = 0;
			}
			new_free(&(aliases[i]));
		    }
		    if (strlen(buffer) > 1)
			put_it("*** %s", buffer);
		    new_free(&aliases);
		}
		if (*buffer == null(char))
		    term_beep();
	    }
	} else
	    term_beep();
    } else
	term_beep();
    new_free(&line);
}

/*
 * parse_command: parses a line of input from the user.  If the first
 * character of the line is equal to irc_variable[CMDCHAR_VAR].value, the
 * line is used as an irc command and parsed appropriately.  If the first
 * character is anything else, the line is sent to the current channel or to
 * the current query user.  If hist_flag is true, commands will be added to
 * the command history as appropriate.  Otherwise, parsed commands will not
 * be added. 
 */
void parse_command(org_line, hist_flag)
char *org_line;
int hist_flag;
{
    static unsigned int level = 0;
    char add_to_hist;
    char *ptr = null(char *),
        *free_line,
        *cmdchars,
        *line = null(char *),
        *this_cmd = null(char *);

    if ((org_line == null(char *)) || (*org_line == null(char)))
	return;
    level++;
    if (get_int_var(INLINE_ALIASES_VAR))
	line = inline_aliases(org_line);
    else
	malloc_strcpy(&line, org_line);
    free_line = ptr = line;
    while (line = index(line, QUOTE_CHAR)) {
	line++;
	/* convert \n to ^J */
	if ((*line == 'n') || (*line == 'N')) {
	    *(line - 1) = '\n';
	    strcpy(line, line + 1);
	    line++;
	} else {
	    /* covert \\ to \, so you can \\n in aliase */
	    if (*line == QUOTE_CHAR) {
		strcpy(line, line + 1);
		line++;
	    }
	}
    }
    while (ptr) {
	line = ptr;
	if (ptr = sindex(ptr, "\n\r"))
	    *(ptr++) = null(char);
	if ((cmdchars = get_string_var(CMDCHARS_VAR)) == null(char *))
	    cmdchars = DEFAULT_CMDCHARS;
	malloc_strcpy(&this_cmd, line);
	add_to_hist = 1;
	if (index(cmdchars, *line)) {
	    char *com,
	        *rest,
	        *alias = null(char *),
	        *alias_name;
	    int cmd_cnt,
	        alias_cnt;
	    IrcCommand *command = null(IrcCommand *);

	    com = line + 1;
	    if (rest = index(com, ' '))
		*(rest++) = null(char);
	    else
		rest = empty_string;
	    upper(com);

	    /* first, check aliases */
	    if (*com && index(cmdchars, *com)) {
		alias_cnt = 0;
		com++;
	    } else
		alias = get_alias(com, rest, &alias_cnt, &alias_name);
	    if (alias && (alias_cnt == 0)) {
		if (hist_flag && add_to_hist) {
		    add_to_history(this_cmd);
		    set_input(empty_string);
		}
		execute_alias(alias_name, alias);
		new_free(&alias);
		new_free(&alias_name);
	    } else {
		/* Check if we're parsing a history command */
		if (*com == '!') {
		    if (com = do_history(com + 1, rest)) {
			if (level == 1) {
			    set_input(com);
			    update_input(UPDATE_ALL);
			} else
			    parse_command(com, 0);
			new_free(&com);
		    } else
			set_input(empty_string);
		    add_to_hist = 0;
		} else {
		    if (hist_flag && add_to_hist) {
			add_to_history(this_cmd);
			set_input(empty_string);
		    }
		    command = find_command(com, &cmd_cnt, operator);
		    if ((command && (cmd_cnt < 0)) ||
			((alias_cnt == 0) && (cmd_cnt == 1)))
			(command->func) (command->server_func, rest);
		    else if (alias && (alias_cnt == 1) && (cmd_cnt == 1) &&
			     (strcmp(alias_name, command[0]) == 0))
			execute_alias(alias_name, alias);
		    else if ((alias_cnt + cmd_cnt) > 1)
			put_it("*** Ambiguous command: %s", com);
		    else if (alias && (alias_cnt == 1))
			execute_alias(alias_name, alias);
		    else
			put_it("*** Unknown command: %s", com);
		}
		if (alias) {
		    new_free(&alias);
		    new_free(&alias_name);
		}
	    }
	} else {
	    send_text(query_nick(), line, null(char *));
	    if (hist_flag && add_to_hist) {
		add_to_history(this_cmd);
		set_input(empty_string);
	    }
	}
    }
    new_free(&this_cmd);
    new_free(&free_line);
    level--;
}

/*
 * load: the /LOAD command.  Reads the named file, parsing each line as
 * though it were typed in (passes each line to parse_command). 
 */
void load(command, args)
char *command,
    *args;
{
    FILE *fp;
    char *filename,
        *expanded,
         flag = 0;
    struct stat stat_buf;
    static load_depth = 0;

    if (load_depth == MAX_LOAD_DEPTH) {
	put_it("*** No more than %d levels of LOADs allowed", MAX_LOAD_DEPTH);
	return;
    }
    load_depth++;
    if (filename = next_arg(args, &args)) {
	if (expanded = expand_twiddle(filename)) {
	    if (stat(expanded, &stat_buf) == 0) {
		if (stat_buf.st_mode & S_IFDIR) {
		    put_it("*** %s is a directory", expanded);
		    load_depth--;
		    return;
		}
		if (stat_buf.st_mode & 0111) {
		    put_it("*** %s is executable and may not be loaded", expanded);
		    load_depth--;
		    return;
		}
	    }
	    if (filename = next_arg(args, &args))
		flag = (strcmp(filename, "-") == 0);
	    if (fp = fopen(expanded, "r")) {
		while (fgets(buffer, BUFFER_SIZE, fp)) {
		    if (*(buffer + strlen(buffer) - 1) == '\n')
			*(buffer + strlen(buffer) - 1) = null(char);
		    if (flag) {
			filename = expand_alias(null(char *), buffer, args);
			parse_command(filename, 0);
			new_free(&filename);
		    } else
			parse_command(buffer, 0);
		}
		fclose(fp);
	    } else
		put_it("*** Couldn't open %s: %s", expanded, sys_errlist[errno]);
	} else
	    put_it("*** Unknown user");
    } else
	put_it("*** No filename specified");
    load_depth--;
}

/*
 * get_history: gets the next history entry, either the PREV entry or the
 * NEXT entry, and sets it to the current input string 
 */
void get_history(which)
int which;
{
    char *ptr;

    if (ptr = get_from_history(which)) {
	set_input(ptr);
	update_input(UPDATE_ALL);
    }
}

/* forward_character: The BIND function FORWARD_CHARACTER */
void forward_character()
{
         input_move_cursor(RIGHT);
}

/* backward_character: The BIND function BACKWARD_CHARACTER */
void backward_character()
{
         input_move_cursor(LEFT);
}

/* backward_history: the BIND function BACKWARD_HISTORY */
void backward_history()
{
         get_history(PREV);
}

/* forward_history: the BIND function FORWARD_HISTORY */
void forward_history()
{
         get_history(NEXT);

}

/* toggle_insert_mode: the BIND function TOGGLE_INSERT_MODE */
void toggle_insert_mode()
{
         set_var_value(INSERT_MODE_VAR, "TOGGLE", 0);
}

/* send_line: the BIND function SEND_LINE */
void send_line()
{
    char *ptr = null(char *);

    hold_mode(null(Window *), OFF, 1);
    malloc_strcpy(&ptr, get_input());
    if (*ptr)
	parse_command(ptr, 1);
    update_input(UPDATE_ALL);
    new_free(&ptr);
}

/* toggle_stop_screen: the BIND function TOGGLE_STOP_SCREEN */
void toggle_stop_screen()
{
         hold_mode(null(Window *), TOGGLE, 1);
}

/* meta2_char: the BIND function META2_CHARACTER */
void meta2_char()
{
         meta2_hit = 1;
}

/* meta1_char: the BIND function META1_CHARACTER */
void meta1_char()
{
         meta1_hit = 1;
}

/* quote_char: the BIND function QUOTE_CHARACTER */
void quote_char(c)
char c;
{
    quote_hit = 1;
}

/* type_text: the BIND function TYPE_TEXT */
void type_text(c, str)
char c,
    *str;
{
    for (; *str; str++)
	input_add_character(*str);
}

/*
 * clear_screen: the CLEAR_SCREEN function for BIND.  Clears the screen and
 * starts it if it is held 
 */
void clear_screen(c, str)
char c,
    *str;
{
    hold_mode(null(Window *), OFF, 1);
    clear(null(char *), empty_string);
}

/* parse_text: the bindable function that executes its string */
void parse_text(c, str)
char c,
    *str;
{
    char *ptr;

    ptr = expand_alias(null(char *), str, empty_string);
    parse_command(ptr, 0);
    new_free(&ptr);
}

/*
 * edit_char: handles each character for an input stream.  Not too difficult
 * to work out.  
 */
void edit_char(key)
char key;
{
    void (*func) ();
    char *ptr;

    key &= 127;			/* mask out non-ascii crap */
    if (meta1_hit) {
	func = key_names[meta1_keys[key].index].func;
	ptr = meta1_keys[key].stuff;
	meta1_hit = 0;
    } else if (meta2_hit) {
	func = key_names[meta2_keys[key].index].func;
	ptr = meta2_keys[key].stuff;
	meta2_hit = 0;
    } else {
	func = key_names[keys[key].index].func;
	ptr = keys[key].stuff;
    }
    if (quote_hit && !meta1_hit && !meta2_hit) {
	quote_hit = 0;
	input_add_character(key);
    } else if (func)
	func(key, ptr);
    else
	term_beep();
}
