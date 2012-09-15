/*
 * whois.c: Some tricky routines for querying the server for information
 * about a nickname using WHOIS.... all the time hiding this from the user.  
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "config.h"
#include "whois.h"
#include "hook.h"
#include "lastlog.h"
#include "vars.h"
#include "ignore.h"
#include "ircaux.h"
#include "notify.h"

#define BOGUS_NICK "+++"

static char ignore_whois_crap = 0;
static char eat_away = 0;

typedef struct WhoisStuffStru {
    char *nick;
    char *user;
    char *host;
    char *channel;
    char *name;
    char *server;
    char *server_stuff;
    char *away;
    char oper;
    char chop;
    char not_on;
}              WhoisStuff;

static WhoisStuff whois_stuff =
{null(char *), null(char *), null(char *), null(char *), null(char *),
 null(char *), null(char *), null(char *), 0, 0, 0};

/*
 * WhoisQueue: the structure of the Whois queue.  This maintains the nick
 * name of the person being whois'ed, some additional text to be used at the
 * discretion of the programmer, and a function that will be called when the
 * whois on this person returns. 
 */
typedef struct WhoisQueueStru {
    char *nick;			/* nickname of whois'ed person */
    char *text;			/* additional text */
    /*
     * called with func((WhoisStuff *)stuff,(char *) nick, (char *) text) 
     */
    void (*func) ();
    struct WhoisQueueStru *next;/* next element in queue */
}              WhoisQueue;

/* WQ_head and WQ_tail point to the head and tail of the whois queue */
static WhoisQueue *WQ_head = null(WhoisQueue *);
static WhoisQueue *WQ_tail = null(WhoisQueue *);

/*
 * whois_queue_head: returns the nickname at the head of the whois queue, or
 * NULL if the queue is empty.  It does not modify the queue in any way. 
 */
char *whois_queue_head()
{
    if   (WQ_head)
	     return (WQ_head->nick);
    else
	return (null(char *));
}

/*
 * remove_from_whois_queue: removes the top element of the whois queue and
 * returns the element as its function value.  This routine repairs handles
 * all queue stuff, but the returned element is mallocd and must be freed by
 * the calling routine 
 */
static WhoisQueue *remove_from_whois_queue()
{
    WhoisQueue *new;

    new = WQ_head;
    WQ_head = WQ_head->next;
    if (WQ_head == null(WhoisQueue *))
	WQ_tail = null(WhoisQueue *);
    return (new);
}

/* clean_whois_queue: this empties out the whois queue.  This is used after server reconnection to assure that no bogus entries are left in the whois queue */
void clean_whois_queue()
{
    WhoisQueue *thing;
    
    while(whois_queue_head()){
	thing = remove_from_whois_queue();
	new_free(&thing->nick);
	new_free(&thing->text);
	new_free(&thing);
    }
}

/*
 * whois_name: routine that is called when numeric 311 is received in
 * numbers.c. This routine parses out the information in the numeric and
 * saves it until needed (see whois_server()).  If the whois queue is empty,
 * this routine simply displays the data in the normal fashion.  Why would
 * the queue ever be empty, you ask? If the user does a "WHOIS *" or any
 * other whois with a wildcard, you will get multiple returns from the
 * server.  So, instead of attempting to handle each of these, only the first
 * is handled, and the others fall through.  It is up to the programmer to
 * prevent wildcards from interfering with what they want done.  See
 * channel() in edit.c 
 */
void whois_name(line)
char *line;
{
    char *nick,
        *user,
        *host,
        *channel,
        *ptr;

    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    nick = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    user = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    host = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    channel = line;
    line = ptr;
    if ((ptr = whois_queue_head()) &&
	(stricmp(ptr, nick) == 0)) {
	malloc_strcpy(&whois_stuff.nick, nick);
	malloc_strcpy(&whois_stuff.user, user);
	malloc_strcpy(&whois_stuff.host, host);
	malloc_strcpy(&whois_stuff.name, line + 1);
	malloc_strcpy(&whois_stuff.channel, channel);
	whois_stuff.away = null(char *);
	whois_stuff.oper = 0;
	whois_stuff.chop = 0;
	whois_stuff.not_on = 0;
	ignore_whois_crap = 1;
	eat_away = 1;
    } else {
	ignore_whois_crap = 0;
	eat_away = 0;
	put_it("*** %s is %s@%s (%s) on channel %s", nick, user, host,
	       line + 1, (*channel == '*') ? "*private*" : channel);
    }
}

/*
 * whowas_name: same as whois_name() above but it is called with a numeric of
 * 314 when the user does a WHOWAS or when a WHOIS'd user is no longer on IRC 
 */
void whowas_name(line)
char *line;
{
    char *nick,
        *user,
        *host,
        *channel,
        *ptr;

    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    nick = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    user = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    host = line;
    line = ptr;
    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    channel = line;
    line = ptr;

    if ((ptr = whois_queue_head()) &&
	(stricmp(ptr, nick) == 0)) {
	malloc_strcpy(&whois_stuff.nick, nick);
	malloc_strcpy(&whois_stuff.user, user);
	malloc_strcpy(&whois_stuff.host, host);
	malloc_strcpy(&whois_stuff.name, line + 1);
	malloc_strcpy(&whois_stuff.channel, channel);
	whois_stuff.away = null(char *);
	whois_stuff.oper = 0;
	whois_stuff.chop = 0;
	whois_stuff.not_on = 1;
	ignore_whois_crap = 1;
    } else {
	ignore_whois_crap = 0;
	put_it("*** %s was %s@%s (%s) on channel %s", nick, user, host,
	       line + 1, (*channel == '*') ? "*private*" : channel);
    }
}

/*
 * whois_server: Called in numbers.c when a numeric of 312 is received.  If
 * all went well, this routine collects the needed information, pops the top
 * element off the queue and calls the function as described above in
 * WhoisQueue.  It then releases all the mallocd data.  If the queue is empty
 * (same case as described in whois_name() above), the information is simply
 * displayed in the normal fashion 
 */
void whois_server(line)
char *line;
{
    char *server,
        *ptr;

    if (ptr = index(line, ' '))
	*(ptr++) = null(char);
    else
	return;
    server = line;
    line = ptr;
    if ((ptr = whois_queue_head()) &&
	(stricmp(ptr, whois_stuff.nick) == 0)) {
	malloc_strcpy(&whois_stuff.server, server);
	malloc_strcpy(&whois_stuff.server_stuff, line + 1);
    } else
	put_it("*** on irc via server %s (%s)", server, line + 1);
}

/*
 * whois_oper: This displays the operator status of a user, as returned by
 * numeric 313 from the server.  If the ignore_whois_crap flag is set,
 * nothing is dispayed. 
 */
void whois_oper(line)
char *line;
{
    if (ignore_whois_crap)
	whois_stuff.oper = 1;
    else
	put_it("*** %s (is an IRC operator)", line);
}

void whois_lastcom(line)
char *line;
{
    if (!ignore_whois_crap)
	put_it("*** Command last received: %.24s", line);
}

/*
 * whois_chop: This displays the operator status of a user, as returned by
 * numeric 313 from the server.  If the ignore_whois_crap flag is set,
 * nothing is dispayed. 
 */
void whois_chop(line)
char *line;
{
    if (ignore_whois_crap)
	whois_stuff.chop = 1;
    else
	put_it("*** %s (is a channel operator)", line);
}

/*
 * no_such_nickname: Handler for numeric 401, the no such nickname error. If
 * the nickname given is at the head of the queue, then this routine pops the
 * top element from the queue, sets the whois_stuff.flag to indicate that the
 * user is no longer on irc, then calls the func() as normal.  It is up to
 * that function to set the ignore_whois_crap variable which will determine
 * if any other information is displayed or not. 
 */
void no_such_nickname(from, rest)
char *from,
    *rest;
{
    char *msg,
        *ptr;

    if (msg = index(rest, ' ')) {
	*(msg++) = null(char);
	ptr = whois_queue_head();
	if (ptr && (stricmp(ptr, rest) == 0)) {
	    whois_stuff.not_on = 1;
	    ignore_whois_crap = 1;
	    eat_away = 1;
	    return;
	}
	if (ptr && (strcmp(rest, BOGUS_NICK) == 0)) {
	    WhoisQueue *thing;

	    thing = remove_from_whois_queue();
	    if (whois_stuff.not_on)
		thing->func(null(WhoisStuff *), thing->nick, thing->text);
	    else
		thing->func(&whois_stuff, thing->nick, thing->text);
	    new_free(&thing->nick);
	    new_free(&thing->text);
	    new_free(&thing);
	    return;
	} else {
	    put_it("*** %s: %s", rest, msg + 1);
	    eat_away = 0;
	    if (irc2_6 && get_int_var(AUTO_WHOWAS_VAR))
		send_to_server("WHOWAS %s", rest);
	}
    }
}

/*
 * user_is_away: called when a 301 numeric is received.  Nothing is displayed
 * by this routine if the ignore_whois_crap flag is set 
 */
void user_is_away(from, rest)
char *from,
    *rest;
{
    char *message,
        *who;

    if (who = next_arg(rest, &message)) {
	if (whois_stuff.nick && (strcmp(who, whois_stuff.nick) == 0) && eat_away) {
	    malloc_strcpy(&whois_stuff.away, message + 1);
	} else
	    put_it("*** %s is away: %s", who, message + 1);
	eat_away = 0;
    }
}

/*
 * The stuff below this point are all routines suitable for use in the
 * add_to_whois_queue() call as the func parameter 
 */

/*
 * whois_ignore_msgs: This is used when you are ignoring MSGs using the
 * user@hostname format 
 */
void whois_ignore_msgs(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    char *ptr;
    int level;

    level = set_lastlog_msg_level(LOG_MSG);
    message_from(stuff->nick);
    if (stuff) {
	ptr = (char *) new_malloc(strlen(stuff->user) + strlen(stuff->host) + 2);
	sprintf(ptr, "%s@%s", stuff->user, stuff->host);
	if (is_ignored(ptr, IGNORE_MSGS) != IGNORED) {
	    if (do_hook(MSG_LIST, stuff->nick, text)) {
		put_it("*%s* %s", stuff->nick, text);
		if (away_message)
		    beep_em(get_int_var(BEEP_WHEN_AWAY_VAR));
		else
		    beep_em(get_int_var(BEEP_ON_MSG_VAR));
	    }
	} else
	    send_to_server("NOTICE %s %s is ignoring you.", nick, nickname);
	new_free(&ptr);
    }
    message_from(null(char *));
    set_lastlog_msg_level(level);
}

/*
 * whois_ignore_notices: This is used when you are ignoring NOTICEs using the
 * user@hostname format 
 */
void whois_ignore_notices(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    char *ptr;
    int level;

    level = set_lastlog_msg_level(LOG_NOTICE);
    message_from(stuff->nick);
    if (stuff) {
	ptr = (char *) new_malloc(strlen(stuff->user) + strlen(stuff->host) + 2);
	sprintf(ptr, "%s@%s", stuff->user, stuff->host);
	if (is_ignored(ptr, IGNORE_NOTICES) != IGNORED) {
	    if (do_hook(NOTICE_LIST, stuff->nick, text))
		put_it("-%s- %s", stuff->nick, text);
	}
	new_free(&ptr);
    }
    message_from(null(char *));
    set_lastlog_msg_level(level);
}

/*
 * whois_ignore_invites: This is used when you are ignoring INVITES using the
 * user@hostname format 
 */
void whois_ignore_invites(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    char *ptr;

    if (stuff) {
	ptr = (char *) new_malloc(strlen(stuff->user) + strlen(stuff->host) + 2);
	sprintf(ptr, "%s@%s", stuff->user, stuff->host);
	if (is_ignored(ptr, IGNORE_INVITES) != IGNORED) {
	    if (do_hook(INVITE_LIST, stuff->nick, text))
		put_it("*** %s invites you to channel %s", stuff->nick, text);
	} else
	    send_to_server("NOTICE %s %s is ignoring you.", nick, nickname);
	new_free(&ptr);
    }
}

/*
 * whois_ignore_walls: This is used when you are ignoring WALLS using the
 * user@hostname format 
 */
void whois_ignore_walls(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    char *ptr;
    int level;

    level = set_lastlog_msg_level(LOG_WALL);
    message_from(stuff->nick);
    if (stuff) {
	ptr = (char *) new_malloc(strlen(stuff->user) + strlen(stuff->host) + 2);
	sprintf(ptr, "%s@%s", stuff->user, stuff->host);
	if (is_ignored(ptr, IGNORE_WALLS) != IGNORED) {
	    if (do_hook(WALL_LIST, stuff->nick, text))
		put_it("#%s# %s", stuff->nick, text);
	}
	new_free(&ptr);
    }
    message_from(null(char *));
    set_lastlog_msg_level(level);
}

/*
 * whois_ignore_wallops: This is used when you are ignoring WALLOPS using the
 * user@hostname format 
 */
void whois_ignore_wallops(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    char *ptr;
    int level;

    level = set_lastlog_msg_level(LOG_WALLOP);
    message_from(stuff->nick);
    if (stuff) {
	ptr = (char *) new_malloc(strlen(stuff->user) + strlen(stuff->host) + 2);
	sprintf(ptr, "%s@%s", stuff->user, stuff->host);
	if (is_ignored(ptr, IGNORE_WALLOPS) != IGNORED) {
	    if (do_hook(WALLOP_LIST, stuff->nick, text))
		put_it("!%s! %s", stuff->nick, text);
	}
	new_free(&ptr);
    }
    message_from(null(char *));
    set_lastlog_msg_level(level);
}

/*
 * whois_join: This is called when the user does a "JOIN -NICK <nickname>".
 * It switches to the channel that that user is on.  
 */
void whois_join(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    if (stuff) {
	if (*stuff->channel == '*')
	    put_it("*** %s is on a private channel", stuff->nick);
	else if (irc2_6)
	    send_to_server("JOIN %s", stuff->channel);
	else
	    send_to_server("CHANNEL %s", stuff->channel);
    } else
	put_it("*** %s: No such nickname", nick);
}

/*
 * whois_privmsg: handles the -CHANNEL switch for both MSG and NOTICE. Note
 * that in this case, the text parameter is set up as the format string for
 * send_to_server, where the command and the messages are filled in leaving
 * only the channel to be inserted 
 */
void whois_privmsg(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    if (stuff) {
	if (*stuff->channel == '*')
	    put_it("*** %s is on a private channel", stuff->nick);
	else
	    send_to_server(text, stuff->channel, text);
    } else
	put_it("*** %s: No such nickname", nick);
}

/*
 * whois_notify: used by the routines in notify.c to tell when someone has
 * signed on or off irc 
 */
void whois_notify(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    int level;

    level = set_lastlog_msg_level(LOG_CRAP);
    if (stuff) {
	notify_mark(stuff->nick, stuff->channel, stuff->away, 1);
    } else {
	notify_mark(nick, null(char *), null(char *), 0);
    }
    set_lastlog_msg_level(level);
}

void whois_new_wallops(stuff, nick, text)
WhoisStuff *stuff;
char *nick;
char *text;
{
    int flag,
        level;
    char *left,
        *right;

    message_from(nick, LOG_WALLOP);
    flag = is_ignored(nick, IGNORE_WALLOPS);
    if (flag != IGNORED) {
	if (flag == HIGHLIGHTED) {
	    left = "\002!";
	    right = "!\002";
	} else
	    left = right = "!";
	if (ignore_usernames & IGNORE_WALLOPS)
	    add_to_whois_queue(nick, whois_ignore_wallops, "%s", text);
	else {
	    level = set_lastlog_msg_level(LOG_WALLOP);
	    if (stuff && stuff->oper)
		sprintf(buffer, "%s*", nick);
	    else
		strcpy(buffer, nick);
	    if (do_hook(WALLOP_LIST, buffer, text))
		put_it("%s%s%s %s", left, buffer, right, text);
	    set_lastlog_msg_level(level);
	}
    }
    message_from(null(char *), LOG_CRAP);
}

/* I put the next routine down here to keep my compile quite */

/*
 * add_to_whois_queue: This routine is called whenever you want to do a WHOIS
 * or WHOWAS.  What happens is this... each time this function is called it
 * adds a new element to the whois queue using the nick and func as in
 * WhoisQueue, and creating the text element using the format and args.  It
 * then issues the WHOIS or WHOWAS.  
 */
void add_to_whois_queue(nick, func, format,
			     arg1, arg2, arg3, arg4, arg5,
			     arg6, arg7, arg8, arg9, arg10)
void (*func) ();
char *format,
    *nick;
int arg1,
    arg2,
    arg3,
    arg4,
    arg5,
    arg6,
    arg7,
    arg8,
    arg9,
    arg10;
{
    char buffer[BUFFER_SIZE];
    WhoisQueue *new;

    if (nick == null(char *))
	return;
    if (index(nick, '*') == null(char *)) {
	new = (WhoisQueue *) new_malloc(sizeof(WhoisQueue));
	new->text = null(char *);
	new->nick = null(char *);
	new->func = func;
	new->next = null(WhoisQueue *);
	if (format) {
	    sprintf(buffer, format, arg1, arg2, arg3, arg4, arg5,
		    arg6, arg7, arg8, arg9, arg10);
	    malloc_strcpy(&(new->text), buffer);
	}
	malloc_strcpy(&(new->nick), nick);
	if (WQ_head == null(WhoisQueue *))
	    WQ_head = new;
	if (WQ_tail)
	    WQ_tail->next = new;
	WQ_tail = new;
    }
    send_to_server("WHOIS %s", nick);
    send_to_server("WHOIS %s", BOGUS_NICK);
}
