/*
 * numbers.c:handles all those strange numeric response dished out by that
 * wacky, nutty program we call ircd 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright(c) 1990 
 *
 * All Rights Reserved 
 */
#include "irc.h"

extern char *index();

static void whois_name(line)
char *line;
{
    char *nick,
        *user,
        *host,
        *channel;

    nick = next_arg(line, &line);
    user = next_arg(line, &line);
    host = next_arg(line, &line);
    channel = next_arg(line, &line);
    if (nick && user && host && channel)
	put_it("*** %s is %s@%s (%s) on channel %s", nick, user, host,
	       line + 1, (*channel == '*') ? "*private*" : channel);
}

static void whowas_name(line)
char *line;
{
    char *nick,
        *user,
        *host,
        *channel;

    nick = next_arg(line, &line);
    user = next_arg(line, &line);
    host = next_arg(line, &line);
    channel = next_arg(line, &line);
    if (nick && user && host && channel)
	put_it("*** %s was %s@%s (%s) on channel %s", nick, user, host,
	       line + 1, (*channel == '*') ? "*private*" : channel);
}

static void whois_server(line)
char *line;
{
    char *server;

    if (server = next_arg(line, &line))
	put_it("*** on irc via server %s (%s)", server, line + 1);
}

static void list(from, line)
char *line;
char *from;
{
    char *channel,
        *user_cnt;

    channel = next_arg(line, &line);
    user_cnt = next_arg(line, &line);
    if (channel && user_cnt) {
	if (*channel == '*')
	    put_it("*** Prv %5s  %s", user_cnt, line + 1);
	else
	    put_it("*** %-3s %5s  %s", channel, user_cnt, line + 1);
    }
}

static void away(from, rest)
char *from,
    *rest;
{
    char *message,
        *who;

    if (who = next_arg(rest, &message))
	put_it("*** %s is away: %s", who, message + 1);
}

static void version(from, rest)
char *from,
    *rest;
{
    char *host,
        *version;

    if (version = next_arg(rest, &host))
	put_it("*** Host %s is running server %s", host, version);
}

static void invite(from, rest)
char *from,
    *rest;
{
    char *who,
        *channel;

    if (who = next_arg(rest, &channel))
	put_it("*** Inviting %s to channel %s", who, channel);
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
    char *user,
        *ptr;

    if (*rest == ' ')
	user = null(char *);
    else {
	user = rest;
	if ((rest = index(user, ' ')) == null(char *))
	    return;
	*(rest++) = null(char);
    }
    switch (comm) {
	case 341:		/* invite confirmation */
	    invite(from, rest);
	    break;
	case 301:		/* user is away */
	    away(from, rest);
	    break;
	case 311:		/* whois name info */
	    whois_name(rest);
	    break;
	case 312:		/* whois host/server info */
	    whois_server(rest);
	    break;
	case 314:
	    whowas_name(rest);  /* whowas name info */
	    break;
	case 313:		/* whois operator info */
	    put_it("*** %s", rest);
	    break;
	case 332:		/* topic */
	    put_it("Topic%s", rest);
	    break;
	case 321:		/* list header line */
	    put_it("*** Chn Users  Names");
	    break;
	case 322:		/* list entries */
	    list(from, rest);
	    break;
	case 351:		/* version response */
	    version(from, rest);
	    break;
	case 381:		/* /oper response ok */
	    operator = 1;
	    make_status();	/* fix the status line */
	case 432:		/* bad nickname */
	case 391:		/* time */
	case 491:		/* another oper command */
	case 371:		/* info response */
	case 461:		/* not enough params */
	case 431:		/* no nickname given */
	case 481:		/* not an operator error */
	case 372:		/* motd stuff */
	case 382:		/* rehash */
	case 331:		/* topic error */
	case 411:		/* no recipient given */
	case 412:		/* no text to send */
	case 441:		/* not joined a channel yet */
	case 401:		/* no such nickname */
	case 421:		/* unknown command */
	case 471:		/* channel full */
	case 451:		/* Not yet registered */
	case 433:		/* nickname in use */
	case 464:		/* no oper privs */
	    if (ptr = index(rest, ':')) {
		*(ptr++) = null(char);
		if (strlen(rest))
		    put_it("*** %s: %s", rest, ptr);
		else
		    put_it("*** %s: %s", from, ptr);
	    } else
		put_it("*** %s: %s", from, rest);
	    break;
	default:
	    put_it("*** %s:%d:%s", from, comm, rest);
	    break;
    }
}
