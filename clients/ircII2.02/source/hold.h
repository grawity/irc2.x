/*
 * hold.c: handles buffering of display output. 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#ifndef HOLD_LOADED
#define HOLD_LOADED
/* Hold: your general doubly-linked list type structure */
typedef struct HoldStru {
    char *str;
    struct HoldStru *next;
    struct HoldStru *prev;
}        Hold;

extern void remove_from_hold_list();
extern void add_to_hold_list();
extern void hold_mode();
extern int hold_output();
extern char *hold_queue();

#endif HOLD_LOADED
