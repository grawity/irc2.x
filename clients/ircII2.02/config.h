/*
 * config.h: configuration info for IRC II 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

/*
 * Set your favorite default server list here.  This list should be a
 * whitespace separated hostname:portnum:password list (with portnums and
 * passwords optional).  This IS NOT an optional definition.  PLEASE set this
 * to your at least your nearest server 
 */
#define DEFAULT_SERVER "change.this.to.a.server"

/*
 * Uncomment the following to set the path of a file which will contain
 * default server specifications. The contents of this file should be
 * whitespace separated hostname:portnum:password (with the portnum and
 * password being optional). This server list will supercede the
 * DEFAULT_SERVER.  
 */
/* #define DEFAULT_SERVER_FILE "/afs/andrew/usr3/ms5n/.ircserver"  /* */

/* And here is the port number for default client connections.  */
#define IRC_PORT 6667

/*
 * Here you can set the in-line quote character, normally backslash, to
 * whatever you want.  Note that we use two backslashes since a backslash is
 * also C's quote character.  You do not need two of any other character. 
 */
#define QUOTE_CHAR '\\'

/*
 * Uncomment the following and set the path to that of a global .ircrc file
 * that will be loaded BEFORE the users .ircrc.  If left commented, no global
 * .ircrc will be used. 
 */
/* #define GLOBAL_IRCRC "/usr/tmp/global.ircrc" /* */

/*
 * Uncomment the following and set the path to that of a motd that will be
 * displayed AFTER loading of global and local .ircrc files.  If left
 * commented, no motd will be displayed. 
 */
/* #define MOTD "/afs/andrew/usr3/ms5n/etc/irc/IRCII/IRCII" /* */

/*
 * Uncomment the following if the gecos field of your /etc/passwd has other
 * information in it that you don't want as the user name (such as office
 * locations, phone numbers, etc).  The default delimiter is a comma, change
 * it if you need to. If left commented, the entire gecos field is used. 
 */
/* #define GECOS_DELIMITER ',' /* */

/*
 * Uncomment the following and set it to the name of an IRCII Help service. I
 * am attempting to maintain just such a help service called IRCIIHelp.
 * Anyway, uncommenting this will cause the HELP command to send a MSG to the
 * named help service rather than use the built-in help routines. If you
 * uncomments this, you do not need to install the help files.  See the
 * INSTALL file about this. 
 */
/* #define HELP_SERVICE "IRCIIHelp" /* */

/* Uncomment the next line for HP-UX systems */
/* #define HPUX /* */

/*
 * Set the following to 1 if you wish for IRCII not to disturb the tty's flow
 * control characters as the default.  Normally, these are ^Q and ^S.  You
 * may have to rebind them in IRCII.  Set it to 0 for IRCII to take over the
 * tty's flow control. 
 */
#define USE_FLOW_CONTROL 0

/*
 * Below you can set what type of mail you system uses and the path to the
 * appropriate mail file/directory.  Only one may be selected. 
 */
/*
 * UNIX_MAIL is the normal unix mail format.  A users mail messages are all
 * stored in a single file in the systems mail directory.  UNIX_MAIL should
 * be set to the path of that system mail directory. For example, if a given
 * users mail files are located in "/usr/spool/mail/user" where "user" is the
 * mail file itself, then UNIX_MAIL should be set to "/use/spool/mail". 
 */
/* #define UNIX_MAIL "/usr/spool/mail"	/* */
/*
 * AMS_MAIL is the Andrew Mail System mail format.  In this format, each mail
 * message is stored as a separate file in a mailbox directory.  This should
 * be the directory in your top level directory which contains the mail
 * files.  For example, if your mail files are located in "/usr/me/Mailbox",
 * then AMS_MAIL should be simply "Mailbox" 
 */
#define AMS_MAIL "Mailbox"	/* */

/*
 * MAIL_DELIMITER is only used for the UNIX_MAIL format and is ignored for
 * AMS_MAIL.  This specifies the unique text that separates one mail message
 * from another in the mail spool file.  The text is matched against the
 * beginning of each line in the mail file.  Thus, the default shown below
 * will match any line starting with "From ".  This is not
 * case-sensitive.   The default case can be found on many unix systems.
 */
#define MAIL_DELIMITER "From "

/*
 * Below are the IRC II variable defaults.  For boolean variables, use 1 for
 * ON and 0 for OFF.  You may set string variable to NULL if you wish them to
 * have no value 
 */
#define DEFAULT_AUDIBLE_BELL 1
#define DEFAULT_AUTO_UNMARK_AWAY 0
#define DEFAULT_AUTO_WHOWAS 0
#define DEFAULT_BEEP_ON_MSG 0
#define DEFAULT_BEEP_WHEN_AWAY 0
#define DEFAULT_CHANNEL_NAME_WIDTH 10
#define DEFAULT_CLOCK 0
#define DEFAULT_CLOCK_24HOUR 0
#define DEFAULT_CLOCK_ALARM NULL
#define DEFAULT_CMDCHARS "/"
#define DEFAULT_CONTINUED_LINE "+"
#define DEFAULT_DISPLAY 1
#define DEFAULT_ENCRYPT_PUBLIC_MSGS 0
#define DEFAULT_ENCRYPT_PROGRAM "/usr/bin/crypt"
#define DEFAULT_FULL_STATUS_LINE 0
#define DEFAULT_HELP_WINDOW 0
#define DEFAULT_HIDE_PRIVATE_CHANNELS 0
#define DEFAULT_HISTORY 10
#define DEFAULT_HISTORY_FILE NULL
#define DEFAULT_HOLD_MODE 0
#define DEFAULT_HOLD_MODE_MAX 0
#define DEFAULT_IGNORE_INVITES 0
#define DEFAULT_IGNORE_WALLOPS 0
#define DEFAULT_IGNORE_WALLS 0
#define DEFAULT_INDENT 0
#define DEFAULT_INLINE_ALIASES 0
#define DEFAULT_INPUT_PROMPT NULL
#define DEFAULT_INSERT_MODE 1
#define DEFAULT_INVERSE_VIDEO 1
#define DEFAULT_LASTLOG 10
#define DEFAULT_LASTLOG_LEVEL "ALL -CRAP"
#define DEFAULT_LIST_NO_SHOW 0
#define DEFAULT_LOG 0
#define DEFAULT_LOGFILE "IrcLog"
#define DEFAULT_MAIL 0
#define DEFAULT_MINIMUM_SERVERS 0
#define DEFAULT_MINIMUM_USERS 0
#define DEFAULT_NOTIFY_ON_TERMINATION 0
#define DEFAULT_PAUSE_AFTER_MOTD 0
#define DEFAULT_SCROLL 0
#define DEFAULT_SCROLL_LINES 1
#define DEFAULT_SEND_IGNORE_MSG 1
#define DEFAULT_SEND_MULTI_NICKS 0
#define DEFAULT_SHELL "/bin/csh"
#define DEFAULT_SHELL_FLAGS "-fc"
#define DEFAULT_SHELL_LIMIT 0
#define DEFAULT_SHOW_CHANNEL_NAMES 0
#define DEFAULT_SHOW_END_OF_MSGS 0
#define DEFAULT_STATUS_AWAY "(Away) "
#define DEFAULT_STATUS_CHANNEL "Chn: %C"
#define DEFAULT_STATUS_CLOCK "%T "
#define DEFAULT_STATUS_FORMAT " *** IRCII: /HELP for help  %H%N%*  %C%Q (%I%O) %A%M%T%W***"
#define DEFAULT_STATUS_HOLD "--- STOPPED ---  "
#define DEFAULT_STATUS_INSERT "I"
#define DEFAULT_STATUS_MAIL " (Mail: %M)"
#define DEFAULT_STATUS_OPER "*"
#define DEFAULT_STATUS_OVERWRITE "O"
#define DEFAULT_STATUS_QUERY " Query: %Q"
#define DEFAULT_STATUS_USER NULL
#define DEFAULT_STATUS_WINDOW " ^^^^^^^^^^ "
#define DEFAULT_USE_OLD_MSG 0
#define DEFAULT_VERBOSE_QUERY 1
#define DEFAULT_WARN_OF_IGNORES 1
