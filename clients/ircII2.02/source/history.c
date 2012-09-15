/*
 * history.c: stuff to handle command line history 
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
#include "ircaux.h"
#include "vars.h"
#include "history.h"

static FILE *hist_file = null(FILE *);

typedef struct HistoryStru {
    int number;
    char *stuff;
    struct HistoryStru *next;
    struct HistoryStru *prev;
}           History;

/* command_history: pointer to head of command_history list */
static History *command_history_head = null(History *);
static History *command_history_tail = null(History *);
static History *command_history_pos = null(History *);

/* hist_size: the current size of the command_history array */
static int hist_size = 0;

/* hist_count: the absolute counter for the history list */
static int hist_count = 0;

/*
 * last_dir: the direction (next or previous) of the last get_from_history()
 * call.... reset by add to history 
 */
static int last_dir = PREV;

/*
 * file_pos: The position in the history file of the last history entry
 * zwooped out by get_from_history_file()... look there for how this is used 
 */
static long file_pos = 0;

/*
 * history_match: using wild_match(), this finds the latest match in the
 * history file and returns it as the function result.  Returns null if there
 * is no match.  Note that this sticks a '*' at the end if one is not already
 * there. 
 */
static char *history_match(match)
char *match;
{
    char *ptr;
    char *match_str = null(char *);

    if (*(match + strlen(match) - 1) == '*')
	malloc_strcpy(&match_str, match);
    else {
	match_str = (char *) new_malloc(strlen(match) + 2);
	strcpy(match_str, match);
	strcat(match_str, "*");
    }
    if (hist_file) {
	if (last_dir == -1)
	    fseek(hist_file, 0, 2);
	else
	    fseek(hist_file, file_pos, 0);
	while (rfgets(buffer, BUFFER_SIZE, hist_file)) {
	    parse_history(buffer, &ptr);
	    if (wild_match(match_str, ptr)) {
		new_free(&match_str);
		*(ptr + strlen(ptr) - 1) = null(char);
		file_pos = ftell(hist_file);
		last_dir = PREV;
		return (ptr);
	    }
	}
	last_dir = -1;
	file_pos = 0;
    } else if (get_int_var(HISTORY_VAR)) {
	static History *tmp;

	if (last_dir == -1)
	    tmp = command_history_head;
	else
	    tmp = tmp->next;
	for (; tmp; tmp = tmp->next) {
	    if (wild_match(match_str, tmp->stuff)) {
		new_free(&match_str);
		last_dir = PREV;
		return (tmp->stuff);
	    }
	}
	last_dir = -1;
    }
    new_free(&match_str);
    return (null(char *));
}

/*
 * add_to_history_file: This adds the given line of text to the end of the
 * history file using cnt as the history index number. 
 */
static void add_to_history_file(cnt, line)
int cnt;
char *line;
{
    if (hist_file) {
	fseek(hist_file, 0, 2);
	fprintf(hist_file, "%d: %s\n", cnt, line);
	fflush(hist_file);
	file_pos = ftell(hist_file);
    }
}

static void add_to_history_list(cnt, stuff)
char *stuff;
int cnt;
{
    History *new;

    if (get_int_var(HISTORY_VAR) == 0)
	return;
    if ((hist_size == get_int_var(HISTORY_VAR)) && command_history_tail) {
	new = command_history_tail;
	command_history_tail = command_history_tail->prev;
	command_history_tail->next = null(History *);
	new_free(&new->stuff);
	new_free(&new);
	if (command_history_tail == null(History *))
	    command_history_head = null(History *);
    } else
	hist_size++;
    new = (History *) new_malloc(sizeof(History));
    new->stuff = null(char *);
    new->number = cnt;
    new->next = command_history_head;
    new->prev = null(History *);
    malloc_strcpy(&(new->stuff), stuff);
    if (command_history_head)
	command_history_head->prev = new;
    command_history_head = new;
    if (command_history_tail == null(History *))
	command_history_tail = new;
    command_history_pos = null(History *);
}

/*
 * set_history_file: this sets the file to be used by the command history
 * function to whatever you send as file.  This expands twiddles and opens
 * the file if all is well 
 */
void set_history_file(file)
char *file;
{
    char *ptr;
    int i,
        cnt;
    History *tmp;

    if (file) {
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID) {
	    put_it("*** You are not permitted to use a HISTORY_FILE");
	    set_string_var(HISTORY_FILE_VAR, null(char *));
	    return;
	}
#endif DAEMON_UID
	if ((ptr = expand_twiddle(file)) == null(char *)) {
	    put_it("*** Bad filename");
	    set_string_var(HISTORY_FILE_VAR, null(char *));
	    return;
	}
	set_string_var(HISTORY_FILE_VAR, ptr);
	if (hist_file)
	    fclose(hist_file);
	if ((hist_file = fopen(ptr, "w+")) == null(FILE *))
	    put_it("*** Unable to open %s: %s", ptr, sys_errlist[errno]);
	else if (hist_size) {
	    cnt = hist_size;
	    tmp = command_history_tail;
	    for (i = 0; i < cnt; i++, tmp = tmp->prev)
		add_to_history_file(tmp->number, tmp->stuff);
	}
    } else if (hist_file) {
	fclose(hist_file);
	hist_file = null(FILE *);
    }
}

/*
 * set_history_size: adjusts the size of the command_history to be size. If
 * the array is not yet allocated, it is set to size and all the entries
 * nulled.  If it exists, it is resized to the new size with a realloc.  Any
 * new entries are nulled. 
 */
void set_history_size(size)
int size;
{
    int i,
        cnt;
    History *ptr;

    if (size < hist_size) {
	cnt = hist_size - size;
	for (i = 0; i < cnt; i++) {
	    ptr = command_history_tail;
	    command_history_tail = ptr->prev;
	    new_free(&(ptr->stuff));
	    new_free(&ptr);
	}
	if (command_history_tail == null(History *))
	    command_history_head = null(History *);
	else
	    command_history_tail->next = null(History *);
	hist_size = size;
    }
}

/*
 * parse_history: given a string of the form "number: stuff", this returns
 * the number as an integer and points ret to stuff 
 */
static int parse_history(buffer, ret)
char *buffer;
char **ret;
{
    char *ptr;
    int entry;

    if (ptr = index(buffer, ':')) {
	entry = atoi(buffer);
	*ret = ptr + 2;
	return (entry);
    }
    *ret = null(char *);
    return (-1);
}

/*
 * add_to_history: adds the given line to the history array.  The history
 * array is a circular buffer, and add_to_history handles all that stuff. It
 * automagically allocted and deallocated memory as needed 
 */
void add_to_history(line)
char *line;
{
    char *ptr;

    if (line && *line) {
	while (line) {
	    if (ptr = sindex(line, "\n\r"))
		*(ptr++) = null(char);
	    add_to_history_list(hist_count, line);
	    add_to_history_file(hist_count, line);
	    last_dir = PREV;
	    hist_count++;
	    line = ptr;
	}
    }
}

static char *get_from_history_file(which)
int which;
{
    char *ptr;

    if (last_dir == -1)
	last_dir = which;
    else if (last_dir != which) {
	last_dir = which;
	get_from_history(which);
    }
    if (which == NEXT) {
	fseek(hist_file, file_pos, 0);
	if (fgets(buffer, BUFFER_SIZE, hist_file) == null(char *)) {
	    file_pos = 0L;
	    fseek(hist_file, 0, 0);
	    if (fgets(buffer, BUFFER_SIZE, hist_file) == null(char *))
		return (null(char *));
	}
	file_pos = ftell(hist_file);
	*(buffer + strlen(buffer) - 1) = null(char);
	parse_history(buffer, &ptr);
	return (ptr);
    } else {
	fseek(hist_file, file_pos, 0);
	if (rfgets(buffer, BUFFER_SIZE, hist_file) == null(char *)) {
	    fseek(hist_file, 0, 2);
	    file_pos = ftell(hist_file);
	    if (rfgets(buffer, BUFFER_SIZE, hist_file) == null(char *))
		return (null(char *));
	}
	file_pos = ftell(hist_file);
	*(buffer + strlen(buffer) - 1) = null(char);
	parse_history(buffer, &ptr);
	return (ptr);
    }
}

static char *get_from_history_buffer(which)
int which;
{
    if ((get_int_var(HISTORY_VAR) == 0) || (hist_size == 0))
	return (null(char *));
    /*
     * if (last_dir != which) { last_dir = which; get_from_history(which); } 
     */
    if (which == NEXT) {
	if (command_history_pos) {
	    if (command_history_pos->prev)
		command_history_pos = command_history_pos->prev;
	    else
		command_history_pos = command_history_tail;
	} else
	    command_history_pos = command_history_tail;
	return (command_history_pos->stuff);
    } else {
	if (command_history_pos) {
	    if (command_history_pos->next)
		command_history_pos = command_history_pos->next;
	    else
		command_history_pos = command_history_head;
	} else
	    command_history_pos = command_history_head;
	return (command_history_pos->stuff);
    }
}

char *get_from_history(which)
int which;
{
    if (get_string_var(HISTORY_FILE_VAR))
	return (get_from_history_file(which));
    else
	return (get_from_history_buffer(which));
}

/* history: the /HISTORY command, shows the command history buffer. */
void history(command, args)
char *command,
    *args;
{
    int cnt = 0,
        max;
    char *value;
    History *tmp;

    put_it("*** Command History:");
    if (hist_file) {
	if (value = next_arg(args, &args)) {
	    if ((max = atoi(value)) > hist_count)
		max = 0;
	    else
		max = hist_count - max + 1;
	} else
	    max = 0;

	fseek(hist_file, 0, 0);
	while (--max > 0)
	    fgets(buffer, BUFFER_SIZE, hist_file);
	while (fgets(buffer, BUFFER_SIZE, hist_file)) {
	    *(buffer + strlen(buffer) - 1) = null(char);
	    put_it("%s", buffer);
	}
    } else if (get_int_var(HISTORY_VAR)) {
	if (value = next_arg(args, &args)) {
	    if ((max = atoi(value)) > get_int_var(HISTORY_VAR))
		max = get_int_var(HISTORY_VAR);
	} else
	    max = get_int_var(HISTORY_VAR);
	for (tmp = command_history_tail, cnt = 0; tmp && (cnt < max); tmp = tmp->prev, cnt++) {
	    put_it("%d: %s", tmp->number, tmp->stuff);
	}
    }
}

/*
 * do_history: This finds the given history entry in either the history file,
 * or the in memory history buffer (if no history file is given). It then
 * returns the found entry as its function value, or null if the entry is not
 * found for some reason.  Note that this routine mallocs the string returned  
 */
char *do_history(com, rest)
char *com;
char *rest;
{
    int hist_num;
    char *ptr,
        *ret = null(char *);
    static char *last_com = null(char *);

    if ((*com == null(char)) || (com == null(char *))) {
	if (last_com)
	    com = last_com;
    } else {
	last_dir = -1;
	malloc_strcpy(&last_com, com);
    }
    if (!is_number(com)) {
	if (ptr = history_match(com)) {
	    if (rest && *rest) {
		ret = (char *) new_malloc(strlen(ptr) + strlen(rest) + 2);
		strcpy(ret, ptr);
		strcat(ret, " ");
		strcat(ret, rest);
	    } else
		malloc_strcpy(&ret, ptr);
	} else
	    put_it("*** No Match");
	return (ret);
    }
    hist_num = atoi(com);
    if (hist_file) {
	fseek(hist_file, 0, 0);
	while (fgets(buffer, BUFFER_SIZE, hist_file)) {
	    if (parse_history(buffer, &ptr) == hist_num) {
		*(ptr + strlen(ptr) - 1) = null(char);
		if (rest && *rest) {
		    ret = (char *) new_malloc(strlen(ptr) + strlen(rest) + 2);
		    strcpy(ret, ptr);
		    strcat(ret, " ");
		    strcat(ret, rest);
		} else
		    malloc_strcpy(&ret, ptr);
		last_dir = PREV;
		file_pos = ftell(hist_file);
		return (ret);
	    }
	}
	put_it("*** No such history entry: %d", hist_num);
	file_pos = 0;
	return (null(char *));
    } else {
	History *tmp;

	for (tmp = command_history_head; tmp; tmp = tmp->next) {
	    if (tmp->number == hist_num) {
		if (rest && *rest) {
		    ret = (char *) new_malloc(strlen(tmp->stuff) + strlen(rest) + 2);
		    strcpy(ret, tmp->stuff);
		    strcat(ret, " ");
		    strcat(ret, rest);
		} else
		    malloc_strcpy(&ret, tmp->stuff);
		return (ret);
	    }
	}
	put_it("*** No such history entry: %d", hist_num);
	return (null(char *));
    }
}
