/*
 * numbers.c:handles all those strange numeric response dished out by that
 * wacky, nutty program we call ircd 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "input.h"
#include "ircaux.h"
#include "vars.h"
#include "lastlog.h"
#include "hook.h"

extern char ignore_names;

/*
 * display_msg: Handles the displaying of messages from the variety of
 * possible formats that the irc server spits out.  You'd think someone would
 * simplify this 
 */
static void display_msg(from, rest)
char *from,
    *rest;
{
    char *ptr;

    if (strnicmp(current_server_name(null(char *)), from,
		 strlen(current_server_name(null(char *)))) == 0)
	from = null(char *);
    if (ptr = index(rest, ':')) {
	*(ptr++) = null(char);
	if (strlen(rest)) {
	    if (from)
		put_it("*** %s: %s (from %s)", rest, ptr, from);
	    else
		put_it("*** %s: %s", rest, ptr);
	} else {
	    if (from)
		put_it("*** %s (from %s)", ptr, from);
	    else
		put_it("*** %s", ptr);
	}
    } else {
	if (from)
	    put_it("*** %s (from %s)", rest, from);
	else
	    put_it("*** %s", rest);
    }
}

/*
 * password_sendline: called by irc_io() in get_password() to handle hitting
 * of the return key, etc 
 */
static void password_sendline()
{
         current_server_password(get_input());
    irc_io_loop = 0;
}

/*
 * get_password: When a host responds that the user needs to supply a
 * password, it gets handled here!  The user is prompted for a password and
 * then reconnection is attempted with that password.  But, the reality of
 * the situation is that no one really uses user passwords.  Ah well 
 */
static void get_password()
{
         put_it("*** Password required for connection to server %s",
		     current_server_name(null(char *)));
    close_server();
    term_echo(0);
    if (irc_io("Password:", password_sendline)) {
	term_echo(1);
	return;
    }
    get_connected();
    term_echo(1);
}

static void nickname_sendline()
{
    char *nick;

    nick = get_input();
    if (nick = check_nickname(nick)) {
	if (stricmp(nick, old_nickname))
	    send_to_server("NICK %s", nick);
	strncpy(nickname, nick, NICKNAME_LEN);
	irc_io_loop = 0;
    } else
	put_it("*** Illegal nickname, try again");
}

/*
 * reset_nickname: When the server reports that the selected nickname is not
 * a good one, it gets reset here. 
 */
static void reset_nickname(from, rest)
char *from,
    *rest;
{
    static char in_it = 0;

    if (in_it)
	return;
    in_it = 1;
    display_msg(from, rest);
    put_it("*** You have specified an illegal nickname");
    put_it("*** Please enter your nickname");
    if (irc_io("Nickname: ", nickname_sendline)) {
	in_it = 0;
	return;
    }
    update_all_status();
    in_it = 0;
}

static not_valid_channel(from, rest)
char *from,
    *rest;
{
    char *channel;

    if (channel = next_arg(rest, &rest)) {
	remove_channel(channel);
	put_it("*** %s%s", channel, rest);
    }
}

static void list(from, line)
char *line;
char *from;
{
    char *channel,
        *user_cnt;
    int min;
    static char format[25];
    static unsigned int last_width = -1;

    channel = next_arg(line, &line);
    if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR)) {
	if (last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR))
	    sprintf(format, "*** %%-%us %%-5s  %%s", (unsigned char) last_width);
	else
	    strcpy(format, "*** %s\t%-5s  %s");
    }
    user_cnt = next_arg(line, &line);
    if (min = get_int_var(LIST_NO_SHOW)) {
	if ((*channel == '*') && (atoi(user_cnt) <= min) && (*(line + 1) == null(char)))
	    return;
    }
    sprintf(buffer, "%s %s", user_cnt, line + 1);
    if (do_hook(LIST_LIST, channel, buffer)) {
	if (last_width && (strlen(channel) > last_width))
	    channel[last_width] = null(char);
	if (channel && user_cnt) {
	    if (*channel == '*')
		put_it(format, "Prv", user_cnt, line + 1);
	    else
		put_it(format, channel, user_cnt, line + 1);
	}
    }
}

static void version(from, rest)
char *from,
    *rest;
{
    char *host,
        *version;

    if (host = index(rest, ':')) {
	*(host++) = null(char);
	put_it("*** Host %s is running server %s", host, rest);
    } else {
	if (version = next_arg(rest, &host))
	    put_it("*** Host %s is running server %s", host, version);
    }
}

static void invite(from, rest)
char *from,
    *rest;
{
    char *who,
        *channel;

    if (who = next_arg(rest, &channel)) {
	message_from(channel, LOG_CRAP);
	put_it("*** Inviting %s to channel %s", who, channel);
    }
    message_from(null(char *), LOG_CRAP);
}

static void mode(from, rest)
char *from,
    *rest;
{
    put_it("*** Channel mode is \"%s\"", rest);
}

/*
 * numbered_command: does (hopefully) the right thing with the numbered
 * responses from the server.  I wasn't real careful to be sure I got them
 * all, but the default case should handle any I missed (sorry) 
 */
void numbered_command(from, comm, rest)
char *from;
int comm;
char *rest;
{
    char *user;

    if (*rest == ' ') {
	user = null(char *);
	rest++;
    } else {
	user = rest;
	if ((rest = index(user, ' ')) == null(char *))
	    return;
	*(rest++) = null(char);
    }
    /* Protect from numbered commands from non-servers */
    if (index(from, '.')) {
	switch (comm) {
	    case 315:		/* end of who list */
	    case 323:		/* end of list */
	    case 365:		/* end of links list */
	    case 366:		/* end of names list */
		if (ignore_names)
		    ignore_names--;
		else {
		    if (get_int_var(SHOW_END_OF_MSGS_VAR))
			display_msg(from, rest);
		}
		break;
	    case 324:		/* MODE reply */
		mode(from, rest);
		break;
	    case 341:		/* invite confirmation */
		invite(from, rest);
		break;
	    case 301:		/* user is away */
		user_is_away(from, rest);
		break;
	    case 311:		/* whois name info */
		whois_name(rest);
		break;
	    case 312:		/* whois host/server info */
		whois_server(rest);
		break;
	    case 314:
		whowas_name(rest);	/* whowas name info */
		break;
	    case 313:		/* whois operator info */
		whois_oper(rest);
		break;
	    case 317:
		whois_lastcom(rest);
		break;
	    case 316:
		whois_chop(rest);
		break;
	    case 332:		/* topic */
		message_from(null(char *), LOG_CURRENT);
		put_it("*** Topic: %s", rest + 1);
		message_from(null(char *), LOG_CRAP);
		break;
	    case 321:		/* list header line */
		/* put_it("*** Chn        Users  Topic"); */
		break;
	    case 322:		/* list entries */
		list(from, rest);
		break;
	    case 351:		/* version response */
		version(from, rest);
		break;
	    case 401:		/* no such nickname */
		no_such_nickname(from, rest);
		break;
	    case 451:		/* not registered as a user */
		/*
		 * Sometimes the server doesn't catch the USER line, so here
		 * we send a simplified version again  -lynx 
		 */
		send_to_server("USER %s . . :%s", username, realname);
		break;
	    case 433:		/* nickname in use */
	    case 432:		/* bad nickname */
		reset_nickname(from, rest);
		break;
	    case 464:		/* invalid password */
		if (oper_command)
		    display_msg(from, rest);
		else
		    get_password();
		break;
	    case 421:		/* unknown command */
		if (strncmp("MODE", rest, 4) && strncmp("KICK", rest, 4))
		    display_msg(from, rest);
		break;
	    case 403:		/* not a valid channel: for 2.5/2.6
				 * compatability */
		not_valid_channel(from, rest);
		break;
	    case 381:		/* /oper response ok */
		operator = 1;
		update_all_status();	/* fix the status line */
	    default:
		display_msg(from, rest);
		break;
	}
    } else
	display_msg(from, rest);
}
