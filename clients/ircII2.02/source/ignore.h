/*
 * ignore.h: header for ignore.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
/* declared in ignore.c */
extern int ignore_usernames;
extern int is_ignored();
/* Type of ignored nicks */
#define IGNORE_MSGS 1
#define IGNORE_PUBLIC 2
#define IGNORE_WALLS 4
#define IGNORE_WALLOPS 8
#define IGNORE_INVITES 16
#define IGNORE_NOTICES 32
#define IGNORE_MAIL 64
#define IGNORE_ALL (IGNORE_MSGS | IGNORE_PUBLIC | IGNORE_WALLS | IGNORE_WALLOPS | IGNORE_INVITES | IGNORE_NOTICES | IGNORE_MAIL)

#define IGNORED 1
#define HIGHLIGHTED -1
