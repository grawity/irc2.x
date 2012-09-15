/*
 * keys.c: Does command line parsing, etc 
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
#include "config.h"
#include "irc.h"
#include "keys.h"
#include "names.h"
#include "ircaux.h"
#include "window.h"

extern void input_expand_alias();
extern void input_add_character();
extern void input_backward_word();
extern void input_forward_word();
extern void input_delete_previous_word();
extern void input_delete_next_word();
extern void forward_character();
extern void backward_character();
extern void input_clear_to_bol();
extern void input_beginning_of_line();
extern void input_end_of_line();
extern void input_clear_to_eol();
extern void refresh_screen();
extern void input_delete_character();
extern void input_backspace();
extern void backward_history();
extern void forward_history();
extern void toggle_insert_mode();
extern void input_transpose_characters();
extern void input_yank_cut_buffer();
extern void send_line();
extern void help_char();
extern void meta1_char();
extern void meta2_char();
extern void irc_quit();
#ifndef MUNIX
extern void term_pause();
#else
#define term_pause self_insert
#endif
extern void quote_char();
extern void type_text();
extern void parse_text();
extern void toggle_stop_screen();
extern void command_completion();
extern void clear_screen();

#define BACKSPACE 0
#define BACKWARD_CHARACTER 1
#define BACKWARD_HISTORY 2
#define BACKWARD_WORD 3
#define BEGINNING_OF_LINE 4
#define CLEAR_SCREEN 5
#define COMMAND_COMPLETION 6
#define DELETE_CHARACTER 7
#define DELETE_NEXT_WORD 8
#define DELETE_PREVIOUS_WORD 9
#define END_OF_LINE 10
#define ERASE_LINE 11
#define ERASE_TO_END_OF_LINE 12
#define EXPAND_ALIAS 13
#define FORWARD_CHARACTER 14
#define FORWARD_HISTORY 15
#define FORWARD_WORD 16
#define META1_CHARACTER 17
#define META2_CHARACTER 18
#define NEXT_WINDOW 19
#define NOTHING 20
#define PARSE_COMMAND 21
#define PREVIOUS_WINDOW 22
#define QUIT_IRC 23
#define QUOTE_CHARACTER 24
#define REFRESH_SCREEN 25
#define SELF_INSERT 26
#define SEND_LINE 27
#define STOP_IRC 28
#define SWITCH_CHANNELS 29
#define TOGGLE_INSERT_MODE 30
#define TOGGLE_STOP_SCREEN 31
#define TRANSPOSE_CHARACTERS 32
#define TYPE_TEXT 33
#define YANK_FROM_CUTBUFFER 34
#define NUMBER_OF_FUNCTIONS 35

/*
 * lookup_function: looks up an irc function by name and returns the index
 * number of that function in the table.  If the name doesn't match any
 * function, -1 is returned 
 */
int lookup_function(name, index)
char *name;
int *index;
{
    int len,
        cnt,
        i;

    if (name) {
	upper(name);
	len = strlen(name);
	cnt = 0;
	*index = -1;
	for (i = 0; i < NUMBER_OF_FUNCTIONS; i++) {
	    if (strnicmp(name, key_names[i].name, len) == 0) {
		cnt++;
		if (*index == -1)
		    *index = i;
	    }
	}
	if (*index == -1)
	    return (0);
	if (stricmp(name, key_names[*index].name) == 0)
	    return (1);
	else
	    return (cnt);
    }
    return (0);
}

/*
 * display_key: converts the character c to a displayable form and returns
 * it.  Very simple indeed 
 */
char *display_key(c)
char c;
{
    static char key[3];

    key[2] = null(char);
    if (c < 32) {
	key[0] = '^';
	key[1] = c + 64;
    } else if (c == '\177') {
	key[0] = '^';
	key[1] = '?';
    } else {
	key[0] = c;
	key[1] = null(char);
    }
    return (key);
}

/*
 * show_binding: given the ascii value of a key and a meta key status (1 for
 * meta1 keys, 2 for meta2 keys, anything else for normal keys), this will
 * display the key binding for the key in a nice way 
 */
void show_binding(c, meta)
char c,
     meta;
{
    KeyMap *map;
    char *meta_str;

    switch (meta) {
	case 1:
	    map = meta1_keys;
	    meta_str = "META1-";
	    break;
	case 2:
	    map = meta2_keys;
	    meta_str = "META2-";
	    break;
	default:
	    map = keys;
	    meta_str = empty_string;
	    break;
    }
    put_it("*** %s%s is bound to %s %s", meta_str, display_key(c),
	   key_names[map[c].index].name, (map[c].stuff && (*(map[c].stuff))) ? map[c].stuff : "");
}

/*
 * parse_key: converts a key string. Accepts any key, or ^c where c is any
 * key (representing control characters), or META1- or META2- for meta1 or
 * meta2 keys respectively.  The string itself is converted to true ascii
 * value, thus "^A" is converted to 1, etc.  Meta key info is removed and
 * returned as the function value, 0 for no meta key, 1 for meta1, and 2 for
 * meta2.  Thus, "META1-a" is converted to "a" and a 1 is returned.
 * Furthermore, if ^X is bound to META2_CHARACTER, and "^Xa" is passed to
 * parse_key(), it is converted to "a" and 2 is returned.  Do ya understand
 * this? 
 */
int parse_key(key_str)
char *key_str;
{
    char *ptr1,
        *ptr2,
         c;
    int meta = 0;

    ptr2 = ptr1 = key_str;
    while (*ptr1) {
	if (*ptr1 == '^') {
	    ptr1++;
	    switch (*ptr1) {
		case 0:
		    *(ptr2++) = '^';
		    break;
		case '?':
		    *(ptr2++) = '\177';
		    ptr1++;
		    break;
		default:
		    c = *(ptr1++);
		    if (islower(c))
			c = toupper(c);
		    if (c < 64) {
			put_it("*** Illegal key sequence: ^%c", c);
			return (-1);
		    }
		    *(ptr2++) = c - 64;
	    }
	} else
	    *(ptr2++) = *(ptr1++);
    }
    *ptr2 = null(char);
    if (strlen(key_str) > 1) {
	if (strnicmp(key_str, "META1-", 6) == 0) {
	    strcpy(key_str, key_str + 6);
	    meta = 1;
	} else if (strnicmp(key_str, "META2-", 6) == 0) {
	    strcpy(key_str, key_str + 6);
	    meta = 2;
	} else if (keys[*key_str].index == META1_CHARACTER) {
	    meta = 1;
	    strcpy(key_str, key_str + 1);
	} else if (keys[*key_str].index == META2_CHARACTER) {
	    meta = 2;
	    strcpy(key_str, key_str + 1);
	} else {
	    put_it("*** Illegal key sequence: %s is not a meta-key",
		   display_key(*key_str));
	    return (-1);
	}
    }
    return (meta);
}

/*
 * bind_it: does the actually binding of the function to the key with the
 * given meta modifier 
 */
static void bind_it(function, string, key, meta)
unsigned char key;
char *function,
    *string;
int meta;
{
    KeyMap *km;
    int cnt,
        index,
        i;

    switch (meta) {
	case 0:
	    km = keys;
	    break;
	case 1:
	    km = meta1_keys;
	    break;
	case 2:
	    km = meta2_keys;
	    break;
    }
    switch (cnt = lookup_function(function, &index)) {
	case 0:
	    put_it("*** No such function: %s", function);
	    break;
	case 1:
	    km[key].index = index;
	    malloc_strcpy(&(km[key].stuff), string);
	    show_binding(key, meta);
	    break;
	default:
	    put_it("*** Ambiguous function name: %s", function);
	    for (i = 0; i < cnt; i++, index++)
		put_it("%s", key_names[index].name);
	    break;
    }
}

/*
 * bindcmd: the bind command, takes a key sequence followed by a function
 * name followed by option arguments (used depending on the function) and
 * binds a key.  If no function is specified, the current binding for that
 * key is shown 
 */
void bindcmd(command, args)
char *command,
    *args;
{
    char *key,
        *function;
    int meta;

    if (key = next_arg(args, &args)) {
	if ((meta = parse_key(key)) == -1)
	    return;
	if (strlen(key) > 1) {
	    put_it("*** Key sequences may not contain more than two keys");
	    return;
	}
	if (function = next_arg(args, &args))
	    bind_it(function, args, *key, meta);
	else
	    show_binding(*key, meta);
    } else {
	int i;

	for (i = 0; i < 128; i++) {
	    if ((keys[i].index != NOTHING) && (keys[i].index != SELF_INSERT))
		show_binding(i, 0);
	}
	for (i = 0; i < 128; i++) {
	    if ((meta1_keys[i].index != NOTHING) && (meta1_keys[i].index != SELF_INSERT))
		show_binding(i, 1);
	}
	for (i = 0; i < 128; i++) {
	    if ((meta2_keys[i].index != NOTHING) && (meta2_keys[i].index != SELF_INSERT))
		show_binding(i, 2);
	}
    }
}

/*
 * change_send_line: Allows you to change the everything bound to SENDLINE in
 * one fell swoop.  Used by the various functions that gather input using the
 * normal irc interface but dont wish to parse it and send it to the server.
 * Sending NULL resets it to send_line() 
 */
void change_send_line(func)
void (*func) ();
{
    if (func)
	key_names[SEND_LINE].func = func;
    else
	key_names[SEND_LINE].func = send_line;
}

/*
 * type: The TYPE command.  This parses the given string and treats each
 * character as tho it were typed in by the user.  Thus key bindings are used
 * for each character parsed.  Special case characters are control character
 * sequences, specified by a ^ follow by a legal control key.  Thus doing
 * "/TYPE ^B" will be as tho ^B were hit at the keyboard, probably moving the
 * cursor backward one character 
 */
void type(command, args)
char *command,
    *args;
{
    int c;
    char key;

    while (*args) {
	if (*args == '^') {
	    switch (*(++args)) {
		case '?':
		    key = '\177';
		    args++;
		    break;
		case '0':
		    key = '^';
		    break;
		default:
		    c = *(args++);
		    if (islower(c))
			c = toupper(c);
		    if (c < 64) {
			put_it("*** Illegal key sequence: ^%c", c);
			return;
		    }
		    key = c - 64;
		    break;
	    }
	} else
	    key = *(args++);
	edit_char(key);
    }
}

KeyMapNames key_names[] =
{
 "BACKSPACE", input_backspace,
 "BACKWARD_CHARACTER", backward_character,
 "BACKWARD_HISTORY", backward_history,
 "BACKWARD_WORD", input_backward_word,
 "BEGINNING_OF_LINE", input_beginning_of_line,
 "CLEAR_SCREEN", clear_screen,
 "COMMAND_COMPLETION", command_completion,
 "DELETE_CHARACTER", input_delete_character,
 "DELETE_NEXT_WORD", input_delete_next_word,
 "DELETE_PREVIOUS_WORD", input_delete_previous_word,
 "END_OF_LINE", input_end_of_line,
 "ERASE_LINE", input_clear_to_bol,
 "ERASE_TO_END_OF_LINE", input_clear_to_eol,
 "EXPAND_ALIAS", input_expand_alias,
 "FORWARD_CHARACTER", forward_character,
 "FORWARD_HISTORY", forward_history,
 "FORWARD_WORD", input_forward_word,
 "META1_CHARACTER", meta1_char,
 "META2_CHARACTER", meta2_char,
 "NEXT_WINDOW", next_window,
 "NOTHING", NULL,
 "PARSE_COMMAND", parse_text,
 "PREVIOUS_WINDOW", previous_window,
 "QUIT_IRC", irc_quit,
 "QUOTE_CHARACTER", quote_char,
 "REFRESH_SCREEN", refresh_screen,
 "SELF_INSERT", input_add_character,
 "SEND_LINE", send_line,
 "STOP_IRC", term_pause,
 "SWITCH_CHANNELS", switch_channels,
 "TOGGLE_INSERT_MODE", toggle_insert_mode,
 "TOGGLE_STOP_SCREEN", toggle_stop_screen,
 "TRANSPOSE_CHARACTERS", input_transpose_characters,
 "TYPE_TEXT", type_text,
 "YANK_FROM_CUTBUFFER", input_yank_cut_buffer
};


KeyMap keys[] =
{
 SELF_INSERT, null(char *),	/* 0 */
 BEGINNING_OF_LINE, null(char *),
 BACKWARD_CHARACTER, null(char *),
 QUIT_IRC, null(char *),
 DELETE_CHARACTER, null(char *),
 END_OF_LINE, null(char *),
 FORWARD_CHARACTER, null(char *),
 SELF_INSERT, null(char *),

 BACKSPACE, null(char *),	/* 8 */
 TOGGLE_INSERT_MODE, null(char *),
 SEND_LINE, null(char *),
 ERASE_TO_END_OF_LINE, null(char *),
 REFRESH_SCREEN, null(char *),
 SEND_LINE, null(char *),
 FORWARD_HISTORY, null(char *),
 SELF_INSERT, null(char *),

 BACKWARD_HISTORY, null(char *),/* 16 */
 QUOTE_CHARACTER, null(char *),
 SELF_INSERT, null(char *),
 TOGGLE_STOP_SCREEN, null(char *),
 TRANSPOSE_CHARACTERS, null(char *),
 ERASE_LINE, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 META2_CHARACTER, null(char *),	/* 24 */
 YANK_FROM_CUTBUFFER, null(char *),
 STOP_IRC, null(char *),
 META1_CHARACTER, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 32 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 40 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 48 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 56 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 64 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 72 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 80 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 88 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 96 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 104 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 112 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),

 SELF_INSERT, null(char *),	/* 120 */
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 SELF_INSERT, null(char *),
 BACKSPACE, null(char *)
};


KeyMap meta1_keys[] =
{
 NOTHING, null(char *),		/* 0 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 8 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 16 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 24 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 COMMAND_COMPLETION, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 32 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 40 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 CLEAR_SCREEN, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 48 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 56 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 64 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 72 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 80 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 88 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 96 */
 NOTHING, null(char *),
 BACKWARD_WORD, null(char *),
 NOTHING, null(char *),
 DELETE_NEXT_WORD, null(char *),
 NOTHING, null(char *),
 FORWARD_WORD, null(char *),
 NOTHING, null(char *),

 DELETE_PREVIOUS_WORD, null(char *),	/* 104 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 SELF_INSERT, null(char *),

 NOTHING, null(char *),		/* 112 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 EXPAND_ALIAS, null(char *),	/* 120 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *)
};

KeyMap meta2_keys[] =
{
 NOTHING, null(char *),		/* 0 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 8 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 16 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 24 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 32 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 40 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 48 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 56 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 64 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 72 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 80 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 88 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 96 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 104 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NEXT_WINDOW, null(char *),
 NOTHING, null(char *),

 PREVIOUS_WINDOW, null(char *),	/* 112 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 120 */
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *),
 NOTHING, null(char *)
};

/*
 * write_binding: This will write to the given FILE pointer the information
 * about the specified key binding.  The format it writes it out is such that
 * it can be parsed back in later using LOAD or with the -l switch 
 */
static void write_binding(c, meta, fp)
char c,
     meta;
FILE *fp;
{
    KeyMap *map;
    char *meta_str,
        *ptr;

    switch (meta) {
	case 1:
	    map = meta1_keys;
	    meta_str = "META1-";
	    break;
	case 2:
	    map = meta2_keys;
	    meta_str = "META2-";
	    break;
	default:
	    map = keys;
	    meta_str = empty_string;
	    break;
    }
    ptr = DEFAULT_CMDCHARS;
    fprintf(fp, "%cBIND %s%s %s", ptr[0], meta_str, display_key(c), key_names[map[c].index].name);
    if (map[c].stuff && (*(map[c].stuff))) {
	char *stuff,
	    *ptr;

	for (ptr = buffer, stuff = map[c].stuff; *stuff; stuff++) {
	    if (*stuff == QUOTE_CHAR)
		*(ptr++) = QUOTE_CHAR;
	    *(ptr++) = *stuff;
	}
	*ptr = null(char);
	fprintf(fp, " %s\n", buffer);
    } else
	fprintf(fp, "\n");
}

/*
 * save_bindings: this writes all the keys bindings for IRCII to the given
 * FILE pointer using the write_binding function 
 */
void save_bindings(fp)
FILE *fp;
{
    int i;

    for (i = 0; i < 128; i++)
	write_binding(i, 0, fp);
    for (i = 0; i < 128; i++)
	write_binding(i, 1, fp);
    for (i = 0; i < 128; i++)
	write_binding(i, 2, fp);
}
