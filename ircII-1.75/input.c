/*
 * input.c: does the actual input line stuff... keeps the appropriate stuff
 * on the input line, handles insert/delete of characters/words... the whole
 * ball o wax 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <ctype.h>
#include "irc.h"
#include "term.h"

#define WIDTH 10

#define INPUT_BUFFER_SIZE 256

/* input_buffer: where all that input is kept */
static char input_buffer[INPUT_BUFFER_SIZE + 1];

/* buffer_pos: position of curson in input_buffer, not screen cursor */
static int buffer_pos;

/* input_line: the actual screen line where the input goes */
static int input_line;

static char *cut_buffer = null(char *);

static int str_start = 0;
static int lower_mark;
static int upper_mark;
static int cursor = 0;
static int zone;

/* cursor_to_input: move the cursor to the input line, if not there already */
void cursor_to_input()
{
    if   (cursor_in_display) {
	     term_move_cursor(cursor, input_line);
	cursor_in_display = 0;
    }
}

/*
 * update_input: does varying amount of updating on the input line depending
 * upon the position of the cursor and the update flag.  If the cursor has
 * move toward one of the edge boundaries on the screen, update_cursor()
 * flips the input line to the next (previous) line of text. The update flag
 * may be: NO_UPDATE - only do the above bounds checking. UPDATE_JUST_CURSOR
 * - do bounds checking and position cursor where is should be.
 * UPDATE_FROM_CURSOR - does all of the above, and makes sure everything from
 * the cursor to the right edge of the screen is current (by redrawing it).
 * UPDATE_ALL - redraws the entire line 
 */
void update_input(update)
char update;
{
    int old_start;
    static int co = 0,
        li = 0;

    cursor_to_input();
    if ((li != LI) || (co != CO)) {
	/* resized?  Keep it simple and reset everything */
	input_line = LI - 1;
	zone = CO - (WIDTH * 2);
	lower_mark = WIDTH;
	upper_mark = CO - WIDTH;
	cursor = 0;
	buffer_pos = 0;
	str_start = 0;
	li = LI;
	co = CO;
    }
    old_start = str_start;
    while ((buffer_pos < lower_mark) && (lower_mark > WIDTH)) {
	upper_mark = lower_mark;
	lower_mark -= zone;
	str_start -= zone;
    }
    while (buffer_pos >= upper_mark) {
	lower_mark = upper_mark;
	upper_mark += zone;
	str_start += zone;
    }
    cursor = buffer_pos - str_start;
    if ((old_start != str_start) || (update == UPDATE_ALL)) {
	int len;

	term_move_cursor(0, input_line);
	if ((len = strlen(&(input_buffer[str_start]))) > CO)
	    len = CO;
	term_puts(&(input_buffer[str_start]), len);
	term_clear_to_eol(len, input_line);
	term_move_cursor(cursor, input_line);
    } else if (update == UPDATE_FROM_CURSOR) {
	int len,
	    max;

	term_move_cursor(cursor, input_line);
	max = CO - (buffer_pos - str_start);
	if ((len = strlen(&(input_buffer[buffer_pos]))) > max)
	    len = max;
	term_puts(&(input_buffer[buffer_pos]), len);
	term_clear_to_eol(len, input_line);
	term_move_cursor(cursor, input_line);
    } else if (update == UPDATE_JUST_CURSOR)
	term_move_cursor(cursor, input_line);
}

void move_cursor(dir)
int dir;
{
    cursor_to_input();
    if (dir) {
	if (input_buffer[buffer_pos]) {
	    buffer_pos++;
	    if (term_cursor_right())
		term_move_cursor(cursor + 1, input_line);
	}
    } else {
	if (buffer_pos) {
	    buffer_pos--;
	    if (term_cursor_left())
		term_move_cursor(cursor - 1, input_line);
	}
    }
    update_input(NO_UPDATE);
}

void forward_word()
{
         cursor_to_input();
    while ((isspace(input_buffer[buffer_pos]) ||
	    ispunct(input_buffer[buffer_pos])) &&
	   input_buffer[buffer_pos])
	buffer_pos++;
    while (!(ispunct(input_buffer[buffer_pos]) ||
	     isspace(input_buffer[buffer_pos])) &&
	   input_buffer[buffer_pos])
	buffer_pos++;
    update_input(UPDATE_JUST_CURSOR);
}

void backward_word()
{
         cursor_to_input();
    while (buffer_pos &&
	   (isspace(input_buffer[buffer_pos - 1]) ||
	    ispunct(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    while (buffer_pos &&
	   !(ispunct(input_buffer[buffer_pos - 1]) ||
	     isspace(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    update_input(UPDATE_JUST_CURSOR);
}

void delete()
{
         cursor_to_input();
    if (input_buffer[buffer_pos]) {
	char *ptr = null(char *);
	int pos;

	malloc_strcpy(&ptr, &(input_buffer[buffer_pos + 1]));
	strcpy(&(input_buffer[buffer_pos]), ptr);
	free(ptr);
	if (term_delete())
	    update_input(UPDATE_FROM_CURSOR);
	else {
	    pos = str_start + CO - 1;
	    if (pos < strlen(input_buffer)) {
		term_move_cursor(CO - 1, input_line);
		term_putchar(input_buffer[pos]);
		term_move_cursor(cursor, input_line);
	    }
	    update_input(NO_UPDATE);
	}
    }
}

void backspace()
{
         cursor_to_input();
    if (buffer_pos) {
	char *ptr = null(char *);
	int pos;

	malloc_strcpy(&ptr, &(input_buffer[buffer_pos]));
	strcpy(&(input_buffer[buffer_pos - 1]), ptr);
	free(ptr);
	buffer_pos--;
	if (term_cursor_left())
	    term_move_cursor(cursor - 1, input_line);
	if (input_buffer[buffer_pos]) {
	    if (term_delete()) {
		update_input(UPDATE_FROM_CURSOR);
		return;
	    } else {
		pos = str_start + CO - 1;
		if (pos < strlen(input_buffer)) {
		    term_move_cursor(CO - 1, input_line);
		    term_putchar(input_buffer[pos]);
		}
		update_input(NO_UPDATE);
	    }
	} else {
	    term_putchar(' ');
	    if (term_cursor_left())
		term_move_cursor(cursor - 1, input_line);
	    update_input(NO_UPDATE);
	}
    }
}

void beginning_of_line()
{
         cursor_to_input();
    buffer_pos = 0;
    update_input(UPDATE_JUST_CURSOR);
}

void end_of_line()
{
         cursor_to_input();
    buffer_pos = strlen(input_buffer);
    update_input(UPDATE_JUST_CURSOR);
}

void delete_previous_word()
{
    int old_pos;
    char c;

    cursor_to_input();
    old_pos = buffer_pos;
    while (buffer_pos &&
	   (isspace(input_buffer[buffer_pos - 1]) ||
	    ispunct(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    while (buffer_pos &&
	   !(ispunct(input_buffer[buffer_pos - 1]) ||
	     isspace(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    c = input_buffer[old_pos];
    input_buffer[old_pos] = null(char);
    malloc_strcpy(&cut_buffer, &(input_buffer[buffer_pos]));
    input_buffer[old_pos] = c;
    strcpy(&(input_buffer[buffer_pos]), &(input_buffer[old_pos]));
    update_input(UPDATE_FROM_CURSOR);
}

void delete_next_word()
{
    int pos;
    char *ptr = null(char *), c;

    cursor_to_input();
    pos = buffer_pos;
    while ((isspace(input_buffer[pos]) ||
	    ispunct(input_buffer[pos])) &&
	   input_buffer[pos])
	pos++;
    while (!(ispunct(input_buffer[pos]) ||
	     isspace(input_buffer[pos])) &&
	   input_buffer[pos])
	pos++;
    c = input_buffer[pos];
    input_buffer[pos] = null(char);
    malloc_strcpy(&cut_buffer, &(input_buffer[buffer_pos]));
    input_buffer[pos] = c;
    malloc_strcpy(&ptr, &(input_buffer[pos]));
    strcpy(&(input_buffer[buffer_pos]), ptr);
    free(ptr);
    update_input(UPDATE_FROM_CURSOR);
}

void add_ch(c)
char c;
{
    char display_flag = NO_UPDATE;

    cursor_to_input();
    if (buffer_pos < INPUT_BUFFER_SIZE) {
	if (get_int_var(INSERT_MODE_VAR)) {
	    if (input_buffer[buffer_pos]) {
		char *ptr = null(char *);

		malloc_strcpy(&ptr, &(input_buffer[buffer_pos]));
		input_buffer[buffer_pos] = c;
		input_buffer[buffer_pos + 1] = null(char);
		strmcat(input_buffer, ptr, INPUT_BUFFER_SIZE);
		free(ptr);
		if (term_insert(c)) {
		    term_putchar(c);
		    if (input_buffer[buffer_pos + 1])
			display_flag = UPDATE_FROM_CURSOR;
		    else
			display_flag = NO_UPDATE;
		}
	    } else {
		input_buffer[buffer_pos] = c;
		input_buffer[buffer_pos + 1] = null(char);
		term_putchar(c);
	    }
	} else {
	    if (input_buffer[buffer_pos] == null(char))
		input_buffer[buffer_pos + 1] = null(char);
	    input_buffer[buffer_pos] = c;
	    term_putchar(c);
	}
	buffer_pos++;
	update_input(display_flag);
    }
}

void set_input(str)
char *str;
{
    strncpy(input_buffer, str, INPUT_BUFFER_SIZE);
    buffer_pos = 0;
}

char *get_input()
{
         return (input_buffer);
}

void clear_to_eol()
{
         cursor_to_input();
    malloc_strcpy(&cut_buffer, &(input_buffer[buffer_pos]));
    input_buffer[buffer_pos] = null(char);
    term_clear_to_eol(cursor, input_line);
    update_input(NO_UPDATE);
}

void clear_to_bol()
{
         cursor_to_input();
    malloc_strcpy(&cut_buffer, input_buffer);
    *input_buffer = null(char);
    buffer_pos = 0;
    term_move_cursor(0, input_line);
    term_clear_to_eol(0, input_line);
    update_input(NO_UPDATE);
}

void transpose_characters()
{
         cursor_to_input();
    if (buffer_pos > 1) {
	char c1,
	     c2;

	c1 = input_buffer[buffer_pos - 1];
	c2 = input_buffer[buffer_pos - 1] = input_buffer[buffer_pos - 2];
	input_buffer[buffer_pos - 2] = c1;
	if (term_cursor_left())
	    term_move_cursor(cursor - 2, input_line);
	else
	    term_cursor_left();
	term_putchar(c1);
	term_putchar(c2);
	update_input(NO_UPDATE);
    }
}

void init_input()
{
        *input_buffer = null(char);
    buffer_pos = 0;
}

void yank_cut_buffer()
{
    char *ptr = null(char *);

    if (cut_buffer) {
	malloc_strcpy(&ptr, &(input_buffer[buffer_pos]));
	input_buffer[buffer_pos] = null(char);
	strmcat(input_buffer, cut_buffer, INPUT_BUFFER_SIZE);
	strmcat(input_buffer, ptr, INPUT_BUFFER_SIZE);
	free(ptr);
	update_input(UPDATE_FROM_CURSOR);
	buffer_pos += strlen(cut_buffer);
	if (buffer_pos > INPUT_BUFFER_SIZE)
	    buffer_pos = INPUT_BUFFER_SIZE;
	update_input(UPDATE_JUST_CURSOR);
    }
}
