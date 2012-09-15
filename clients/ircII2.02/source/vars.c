/*
 * vars.c: All the dealing of the irc variables are handled here. 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "config.h"
#include "irc.h"
#include "status.h"
#include "window.h"
#include "lastlog.h"
#include "log.h"
#include "crypt.h"
#include "history.h"
#include "vars.h"
#include "input.h"
#include "ircaux.h"

/* IrcVariable: structure for each variable in the variable table */
typedef struct {
    char *name;			/* what the user types */
    char type;			/* variable types, see below */
    unsigned int integer;	/* int value of variable */
    char *string;		/* string value of variable */
    void (*func) ();		/* function to do every time variable is set */
}      IrcVariable;

/* the types of IrcVariables */
#define BOOL_TYPE_VAR 0
#define CHAR_TYPE_VAR 1
#define INT_TYPE_VAR 2
#define STRING_TYPE_VAR 3

char *var_settings[] = {"OFF", "ON", "TOGGLE"};

/*
 * irc_variable: all the irc variables used.  Note that the integer and
 * boolean defaults are set here, which the string default value are set in
 * the init_variables() procedure 
 */
static IrcVariable irc_variable[] =
{"AUDIBLE_BELL", BOOL_TYPE_VAR, DEFAULT_AUDIBLE_BELL, NULL, NULL,
 "AUTO_UNMARK_AWAY", BOOL_TYPE_VAR, DEFAULT_AUTO_UNMARK_AWAY, NULL, NULL,
 "AUTO_WHOWAS", BOOL_TYPE_VAR, DEFAULT_AUTO_WHOWAS, NULL, NULL,
 "BEEP_ON_MSG", INT_TYPE_VAR, DEFAULT_BEEP_ON_MSG, NULL, NULL,
 "BEEP_WHEN_AWAY", INT_TYPE_VAR, DEFAULT_BEEP_WHEN_AWAY, NULL, NULL,
 "CHANNEL_NAME_WIDTH", INT_TYPE_VAR, DEFAULT_CHANNEL_NAME_WIDTH, NULL, update_all_status,
 "CLOCK", BOOL_TYPE_VAR, DEFAULT_CLOCK, NULL, update_all_status,
"CLOCK_24HOUR", BOOL_TYPE_VAR, DEFAULT_CLOCK_24HOUR, NULL, update_all_status,
 "CLOCK_ALARM", STRING_TYPE_VAR, 0, NULL, set_alarm,
 "CMDCHARS", STRING_TYPE_VAR, 0, NULL, NULL,
 "CONTINUED_LINE", STRING_TYPE_VAR, 0, NULL, set_continued_line,
 "DISPLAY", BOOL_TYPE_VAR, DEFAULT_DISPLAY, NULL, NULL,
 "ENCRYPT_PROGRAM", STRING_TYPE_VAR, 0, NULL, NULL,
 "FULL_STATUS_LINE", BOOL_TYPE_VAR, DEFAULT_FULL_STATUS_LINE, NULL, update_all_status,
 "HELP_WINDOW", BOOL_TYPE_VAR, DEFAULT_HELP_WINDOW, NULL, NULL,
 "HIDE_PRIVATE_CHANNELS", BOOL_TYPE_VAR, DEFAULT_HIDE_PRIVATE_CHANNELS, NULL, update_all_status,
 "HISTORY", INT_TYPE_VAR, DEFAULT_HISTORY, NULL, set_history_size,
 "HISTORY_FILE", STRING_TYPE_VAR, 0, NULL, set_history_file,
 "HOLD_MODE", BOOL_TYPE_VAR, DEFAULT_HOLD_MODE, NULL, reset_line_cnt,
 "HOLD_MODE_MAX", INT_TYPE_VAR, DEFAULT_HOLD_MODE_MAX, NULL, NULL,
 "INDENT", BOOL_TYPE_VAR, DEFAULT_INDENT, NULL, NULL,
 "INLINE_ALIASES", BOOL_TYPE_VAR, DEFAULT_INLINE_ALIASES, NULL, NULL,
 "INPUT_PROMPT", STRING_TYPE_VAR, 0, NULL, set_input_prompt,
 "INSERT_MODE", BOOL_TYPE_VAR, DEFAULT_INSERT_MODE, NULL, update_all_status,
 "INVERSE_VIDEO", BOOL_TYPE_VAR, DEFAULT_INVERSE_VIDEO, NULL, NULL,
 "LASTLOG", INT_TYPE_VAR, DEFAULT_LASTLOG, NULL, set_lastlog_size,
 "LASTLOG_LEVEL", STRING_TYPE_VAR, 0, NULL, set_lastlog_level,
 "LIST_NO_SHOW", INT_TYPE_VAR, DEFAULT_LIST_NO_SHOW, NULL, NULL,
 "LOG", BOOL_TYPE_VAR, DEFAULT_LOG, NULL, logger,
 "LOGFILE", STRING_TYPE_VAR, 0, NULL, set_log_file,
 "MAIL", BOOL_TYPE_VAR, DEFAULT_MAIL, NULL, update_all_status,
 "MINIMUM_SERVERS", INT_TYPE_VAR, DEFAULT_MINIMUM_SERVERS, NULL, NULL,
 "MINIMUM_USERS", INT_TYPE_VAR, DEFAULT_MINIMUM_USERS, NULL, NULL,
 "NOTIFY_ON_TERMINATION", BOOL_TYPE_VAR, DEFAULT_NOTIFY_ON_TERMINATION, NULL, NULL,
 "PAUSE_AFTER_MOTD", BOOL_TYPE_VAR, DEFAULT_PAUSE_AFTER_MOTD, NULL, NULL,
 "SCROLL", BOOL_TYPE_VAR, DEFAULT_SCROLL, NULL, set_scroll,
 "SCROLL_LINES", INT_TYPE_VAR, DEFAULT_SCROLL_LINES, NULL, set_scroll_lines,
 "SEND_IGNORE_MSG", BOOL_TYPE_VAR, DEFAULT_SEND_IGNORE_MSG, NULL, NULL,
 "SEND_MULTI_NICKS", BOOL_TYPE_VAR, DEFAULT_SEND_MULTI_NICKS, NULL, NULL,
 "SHELL", STRING_TYPE_VAR, 0, NULL, NULL,
 "SHELL_FLAGS", STRING_TYPE_VAR, 0, NULL, NULL,
 "SHELL_LIMIT", INT_TYPE_VAR, DEFAULT_SHELL_LIMIT, NULL, NULL,
 "SHOW_CHANNEL_NAMES", BOOL_TYPE_VAR, DEFAULT_SHOW_CHANNEL_NAMES, NULL, NULL,
 "SHOW_END_OF_MSGS", BOOL_TYPE_VAR, DEFAULT_SHOW_END_OF_MSGS, NULL, NULL,
 "STATUS_AWAY", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_CHANNEL", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_CLOCK", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_FORMAT", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_HOLD", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_INSERT", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_MAIL", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_OPER", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_OVERWRITE", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_QUERY", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_USER", STRING_TYPE_VAR, 0, NULL, build_status,
 "STATUS_WINDOW", STRING_TYPE_VAR, 0, NULL, build_status,
 "USE_OLD_MSG", BOOL_TYPE_VAR, DEFAULT_USE_OLD_MSG, NULL, NULL,
 "VERBOSE_QUERY", BOOL_TYPE_VAR, DEFAULT_VERBOSE_QUERY, NULL, NULL,
 "WARN_OF_IGNORES", BOOL_TYPE_VAR, DEFAULT_WARN_OF_IGNORES, NULL, NULL,
 null(char *), 0, 0, NULL, NULL};

/*
 * init_variables: initializes the string variables that can't really be
 * initialized properly above 
 */
void init_variables()
{
         set_string_var(CMDCHARS_VAR, DEFAULT_CMDCHARS);
    set_string_var(LOGFILE_VAR, DEFAULT_LOGFILE);
    set_string_var(SHELL_VAR, DEFAULT_SHELL);
    set_string_var(SHELL_FLAGS_VAR, DEFAULT_SHELL_FLAGS);
    set_string_var(ENCRYPT_PROGRAM_VAR, DEFAULT_ENCRYPT_PROGRAM);
    set_string_var(CLOCK_ALARM_VAR, DEFAULT_CLOCK_ALARM);
    set_string_var(CONTINUED_LINE_VAR, DEFAULT_CONTINUED_LINE);
    set_string_var(INPUT_PROMPT_VAR, DEFAULT_INPUT_PROMPT);
    set_string_var(HISTORY_FILE_VAR, DEFAULT_HISTORY_FILE);
    set_string_var(LASTLOG_LEVEL_VAR, DEFAULT_LASTLOG_LEVEL);
    set_string_var(STATUS_FORMAT_VAR, DEFAULT_STATUS_FORMAT);
    set_string_var(STATUS_AWAY_VAR, DEFAULT_STATUS_AWAY);
    set_string_var(STATUS_CHANNEL_VAR, DEFAULT_STATUS_CHANNEL);
    set_string_var(STATUS_CLOCK_VAR, DEFAULT_STATUS_CLOCK);
    set_string_var(STATUS_HOLD_VAR, DEFAULT_STATUS_HOLD);
    set_string_var(STATUS_INSERT_VAR, DEFAULT_STATUS_INSERT);
    set_string_var(STATUS_MAIL_VAR, DEFAULT_STATUS_MAIL);
    set_string_var(STATUS_OPER_VAR, DEFAULT_STATUS_OPER);
    set_string_var(STATUS_OVERWRITE_VAR, DEFAULT_STATUS_OVERWRITE);
    set_string_var(STATUS_QUERY_VAR, DEFAULT_STATUS_QUERY);
    set_string_var(STATUS_USER_VAR, DEFAULT_STATUS_USER);
    set_string_var(STATUS_WINDOW_VAR, DEFAULT_STATUS_WINDOW);
    set_lastlog_size(irc_variable[LASTLOG_VAR].integer);
    set_history_size(irc_variable[HISTORY_VAR].integer);
    set_history_file(irc_variable[HISTORY_FILE_VAR].string);
    set_lastlog_level(irc_variable[LASTLOG_LEVEL_VAR].string);
}

/*
 * find_variable: looks up variable name in the variable table and returns
 * the index into the variable array of the match.  If there is no match, cnt
 * is set to 0 and -1 is returned.  If more than one match the string, cnt is
 * set to that number, and it returns the first match.  Index will contain
 * the index into the array of the first found entry 
 */
int find_variable(name, cnt)
char *name;
int *cnt;
{
    IrcVariable *v,
               *first;
    int len,
        index;

    len = strlen(name);
    index = 0;
    for (first = irc_variable; first->name; first++, index++) {
	if (strncmp(name, first->name, len) == 0) {
	    *cnt = 1;
	    break;
	}
    }
    if (first->name) {
	if (strlen(first->name) != len) {
	    v = first;
	    for (v++; v->name; v++, (*cnt)++) {
		if (strncmp(name, v->name, len) != 0)
		    break;
	    }
	}
	return (index);
    } else {
	*cnt = 0;
	return (-1);
    }
}

/*
 * do_boolean: just a handy thing.  Returns 1 if the str is not ON, OFF, or
 * TOGGLE 
 */
int do_boolean(str, value)
char *str;
int *value;
{
    upper(str);
    if (strcmp(str, var_settings[ON]) == 0)
	*value = 1;
    else if (strcmp(str, var_settings[OFF]) == 0)
	*value = 0;
    else if (strcmp(str, "TOGGLE") == 0) {
	if (*value)
	    *value = 0;
	else
	    *value = 1;
    } else
	return (1);
    return (0);
}

/*
 * set_var_value: Given the variable structure and the string representation
 * of the value, this sets the value in the most verbose and error checking
 * of manors.  It displays the results of the set and executes the function
 * defined in the var structure 
 */
void set_var_value(index, value, silent)
int index;
char *value;
char silent;
{
    char *rest;
    IrcVariable *var;

    var = &(irc_variable[index]);
    switch (var->type) {
	case BOOL_TYPE_VAR:
	    if (value && *value && (value = next_arg(value, &rest))) {
		if (do_boolean(value, &(var->integer))) {
		    put_it("*** Value must be either ON, OFF, or TOGGLE");
		    break;
		}
		if (var->func)
		    (var->func) (var->integer);
		if ((index != DISPLAY_VAR) && !silent)
		    put_it("*** Value of %s set to %s", var->name, (var->integer) ? var_settings[ON] : var_settings[OFF]);
	    } else
		put_it("*** Current value of %s is %s", var->name, (var->integer) ? var_settings[ON] : var_settings[OFF]);
	    break;
	case CHAR_TYPE_VAR:
	    if (value && *value && (value = next_arg(value, &rest))) {
		if (strlen(value) > 1)
		    put_it("*** Value of %s must be a single character", var->name);
		else {
		    var->integer = *value;
		    if (var->func)
			(var->func) (var->integer);
		    if (!silent)
			put_it("*** Value of %s set to '%c'", var->name, var->integer);
		}
	    } else
		put_it("*** Current value of %s is '%c'", var->name, var->integer);
	    break;
	case INT_TYPE_VAR:
	    if (value && *value && (value = next_arg(value, &rest))) {
		int val;

		if (!is_number(value)) {
		    put_it("*** Value of %s must be numeric!", var->name);
		    break;
		}
		if ((val = atoi(value)) < 0) {
		    put_it("*** Value of %s must be greater than 0", var->name);
		    break;
		}
		var->integer = val;
		if (var->func)
		    (var->func) (var->integer);
		if (!silent)
		    put_it("*** Value of %s set to %d", var->name, var->integer);
	    } else
		put_it("*** Current value of %s is %d", var->name, var->integer);
	    break;
	case STRING_TYPE_VAR:
	    if (value && *value) {
		if (stricmp("<EMPTY>", value))
		    malloc_strcpy(&(var->string), value);
		else
		    new_free(&(var->string));
		if (var->func)
		    (var->func) (var->string);
		if (!silent)
		    put_it("*** Value of %s set to %s", var->name, (var->string ? var->string : "<EMPTY>"));
	    } else {
		if (var->string)
		    put_it("*** Current value of %s is %s", var->name, var->string);
		else
		    put_it("*** No value for %s has been set", var->name);
	    }
	    break;
    }
}

/*
 * set_variable: The SET command sets one of the irc variables.  The args
 * should consist of "variable-name setting", where variable name can be
 * partial, but non-ambbiguous, and setting depends on the variable being set 
 */
void set_variable(command, args)
char *command,
    *args;
{
    char *var,
         no_args = 1,
         silent = 0;
    int cnt,
        index;

    while (var = next_arg(args, &args)) {
	if (*var == '-') {
	    if (strnicmp(var, "-SILENT", strlen(var)) == 0)
		silent = 1;
	    else {
		put_it("*** Unknown flag: %s", var);
		return;
	    }
	} else {
	    no_args = 0;
	    upper(var);
	    index = find_variable(var, &cnt);
	    switch (cnt) {
		case 0:
		    put_it("*** No such variable \"%s\"", var);
		    return;
		case 1:
		    set_var_value(index, args, silent);
		    return;
		default:
		    if (!silent) {
			put_it("*** %s is ambiguous", var);
			for (cnt += index; index < cnt; index++)
			    set_var_value(index, null(char *), silent);
		    }
		    return;
	    }
	}
    }
    if (no_args && !silent) {
	for (index = 0; index < NUMBER_OF_VARIABLES; index++)
	    set_var_value(index, null(char *), silent);
    }
}

/*
 * get_string_var: returns the value of the string variable given as an index
 * into the variable table.  Does no checking of variable types, etc 
 */
char *get_string_var(var)
int var;
{
    return (irc_variable[var].string);
}

/*
 * get_int_var: returns the value of the integer string given as an index
 * into the variable table.  Does no checking of variable types, etc 
 */
unsigned int get_int_var(var)
int var;
{
    return (irc_variable[var].integer);
}

/*
 * set_string_var: sets the string variable given as an index into the
 * variable table to the given string.  If string is null, the current value
 * of the string variable is freed and set to null 
 */
void set_string_var(var, string)
int var;
char *string;
{
    if (string)
	malloc_strcpy(&(irc_variable[var].string), string);
    else
	new_free(&(irc_variable[var].string));
}

/*
 * set_int_var: sets the integer value of the variable given as an index into
 * the variable table to the given value 
 */
void set_int_var(var, value)
int var;
unsigned int value;
{
    irc_variable[var].integer = value;
}

/*
 * save_variables: this writes all of the IRCII variables to the given FILE
 * pointer in such a way that they can be loaded in using LOAD or the -l
 * switch 
 */
void save_variables(fp)
FILE *fp;
{
    IrcVariable *var;
    char *cmdchars;

    cmdchars = DEFAULT_CMDCHARS;
    for (var = irc_variable; var->name; var++) {
	if (strcmp(var->name, "DISPLAY") == 0)
	    continue;
	if (strcmp(var->name, "CMDCHARS") == 0)
	    continue;
	fprintf(fp, "%cSET %s ", cmdchars[0], var->name);
	switch (var->type) {
	    case BOOL_TYPE_VAR:
		fprintf(fp, "%s\n", (var->integer) ? var_settings[ON] : var_settings[OFF]);
		break;
	    case CHAR_TYPE_VAR:
		fprintf(fp, "%c\n", var->integer);
		break;
	    case INT_TYPE_VAR:
		fprintf(fp, "%u\n", var->integer);
		break;
	    case STRING_TYPE_VAR:
		if (var->string)
		    fprintf(fp, "%s\n", var->string);
		else
		    fprintf(fp, "<EMPTY>\n");
		break;
	}
    }
    var = &(irc_variable[CMDCHARS_VAR]);
    fprintf(fp, "%cSET %s %c\n", cmdchars[0], var->name, var->integer);
}

char *make_string_var(var_name)
char *var_name;
{
    int cnt,
        index;
    char buffer[BUFFER_SIZE + 1],
        *ret = null(char *);

    if ((index = find_variable(var_name, &cnt)) == -1)
	return (null(char *));
    switch (irc_variable[index].type) {
	case STRING_TYPE_VAR:
	    malloc_strcpy(&ret, irc_variable[index].string);
	    break;
	case INT_TYPE_VAR:
	    sprintf(buffer, "%u", irc_variable[index].integer);
	    malloc_strcpy(&ret, buffer);
	    break;
	case BOOL_TYPE_VAR:
	    malloc_strcpy(&ret, var_settings[irc_variable[index].integer]);
	    break;
	case CHAR_TYPE_VAR:
	    sprintf(buffer, "%c", irc_variable[index].integer);
	    malloc_strcpy(&ret, buffer);
	    break;
    }
    return (ret);
}
