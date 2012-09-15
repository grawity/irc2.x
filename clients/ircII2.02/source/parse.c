/*
 * parse.c: handles messages from the server.   Believe it or not.  I
 * certainly wouldn't if I were you. 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "irc.h"
#include "names.h"
#include "vars.h"
#include "hook.h"
#include "edit.h"
#include "ignore.h"
#include "whois.h"
#include "lastlog.h"
#include "ircaux.h"
#include "crypt.h"
#include "term.h"

#define STRING_CHANNEL '+'
#define MULTI_CHANNEL '#'

/*
 * joined_nick: the nickname of the last person who joined the current
 * channel 
 */
char *joined_nick = null(char *);

/* public_nick: nick of the last person to send a message to your channel */
char *public_nick = null(char *);

char ignore_names = 0;

/*
 * is_channel: determines if the argument is a channel.  If it's a number,
 * begins with MULTI_CHANNEL, or STRING_CHANNEL, then its a channel 
 */
int is_channel(to)
char *to;
{
    return (isdigit(*to) || (*to == STRING_CHANNEL) ||
	    (*to == MULTI_CHANNEL) || (*to == '-'));
}

/* beep_em: Not hard to figure this one out */
void beep_em(beeps)
int beeps;
{
    int cnt,
        i;

    for (cnt = beeps, i = 0; i < cnt; i++)
	term_beep();
}

/* in response to a TOPIC message from the server */
static void topic(from, line)
char *from,
    *line;
{
    message_from(null(char *), LOG_CURRENT);
    if (*line == ':')
	line++;
    if (do_hook(TOPIC_LIST, from, line))
	put_it("*** %s has changed the topic to \"%s\"", from, line);
    message_from(null(char *), LOG_CRAP);
}

static void linreply(line)
char *line;
{
    put_it("*** %s", line);
}

static void wall(from, line)
char *from,
    *line;
{
    int flag,
        level;
    char *left,
        *right;

    message_from(from, LOG_WALL);
    flag = is_ignored(from, IGNORE_WALLS);
    if (flag != IGNORED) {
	if (flag == HIGHLIGHTED) {
	    left = "\002#";
	    right = "#\002";
	} else
	    left = right = "#";
	if (ignore_usernames & IGNORE_WALLS)
	    add_to_whois_queue(from, whois_ignore_walls, "%s", line + 1);
	else {
	    level = set_lastlog_msg_level(LOG_WALL);
	    if (do_hook(WALL_LIST, from, line + 1))
		put_it("%s%s%s %s", left, from, right, line + 1);
	    set_lastlog_msg_level(level);
	}
    }
    message_from(null(char *), LOG_CRAP);
}

static void wallops(from, line)
char *from,
    *line;
{
    add_to_whois_queue(from, whois_new_wallops, line + 1);
}

static void namreply(line)
char *line;
{
    char *type,
        *nick,
        *channel;
    static char format[25];
    static unsigned int last_width = -1;

    type = next_arg(line, &line);
    channel = next_arg(line, &line);
    if (ignore_names) {
	if (get_int_var(SHOW_CHANNEL_NAMES_VAR))
	    put_it("*** Users on %s:", current_channel(null(char *)));
	while (nick = next_arg(line, &line)) {
	    add_to_channel(channel, nick);
	    if (get_int_var(SHOW_CHANNEL_NAMES_VAR))
		put_it("***\t%s", nick);
	}
	return;
    }
    if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR)) {
	if (last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR))
	    sprintf(format, "%%s: %%-%us %%s", (unsigned char) last_width);
	else
	    strcpy(format, "%s: %s\t%s");
    }
    if (type && channel) {
	switch (*type) {
	    case '=':
		if (do_hook(NAMES_LIST, channel, line)) {
		    if (last_width && (strlen(channel) > last_width))
			channel[last_width] = null(char);
		    put_it(format, "Pub", channel, line);
		}
		break;
	    case '*':
		if (do_hook(NAMES_LIST, channel, line)) {
		    if (last_width && (strlen(channel) > last_width))
			channel[last_width] = null(char);
		    put_it(format, "Prv", channel, line);
		}
		break;
	}
    }
}

static void whoreply(from, line)
char *line,
    *from;
{
    static char format[40];
    static int last_width = -1;
    char *channel,
        *user,
        *host,
        *server,
        *nick,
        *stat,
        *name,
         ok = 1;

    if (last_width != get_int_var(CHANNEL_NAME_WIDTH_VAR)) {
	if (last_width = get_int_var(CHANNEL_NAME_WIDTH_VAR))
	    sprintf(format, "%%-%us %%-9s %%-3s %%s@%%s (%%s)", (unsigned char) last_width);
	else
	    strcpy(format, "%s\t%-9s %-3s %s@%s (%s)");
    }
    channel = next_arg(line, &line);
    user = next_arg(line, &line);
    host = next_arg(line, &line);
    server = next_arg(line, &line);
    nick = next_arg(line, &line);
    stat = next_arg(line, &line);
    if (*stat == 'S') {		/* this only true for the header WHOREPLY */
	sprintf(buffer, "%s %s %s %s %s", nick, stat, user, host, line + 1);
	channel = "Channel";
	if (do_hook(WHO_LIST, channel, buffer)) {
	    if (last_width && (strlen(channel) > last_width))
		channel[last_width] = null(char);
	    put_it(format, channel, nick, stat, user, host, line + 1);
	}
	return;
    }
    if (name = index(line, ':'))
	name++;
    else
	name = line;
    if (who_mask) {
	if (who_mask == WHO_ALL) {
	    ok = (wild_match(who_name, user) ||
		  wild_match(who_host, host) ||
		  wild_match(who_server, server) ||
		  wild_match(who_nick, nick) ||
		  wild_match(who_name, user) ||
		  wild_match(who_nick, name));
	} else {
	    if (who_mask & WHO_OPS)
		ok = ok && (*(stat + 1) == '*');
	    if (who_mask & WHO_CHOPS)
		ok = ok && ((*(stat + 1) == '@') || (*(stat + 2) == '@'));
	    if (who_mask & WHO_NAME)
		ok = ok && wild_match(who_name, user);
	    if (who_mask & WHO_HOST)
		ok = ok && wild_match(who_host, host);
	    if (who_mask & WHO_SERVER)
		ok = ok && wild_match(who_server, server);
	    if (who_mask & WHO_NICK)
		ok = ok && wild_match(who_nick, nick);
	}
	if (who_mask & WHO_ZERO) {
	    if (who_mask == WHO_ZERO)
		ok = (atoi(channel) == 0);
	    else
		ok = (ok && (atoi(channel) == 0));
	}
    }
    if (ok) {
	sprintf(buffer, "%s %s %s %s %s", nick, stat, user, host, name);
	if (do_hook(WHO_LIST, channel, buffer)) {
	    if (last_width && (strlen(channel) > last_width))
		channel[last_width] = null(char);
	    put_it(format, channel, nick, stat, user, host, name);
	}
    }
}

/*
 * parse_mail: handles the parsing of irc mail messages which are sent as
 * NOTICES.  The notice() function determines which notices are mail messages
 * and send that info to parse_mail() 
 */
static void parse_mail(server, line)
char *server;
char *line;
{
    char *date,
        *nick,
        *flags,
        *high,
        *name,
        *message;
    int ign1,
        ign2,
        level;
    long time;
    
    flags = next_arg(line, &date);	/* what to do with these flags */
    nick = next_arg(date, &date);
    name = next_arg(date, &date);
    if (message = index(date, '*'))
	*message = null(char);
    if (((ign1 = is_ignored(nick, IGNORE_MAIL)) == IGNORED) ||
	((ign2 = is_ignored(name, IGNORE_MAIL)) == IGNORED))
	return;
    if ((ign1 == HIGHLIGHTED) || (ign2 == HIGHLIGHTED))
	high = "\002";
    else
	high = "";
    time = atol(date);
    date = ctime(&time);
    date[24] = null(char);
    sprintf(buffer, "%s %s %s %s %s", name, flags, date, server, message + 2);
    level = set_lastlog_msg_level(LOG_MAIL);
    if (do_hook(MAIL_IRC_LIST, nick, buffer)) {
	put_it("You have mail from %s (%s) %s", nick, name, flags);
	put_it("It was queued %s from server %s", date, server);
	put_it("%s[%s]%s %s", high, nick, high, message + 2);
    }
    set_lastlog_msg_level(level);
}

static void notice(from, line)
char *from;
char *line;
{
    int level,
        type;
    char *to;
    int user_cnt,
        server_cnt;
    char server[81],
         version[21];
    static char just_a_flag = 0;

    if (*line != ' ') {
	if (to = next_arg(line, &line)) {
	    if (is_channel(to)) {
		message_from(to, LOG_NOTICE);
		type = PUBLIC_NOTICE_LIST;
	    } else {
		message_from(from, LOG_NOTICE);
		type = NOTICE_LIST;
	    }
	    if (from && *from) {
		int flag;
		char *high;

		flag = is_ignored(from, IGNORE_NOTICES);
		if (flag != IGNORED) {
		    if (flag == HIGHLIGHTED)
			high = "\002";
		    else
			high = empty_string;
		    if (index(from, '.')) {	/* only dots in servernames,
						 * right? */
			line++;
			if (strncmp(line, "*/", 2)) {
			    level = set_lastlog_msg_level(LOG_NOTICE);
			    if (type == NOTICE_LIST) {
				if (do_hook(type, from, line))
				    put_it("%s-%s-%s %s", high, from, high, line);
			    } else {
				sprintf(buffer, "%s %s", to, line);
				if (do_hook(type, from, buffer))
				    put_it("%s-%s:%s-%s %s", high, from, to, high, line);
			    }
			    set_lastlog_msg_level(level);
			} else
			    parse_mail(from, line + 1);
		    } else {
			if (ignore_usernames & IGNORE_NOTICES)
			    add_to_whois_queue(from, whois_ignore_notices,
					       "%s", line + 1);
			else {
			    level = set_lastlog_msg_level(LOG_NOTICE);
			    if (type == NOTICE_LIST) {
				if (do_hook(type, from, line + 1))
				    put_it("%s-%s-%s %s", high, from, high, line + 1);
			    } else {
				sprintf(buffer, "%s %s", to, line + 1);
				if (do_hook(type, from, buffer))
				    put_it("%s-%s:%s-%s %s", high, from, to, high, line + 1);
			    }
			    set_lastlog_msg_level(level);
			}
		    }
		}
	    } else {
		put_it("%s", line + 1);
		if (sscanf(line + 1, "*** There are %d users on %d servers",
			   &user_cnt, &server_cnt) == 2) {
		    if ((server_cnt < get_int_var(MINIMUM_SERVERS_VAR)) ||
			(user_cnt < get_int_var(MINIMUM_USERS_VAR))) {
			put_it("*** Trying better populated server...");
			close_server();
			get_connected(current_server_index() + 1);
		    } else if (just_a_flag) {
			just_a_flag = 0;
			/* put this in server.c when irc2.6 is everywhere */
			reconnect_all_channels();
			update_all_status();
			do_hook(CONNECT_LIST, current_server_name(null(char *)), empty_string);
		    }
		} else if (sscanf(line + 1, "*** Your host is %80[^,], running version %20s", server, version) == 2) {
		    just_a_flag = 1;
		    if (strncmp(version, "2.5", 3) == 0)
			irc2_6 = 0;
		    else
			irc2_6 = 1;
		}
	    }
	}
    } else
	put_it("%s", line + 2);
    message_from(null(char *), LOG_CRAP);
}

static void privmsg(from, line)
char *from,
    *line;
{
    int level,
        list_type,
        ignore_type,
        log_type;
    char *ptr,
        *key,
        *to;
    char *high,
        *crypt_who,
         crypted;

    if ((to = next_arg(line, &ptr)) == null(char *))
	return;
    ptr++;
    if (is_channel(to)) {
	crypt_who = to;
	message_from(to, LOG_MSG);
	malloc_strcpy(&public_nick, from);
	if (!is_on_channel(to, from)) {
	    log_type = LOG_PUBLIC;
	    ignore_type = IGNORE_PUBLIC;
	    list_type = PUBLIC_MSG_LIST;
	} else {
	    log_type = LOG_PUBLIC;
	    ignore_type = IGNORE_PUBLIC;
	    if (is_current_channel(to, 0))
		list_type = PUBLIC_LIST;
	    else
		list_type = PUBLIC_OTHER_LIST;
	}
    } else {
	crypt_who = from;
	message_from(from, LOG_MSG);
	log_type = LOG_MSG;
	ignore_type = IGNORE_MSGS;
	list_type = MSG_LIST;
    }
    switch (is_ignored(from, ignore_type)) {
	case IGNORED:
	    if ((list_type == MSG_LIST) && get_int_var(SEND_IGNORE_MSG_VAR))
		send_to_server("NOTICE %s %s is ignoring you", from, nickname);
	    return;
	case HIGHLIGHTED:
	    high = "\002";
	    break;
	default:
	    high = empty_string;
	    break;
    }
    if (list_type == MSG_LIST) {
	malloc_strcpy(&recv_nick, from);
	/* Thank Dave Bolen and xyzzy */
	if ((strcmp(ptr, ". .") == 0) && (strcmp(from, "BigCheese") == 0)) {
	    send_to_server("NOTICE %s IRCII %s", from, irc_version);
	    return;
	}
    }
    crypted = !strncmp(ptr, CRYPT_HEADER, CRYPT_HEADER_LEN);
    if (key = is_crypted(crypt_who)) {
	if (crypted) {
	    if ((ptr = crypt_msg(ptr, key, 0)) == null(char *))
		ptr = "[ENCRYPTED MESSAGE]";
	}
    } else {
	if (crypted)
	    ptr = "[ENCRYPTED MESSAGE]";
    }
    level = set_lastlog_msg_level(log_type);
    if (ignore_usernames & ignore_type)
	add_to_whois_queue(from, whois_ignore_msgs, "%s", ptr);
    else {
	switch (list_type) {
	    case PUBLIC_MSG_LIST:
		sprintf(buffer, "%s %s", to, ptr);
		if (do_hook(list_type, from, buffer))
		    put_it("%s<%s/%s>%s %s", high, from, to, high, ptr);
		break;
	    case MSG_LIST:
		if (away_message) {
		    long t;
		    char *msg = null(char *);

		    t = time(0);
		    msg = (char *) new_malloc(strlen(ptr) + 20);
		    sprintf(msg, "%s <%.16s>", ptr, ctime(&t));
		    if (do_hook(list_type, from, msg))
			put_it("%s*%s*%s %s", high, from, high, msg);
		    new_free(&msg);
		    beep_em(get_int_var(BEEP_WHEN_AWAY_VAR));
		} else {
		    if (do_hook(list_type, from, ptr))
			put_it("%s*%s*%s %s", high, from, high, ptr);
		    beep_em(get_int_var(BEEP_ON_MSG_VAR));
		}
		break;
	    case PUBLIC_LIST:
		if (do_hook(list_type, from, ptr))
		    put_it("%s<%s>%s %s", high, from, high, ptr);
		break;
	    case PUBLIC_OTHER_LIST:
		sprintf(buffer, "%s %s", to, ptr);
		if (do_hook(list_type, from, buffer))
		    put_it("%s<%s:%s>%s %s", high, from, to, high, ptr);
		break;
	}
    }
    set_lastlog_msg_level(level);
    message_from(null(char *), LOG_CRAP);
}

static void msg(from, line)
char *from,
    *line;
{
    char *key,
        *high,
        *channel;
    int crypted,
        log_type;

    switch (is_ignored(from, IGNORE_PUBLIC)) {
	case IGNORED:
	    return;
	case HIGHLIGHTED:
	    high = "\002";
	    break;
	default:
	    high = empty_string;
	    break;
    }
    line++;
    crypted = !strncmp(line, CRYPT_HEADER, CRYPT_HEADER_LEN);
    if (key = is_crypted(current_channel(null(char *)))) {
	if (crypted) {
	    if ((line = crypt_msg(line, key, 0)) == null(char *))
		line = "[ENCRYPTED MESSAGE]";
	}
    } else {
	if (crypted)
	    line = "[ENCRYPTED MESSAGE]";
    }
    channel = real_channel();
    message_from(channel, LOG_PUBLIC);
    malloc_strcpy(&public_nick, from);
    log_type = set_lastlog_msg_level(LOG_PUBLIC);
    if (is_current_channel(channel, 0)) {
	if (do_hook(PUBLIC_LIST, from, line))
	    put_it("%s<%s>%s %s", high, from, high, line);
    } else {
	sprintf(buffer, "%s %s", channel, line);
	if (do_hook(PUBLIC_OTHER_LIST, from, buffer))
	    put_it("%s<%s:%s>%s %s", high, from, channel, high, line);
    }
    set_lastlog_msg_level(log_type);
    message_from(null(char *), LOG_CRAP);
}

static void quit(from, line)
char *from,
    *line;
{
    message_from(null(char *), LOG_CURRENT);
    put_it("*** Signoff: %s", from);
    remove_from_channel(null(char *), from);
    message_from(null(char *), LOG_CRAP);
}

static void pong(from, line)
char *from,
    *line;
{
    char *host;

    if (host = next_arg(line, &line))
	put_it("*** %s: PONG received from %s", host, line);
}

static void error(from, line)
char *from,
    *line;
{
    put_it("*** %s", line + 1);
}

static void channel(from, line)
char *from;
char *line;
{
    char join;

    if (strcmp(line, "0")) {
	join = 1;
	message_from(line, LOG_CRAP);
	malloc_strcpy(&joined_nick, from);
    } else {
	join = 0;
	message_from(real_channel(), LOG_CRAP);
	if (do_hook(LEAVE_LIST, from, real_channel()))
	    put_it("*** %s has left channel %s", from, real_channel());
    }
    if (strcmp(from, nickname) == 0) {
	if (join) {
	    add_channel(line);
	    if (!irc2_6)
		send_to_server("NAMES %s", line);
	    ignore_names++;
	} else
	    remove_channel(real_channel());
    } else {
	if (join)
	    add_to_channel(line, from);
	else
	    remove_from_channel(real_channel(), from);
    }
    if (join) {
	if (do_hook(JOIN_LIST, from, line))
	    put_it("*** %s has joined channel %s", from, line);
    }
    message_from(null(char *), LOG_CRAP);
}

static void invite(from, line)
char *from,
    *line;
{
    char *high;

    switch (is_ignored(from, IGNORE_INVITES)) {
	case IGNORED:
	    if (get_int_var(SEND_IGNORE_MSG_VAR))
		send_to_server("NOTICE %s %s is ignoring you", from, nickname);
	    return;
	case HIGHLIGHTED:
	    high = "\002";
	    break;
	default:
	    high = empty_string;
	    break;
    }
    if (next_arg(line, &line)) {
	if (ignore_usernames & IGNORE_INVITES)
	    add_to_whois_queue(from, whois_ignore_invites, "%s", line);
	else {
	    message_from(from, LOG_CRAP);
	    if (do_hook(INVITE_LIST, from, line))
		put_it("*** %s%s%s invites you to channel %s", high, from, high, line);
	    malloc_strcpy(&invite_channel, line);
	}
    }
    message_from(null(char *), LOG_CRAP);
}

static void kill(from, line)
char *from,
    *line;
{
    next_arg(line, &line);
    if (index(from, '.'))
	put_it("*** You have been killed by server %s at %s", from, line + 1);
    else {
	put_it("*** You have been killed by %s at %s", from, line + 1);
	put_it("*** Use /SERVER to reconnect to a server");
	close_server();
    }
}

static void ping(from)
char *from;
{
    send_to_server("PONG %s@%s %s", username, hostname, from);
}

static void nick(from, line)
char *from,
    *line;
{
    message_from(null(char *), LOG_CURRENT);
    put_it("*** %s is now known as %s", from, line);
    rename_nick(from, line);
    message_from(null(char *), LOG_CRAP);
}

static void mode(from, line)
char *from,
    *line;
{
    char *channel;

    channel = next_arg(line, &line);
    message_from(channel, LOG_CURRENT);
    if (channel)
	put_it("*** Mode change \"%s\" on channel %s by %s", line, channel, from);
    message_from(null(char *), LOG_CRAP);
}

static void kick(from, line)
char *from,
    *line;
{
    char *channel,
        *who;

    channel = next_arg(line, &line);
    who = next_arg(line, &line);
    message_from(channel, LOG_CURRENT);
    if (channel && who) {
	if (stricmp(who, nickname) == 0) {
	    remove_channel(channel);
	    update_all_status();
	    put_it("*** You have been kicked off channel %s by %s", channel, from);
	} else {
	    if (stricmp(channel, current_channel(null(char *))) == 0)
		remove_from_channel(channel, who);
	    put_it("*** %s has been kicked off channel %s by %s", who, channel, from);
	}
    }
    message_from(null(char *), LOG_CRAP);
}

static void part(from, line)
char *from,
    *line;
{
    message_from(line, LOG_CRAP);
    if (do_hook(LEAVE_LIST, from, line))
	put_it("*** %s has left channel %s", from, line);
    if (strcmp(from, nickname) == 0)
	remove_channel(line);
    else
	remove_from_channel(line, from);
    message_from(null(char *), LOG_CRAP);
}

/*
 * parse_server: parses messages from the server, doing what should be done
 * with them 
 */
void parse_server(line)
char *line;
{
    char *rest,
        *from,
        *comm;

    if (*(line + strlen(line) - 1) == '\n')
	*(line + strlen(line) - 1) = null(char);
    if (*line == ':') {
	if ((comm = index(line, ' ')) == null(char *))
	    return;
	*(comm++) = null(char);
	from = line + 1;
    } else {
	comm = line;
	from = null(char *);
    }
    if ((rest = index(comm, ' ')) == null(char *))
	return;
    *(rest++) = null(char);
    if (strcmp(comm, "WHOREPLY") == 0)
	whoreply(from, rest);
    else if (strcmp(comm, "NOTICE") == 0)
	notice(from, rest);
    else if (strcmp(comm, "PRIVMSG") == 0)
	privmsg(from, rest);
    else if (strcmp(comm, "NAMREPLY") == 0)
	namreply(rest);
    else if (strcmp(comm, "JOIN") == 0)
	channel(from, rest);
    else if (strcmp(comm, "PART") == 0)
	part(from, rest);
    /* CHANNEL will go away with 2.6 */
    else if (strcmp(comm, "CHANNEL") == 0)
	channel(from, rest);
    else if (strcmp(comm, "MSG") == 0)
	msg(from, rest);
    else if (strcmp(comm, "QUIT") == 0)
	quit(from, rest);
    else if (strcmp(comm, "WALL") == 0)
	wall(from, rest);
    else if (strcmp(comm, "WALLOPS") == 0)
	wallops(from, rest);
    else if (strcmp(comm, "LINREPLY") == 0)
	linreply(rest);
    else if (strcmp(comm, "PING") == 0)
	ping(rest);
    else if (strcmp(comm, "TOPIC") == 0)
	topic(from, rest);
    else if (strcmp(comm, "PONG") == 0)
	pong(from, rest);
    else if (strcmp(comm, "ERROR") == 0)
	error(from, rest);
    else if (strcmp(comm, "INVITE") == 0)
	invite(from, rest);
    else if (strcmp(comm, "NICK") == 0)
	nick(from, rest);
    else if (strcmp(comm, "KILL") == 0)
	kill(from, rest);
    else if (strcmp(comm, "MODE") == 0)
	mode(from, rest);
    else if (strcmp(comm, "KICK") == 0)
	kick(from, rest);
    else {
	/* no very good eh?  Well, something else to be fixed */
	if (atoi(comm))
	    numbered_command(from, atoi(comm), rest);
	else {
	    if (from)
		put_it("*** Odd server stuff: \"%s\" \"%s\" \"%s\"", from, comm, rest);
	    else
		put_it("*** Odd server stuff: \"%s\" \"%s\"", comm, rest);
	}
    }
}
