/*
 * lastlog.c: handles the lastlog features of irc. 
 *
 * Written by Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <ctype.h>
#include "irc.h"

/* lastlog_buffer: keeps all the lastlog stuff */
static char **lastlog_buffer = null(char **);

/* lastlog_size: number of elements in the lastlog buffer */
static int lastlog_size = 0;

/* lastlog_pos: the insertion position in the lastlog_buffer */
static int lastlog_pos = 0;

/* lastlog_on: if 0, lastlogging is disabled */
static char lastlog_on = 1;

/*
 * set_lastlog_size: sets up a lastlog buffer of size given.  If the lastlog
 * has gotten larger than it was before, all previous lastlog entry remain.
 * If it get smaller, some are deleted from the end. 
 */
void set_lastlog_size(size)
int size;
{
    int i;

    if (lastlog_buffer)
	lastlog_buffer = (char **) realloc(lastlog_buffer, size * sizeof(char *));
    else
	lastlog_buffer = (char **) malloc(size * sizeof(char *));
    lastlog_pos = 0;
    if (lastlog_buffer == null(char **)) {
	put_it("*** Can't allocate enough memory for lastlog!");
	set_var_value(LASTLOG_VAR, "0");
	lastlog_size = 0;
	return;
    }
    if (size > lastlog_size) {
	for (i = lastlog_size; i < size; i++)
	    lastlog_buffer[i] = null(char *);
    }
    lastlog_size = size;
    if (lastlog_pos >= lastlog_size)
	lastlog_pos = 0;
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
    int cnt = 0,
        from = 0,
        start_pos,
        pos;
    char *arg1,
        *arg2;

    lastlog_on = 0;
    if (arg1 = next_arg(args, &args)) {
	if (isdigit(*arg1)) {
	    cnt = atoi(arg1);
	    arg1 = null(char *);
	}
	if (arg2 = next_arg(args, &args)) {
	    if ((from = atoi(arg2)) > lastlog_size) {
		put_it("*** Second argument too large, lastlog size is %d", lastlog_size);
		return;
	    }
	}
    }
    if ((cnt > (lastlog_size - from)) || (cnt == 0))
	cnt = lastlog_size - from;
    start_pos = (lastlog_size + lastlog_pos - (cnt + from) + 1) % lastlog_size;
    for (pos = start_pos; cnt; cnt--) {
	if (lastlog_buffer[pos]) {
	    if (arg1) {
		if (scanstr(lastlog_buffer[pos], arg1))
		    put_it(lastlog_buffer[pos]);
	    } else
		put_it(lastlog_buffer[pos]);
	}
	if (++pos == lastlog_size)
	    pos = 0;
    }
    lastlog_on = 1;
}

/*
 * add_to_lastlog: adds the line to the lastlog.  If the LASTLOG_CONVERSATION
 * variable is on, then only those lines that are user messages (private
 * messages, channel messages, wall's, and any outgoing messages) are
 * recorded, otherwise, everything is recorded 
 */
void add_to_lastlog(line)
char *line;
{
    if (lastlog_on) {
	/* no nulls or empty lines (they contain "> ") */
	if (line && (strlen(line) > 2)) {
	    if (get_int_var(LASTLOG_CONVERSATION_VAR)) {
		if (get_int_var(LASTLOG_MSG_ONLY_VAR)) {
		    if (!(((*line == '-') && (*(line + 1) == '>')) ||
			  ((*line == '*') && (*(line + 1) != '*'))))
			return;
		} else {
		    if (!((*line == '<') ||
			  (*line == '>') ||
			  (*line == '(') ||
			  ((*line == '#') && (*(line + 1) != '#')) ||
			  ((*line == '-') && (*(line + 1) == '>')) ||
			  ((*line == '*') && (*(line + 1) != '*'))))
			return;
		}
	    }
	    if (lastlog_size != get_int_var(LASTLOG_VAR))
		set_lastlog_size(get_int_var(LASTLOG_VAR));
	    if (lastlog_size) {
		if (++lastlog_pos == lastlog_size)
		    lastlog_pos = 0;
		malloc_strcpy(&(lastlog_buffer[lastlog_pos]), line);
	    }
	}
    }
}
