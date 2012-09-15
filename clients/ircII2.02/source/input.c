/*
 * input.c: does the actual input line stuff... keeps the appropriate stuff
 * on the input line, handles insert/delete of characters/words... the whole
 * ball o wax 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <ctype.h>
#include "irc.h"
#include "input.h"
#include "term.h"
#include "alias.h"
#include "vars.h"
#include "ircaux.h"

#define WIDTH 10

#define INPUT_BUFFER_SIZE 256

/* input_buffer: where all that input is kept */
static char input_buffer[INPUT_BUFFER_SIZE + 1];

/* input_prompt: contains the current, unexpanded input prompt */
static char *input_prompt = null(char *);

/* buffer_pos: position of curson in input_buffer, not screen cursor */
static int buffer_pos;

/*
 * buffer_min_pos: The first position in the input buffer that contains
 * actual input.  Everything before this position is the input prompt 
 */
static int buffer_min_pos = 0;

/* input_line: the actual screen line where the input goes */
static int input_line;

/* cut_buffer: this is where we stick any cut text operation is used */
static char *cut_buffer = null(char *);

/* str_start: position in buffer of first visible character in the input line */
static int str_start = 0;

/*
 * upper_mark and lower_mark: marks the upper and lower positions in the
 * input buffer which will cause the input display to switch to the next or
 * previous bunch of text 
 */
static int lower_mark;
static int upper_mark;

/* zone: the amount of editable visible text on the screen */
static int zone;

/* cursor: position of the cursor in the input line on the screen */
static int cursor = 0;

/* cursor_to_input: move the cursor to the input line, if not there already */
void cursor_to_input()
{
    if   (cursor_in_display()) {
	     term_move_cursor(cursor, input_line);
	cursor_not_in_display();
	term_flush();
    }
}

/*
 * update_input: does varying amount of updating on the input line depending
 * upon the position of the cursor and the update flag.  If the cursor has
 * move toward one of the edge boundaries on the screen, update_cursor()
 * flips the input line to the next (previous) line of text. The update flag
 * may be: 
 *
 * NO_UPDATE - only do the above bounds checking. 
 *
 * UPDATE_JUST_CURSOR - do bounds checking and position cursor where is should
 * be. 
 *
 * UPDATE_FROM_CURSOR - does all of the above, and makes sure everything from
 * the cursor to the right edge of the screen is current (by redrawing it). 
 *
 * UPDATE_ALL - redraws the entire line 
 */
void update_input(update)
char update;
{
    int old_start;
    static int co = 0,
        li = 0;
    char *ptr;
    int len;

    if (dumb)
	return;
    cursor_to_input();
    if (input_prompt) {
	char *inp_ptr = null(char *);

	ptr = expand_alias(null(char *), input_prompt, empty_string);
	len = strlen(ptr);
	if (strncmp(ptr, input_buffer, len)) {
	    malloc_strcpy(&inp_ptr, input_buffer + buffer_min_pos);
	    strncpy(input_buffer, ptr, INPUT_BUFFER_SIZE);
	    buffer_pos += (len - buffer_min_pos);
	    buffer_min_pos = strlen(ptr);
	    strmcat(input_buffer, inp_ptr, INPUT_BUFFER_SIZE);
	    new_free(&inp_ptr);
	    update = UPDATE_ALL;
	}
	new_free(&ptr);
    }
    if ((li != LI) || (co != CO)) {
	/* resized?  Keep it simple and reset everything */
	input_line = LI - 1;
	zone = CO - (WIDTH * 2);
	lower_mark = WIDTH;
	upper_mark = CO - WIDTH;
	cursor = buffer_min_pos;
	buffer_pos = buffer_min_pos;
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
	if ((str_start == 0) && (buffer_min_pos > 0)) {
	    char echo;

	    echo = term_echo(1);
	    if (buffer_min_pos > (CO - WIDTH))
		len = CO - WIDTH - 1;
	    else
		len = buffer_min_pos;
	    term_puts(&(input_buffer[str_start]), len);
	    term_echo(echo);
	    term_puts(&(input_buffer[str_start + len]), CO - len);
	} else
	    term_puts(&(input_buffer[str_start]), CO);
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
    term_flush();
}

/* input_move_cursor: moves the cursor left or right... got it? */
void input_move_cursor(dir)
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
	if (buffer_pos > buffer_min_pos) {
	    buffer_pos--;
	    if (term_cursor_left())
		term_move_cursor(cursor - 1, input_line);
	}
    }
    update_input(NO_UPDATE);
}

/*
 * input_forward_word: move the input cursor forward one word in the input
 * line 
 */
void input_forward_word()
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

/* input_backward_word: move the cursor left on word in the input line */
void input_backward_word()
{
         cursor_to_input();
    while ((buffer_pos > buffer_min_pos) &&
	   (isspace(input_buffer[buffer_pos - 1]) ||
	    ispunct(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    while ((buffer_pos > buffer_min_pos) &&
	   !(ispunct(input_buffer[buffer_pos - 1]) ||
	     isspace(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    update_input(UPDATE_JUST_CURSOR);
}

/* input_delete_character: deletes a character from the input line */
void input_delete_character()
{
         cursor_to_input();
    if (input_buffer[buffer_pos]) {
	char *ptr = null(char *);
	int pos;

	malloc_strcpy(&ptr, &(input_buffer[buffer_pos + 1]));
	strcpy(&(input_buffer[buffer_pos]), ptr);
	new_free(&ptr);
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

/* input_backspace: does a backspace in the input buffer */
void input_backspace()
{
         cursor_to_input();
    if (buffer_pos > buffer_min_pos) {
	char *ptr = null(char *);
	int pos;

	malloc_strcpy(&ptr, &(input_buffer[buffer_pos]));
	strcpy(&(input_buffer[buffer_pos - 1]), ptr);
	new_free(&ptr);
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
		update_input(UPDATE_JUST_CURSOR);
	    }
	} else {
	    term_putchar(' ');
	    if (term_cursor_left())
		term_move_cursor(cursor - 1, input_line);
	    update_input(NO_UPDATE);
	}
    }
}

/*
 * input_beginning_of_line: moves the input cursor to the first character in
 * the input buffer 
 */
void input_beginning_of_line()
{
         cursor_to_input();
    buffer_pos = buffer_min_pos;
    update_input(UPDATE_JUST_CURSOR);
}

/*
 * input_end_of_line: moves the input cursor to the last character in the
 * input buffer 
 */
void input_end_of_line()
{
         cursor_to_input();
    buffer_pos = strlen(input_buffer);
    update_input(UPDATE_JUST_CURSOR);
}

/*
 * input_delete_previous_word: deletes from the cursor backwards to the next
 * space character. 
 */
void input_delete_previous_word()
{
    int old_pos;
    char c;

    cursor_to_input();
    old_pos = buffer_pos;
    while ((buffer_pos > buffer_min_pos) &&
	   (isspace(input_buffer[buffer_pos - 1]) ||
	    ispunct(input_buffer[buffer_pos - 1])))
	buffer_pos--;
    while ((buffer_pos > buffer_min_pos) &&
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

/*
 * input_delete_next_word: deletes from the cursor to the end of the next
 * word 
 */
void input_delete_next_word()
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
    new_free(&ptr);
    update_input(UPDATE_FROM_CURSOR);
}

/*
 * input_add_character: adds the character c to the input buffer, repecting
 * the current overwrite/insert mode status, etc 
 */
void input_add_character(c)
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
		new_free(&ptr);
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

/*
 * set_input: sets the input buffer to the given string, discarding whatever
 * was in the input buffer before 
 */
void set_input(str)
char *str;
{
    strncpy(input_buffer + buffer_min_pos, str, INPUT_BUFFER_SIZE);
    buffer_pos = strlen(input_buffer);
}

/*
 * get_input: returns a pointer to the input buffer.  Changing this will
 * actually change the input buffer.  This is a bad way to change the input
 * buffer tho, cause no bounds checking won't be done 
 */
char *get_input()
{
         return (&(input_buffer[buffer_min_pos]));
}

/* intut_clear_to_eol: erases from the cursor to the end of the input buffer */
void input_clear_to_eol()
{
         cursor_to_input();
    malloc_strcpy(&cut_buffer, &(input_buffer[buffer_pos]));
    input_buffer[buffer_pos] = null(char);
    term_clear_to_eol(cursor, input_line);
    update_input(NO_UPDATE);
}

/*
 * input_clear_to_bol: clears from the cursor to the beginning of the input
 * buffer 
 */
void input_clear_to_bol()
{
         cursor_to_input();
    malloc_strcpy(&cut_buffer, input_buffer + buffer_min_pos);
    input_buffer[buffer_min_pos] = null(char);
    buffer_pos = buffer_min_pos;
    term_move_cursor(buffer_min_pos, input_line);
    term_clear_to_eol(0, input_line);
    update_input(NO_UPDATE);
}

/*
 * input_transpose_characters: swaps the positions of the two characters
 * before the cursor position 
 */
void input_transpose_characters()
{
         cursor_to_input();
    if (buffer_pos > (buffer_min_pos + 1)) {
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

/* init_input: initialized the input buffer by clearing it out */
void init_input()
{
        *input_buffer = null(char);
    buffer_pos = buffer_min_pos;
}

/*
 * input_yank_cut_buffer: takes the contents of the cut buffer and inserts it
 * into the input line 
 */
void input_yank_cut_buffer()
{
    char *ptr = null(char *);

    if (cut_buffer) {
	malloc_strcpy(&ptr, &(input_buffer[buffer_pos]));
	input_buffer[buffer_pos] = null(char);
	strmcat(input_buffer, cut_buffer, INPUT_BUFFER_SIZE);
	strmcat(input_buffer, ptr, INPUT_BUFFER_SIZE);
	new_free(&ptr);
	update_input(UPDATE_FROM_CURSOR);
	buffer_pos += strlen(cut_buffer);
	if (buffer_pos > INPUT_BUFFER_SIZE)
	    buffer_pos = INPUT_BUFFER_SIZE;
	update_input(UPDATE_JUST_CURSOR);
    }
}

/*
 * set_input_prompt: sets a prompt that will be displayed in the input
 * buffer.  This prompt cannot be backspaced over, etc.  It's a prompt.
 * Setting the prompt to null uses no prompt 
 */
void set_input_prompt(prompt)
char *prompt;
{
    char *ptr;

    if (prompt) {
	malloc_strcpy(&input_prompt, prompt);
	ptr = expand_alias(null(char *), prompt, empty_string);
	strncpy(input_buffer, ptr, INPUT_BUFFER_SIZE);
	buffer_min_pos = strlen(ptr);
	buffer_pos = buffer_min_pos;
	new_free(&ptr);
    } else {
	*input_buffer = null(char);
	new_free(&input_prompt);
	buffer_min_pos = 0;
	buffer_pos = 0;
    }
    update_input(UPDATE_ALL);
}

/*
 * pause: prompt the user with a message then waits for a single keystroke
 * before continuing.  The key hit is returned. 
 */
char pause(msg)
char *msg;
{
    char *ptr = null(char *);
    char c;

    if (!dumb) {
	malloc_strcpy(&ptr, get_input());
	set_input(msg);
	update_input(UPDATE_ALL);
	read(0, &c, 1);
	set_input(ptr);
	update_input(UPDATE_ALL);
	new_free(&ptr);
    } else {
	puts(msg);
	fflush(stdout);
	read(0, &c, 1);
    }
    return (c);
}

/* input_expand_alias: expands all alias in the input buffer and resets it */
void input_expand_alias()
{
    char *command,
        *args,
        *full,
        *cmdchars;
    int cnt;

    if ((cmdchars = get_string_var(CMDCHARS_VAR)) == null(char *))
	cmdchars = DEFAULT_CMDCHARS;
    if (command = next_arg(get_input(), &args)) {
	if (index(cmdchars, *command) && !index(cmdchars, *(command + 1))) {
	    if (command = get_alias(command+1, args, &cnt, &full)) {
		set_input(command);
		new_free(&command);
		new_free(&full);
		update_input(UPDATE_ALL);
		return;
	    }
	}
    }
    term_beep();
}
