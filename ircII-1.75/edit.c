/*
 * edit.c: Does command line parsing, etc 
 *
 *
 * Written by Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserverd 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include "irc.h"

/* used with move_cursor */
#define RIGHT 1
#define LEFT 0

#define INPUT_WRAP_DIST 10

#define INPUT_BUFFER_SIZE 256

static char meta1_hit = 0;
static char meta2_hit = 0;
static char quote_hit = 0;

/* recv_nick: the nickname of the last person to send you a privmsg */
char *recv_nick = null(char *);

/* sent_nick: the nickname of the last person to whom you sent a privmsg */
char *sent_nick = null(char *);

/* query_nick: the person who was specified in a QUERY command */
char *query_nick = null(char *);

/* a few advance declarations */
extern void edit_char();
extern void quit();
extern void send_com();
extern void clear();
extern void quote();
extern void privmsg();
extern void flush();
extern void alias();
extern void server();
extern void away();
extern void query();
extern void exec();
extern void ignore();
extern void who();
extern void info();
extern void nick();
extern void comment();
extern void bindcmd();
extern void history();

/*
 * irc_command: all the availble irc commands:  Note that the first entry has
 * a zero length string name and a null server command... this little trick
 * make "/ blah blah blah" always get sent to a channel, bypassing queries,
 * etc.  Neato.  This list MUST be sorted and the NUMBER_OF_COMMANDS define
 * must represent the number of commands in this list 
 */
IrcCommand irc_command[] =
{
 "", null(char *), 0, send_text,
 "ADMIN", "ADMIN", 0, send_com,
 "ALIAS", "ALIAS", 0, alias,
 "AWAY", "AWAY", 0, away,
 "BIND", "BIND", 0, bindcmd,
 "BYE", "QUIT", 0, quit,
 "CHANNEL", "CHANNEL", 0, send_com,
 "CLEAR", "CLEAR", 0, clear,
 "COMMENT", "COMMENT", 0, comment,
 "CONNECT", "CONNECT", 1, send_com,
 "DATE", "TIME", 0, send_com,
 "DIE", "DIE", 1, send_com,
 "EXEC", "EXEC", 0, exec,
 "EXIT", "QUIT", 0, quit,
 "FLUSH", "FLUSH", 0, flush,
 "HELP", "HELP", 0, help,
 "HISTORY", "HISTORY", 0, history,
 "IGNORE", "IGNORE", 0, ignore,
 "INFO", "INFO", 0, info,
 "INVITE", "INVITE", 0, send_com,
 "JOIN", "CHANNEL", 0, send_com,
 "KILL", "KILL", 1, send_com,
 "LASTLOG", "LASTLOG", 0, lastlog,
 "LINKS", "LINKS", 0, send_com,
 "LIST", "LIST", 0, send_com,
 "LOAD", "LOAD", 0, load,
 "LUSERS", "LUSERS", 0, send_com,
 "MOTD", "MOTD", 0, send_com,
 "MSG", "PRIVMSG", 0, privmsg,
 "NAMES", "NAMES", 0, send_com,
 "NICK", "NICK", 0, nick,
 "OPER", "OPER", 0, send_com,
 "QUERY", "QUERY", 0, query,
 "QUIT", "QUIT", 0, quit,
 "QUOTE", "QUOTE", 0, quote,
 "REHASH", "REHASH", 1, send_com,
 "SERVER", "SERVER", 0, server,
 "SET", "SET", 0, set_variable,
 "SIGNOFF", "QUIT", 0, quit,
 "STATS", "STATS", 0, send_com,
 "SUMMON", "SUMMON", 0, send_com,
 "TIME", "TIME", 0, send_com,
 "TOPIC", "TOPIC", 0, send_com,
 "TRACE", "TRACE", 1, send_com,
 "USERS", "USERS", 0, send_com,
 "VERSION", "VERSION", 0, send_com,
 "WALL", "WALL", 1, send_com,
 "WHO", "WHO", 0, who,
 "WHOIS", "WHOIS", 0, send_com,
 "WHOWAS", "WHOWAS", 0, send_com,
 null(char *), null(char *), 0, NULL,
};

/*
 * find_command: looks for the given name in the command list, returning a
 * pointer to the first match and the number of matches in cnt.  If no
 * matches are found, null is returned (as well as cnt being 0). The command
 * list is sorted, so we do a binary search.  The returned commands always
 * points to the first match in the list.  If the oper flag is set, then all
 * commands are considered in the cnt, even operator commands.  If oper is
 * false, only user commands are counted. 
 */
IrcCommand *find_command(com, cnt, oper)
char *com;
int *cnt;
char oper;
{
    int len;

    if (com && (len = strlen(com))) {
	int min,
	    max,
	    pos,
	    old_pos,
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
		*cnt = 1;
	    return (&(irc_command[min]));
	} else
	    return (null(IrcCommand *));
    } else {
	*cnt = 1;
	return (irc_command);
    }
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
    if (args = check_nickname(args)) {
	strncpy(nickname, args, NICKNAME_LEN);
	send_to_server("NICK %s", args);
	make_status();
    } else
	put_it("*** Bad nickname");
}

/* info: does the /INFO command.  I just added some credits */
static void info(command, args)
char *command,
    *args;
{
    put_it("*** IRC II:  Written by Michael Sandrof (ms5n@andrew.cmu.edu)");
    put_it("*** IRC II:  HP-UX modifications by Mark T. Dame (Mark.Dame@uc.edu)");
    send_to_server(command);
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
    char *arg1,
        *arg2,
        *arg3,
         c;

    arg1 = next_arg(args, &args);
    arg2 = next_arg(args, &args);
    arg3 = next_arg(args, &args);
    who_mask = 0;
    if (arg1) {
	if (*arg1 == '-') {
	    int len;

	    c = *(arg1 + 1);
	    if (isdigit(c))
		send_com(command, arg1);
	    else {
		if ((len = strlen(++arg1)) == 0) {
		    put_it("*** Unknown or missing flag");
		    return;
		}
		if (strnicmp(arg1, "operators", len) == 0) {
		    who_mask |= WHO_OPS;
		    arg3 = arg2;
		} else if (strnicmp(arg1, "name", len) == 0) {
		    who_mask |= WHO_NAME;
		    malloc_strcpy(&who_info, arg2);
		} else if (strnicmp(arg1, "host", len) == 0) {
		    who_mask |= WHO_HOST;
		    malloc_strcpy(&who_info, arg2);
		} else if (strnicmp(arg1, "server", len) == 0) {
		    who_mask |= WHO_SERVER;
		    malloc_strcpy(&who_info, arg2);
		} else if (strnicmp(arg1, "all", len) == 0) {
		    who_mask |= WHO_ALL;
		    malloc_strcpy(&who_nick, arg2);
		    malloc_strcpy(&who_info, arg2);
		} else {
		    put_it("*** Unknown or missing flag");
		    return;
		}
		if (arg3) {
		    if (is_number(arg3)) {
			if (atoi(arg3) == 0)
			    who_mask |= WHO_ZERO;
			send_com(command, arg3);
		    } else {
			if (strcmp(arg3, "*")) {
			    who_mask |= WHO_NICK;
			    malloc_strcpy(&who_nick, arg3);
			    send_com(command, "");
			} else
			    send_com(command, arg3);
		    }
		} else
		    send_com(command, "");
	    }
	    return;
	}
	if (is_number(arg1)) {
	    if (atoi(arg1) == 0)
		who_mask |= WHO_ZERO;
	    send_com(command, arg1);
	} else {
	    if (strcmp(arg1, "*")) {
		who_mask |= WHO_NICK;
		malloc_strcpy(&who_nick, arg1);
		send_com(command, "");
	    } else
		send_com(command, arg1);
	}
	return;
    }
    send_com(command, args);
}

/*
 * ignore: does the /IGNORE command.  Figures out what type of ignoring the
 * user wants to do and calls the proper ignorance command to do it. 
 */
static void ignore(command, args)
char *command,
    *args;
{
    char *nick,
        *type;

    if (nick = next_arg(args, &args)) {
	if (type = next_arg(args, &args)) {
	    int len;

	    upper(type);
	    len = strlen(type);
	    if (strncmp(type, "ALL", len) == 0) {
		ignore_nickname(nick, IGNORE_ALL);
		put_it("*** Ignoring all messages from %s", nick);
	    } else if (strncmp(type, "PRIVATE", len) == 0) {
		ignore_nickname(nick, IGNORE_PRIV);
		put_it("*** Ignoring private messages from %s", nick);
	    } else if (strncmp(type, "PUBLIC", len) == 0) {
		ignore_nickname(nick, IGNORE_PUB);
		put_it("*** Ignoring public messages from %s", nick);
	    } else if (strncmp(type, "NONE", len) == 0) {
		if (remove_ignore(nick))
		    put_it("*** %s is not in the ignorance list!", nick);
		else
		    put_it("*** %s removed from ignorance list", nick);
	    } else
		put_it("*** Unknown designation: Use ALL, PRIVATE, PUBLIC, NONE");
	} else {
	    ignore_list(nick);
	}
    } else
	ignore_list(null(char *));
}

/*
 * exec: the /EXEC command.  Allows a single process at a time to be started,
 * sending its stdout and stderr to the descriptors process_stdout and
 * process_stderr, where they will eventually be read in the Main Loop.  
 */
static void exec(command, args)
char *command,
    *args;
{
    static int pid;
    static char *proc_name = null(char *);
    int p1[2],
        p2[2];
    char *flag;

    if (*args == null(char)) {
	if ((process_stdout == -1) && (process_stderr == -1))
	    put_it("*** No process is currently running");
	else
	    put_it("*** Currently running pid %d: %s", pid, proc_name);
	return;
    }
    if (*args == '-') {
	if (*(++args) != '-') {
	    if (flag = next_arg(args, &args)) {
		int len;

		len = strlen(flag);
		if (strnicmp(flag, "KILL", len) == 0) {
		    if ((process_stdout != -1) || (process_stderr != -1)) {
			kill(pid, SIGKILL);	/* gets waited on in main() */
			close(process_stdout);
			close(process_stderr);
			process_stdout = -1;
			process_stderr = -1;
			put_it("*** Killing pid %d: %s", pid, proc_name);
		    } else
			put_it("*** No process to kill");
		    return;
		} else if (strnicmp(flag, "OUT", len) == 0) {
		    free(process_who);
		    process_redirect = 1;
		    process_who = null(char *);
		} else if (strnicmp(flag, "MSG", len) == 0) {
		    process_redirect = 1;
		    if (flag = next_arg(args, &args))
			malloc_strcpy(&process_who, flag);
		    else {
			put_it("*** EXEC: no nicknames specified.");
			return;
		    }
		} else {
		    put_it("*** EXEC: unknown flag: use -KILL, -OUT, or -MSG");
		    return;
		}
	    } else {
		put_it("*** EXEC: missing flag: use -KILL, -OUT, or -MSG");
		return;
	    }
	} else
	    process_redirect = 0;
    } else
	process_redirect = 0;
    if ((process_stdout == -1) && (process_stderr == -1)) {
	process_counter = 0;
	pipe(p1);
	pipe(p2);
	malloc_strcpy(&proc_name, args);
	switch (pid = fork()) {
	    case -1:
		put_it("*** Couldn't start new process!");
		break;
	    case 0:
		dup2(p1[1], 1);
		dup2(p2[1], 2);
		close(p1[0]);
		close(p2[0]);
		close(0);
		execl(get_string_var(SHELL_VAR),
		      get_string_var(SHELL_VAR),
		      get_string_var(SHELL_FLAGS_VAR),
		      args, null(char *));
		put_it("*** Error execing process!");
		_exit(0);
		break;
	    default:
		close(p1[1]);
		close(p2[1]);
		process_stdout = p1[0];
		process_stderr = p2[0];
		break;
	}
    } else
	put_it("*** A process is already running");
}
/*
 * query: the /QUERY command.  Works much like the /MSG, I'll let you figure
 * it out. 
 */
static void query(command, args)
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
	malloc_strcpy(&query_nick, nick);
	put_it("*** Starting conversation with %s", query_nick);
    } else {
	if (query_nick) {
	    put_it("*** Ending conversation with %s", query_nick);
	    free(query_nick);
	    query_nick = null(char *);
	} else
	    put_it("*** You aren't querying anyone!");
    }
    make_status();
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
    else {
	free(away_message);
	away_message = null(char *);
    }
    make_status();
    send_to_server("%s %s", command, args);
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

    if (server = next_arg(args, &args)) {
	if (port = next_arg(args, &args))
	    connect_to_server(server, atoi(port));
	else
	    connect_to_server(server, irc_port);
    } else
	put_it("*** No server specified");
}

/* quit: The /QUIT, /EXIT, etc command */
static void quit(command, args)
char *command,
    *args;
{
    put_it("*** Ending IRC session");
    send_com(command, args);
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
	    if (*name == '-')
		delete_alias(name + 1);
	    else
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
    put_it("*** Flushing server output...");
    flush_server();
    put_it("*** All server output has been flushed");
}

/*
 * privmsg: The MSG command, displaying a message on the screen indicating
 * the message was sent 
 */
static void privmsg(command, args)
char *command,
    *args;
{
    char *nick,
        *rest,
        *thing;

    if (nick = next_arg(args, &rest)) {
	if (strcmp(nick, ".") == 0) {
	    if ((nick = sent_nick) == null(char *))
		nick = "";
	    thing = "##";
	} else if (strcmp(nick, ",") == 0) {
	    if ((nick = recv_nick) == null(char *))
		nick = "";
	    thing = "!!";
	} else
	    thing = "";
	send_to_server("%s %s :%s", command, nick, rest);
	put_it("-> %s*%s* %s", thing, nick, rest);
	if (get_int_var(WARN_OF_IGNORES_VAR) &&
	    is_ignored(nick, IGNORE_PRIV, 0))
	    put_it("*** Warning: You are ignoring private messages from %s", nick);
	malloc_strcpy(&sent_nick, nick);
    } else
	put_it("*** No nickname specified");
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
    if (!dumb) {
	erase_display();
	refresh_screen();
    }
}

/*
 * send_com: the generic command function.  Uses the full command name fount
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
 * it's the current channel 
 */
void send_text(nick, line)
char *nick;
char *line;
{
    if (nick) {
	put_it("-> *%s* %s", nick, line);
	send_to_server("PRIVMSG %s :%s", nick, line);
    } else {
	put_it("> %s", line);
	send_to_server("MSG :%s", line);
    }
}

/*
 * parse_command: parses a line of input from the user.  If the first
 * character of the line is equal to irc_variable[CMDCHAR_VAR].value, the
 * line is used as an irc command and parsed appropriately.  If the first
 * character is anything else, the line is sent to the current channel using
 * the MSG command 
 */
void parse_command(org_line)
char *org_line;
{
    char *ptr = null(char *),
        *free_line,
        *line = null(char *);

    if (get_int_var(INLINE_ALIASES))
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
	if (*line == get_int_var(CMDCHAR_VAR)) {
	    char *com,
	        *rest,
	        *alias;
	    int cnt;
	    IrcCommand *command = null(IrcCommand *);

	    if (com = next_arg(line, &rest)) {
		upper(com++);
		/* first, check aliases */
		if (*com != get_int_var(CMDCHAR_VAR)) {
		    if (alias = get_alias(com, rest)) {
			if (mark_alias(com, 1)) {
			    put_it("*** Illegal recursive alias: %s", com);
			    continue;
			}
			parse_command(alias);
			free(alias);
			mark_alias(com, 0);
			continue;
		    }
		} else
		    com++;
		if ((command = find_command(com, &cnt, operator)) == null(IrcCommand *))
		    put_it("*** Unknown command: %s", com);
		else if (cnt > 1)
		    put_it("*** Ambiguous command: %s", com);
		else
		    (command->func) (command->server_func, rest);
	    }
	} else
	    send_text(query_nick, line);
    }
    free(free_line);
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
        *expanded;
    struct stat stat_buf;

    if (filename = next_arg(args, &args)) {
	if (expanded = expand_twiddle(filename)) {
	    if (stat(expanded, &stat_buf) == 0) {
		if (stat_buf.st_mode & S_IFDIR) {
		    put_it("*** %s is a directory", expanded);
		    return;
		}
		if (stat_buf.st_mode & 0111) {
		    put_it("*** %s is executable and may not be loaded", expanded);
		    return;
		}
	    }
	    if (fp = fopen(expanded, "r")) {
		while (fgets(buffer, BUFFER_SIZE, fp)) {
		    *(buffer + strlen(buffer) - 1) = null(char);
		    parse_command(buffer);
		}
		fclose(fp);
	    } else
		put_it("*** Couldn't open %s: %s", expanded, sys_errlist[errno]);
	} else
	    put_it("*** Unknown user");
    } else
	put_it("*** No filename specified");
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

void forward_character()
{
         move_cursor(RIGHT);
}

void backward_character()
{
         move_cursor(LEFT);
}

void backward_history()
{
         get_history(PREV);
}

void forward_history()
{
         get_history(NEXT);

}

void toggle_insert_mode()
{
    /*
     * Yes, this could be done quicker and easier, but it wouldn't look as
     * nice 
     */
         set_var_value(INSERT_MODE_VAR, "TOGGLE");
}

void send_line()
{
    char *ptr;
    server_paused = 0;
    ptr = get_input();
    add_to_history(ptr);
    parse_command(ptr);
    set_input("");
    update_input(UPDATE_ALL);
}

void help_char(c)
char c;
{
    char *ptr;

    ptr = get_input();
    help_line(ptr);
}

void toggle_stop_screen()
{
         server_paused = !server_paused;
}

void meta2_char()
{
         meta2_hit = 1;
}

void meta1_char()
{
         meta1_hit = 1;
}

void quote_char(c)
char c;
{
    quote_hit = 1;
}

void type_text(c, str)
char c,
    *str;
{
    for (; *str; str++)
	add_ch(*str);
}

/* parse_text: the bindable function that executes its string */
void parse_text(c, str)
char c,
    *str;
{
    parse_command(str);
}

/*
 * edit_char: handles each character for an input stream.  Not too difficult
 * to work out.  Perhaps in the near future, this will be a user-redefinable
 * keymap.  Key lookup would speedup, that's fer sure. Why not do this, you
 * ask?  Cause I have other things to do right now. It's on my TODO list 
 */
void edit_char(key)
char key;
{
    void (*func) ();
    char *ptr;

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
	add_ch(key);
    } else if (func)
	func(key, ptr);
    fflush(stdout);
}
