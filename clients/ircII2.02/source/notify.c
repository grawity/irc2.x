/*
 * notify.c: a few handy routines to notify you when people enter and leave
 * irc 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "irc.h"
#include "config.h"
#include "list.h"
#include "notify.h"
#include "ircaux.h"
#include "whois.h"
#include "hook.h"

/* NotifyList: the structure for the notify stuff */
typedef struct notify_stru {
    struct notify_stru *next;	/* pointer to next notify person */
    char *nick;			/* nickname of person to notify about */
    char *channel;		/* The channel that person is on */
    char *away;			/* Away message for user */
    char verbose;		/* 1=Notify on channel change */
    int flag;			/* 1=person on irc, 0=person not on irc */
}           NotifyList;

static NotifyList *notify_list = null(NotifyList *);

/* notify: the NOTIFY command.  Does the whole ball-o-wax */
void notify(command, args)
char *command,
    *args;
{
    char *nick,
        *ptr;
    NotifyList *new;
    char no_nicks = 1,
         verbose;

    while (nick = next_arg(args, &args)) {
	no_nicks = 0;
	while (nick) {
	    if (ptr = index(nick, ','))
		*(ptr++) = null(char);
	    if (*nick == '-') {
		nick++;
		if (*nick) {
		    if (new = (NotifyList *) remove_from_list(&notify_list, nick)) {
			put_it("*** %s has been removed from the notification list", nick);
			new_free(&(new->nick));
			new_free(&(new->channel));
			new_free(&(new->away));
			new_free(&new);
		    } else
			put_it("*** %s is not on the notification list", nick);
		}
	    } else {
		if (*nick == '+') {
		    nick++;
		    verbose = 1;
		} else
		    verbose = 0;
		if (*nick) {
		    if (index(nick, '*'))
			put_it("*** Wildcards not allowed in NOTIFY nicknames!");
		    else {
			if (new = (NotifyList *) remove_from_list(&notify_list, nick)){
			    new_free(&(new->nick));
			    new_free(&(new->channel));
			    new_free(&(new->away));
			    new_free(&new);
			}
			new = (NotifyList *) new_malloc(sizeof(NotifyList));
			new->nick = null(char *);
			new->channel = null(char *);
			new->away = null(char *);
			malloc_strcpy(&(new->nick), nick);
			new->flag = -1;
			new->verbose = verbose;
			add_to_list(&notify_list, new, sizeof(new));
			add_to_whois_queue(new->nick, whois_notify, null(char *));
			put_it("*** %s has been added to the notification list%s", nick, verbose ? " [VERBOSE]" : empty_string);
		    }
		}
	    }
	    nick = ptr;
	}
    }
    if (no_nicks) {
	put_it("*** Notify List:");
	for (new = notify_list; new; new = new->next) {
	    switch (new->flag) {
		case 0:
		    put_it("***  %s is not on irc", new->nick);
		    break;
		case 1:
		    put_it("***  %s is on channel %s", new->nick, new->channel);
		    if (new->away)
			put_it("***    %s is away:%s", new->nick, new->away);
		    break;
	    }
	}
    }
}

/*
 * do_notify: This simply goes through the notify list, sending out a WHOIS
 * for each person on it.  This uses the fancy whois stuff in whois.c to
 * figure things out.  Look there for more details, if you can figure it out.
 * I wrote it and I can't figure it out 
 */
void do_notify()
{
    NotifyList *tmp;

    for (tmp = notify_list; tmp; tmp = tmp->next)
	add_to_whois_queue(tmp->nick, whois_notify, null(char *));
}

/*
 * notify_mark: This marks a given person on the notify list as either on irc
 * (if flag is 1), or not on irc (if flag is 0).  If the person's status has
 * changed since the last check, a message is displayed to that effect.  If
 * the person is not on the notify list, this call is ignored 
 */
void notify_mark(nick, channel, away, flag)
char *nick;
char *channel;
char *away;
int flag;
{
    NotifyList *tmp;

    if (tmp = (NotifyList *) list_lookup(&notify_list, nick, !USE_WILDCARDS, !REMOVE_FROM_LIST)) {
	if (flag) {
	    if (tmp->flag != 1) {
		tmp->flag = 1;
		if (do_hook(NOTIFY_SIGNON_LIST, nick, channel))
		    put_it("*** Signon: %s is on channel %s", nick, channel);
	    } else if (tmp->verbose && stricmp(channel, tmp->channel)) {
		if (do_hook(NOTIFY_CHANGE_LIST, nick, channel))
		    put_it("*** Change: %s is now on channel %s", nick, channel);
	    }
	    malloc_strcpy(&(tmp->channel), channel);
	    malloc_strcpy(&(tmp->away), away);
	} else {
	    if (tmp->flag == 1) {
		tmp->flag = 0;
		if (do_hook(NOTIFY_SIGNOFF_LIST, nick, empty_string))
		    put_it("*** Signoff: %s", nick);
	    } else if (tmp->flag == -1)
		tmp->flag = 0;
	}
    }
}

void save_notify(fp)
FILE *fp;
{
    NotifyList *tmp;
    char first = 1;
    char *ptr;

    if (notify_list) {
	ptr = DEFAULT_CMDCHARS;
	fprintf(fp, "%cNOTIFY ", ptr[0]);
	for (tmp = notify_list; tmp; tmp = tmp->next) {
	    if (first)
		first = 0;
	    else
		fprintf(fp, " ");
	    fprintf(fp, "%s%s", tmp->verbose ? "+" : empty_string, tmp->nick);
	}
	fprintf(fp, "\n");
    }
}
