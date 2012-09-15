/*
 * exec.h: header for exec.c 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void check_wait_status();
extern void check_process_list();
extern void do_processes();
extern void set_process_bits();
extern int text_to_process();
extern char *signals[];
