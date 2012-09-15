/*
 * history.h: header for history.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void set_history_size();
extern void set_history_file();
extern void add_to_history();
extern char *get_from_history();
extern char *do_history();

/* used by get_from_history */
#define NEXT 0
#define PREV 1
