/*
 * keys.c: Does command line parsing, etc 
 *
 *
 * Written by Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserverd 
 */
#include "irc.h"
#include <ctype.h>

#define NULL 0L

extern void add_ch();
extern void backward_word();
extern void forward_word();
extern void delete_previous_word();
extern void delete_next_word();
extern void forward_character();
extern void backward_character();
extern void clear_to_bol();
extern void beginning_of_line();
extern void end_of_line();
extern void clear_to_eol();
extern void refresh_screen();
extern void delete();
extern void backspace();
extern void backward_history();
extern void forward_history();
extern void toggle_insert_mode();
extern void transpose_characters();
extern void yank_cut_buffer();
extern void send_line();
extern void help_char();
extern void meta1_char();
extern void meta2_char();
extern void irc_exit();
extern void term_pause();
extern void quote_char();
extern void type_text();
extern void parse_text();
extern void toggle_stop_screen();

#define BACKSPACE 0
#define BACKWARD_CHARACTER 1
#define BACKWARD_HISTORY 2
#define BACKWARD_WORD 3
#define BEGINNING_OF_LINE 4
#define DELETE_CHARACTER 5
#define DELETE_NEXT_WORD 6
#define DELETE_PREVIOUS_WORD 7
#define END_OF_LINE 8
#define ERASE_LINE 9
#define ERASE_TO_END_OF_LINE 10
#define FORWARD_CHARACTER 11
#define FORWARD_HISTORY 12
#define FORWARD_WORD 13
#define NOTHING 14
#define HELP_CHARACTER 15
#define META1_CHARACTER 16
#define META2_CHARACTER 17
#define PARSE_COMMAND 18
#define QUIT_IRC 19
#define QUOTE_CHARACTER 20
#define REFRESH_SCREEN 21
#define SELF_INSERT 22
#define SEND_LINE 23
#define STOP_IRC 24
#define TOGGLE_INSERT_MODE 25
#define TOGGLE_STOP_SCREEN 26
#define TRANSPOSE_CHARACTERS 27
#define TYPE_TEXT 28
#define YANK_FROM_CUTBUFFER 29
#define NUMBER_OF_FUNCTIONS 30

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
	    meta_str = "";
	    break;
    }
    put_it("*** %s%s is bound to %s", meta_str, display_key(c),
	   key_names[map[c].index].name);
    if (map[c].stuff && (*(map[c].stuff)))
	put_it("*** String: %s", map[c].stuff);
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
		put_it(key_names[index].name);
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
    } else
	put_it("*** BIND requires more parameters!");
}

KeyMapNames key_names[] =
{
 "BACKSPACE", backspace,
 "BACKWARD_CHARACTER", backward_character,
 "BACKWARD_HISTORY", backward_history,
 "BACKWARD_WORD", backward_word,
 "BEGINNING_OF_LINE", beginning_of_line,
 "DELETE_CHARACTER", delete,
 "DELETE_NEXT_WORD", delete_next_word,
 "DELETE_PREVIOUS_WORD", delete_previous_word,
 "END_OF_LINE", end_of_line,
 "ERASE_LINE", clear_to_bol,
 "ERASE_TO_END_OF_LINE", clear_to_eol,
 "FORWARD_CHARACTER", forward_character,
 "FORWARD_HISTORY", forward_history,
 "FORWARD_WORD", forward_word,
 "NOTHING", NULL,
 "HELP_CHARACTER", help_char,
 "META1_CHARACTER", meta1_char,
 "META2_CHARACTER", meta2_char,
 "PARSE_COMMAND", parse_text,
 "QUIT_IRC", irc_exit,
 "QUOTE_CHARACTER", quote_char,
 "REFRESH_SCREEN", refresh_screen,
 "SELF_INSERT", add_ch,
 "SEND_LINE", send_line,
 "STOP_IRC", term_pause,
 "TOGGLE_INSERT_MODE", toggle_insert_mode,
 "TOGGLE_STOP_SCREEN", toggle_stop_screen,
 "TRANSPOSE_CHARACTERS", transpose_characters,
 "TYPE_TEXT", type_text,
 "YANK_FROM_CUTBUFFER", yank_cut_buffer
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

 SELF_INSERT, null(char *),	/* 24 */
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
 SELF_INSERT, null(char *),
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
 HELP_CHARACTER, null(char *),

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
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 112 */
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
 NOTHING, null(char *),
 NOTHING, null(char *),

 NOTHING, null(char *),		/* 112 */
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
