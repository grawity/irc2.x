/*
 * ignore.c: handles the ingore command for irc 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include "irc.h"

/*
 * Ignore: the ignore list structure,  consists of the nickname, and the type
 * of ignorance which is to take place 
 */
typedef struct IgnoreStru {
    char *nick;
    char type;
    struct IgnoreStru *next;
}          Ignore;

/* ignored_nicks: pointer to the head of the ignore list */
static Ignore *ignored_nicks = null(Ignore *);

/*
 * find_ignore: Locates the given nick in the ignore list and returns a
 * pointer to it.  If the nick is not found, null is returned.  If the remove
 * flag is set, the entry is removed from the list (but the memory is not
 * freed) and still returned.  
 */
static Ignore *find_ignore(nick, remove)
char *nick;
char remove;
{
    Ignore *tmp,
          *last;

    last = null(Ignore *);
    for (tmp = ignored_nicks; tmp; tmp = tmp->next) {
	if (stricmp(nick, tmp->nick) == 0)
	    break;
	last = tmp;
    }
    if (remove && tmp) {
	if (last)
	    last->next = tmp->next;
	else
	    ignored_nicks = tmp->next;
    }
    return (tmp);
}

/*
 * ignore_nickname: adds nick to the ignore list, using type as the type of
 * ignorance to take place 
 */
void ignore_nickname(nick, type)
char *nick;
char type;
{
    Ignore *tmp,
          *new;

    if ((new = find_ignore(nick, 0)) == null(Ignore *)) {
	new = (Ignore *) new_malloc(sizeof(Ignore));
	new->nick = null(char *);
	malloc_strcpy(&(new->nick), nick);
	tmp = ignored_nicks;
	ignored_nicks = new;
	new->next = tmp;
    }
    new->type = type;
}

/*
 * remove_ignore: removes the given nick from the ignore list and returns 0.
 * If the nick wasn't in the ignore list to begin with, 1 is returned. 
 */
int remove_ignore(nick)
char *nick;
{
    Ignore *tmp;

    if (tmp = find_ignore(nick, 1)) {
	if (tmp->nick)
	    free(tmp->nick);
	free(tmp);
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
int is_ignored(nick, type, warn)
char *nick;
char type;
char warn;
{
    Ignore *tmp;
    int ret;

    if (ignored_nicks) {
	if (tmp = find_ignore(nick, 0)){
	    if (ret = ((tmp->type == type) || (tmp->type == IGNORE_ALL))){
		if (warn)
		    send_to_server("NOTICE %s %s is ignoring you.",nick,nickname);
		return(ret);
	    }
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

    put_it("*** Ignore List:");
    if (nick) 
	len = strlen(nick);
    for (tmp = ignored_nicks; tmp; tmp = tmp->next) {
	if (nick) {
	    if (strnicmp(nick, tmp->nick, len))
		continue;
	}
	switch (tmp->type) {
	    case IGNORE_ALL:
		put_it("%s: all messages", tmp->nick);
		break;
	    case IGNORE_PUB:
		put_it("%s: public messages", tmp->nick);
		break;
	    case IGNORE_PRIV:
		put_it("%s: private messages", tmp->nick);
		break;
	}
    }
}
