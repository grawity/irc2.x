/*
 * alias.h: header for alias.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void add_alias();
extern char *get_alias();
extern char *expand_alias();
extern void list_aliases();
extern char mark_alias();
extern void delete_alias();
extern char *inline_aliases();
extern char **match_alias();
