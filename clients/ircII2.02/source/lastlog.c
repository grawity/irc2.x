/*
 * lastlog.c: handles the lastlog features of irc. 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <ctype.h>
#include "irc.h"
#include "lastlog.h"
#include "window.h"
#include "vars.h"
#include "ircaux.h"

/*
 * lastlog_level: current bitmap setting of which things should be stored in
 * the lastlog.  The LOG_MSG, LOG_NOTICE, etc., defines tell more about this 
 */
static int lastlog_level;

/*
 * msg_level: the mask for the current message level.  What?  Did he really
 * say that?  This is set in the set_lastlog_msg_level() routine as it
 * compared to the lastlog_level variable to see if what ever is being added
 * should actually be added 
 */
static int msg_level = LOG_CRAP;

#define NUMBER_OF_LEVELS 7
static char *levels[] =
{"CRAP", "PUBLIC", "MSG", "NOTICE", "WALL", "WALLOP", "MAIL"};


/*
 * bits_to_lastlog_level: converts the bitmap of lastlog levels into a nice
 * string format.  Note that this uses the global buffer, so watch out 
 */
char *bits_to_lastlog_level(level)
{
    int i,
        p;

    if (level == LOG_ALL)
	strcpy(buffer, "ALL");
    else if (level == 0)
	strcpy(buffer, "NONE");
    else {
	strcpy(buffer, "");
	for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p *= 2) {
	    if (level & p) {
		strcat(buffer, levels[i]);
		strcat(buffer, " ");
	    }
	}
    }
    return (buffer);
}

int parse_lastlog_level(str)
char *str;
{
    char *ptr,
        *rest;
    int len,
        i,
        p,
        level;
    char neg;

    level = 0;
    while (str = next_arg(str, &rest)) {
	while (str) {
	    if (ptr = index(str, ','))
		*(ptr++) = null(char);
	    if (len = strlen(str)) {
		if (strnicmp(str, "ALL", len) == 0)
		    level = LOG_ALL;
		else if (strnicmp(str, "NONE", len) == 0)
		    level = 0;
		else {
		    if (*str == '-') {
			str++;
			neg = 1;
		    } else
			neg = 0;
		    for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p *= 2) {
			if (strnicmp(str, levels[i], len) == 0) {
			    if (neg)
				level &= (LOG_ALL ^ p);
			    else
				level |= p;
			    break;
			}
		    }
		    if (i == NUMBER_OF_LEVELS)
			put_it("*** Unknown lastlog level: %s", str);
		}
	    }
	    str = ptr;
	}
	str = rest;
    }
    return (level);
}

/*
 * set_lastlog_level: called whenever a "SET LASTLOG_LEVEL" is done.  It
 * parses the settings and sets the lastlog_level variable appropriately.  It
 * also rewrites the LASTLOG_LEVEL variable to make it look nice 
 */
void set_lastlog_level(str)
char *str;
{
    lastlog_level = parse_lastlog_level(str);
    set_string_var(LASTLOG_LEVEL_VAR, bits_to_lastlog_level(lastlog_level));
    current_window->lastlog_level = lastlog_level;
}

static void remove_from_lastlog(window)
Window *window;
{
    Lastlog *tmp;

    if (window->lastlog_tail) {
	tmp = window->lastlog_tail->prev;
	new_free(&window->lastlog_tail->msg);
	new_free(&window->lastlog_tail);
	window->lastlog_tail = tmp;
	if (tmp)
	    tmp->next = null(Lastlog *);
	else
	    window->lastlog_head = window->lastlog_tail;
	window->lastlog_size--;
    } else
	window->lastlog_size = 0;
}

/*
 * set_lastlog_size: sets up a lastlog buffer of size given.  If the lastlog
 * has gotten larger than it was before, all previous lastlog entry remain.
 * If it get smaller, some are deleted from the end. 
 */
void set_lastlog_size(size)
int size;
{
    int i,
        diff;

    if (current_window->lastlog_size > size) {
	diff = current_window->lastlog_size - size;
	for (i = 0; i < diff; i++)
	    remove_from_lastlog(current_window);
    }
}

/*
 * lastlog: the /LASTLOG command.  Displays the lastlog to the screen. If
 * args contains a valid integer, only that many lastlog entries are shown
 * (if the value is less than lastlog_size), otherwise the entire lastlog is
 * displayed 
 */
void lastlog(command, args)
char *command,
    *args;
{
    int cnt,
        from = 0,
        p,
        i,
        level = 0,
        msg_level,
        len,
        mask = 0;
    Lastlog *start_pos;
    char *match = null(char *),
        *arg,
         header = 1;

    message_from(null(char *), LOG_CURRENT);
    cnt = current_window->lastlog_size;
    while (arg = next_arg(args, &args)) {
	if (*arg == '-') {
	    arg++;
	    if ((len = strlen(arg)) == 0)
		header = 0;
	    else {
		for (i = 0, p = 1; i < NUMBER_OF_LEVELS; i++, p *= 2) {
		    if (strnicmp(levels[i], arg, len) == 0) {
			mask |= p;
			break;
		    }
		}
		if (i == NUMBER_OF_LEVELS) {
		    put_it("*** Unknown flag: %s", arg);
		    message_from(null(char *), LOG_CRAP);
		    return;
		}
	    }
	} else {
	    if (level == 0) {
		if (isdigit(*arg))
		    cnt = atoi(arg);
		else
		    match = arg;
		level++;
	    } else if (level == 1) {
		level++;
		from = atoi(arg);
	    }
	}
    }
    start_pos = current_window->lastlog_head;
    for (i = 0; (i < from) && start_pos; start_pos = start_pos->next) {
	if ((mask == 0) || (mask & start_pos->level))
	    i++;
    }

    for (i = 0; (i < cnt) && start_pos; start_pos = start_pos->next) {
	if ((mask == 0) || (mask & start_pos->level))
	    i++;
    }

    level = current_window->lastlog_level;
    msg_level = set_lastlog_msg_level(0);
    if (start_pos == null(Lastlog *))
	start_pos = current_window->lastlog_tail;
    else
	start_pos = start_pos->prev;

    /* Let's not get confused here, display a seperator.. -lynx */
    if (header)
	put_it("*** Lastlog:");
    for (i = 0; (i < cnt) && start_pos; start_pos = start_pos->prev) {
	if ((mask == 0) || (mask & start_pos->level)) {
	    i++;
	    if (match) {
		if (scanstr(start_pos->msg, match))
		    put_it("%s", start_pos->msg);
	    } else
		put_it("%s", start_pos->msg);
	}
    }
    if (header)
	put_it("*** End of Lastlog");
    current_window->lastlog_level = level;
    set_lastlog_msg_level(msg_level);
    message_from(null(char *), LOG_CRAP);
}

/* set_lastlog_msg_level: sets the message level for recording in the lastlog */
int set_lastlog_msg_level(level)
int level;
{
    int old;

    old = msg_level;
    msg_level = level;
    return (old);
}

/*
 * add_to_lastlog: adds the line to the lastlog.  If the LASTLOG_CONVERSATION
 * variable is on, then only those lines that are user messages (private
 * messages, channel messages, wall's, and any outgoing messages) are
 * recorded, otherwise, everything is recorded 
 */
void add_to_lastlog(window, line)
Window *window;
char *line;
{
    Lastlog *new;

    if (window == null(Window *))
	window = current_window;
    if (window->lastlog_level & msg_level) {
	/* no nulls or empty lines (they contain "> ") */
	if (line && (strlen(line) > 2)) {
	    new = (Lastlog *) new_malloc(sizeof(Lastlog));
	    new->next = window->lastlog_head;
	    new->prev = null(Lastlog *);
	    new->level = msg_level;
	    new->msg = null(char *);
	    malloc_strcpy(&(new->msg), line);

	    if (window->lastlog_head)
		window->lastlog_head->prev = new;
	    window->lastlog_head = new;

	    if (window->lastlog_tail == null(Lastlog *))
		window->lastlog_tail = window->lastlog_head;

	    if (window->lastlog_size++ == get_int_var(LASTLOG_VAR)) {
		remove_from_lastlog(window);
	    }
	}
    }
}

int real_lastlog_level()
{
        return (lastlog_level);
}
