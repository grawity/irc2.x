/*
 * history.c: stuff to handle command line history 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include "irc.h"

/* command_history: the string array containing the history entries */
static char **command_history = null(char **);

/*
 * hist_pos: position of next history entry to be used by calls to
 * get_from_history() 
 */
static int hist_pos = 0;

/* hist_size: the current size of the command_history array */
static int hist_size = 0;

/*
 * hist_add_pos: the position in the command_history array where the next
 * command line will be added 
 */
static int hist_add_pos = 0;


/*
 * last_dir: the direction (next or previous) of the last get_from_history()
 * call.... reset by add to history 
 */
static int last_dir = PREV;


/*
 * set_history_size: adjusts the size of the command_history to be size.  If
 * the array is not yet allocated, it is set to size and all the entries
 * nulled.  If it exists, it is resized to the new size with a realloc.  Any
 * new entries are nulled. 
 */
void set_history_size(size)
int size;
{
    int i;

    if (command_history)
	command_history = (char **) realloc(command_history, size * sizeof(char *));
    else
	command_history = (char **) malloc(size * sizeof(char *));
    hist_pos = 0;
    if (command_history == null(char **)) {
	put_it("***: Can't allocate enough memory for history!");
	set_var_value(COMMAND_HISTORY_VAR, "0");
	hist_size = 0;
	return;
    }
    if (size > hist_size) {
	for (i = hist_size; i < size; i++)
	    command_history[i] = null(char *);
    }
    hist_size = size;
    if (hist_add_pos >= hist_size)
	hist_add_pos = 0;
}

/*
 * add_to_history: adds the given line to the history array.  The history
 * array is a circular buffer, and add_to_history handles all that stuff.  It
 * automagically allocted and deallocated memory as needed 
 */
void add_to_history(line)
char *line;
{
    if (line && *line) {
	if (hist_size) {
	    malloc_strcpy(&(command_history[hist_add_pos]), line);
	    hist_pos = hist_add_pos;
	    if (++hist_add_pos == get_int_var(COMMAND_HISTORY_VAR))
		hist_add_pos = 0;
	    last_dir = PREV;
	}
    }
}

/*
 * get_from_history: gets the next history entry and returns it as its
 * funtion value.  Which can either be NEXT, returning the next history
 * entry, or PREV, returning the previous entry 
 */
char *get_from_history(which)
int which;
{
    if (hist_size != get_int_var(COMMAND_HISTORY_VAR))
	set_history_size(get_int_var(COMMAND_HISTORY_VAR));
    if (hist_size == 0)
	return (null(char *));
    if (last_dir != which) {
	last_dir = which;
	get_from_history(which);
    }
    if (which == NEXT) {
	if (++hist_pos == get_int_var(COMMAND_HISTORY_VAR))
	    hist_pos = 0;
	if (command_history[hist_pos] == null(char *)) {
	    if (hist_pos == 0)
		return (null(char *));
	    hist_pos = 0;
	}
	return (command_history[hist_pos]);
    } else {
	int foo;

	foo = hist_pos;
	if (hist_pos == 0) {
	    hist_pos = get_int_var(COMMAND_HISTORY_VAR) - 1;
	    while (command_history[hist_pos] == null(char *)) {
		if (hist_pos == 0)
		    return (null(char *));
		hist_pos--;
	    }
	} else
	    hist_pos--;
	return (command_history[foo]);
    }
}

/* history: the /HISTORY command, shows the command history buffer. */
void history(command, args)
char *command,
    *args;
{
    int pos,
        cnt = 0,
        max;
    char *value;

    if (value = next_arg(args, &args)) {
	if ((max = atoi(value)) > hist_size)
	    max = hist_size;
    } else
	max = hist_size;
    pos = hist_pos;
    while (command_history[pos] && (cnt < max)) {
	put_it("%d: %s", cnt++, command_history[pos]);
	if (pos-- == 0)
	    pos = hist_size - 1;
    }
}
