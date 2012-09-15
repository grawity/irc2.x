/*
 * ignore.c: handles the ingore command for irc 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "ignore.h"
#include "ircaux.h"
#include "list.h"

#define NUMBER_OF_IGNORE_LEVELS 7

#define IGNORE_REMOVE 1
#define IGNORE_HIGH -1

int ignore_usernames = 0;
static int ignore_usernames_sums[NUMBER_OF_IGNORE_LEVELS] = {0, 0, 0, 0, 0, 0, 0};

/*
 * Ignore: the ignore list structure,  consists of the nickname, and the type
 * of ignorance which is to take place 
 */
typedef struct IgnoreStru {
    struct IgnoreStru *next;
    char *nick;
    unsigned char type;
    unsigned char high;
}          Ignore;

/* ignored_nicks: pointer to the head of the ignore list */
static Ignore *ignored_nicks = null(Ignore *);

static int ignore_usernames_mask(mask, thing)
int mask,
    thing;
{
    int i,
        p;

    for (i = 0, p = 1; i < NUMBER_OF_IGNORE_LEVELS; i++, p *= 2) {
	if (mask & p)
	    ignore_usernames_sums[i] += thing;
    }
    mask = 0;
    for (i = 0, p = 1; i < NUMBER_OF_IGNORE_LEVELS; i++, p *= 2) {
	if (ignore_usernames_sums[i])
	    mask += p;
    }
    return (mask);
}

/*
 * ignore_nickname: adds nick to the ignore list, using type as the type of
 * ignorance to take place.  
 */
static void ignore_nickname(nick, type, flag)
char *nick;
unsigned char type;
int flag;
{
    Ignore *new;
    char *msg,
        *ptr;

    while (nick) {
	if (ptr = index(nick, ','))
	    *ptr = null(char);
	if (index(nick, '@')) {
	    if (flag == IGNORE_HIGH) {
		put_it("*** You may not highlight messages using the user@hostname format");
		return;
	    }
	    if (type & IGNORE_PUBLIC) {
		put_it("*** You may not ignore public messages using the user@hostname format.");
		type -= IGNORE_PUBLIC;
	    }
	    ignore_usernames = ignore_usernames_mask(type, 1);
	}
	if (*nick) {
	    if ((new = (Ignore *) list_lookup(&ignored_nicks, nick, !USE_WILDCARDS, !REMOVE_FROM_LIST)) == null(Ignore *)) {
		if (flag == IGNORE_REMOVE) {
		    put_it("*** %s is not on the ignorance list", nick);
		    if (ptr)
			*(ptr++) = ',';
		    nick = ptr;
		    continue;
		} else {
		    if (new = (Ignore *) remove_from_list(&ignored_nicks, nick)){
			new_free(&(new->nick));
			new_free(&new);
		    }
		    new = (Ignore *) new_malloc(sizeof(Ignore));
		    new->nick = null(char *);
		    new->type = 0;
		    new->high = 0;
		    malloc_strcpy(&(new->nick), nick);
		    upper(new->nick);
		    add_to_list(&ignored_nicks, new, sizeof(Ignore));
		}
	    }
	    switch (flag) {
		case IGNORE_REMOVE:
		    new->type &= (~type);
		    new->high &= (~type);
		    msg = "Not ignoring";
		    break;
		case IGNORE_HIGH:
		    new->high |= type;
		    new->type &= (~type);
		    msg = "Highlighting";
		    break;
		default:
		    new->type |= type;
		    new->high &= (~type);
		    msg = "Ignoring";
		    break;
	    }
	    if (type == IGNORE_ALL) {
		switch (flag) {
		    case IGNORE_REMOVE:
			put_it("*** %s removed from ignorace list", new->nick);
			remove_ignore(new->nick);
			break;
		    case IGNORE_HIGH:
			put_it("*** Highlighting ALL messages from %s", new->nick);
			break;
		    default:
			put_it("*** Ignoring ALL messages from %s", new->nick);
			break;
		}
		return;
	    } else {
		if (type & IGNORE_MSGS)
		    put_it("*** %s MSGS from %s", msg, new->nick);
		if (type & IGNORE_PUBLIC)
		    put_it("*** %s PUBLIC messages from %s", msg, new->nick);
		if (type & IGNORE_WALLS)
		    put_it("*** %s WALLS from %s", msg, new->nick);
		if (type & IGNORE_WALLOPS)
		    put_it("*** %s WALLOPS from %s", msg, new->nick);
		if (type & IGNORE_INVITES)
		    put_it("*** %s INVITES from %s", msg, new->nick);
		if (type & IGNORE_NOTICES)
		    put_it("*** %s NOTICES from %s", msg, new->nick);
		if (type & IGNORE_MAIL)
		    put_it("*** %s MAIL from %s", msg, new->nick);
	    }
	    if ((new->type == 0) && (new->high == 0))
		remove_ignore(new->nick);
	}
	if (ptr)
	    *(ptr++) = ',';
	nick = ptr;
    }
}

/*
 * remove_ignore: removes the given nick from the ignore list and returns 0.
 * If the nick wasn't in the ignore list to begin with, 1 is returned. 
 */
int remove_ignore(nick)
char *nick;
{
    Ignore *tmp;

    if (tmp = (Ignore *) list_lookup(&ignored_nicks, nick, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
	if (index(nick, '@'))
	    ignore_usernames = ignore_usernames_mask(tmp->type, -1);
	new_free(&(tmp->nick));
	new_free(&tmp);
	return (0);
    }
    return (1);
}

/*
 * is_ignored: checks to see if nick is being ignored (poor nick).  Checks
 * against type to see if ignorance is to take place.  If nick is marked as
 * IGNORE_ALL or ignorace types match, 1 is returned, otherwise 0 is
 * returned.  
 */
int is_ignored(nick, type)
char *nick;
unsigned char type;
{
    Ignore *tmp;

    if (ignored_nicks) {
	if (tmp = (Ignore *) list_lookup(&ignored_nicks, nick, USE_WILDCARDS, !REMOVE_FROM_LIST)) {
	    if (tmp->type & type)
		return (IGNORED);
	    if (tmp->high & type)
		return (HIGHLIGHTED);
	}
    }
    return (0);
}

/* ignore_list: shows the entired ignorance list */
void ignore_list(nick)
char *nick;
{
    Ignore *tmp;
    int len;

    if (ignored_nicks) {
	put_it("*** Ignorance List:");
	if (nick) {
	    len = strlen(nick);
	    upper(nick);
	}
	for (tmp = ignored_nicks; tmp; tmp = tmp->next) {
	    if (nick) {
		if (strncmp(nick, tmp->nick, len))
		    continue;
	    }
	    if (tmp->type == IGNORE_ALL)
		sprintf(buffer, "%s:\t ALL", tmp->nick);
	    else if (tmp->high == IGNORE_ALL)
		sprintf(buffer, "%s:\t \002ALL\002", tmp->nick);
	    else {
		sprintf(buffer, "%s:\t", tmp->nick);
		if (tmp->type & IGNORE_PUBLIC)
		    strmcat(buffer, " PUBLIC", BUFFER_SIZE);
		else if (tmp->high & IGNORE_PUBLIC)
		    strmcat(buffer, " \002PUBLIC\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_MSGS)
		    strmcat(buffer, " MSGS", BUFFER_SIZE);
		else if (tmp->high & IGNORE_MSGS)
		    strmcat(buffer, " \002MSGS\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_WALLS)
		    strmcat(buffer, " WALLS", BUFFER_SIZE);
		else if (tmp->high & IGNORE_WALLS)
		    strmcat(buffer, " \002WALLS\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_WALLOPS)
		    strmcat(buffer, " WALLOPS", BUFFER_SIZE);
		else if (tmp->high & IGNORE_WALLOPS)
		    strmcat(buffer, " \002WALLOPS\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_INVITES)
		    strmcat(buffer, " INVITES", BUFFER_SIZE);
		else if (tmp->high & IGNORE_INVITES)
		    strmcat(buffer, " \002INVITES\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_NOTICES)
		    strmcat(buffer, " NOTICES", BUFFER_SIZE);
		else if (tmp->high & IGNORE_NOTICES)
		    strmcat(buffer, " \002NOTICES\002", BUFFER_SIZE);
		if (tmp->type & IGNORE_MAIL)
		    strmcat(buffer, " MAIL", BUFFER_SIZE);
		else if (tmp->high & IGNORE_MAIL)
		    strmcat(buffer, " \002MAIL\002", BUFFER_SIZE);
	    }
	    put_it("***\t%s", buffer);
	}
    } else
	put_it("*** There are no nicknames being igored");
}

/*
 * ignore: does the /IGNORE command.  Figures out what type of ignoring the
 * user wants to do and calls the proper ignorance command to do it. 
 */
void ignore(command, args)
char *command,
    *args;
{
    char *nick,
        *type;
    int len;
    int flag,
        no_flags;

    if (nick = next_arg(args, &args)) {
	no_flags = 1;
	while (type = next_arg(args, &args)) {
	    no_flags = 0;
	    upper(type);
	    switch (*type) {
		case '-':
		    flag = IGNORE_REMOVE;
		    type++;
		    break;
		case '+':
		    flag = IGNORE_HIGH;
		    type++;
		    break;
		default:
		    flag = 0;
		    break;
	    }
	    if ((len = strlen(type)) == 0) {
		put_it("*** You must specify one of the following:");
		put_it("***\tALL MSGS PUBLIC WALLS WALLOPS INVITES NOTICES MAIL NONE");
		return;
	    }
	    if (strncmp(type, "ALL", len) == 0)
		ignore_nickname(nick, IGNORE_ALL, flag);
	    else if (strncmp(type, "MSGS", len) == 0)
		ignore_nickname(nick, IGNORE_MSGS, flag);
	    else if (strncmp(type, "PUBLIC", len) == 0)
		ignore_nickname(nick, IGNORE_PUBLIC, flag);
	    else if (strncmp(type, "WALLS", len) == 0)
		ignore_nickname(nick, IGNORE_WALLS, flag);
	    else if (strncmp(type, "WALLOPS", len) == 0)
		ignore_nickname(nick, IGNORE_WALLOPS, flag);
	    else if (strncmp(type, "INVITES", len) == 0)
		ignore_nickname(nick, IGNORE_INVITES, flag);
	    else if (strncmp(type, "NOTICES", len) == 0)
		ignore_nickname(nick, IGNORE_NOTICES, flag);
	    else if (strncmp(type, "MAIL", len) == 0)
		ignore_nickname(nick, IGNORE_MAIL, flag);
	    else if (strncmp(type, "NONE", len) == 0) {
		char *ptr;

		while (nick) {
		    if (ptr = index(nick, ','))
			*ptr = null(char);
		    if (*nick) {
			if (remove_ignore(nick))
			    put_it("*** %s is not in the ignorance list!", nick);
			else
			    put_it("*** %s removed from ignorance list", nick);
		    }
		    if (ptr)
			*(ptr++) = ',';
		    nick = ptr;
		}
	    } else {
		put_it("*** You must specify one of the following:");
		put_it("***\tALL MSGS PUBLIC WALLS WALLOPS INVITES NOTICES MAIL NONE");
	    }
	}
	if (no_flags)
	    ignore_list(nick);
    } else
	ignore_list(null(char *));
}
