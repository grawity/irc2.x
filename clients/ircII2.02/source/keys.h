/*
 * keys.h: header for keys.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

/* KeyMap: the structure of the irc keymaps */
typedef struct {
    int index;
    char *stuff;
}      KeyMap;

/* KeyMapNames: the structure of the keymap to realname array */
typedef struct {
    char *name;
    void (*func) ();
}      KeyMapNames;

extern KeyMap keys[],
       meta1_keys[],
       meta2_keys[];
extern KeyMapNames key_names[];
