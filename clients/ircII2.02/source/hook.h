/*
 * hook.h: header for hook.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#define CONNECT_LIST 0
#define INVITE_LIST 1
#define JOIN_LIST 2
#define LEAVE_LIST 3
#define LIST_LIST 4
#define MAIL_IRC_LIST 5
#define MAIL_REAL_LIST 6
#define MSG_LIST 7
#define NAMES_LIST 8
#define NOTICE_LIST 9
#define NOTIFY_CHANGE_LIST 10
#define NOTIFY_SIGNOFF_LIST 11
#define NOTIFY_SIGNON_LIST 12
#define PUBLIC_LIST 13
#define PUBLIC_MSG_LIST 14
#define PUBLIC_NOTICE_LIST 15
#define PUBLIC_OTHER_LIST 16
#define SEND_MSG_LIST 17
#define SEND_NOTICE_LIST 18
#define SEND_PUBLIC_LIST 19
#define TIMER_LIST 20
#define TOPIC_LIST 21
#define WALL_LIST 22
#define WALLOP_LIST 23
#define WHO_LIST 24
#define NUMBER_OF_LISTS 25

extern int do_hook();
