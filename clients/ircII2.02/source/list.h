/*
 * list.h: header for list.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

typedef struct list_stru {
    struct list_stru *next;
    char *name;
}         List;

extern void add_to_list();
extern List *find_in_list();
extern List *remove_from_list();
extern List *list_lookup();

#define REMOVE_FROM_LIST 1
#define USE_WILDCARDS 1
