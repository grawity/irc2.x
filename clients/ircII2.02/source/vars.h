/*
 * vars.h: header for vars.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

extern int do_boolean();
extern void set_variable();
extern unsigned int get_int_var();
extern char *get_string_var();
extern void set_int_var();
extern void set_string_var();
extern char *get_string_var();
extern void init_variables();
extern char *var_settings[];
extern char *make_string_var();

/* var_settings indexes ... also used in display.c for highlights */
#define OFF 0
#define ON 1
#define TOGGLE 2

/* indexes for the irc_variable array */

#define AUDIBLE_BELL_VAR 0
#define AUTO_UNMARK_AWAY_VAR 1
#define AUTO_WHOWAS_VAR  2
#define BEEP_ON_MSG_VAR 3
#define BEEP_WHEN_AWAY_VAR 4
#define CHANNEL_NAME_WIDTH_VAR 5
#define CLOCK_VAR 6
#define CLOCK_24HOUR_VAR 7
#define CLOCK_ALARM_VAR 8
#define CMDCHARS_VAR 9
#define CONTINUED_LINE_VAR 10
#define DISPLAY_VAR 11
#define ENCRYPT_PROGRAM_VAR 12
#define FULL_STATUS_LINE_VAR 13
#define HELP_WINDOW_VAR 14
#define HIDE_PRIVATE_CHANNELS_VAR 15
#define HISTORY_VAR 16
#define HISTORY_FILE_VAR 17
#define HOLD_MODE_VAR 18
#define HOLD_MODE_MAX_VAR 19
#define INDENT_VAR 20
#define INLINE_ALIASES_VAR 21
#define INPUT_PROMPT_VAR 22
#define INSERT_MODE_VAR 23
#define INVERSE_VIDEO_VAR 24
#define LASTLOG_VAR 25
#define LASTLOG_LEVEL_VAR 26
#define LIST_NO_SHOW 27
#define LOG_VAR 28
#define LOGFILE_VAR 29
#define MAIL_VAR  30
#define MINIMUM_SERVERS_VAR 31
#define MINIMUM_USERS_VAR 32
#define NOTIFY_ON_TERMINATION_VAR 33
#define PAUSE_AFTER_MOTD_VAR 34
#define SCROLL_VAR 35
#define SCROLL_LINES_VAR 36
#define SEND_IGNORE_MSG_VAR 37
#define SEND_MULTI_NICKS_VAR 38
#define SHELL_VAR 39
#define SHELL_FLAGS_VAR 40
#define SHELL_LIMIT_VAR 41
#define SHOW_CHANNEL_NAMES_VAR 42
#define SHOW_END_OF_MSGS_VAR 43
#define STATUS_AWAY_VAR 44
#define STATUS_CHANNEL_VAR 45
#define STATUS_CLOCK_VAR 46
#define STATUS_FORMAT_VAR 47
#define STATUS_HOLD_VAR 48
#define STATUS_INSERT_VAR 49
#define STATUS_MAIL_VAR 50
#define STATUS_OPER_VAR 51
#define STATUS_OVERWRITE_VAR 52
#define STATUS_QUERY_VAR 53
#define STATUS_USER_VAR 54
#define STATUS_WINDOW_VAR 55
#define USE_OLD_MSG_VAR  56
#define VERBOSE_QUERY_VAR 57
#define WARN_OF_IGNORES_VAR 58
#define NUMBER_OF_VARIABLES 59

