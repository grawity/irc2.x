/*
 * names.h: Header for names.c
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern void add_channel();
extern void add_to_channel();
extern void remove_channel();
extern void remove_from_channel();
extern int is_on_channel();
extern void list_channels();
extern void reconnect_all_channels();
extern void switch_channels();
extern char *real_channel();
extern char *old_current_channel();
extern void rename_nick();
