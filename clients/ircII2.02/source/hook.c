/*
 * hook.c: Does those naughty hook functions. 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "config.h"
#include "irc.h"
#include "hook.h"
#include "vars.h"
#include "ircaux.h"
#include "alias.h"
#include "list.h"

#define SILENT 0
#define QUIET 1
#define NORMAL 2
#define NOISY 3

/*
 * The various ON levels: SILENT means the DISPLAY will be OFF and it will
 * suppress the default action of the event, QUIET means the display will be
 * OFF but the default action will still take place, NORMAL means you will be
 * notified when an action takes place and the default action still occurs,
 * NOISY means you are notified when an action occur plus you see the action
 * in the display and the default actions still occurs 
 */
static char *noise_level[] = {"SILENT", "QUIET", "NORMAL", "NOISY"};

/* Hook: The structure of the entries of the hook functions lists */
typedef struct hook_stru {
    struct hook_stru *next;	/* pointer to next element in list */
    char *nick;			/* The Nickname */
    char not;			/* If true, this entry should be ignored when
				 * matched, otherwise it is a normal entry */
    char noisy;			/* If true, gives verbose output, otherwise
				 * no indication on activation of a hook
				 * function */
    char *stuff;		/* The this that gets done */
}         Hook;

/* HookFunc: A little structure to keep track of the various hook functions */
typedef struct {
    char *name;			/* name of the function */
    Hook *list;			/* pointer to head of the list for this
				 * function */
}      HookFunc;

/* hook_functions: the list of all hook functions available */
static HookFunc hook_functions[] =
{"CONNECT", null(Hook *),
 "INVITE", null(Hook *),
 "JOIN", null(Hook *),
 "LEAVE", null(Hook *),
 "LIST", null(Hook *),
 "MAIL_IRC", null(Hook *),
 "MAIL_REAL", null(Hook *),
 "MSG", null(Hook *),
 "NAMES", null(Hook *),
 "NOTICE", null(Hook *),
 "NOTIFY_CHANGE", null(Hook *),
 "NOTIFY_SIGNOFF", null(Hook *),
 "NOTIFY_SIGNON", null(Hook *),
 "PUBLIC", null(Hook *),
 "PUBLIC_MSG", null(Hook *),
 "PUBLIC_NOTICE", null(Hook *),
 "PUBLIC_OTHER", null(Hook *),
 "SEND_MSG", null(Hook *),
 "SEND_NOTICE", null(Hook *),
 "SEND_PUBLIC", null(Hook *),
 "TIMER", null(Hook *),
 "TOPIC", null(Hook *),
 "WALL", null(Hook *),
 "WALLOP", null(Hook *),
 "WHO", null(Hook *)
};

/*
 * add_hook: Given an index into the hook_functions array, this adds a new
 * entry to the list as specified by the rest of the parameters.  The new
 * entry is added in alphabetical order (by nick). 
 */
static void add_hook(which, nick, stuff, noisy, not)
int which;
char *nick,
    *stuff,
     noisy,
     not;
{
    Hook *new;

    if (new = (Hook *) remove_from_list(&(hook_functions[which].list), nick)){
	new_free(&(new->nick));
	new_free(&(new->stuff));
	new_free(&new);
    }
    new = (Hook *) new_malloc(sizeof(Hook));
    new->nick = null(char *);
    new->noisy = noisy;
    new->not = not;
    new->stuff = null(char *);
    malloc_strcpy(&new->nick, nick);
    malloc_strcpy(&new->stuff, stuff);
    upper(new->nick);
    add_to_list(&(hook_functions[which].list), new, sizeof(Hook));
}

/*
 * show_list: Displays the contents of the list specified by the index into
 * the hook_functions array 
 */
static void show_list(which)
int which;
{
    Hook *list;

    /* Less garbage when issueing /on without args. (lynx) */
    for (list = hook_functions[which].list; list; list = list->next)
	put_it("*** On %s from %s do %s [%s]", hook_functions[which].name, list->nick, (list->not ? "nothing" : list->stuff), noise_level[list->noisy]);
}

/*
 * do_hook: This is what gets called whenever a MSG, INVITES, WALL, (you get
 * the idea) occurs.  The nick is looked up in the appropriate list. If a
 * match is found, the stuff field from that entry in the list is treated as
 * if it were a command. First it gets expanded as though it were an alias
 * (with the args parameter used as the arguments to the alias).  After it
 * gets expanded, it gets parsed as a command.  This will return as its value
 * the value of the noisy field of the found entry, or -1 if not found. 
 */
int do_hook(which, nick, args)
int which;
char *nick,
    *args;
{
    Hook *tmp;
    char buffer[BUFFER_SIZE + 1];
    int display;

    if (tmp = (Hook *) find_in_list(&(hook_functions[which].list), nick, 1)) {
	char *ptr;

	if ((stricmp(nick, nickname) == 0) && stricmp(nick, tmp->nick))
	    return;
	if (tmp->not)
	    return;
	send_text_flag = which;
	if (tmp->noisy > QUIET)
	    put_it("*** %s activated by %s", hook_functions[which].name, nick);
	display = get_int_var(DISPLAY_VAR);
	if (tmp->noisy < NOISY)
	    set_int_var(DISPLAY_VAR, 0);
	sprintf(buffer, "%s %s", nick, args);
	ptr = expand_alias(null(char *), tmp->stuff, buffer);
	save_message_from();
	parse_command(ptr, 0);
	new_free(&ptr);
	set_int_var(DISPLAY_VAR, display);
	send_text_flag = -1;
	restore_message_from();
	return (tmp->noisy);
    }
    return (-1);
}

/* on: The ON command */
void on(command, args)
char *command,
    *args;
{
    char *func,
        *nick,
         noisy = NORMAL,
         not = 0;
    int len,
        which,
        cnt,
        i;

    if (func = next_arg(args, &args)) {
	switch (*func) {
	    case '-':
		noisy = QUIET;
		func++;
		break;
	    case '^':
		noisy = SILENT;
		func++;
		break;
	    case '+':
		noisy = NOISY;
		func++;
		break;
	    default:
		noisy = NORMAL;
		break;
	}
	if ((len = strlen(func)) == 0) {
	    put_it("*** You must specify an event type!");
	    return;
	}
	for (cnt = 0, i = 0; i < NUMBER_OF_LISTS; i++) {
	    if (strnicmp(func, hook_functions[i].name, len) == 0) {
		if (strlen(hook_functions[i].name) == len) {
		    cnt = 1;
		    which = i;
		    break;
		} else {
		    cnt++;
		    which = i;
		}
	    } else if (cnt)
		break;
	}
	if (cnt == 0) {
	    put_it("*** No such ON function: %s", func);
	    return;
	} else if (cnt > 1) {
	    put_it("*** Ambiguous ON function: %s", func);
	    return;
	}
	if (nick = next_arg(args, &args)) {
	    char *ptr;

	    while (nick) {
		if (ptr = index(nick, ','))
		    *(ptr++) = null(char);
		not = 0;
		switch (*nick) {
		    case '-':
			if (*args) {
			    add_hook(which, nick, args, noisy, not);
			    put_it("*** On %s from %s do %s [%s]", hook_functions[which].name, nick, (not ? "nothing" : args), noise_level[noisy]);
			} else {
			    if (strlen(++nick) == 0)
				put_it("*** No nickname specified!");
			    else {
				Hook *tmp;

				if (tmp = (Hook *) remove_from_list(&(hook_functions[which].list), nick)) {
				    put_it("*** %s removed from %s list", nick, hook_functions[which].name);
				    new_free(&(tmp->nick));
				    new_free(&(tmp->stuff));
				    new_free(&tmp);
				} else
				    put_it("*** %s is not on the %s list", nick, hook_functions[which].name);
			    }
			}
			break;
		    case '^':
			not = 1;
			args = empty_string;
			nick++;
		    default:
			if (*nick) {
			    add_hook(which, nick, args, noisy, not);
			    put_it("*** On %s from %s do %s [%s]", hook_functions[which].name, nick, (not ? "nothing" : args), noise_level[noisy]);
			} else
			    put_it("*** No nickname specified!");
			break;
		}
		nick = ptr;
	    }
	} else
	    show_list(which);
    } else {
	put_it("*** ON listings:");
	for (which = 0; which < NUMBER_OF_LISTS; which++)
	    show_list(which);
    }
}

/*
 * save_hooks: for use by the SAVE command to write the hooks to a file so it
 * can be interpreted by the LOAD command 
 */
void save_hooks(fp)
FILE *fp;
{
    Hook *list;
    int which;
    char *ptr,
        *stuff;

    for (which = 0; which < NUMBER_OF_LISTS; which++) {
	for (list = hook_functions[which].list; list; list = list->next) {
	    ptr = DEFAULT_CMDCHARS;
	    switch (list->noisy) {
		case SILENT:
		    stuff = "^";
		    break;
		case QUIET:
		    stuff = "-";
		    break;
		case NORMAL:
		    stuff = empty_string;
		    break;
		case NOISY:
		    stuff = "+";
		    break;
	    }
	    fprintf(fp, "%cON %s%s %s", ptr[0], stuff, hook_functions[which].name, list->nick, list->stuff);
	    for (ptr = buffer, stuff = list->stuff; *stuff; stuff++) {
		if (*stuff == QUOTE_CHAR)
		    *(ptr++) = QUOTE_CHAR;
		*(ptr++) = *stuff;
	    }
	    *ptr = null(char);
	    fprintf(fp, " %s\n", buffer);
	}
    }
}
