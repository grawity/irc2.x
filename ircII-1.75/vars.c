/*
 * vars.c: All the dealing of the irc variables are handled here. 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include "irc.h"

#define NULL 0L

/* irc_variable: all the irc variables used */
static IrcVariable irc_variable[] =
{"CMDCHAR", CHAR_TYPE_VAR, DEFAULT_CMDCHAR, null(char *), make_status,
 "DISPLAY", BOOL_TYPE_VAR, 1, null(char *), NULL, 
 "HISTORY", INT_TYPE_VAR, 10, null(char *), set_history_size, 
 "INLINE_ALIASES", BOOL_TYPE_VAR, 0, null(char *), NULL, 
 "INSERT_MODE", BOOL_TYPE_VAR, 1, null(char *), make_status, 
 "LASTLOG", INT_TYPE_VAR, 10, null(char *), set_lastlog_size, 
 "LASTLOG_CONVERSATION", BOOL_TYPE_VAR, 1, null(char *), NULL, 
 "LASTLOG_MSG_ONLY", BOOL_TYPE_VAR, 0, null(char *), NULL, 
 "LOG", BOOL_TYPE_VAR, 0, null(char *), logger, 
 "LOGFILE", FILE_TYPE_VAR, 0, null(char *), set_log_file, 
 "SCROLL", BOOL_TYPE_VAR, 0, null(char *), NULL, 
 "SHELL", FILE_TYPE_VAR, 0, null(char *), NULL, 
 "SHELL_FLAGS", FILE_TYPE_VAR, 0, null(char *), NULL, 
 "SHELL_LIMIT", INT_TYPE_VAR, 0, null(char *), NULL,
 "WARN_OF_IGNORES", BOOL_TYPE_VAR, 1, null(char *), NULL, 
 null(char *), 0, 0, null(char *), NULL};

void init_variables()
{
         malloc_strcpy(&(irc_variable[LOGFILE_VAR].string), "IrcLog");
    malloc_strcpy(&(irc_variable[SHELL_VAR].string), "/bin/csh");
    malloc_strcpy(&(irc_variable[SHELL_FLAGS_VAR].string), "-fc");
    set_lastlog_size(irc_variable[LASTLOG_VAR].integer);
    set_history_size(irc_variable[COMMAND_HISTORY_VAR].integer);
}

IrcVariable *find_variable(name, cnt)
char *name;
int *cnt;
{
    IrcVariable *v,
               *first;
    int len;

    len = strlen(name);
    for (first = irc_variable; first->name; first++) {
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
	return (first);
    } else {
	*cnt = 0;
	return (null(IrcVariable *));
    }
}

void set_value(var, value)
IrcVariable *var;
char *value;
{
    switch (var->type) {
	case BOOL_TYPE_VAR:
	    if (value && *value) {
		upper(value);
		if (strcmp(value, "ON") == 0) {
		    var->integer = 1;
		    put_it("*** Value of %s set to ON", var->name, value);
		    if (var->func)
			(var->func) (var->integer);
		} else if (strcmp(value, "OFF") == 0) {
		    var->integer = 0;
		    put_it("*** Value of %s set to OFF", var->name, value);
		    if (var->func)
			(var->func) (var->integer);
		} else if (strcmp(value, "TOGGLE") == 0) {
		    var->integer = ! var->integer;
		    put_it("*** Value of %s set to %s", var->name, (var->integer) ? "ON" : "OFF");
		    if (var->func)
			(var->func) (var->integer);
		} else
		    put_it("*** Value must be either ON, OFF, or TOGGLE");
	    } else
		put_it("*** Current value of %s is %s", var->name, (var->integer) ? "ON" : "OFF");
	    break;
	case CHAR_TYPE_VAR:
	    if (value && *value) {
		if (strlen(value) > 1)
		    put_it("*** Value of %s must be a single character", var->name);
		else {
		    var->integer = *value;
		    put_it("*** Value of %s set to %c", var->name, var->integer);
		    if (var->func)
			(var->func) (var->integer);
		}
	    } else
		put_it("*** Current value of %s is '%c'", var->name, var->integer);
	    if (var->func)
		(var->func) (var->integer);
	    break;
	case INT_TYPE_VAR:
	    if (value && *value) {
		var->integer = atoi(value);
		put_it("*** Value of %s set to %d", var->name, var->integer);
		if (var->func)
		    (var->func) (var->integer);
	    } else
		put_it("*** Current value of %s is %d", var->name, var->integer);
	    if (var->func)
		(var->func) (var->integer);
	    break;
	case FILE_TYPE_VAR:
	    if (value && *value) {
		malloc_strcpy(&(var->string), value);
		put_it("*** Value of %s set to %s", var->name, var->string);
		if (var->func)
		    (var->func) (var->string);
	    } else
		put_it("*** Current value of %s is %s", var->name, var->string);
	    break;
    }
}

void set_var_value(var,value)
int var;
char *value;
{
    set_value(&(irc_variable[var]), value);
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
        *value;
    IrcVariable *v;
    int cnt,
        i;

    if (var = next_arg(args, &args)) {
	value = next_arg(args, &args);
	upper(var);
	v = find_variable(var, &cnt);
	switch (cnt) {
	    case 0:
		put_it("*** %s: No such variable", var);
		break;
	    case 1:
		set_value(v, value);
		break;
	    default:
		for (i = 0; i < cnt; i++, v++)
		    set_value(v, null(char *));
		break;
	}
    } else {
	for (v = irc_variable; v->name; v++)
	    set_value(v, null(char *));
    }
}

char *get_string_var(var)
int var;
{
    return(irc_variable[var].string);
}

int get_int_var(var)
int var;
{
    return(irc_variable[var].integer);
}

void set_string_var(var,string)
int var;
char *string;
{
    malloc_strcpy(&(irc_variable[var].string),string);
}
