/*
 * config.h: configuration info for IRC II 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */

/* set your favorite default server here */
#define DEFAULT_SERVER "ousrvr.oulu.fi"

/*
 * Here you can set the in-line quote character, normally backslash, to
 * whatever you want 
 */
#define QUOTE_CHAR '\\'

/*
 * Uncomment the following and set the path to that of a global .ircrc file
 * that will be loaded BEFORE the users .ircrc.  If left commented, no global
 * .ircrc will be used. 
 */
#define GLOBAL_IRCRC "/usr/local/lib/irc/global.ircrc"

/*
 * Uncomment the following and set the path to that of a motd that will be
 * displayed BEFORE loading of global and local .ircrc files.  If left
 * commented, no motd will be displayed. 
 */
/* #define MOTD "/afs/andrew/usr3/ms5n/etc/irc/README.irchelp" /* */

/*
 * Uncomment the following for a status line that always extends the full
 * screen width.  If left commented, the status line will be only as long as
 * it needs to be to display the info in it. 
 */
#define FULL_STATUS_LINE  /* */

/*
 * Uncomment the following if the gecos field of your /etc/passwd has other
 * information in it that you don't want as the user name (such as office
 * locations, phone numbers, etc).  The default delimiter is a comma, change
 * it if you need to. If left commented, the entire gecos field is used. 
 */
#define GECOS_DELIMITER ',' /* */

/* Uncomment the next line for HP-UX systems */
/* #define HPUX */
