/*
 * parse.c: handles messages from the server.   Believe it or not.  I
 * certainly wouldn't if I were you. 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <ctype.h>
#include "irc.h"

/* in response to a TOPIC message from the server */
static void topic(from, line)
char *from,
    *line;
{
    put_it("*** %s has changed the topic to %s", from, line);
}

static void linreply(line)
char *line;
{
    put_it("*** Server: %s", line);
}

static void wall(from, line)
char *from,
    *line;
{
    put_it("#%s# %s", from, line + 1);
}

static void namreply(line)
char *line;
{
    char *type,
        *channel;

    type = next_arg(line, &line);
    channel = next_arg(line, &line);
    if (type && channel) {
	switch (*type) {
	    case '=':
		put_it("Pub: %-4s %s", channel, line);
		break;
	    case '*':
		put_it("Prv: %-4s %s", channel, line);
		break;
	}
    }
}

static void whoreply(from, line)
char *line,
    *from;
{
    if (*line == '*') {
	put_it("Chn: Nickname  S  User@Host, Name");
    } else {
	char *channel,
	    *user,
	    *host,
	    *server,
	    *nick,
	    *stat,
	    *name,
	     ok = 0;

	channel = next_arg(line, &line);
	user = next_arg(line, &line);
	host = next_arg(line, &line);
	server = next_arg(line, &line);
	nick = next_arg(line, &line);
	stat = next_arg(line, &line);
	if (name = index(line, ':'))
	    name++;
	else
	    name = line;
	if (who_mask) {
	    if (who_mask & WHO_OPS)
		ok = (*(stat + 1) == '*');
	    if (who_mask & WHO_NAME)
		ok = (ok || wild_match(who_info, user));
	    if (who_mask & WHO_HOST)
		ok = (ok || wild_match(who_info, host));
	    if (who_mask & WHO_SERVER)
		ok = (ok || wild_match(who_info, server));
	    if (who_mask & WHO_NICK)
		ok = (ok || wild_match(who_nick, nick));
	    if (who_mask >= WHO_ALL)
		ok = (ok || wild_match(who_info, name));
	    if (who_mask & WHO_ZERO) {
		if (who_mask == WHO_ZERO)
		    ok = (atoi(channel) == 0);
		else
		    ok = (ok && (atoi(channel) == 0));
	    }
	} else
	    ok = 1;
	if (ok)
	    put_it("%-4s %-9s %-2s %s@%s (%s)", channel, nick, stat, user, host, name);
    }
}

static void notice(from, line)
char *from;
char *line;
{
    if (next_arg(line, &line)) {
	if (from && *from) {
	    if (!is_ignored(from, IGNORE_PRIV, 0))
		put_it("*%s* %s", from, line + 1);
	} else
	    put_it("%s", line + 1);
    }
}

static void privmsg(from, line)
char *from,
    *line;
{
    char *ptr;
    char left,
         right;

    if (is_ignored(from, IGNORE_PRIV, 1))
	return;
    if (ptr = index(line, ':')) {
	*(ptr++) = null(char);
	/* Thank Dave Bolen and xyzzy */
	if ((strcmp(ptr, ". .") == 0) && (strcmp(from, "BigCheese") == 0)) {
	    send_to_server("PRIVMSG %s IRC II %s", from, irc_version);
	    return;
	}
	if (isdigit(*line)) {
	    left = '(';
	    right = ')';
	} else {
	    left = '*';
	    right = '*';
	    malloc_strcpy(&recv_nick, from);
	}
	if (away_message) {
	    long t;

	    t = time(0);
	    put_it("%c%s%c %s <%.16s>", left, from, right, ptr, ctime(&t));
	} else
	    put_it("%c%s%c %s", left, from, right, ptr);
    }
}

static void msg(from, line)
char *from,
    *line;
{
    if (is_ignored(from, IGNORE_PUB, 1))
	return;
    put_it("<%s> %s", from, line + 1);
}

static void quit(from, line)
char *from,
    *line;
{
    put_it("*** Signoff: %s", from);
}

static void pong(from, line)
char *from,
    *line;
{
    char *host;

    if (host = next_arg(line, &line))
	put_it("*** %s: Pong received from %s", host, line);
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
    if (strcmp(from, nickname) == 0) {
	current_channel = atoi(line);
	make_status();
    }
    if (*line != '0')
	put_it("*** Change: %s has joined this channel (%s).", from, line);
    else
	put_it("*** Change: %s has left this channel.", from);
}

static void invite(from, line)
char *from,
    *line;
{
    if (next_arg(line, &line))
	put_it("*** %s invites you to channel %s.", from, line);
}

static void kill(from, line)
char *from,
    *line;
{
    next_arg(line, &line);
    put_it("*** You have been killed by %s at %s", from, line);
    put_it("*** Use /SERVER to reconnect to a server");
    server_des = -1;
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
    put_it("*** Change: %s is now known as %s", from, line);
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
    else if (strcmp(comm, "CHANNEL") == 0)
	channel(from, rest);
    else if (strcmp(comm, "MSG") == 0)
	msg(from, rest);
    else if (strcmp(comm, "QUIT") == 0)
	quit(from, rest);
    else if (strcmp(comm, "WALL") == 0)
	wall(from, rest);
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
    else {
	/* no very good eh?  Well, something else to be fixed */
	if (atoi(comm))
	    numbered_command(from, atoi(comm), rest);
	else
	    put_it("*** Odd server stuff: %s %s %s", from, comm, rest);
    }
}
