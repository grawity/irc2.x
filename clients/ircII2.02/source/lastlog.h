/*
 * lastlog.h: header for lastlog.c 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#ifndef LASTLOG_LOADED
#define LASTLOG_LOADED

typedef struct lastlog_stru {
    int level;
    char *msg;
    struct lastlog_stru *next;
    struct lastlog_stru *prev;
}            Lastlog;

#define LOG_NONE 0
#define LOG_CURRENT 0
#define LOG_CRAP 1
#define LOG_PUBLIC 2
#define LOG_MSG 4
#define LOG_NOTICE 8
#define LOG_WALL 16
#define LOG_WALLOP 32
#define LOG_MAIL 64
#define LOG_ALL (LOG_CRAP | LOG_PUBLIC | LOG_MSG | LOG_NOTICE | LOG_WALL | LOG_WALLOP | LOG_MAIL)

extern void set_lastlog_level();
extern int set_lastlog_msg_level();
extern void set_lastlog_size();
extern void lastlog();
extern void add_to_lastlog();
extern char *bits_to_lastlog_level();

#endif LASTLOG_LOADED
