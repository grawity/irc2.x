/*
 * help.c: handles the help stuff for irc 
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/file.h>
#include "irc.h"
#include "term.h"
#include <stdio.h>

extern char *index();

static char help_type;
static char *help_args;

#define IRC_TYPE 0
#define VAR_TYPE 1
#define BIND_TYPE 2

/* The width of the columns when help displays lots of choices */
#define COLUMN_WIDTH 15
#define VAR_COLUMN_WIDTH 30
#define BIND_COLUMN_WIDTH 30

/* compar: used by scandir to alphabetize the help entries */
static int compar(e1, e2)
struct direct **e1,
     **e2;
{
    return (stricmp((*e1)->d_name, (*e2)->d_name));
}

/*
 * selectent: used by scandir to decide which entries to include in the help
 * listing.  Note that this strips off the .irchelp or .varhelp extensions. 
 */
static int selectent(entry)
struct direct *entry;
{
    int flag;
    char *ptr;

    if (ptr = index(entry->d_name, '.')) {
	*(ptr++) = null(char);
	switch (help_type) {
	    case IRC_TYPE:
		if (strcmp(ptr, "irchelp") == 0) {
		    if (strnicmp(entry->d_name, help_args, strlen(help_args)))
			return (0);
		    else {
			*(ptr - 1) = '.';
			flag = (operator || access(entry->d_name, X_OK));
			*(ptr - 1) = null(char);
			return (flag);
		    }
		}
		return (0);
	    case VAR_TYPE:
		if (strcmp(ptr, "varhelp") == 0) {
		    if (strnicmp(entry->d_name, help_args, strlen(help_args)))
			return (0);
		    else {
			*(ptr - 1) = '.';
			flag = (operator || access(entry->d_name, X_OK));
			if (help_args == null(char *))
			    return (flag);
			*(ptr - 1) = null(char);
			return (flag);
		    }
		}
		return (0);
	    case BIND_TYPE:
		if (strcmp(ptr, "bndhelp") == 0) {
		    if (strnicmp(entry->d_name, help_args, strlen(help_args)))
			return (0);
		    else {
			*(ptr - 1) = '.';
			flag = (operator || access(entry->d_name, X_OK));
			if (help_args == null(char *))
			    return (flag);
			*(ptr - 1) = null(char);
			return (flag);
		    }
		}
		return (0);
	    default:
		return (0);
	}
    }
    return (0);
}

/*
 * help_topic: displays the given help information is a nice way.  Or so we
 * hope 
 */
static void help_topic(name, type)
char *name;
char type;
{
    int cnt;
    FILE *fp;
    char line[80],
        *path = null(char *),
        *ptr;

    path = (char *) new_malloc(strlen(name) + 9);
    strcpy(path, name);
    switch (type) {
	case IRC_TYPE:
	    strcat(path, ".irchelp");
	    break;
	case VAR_TYPE:
	    strcat(path, ".varhelp");
	    break;
	case BIND_TYPE:
	    strcat(path, ".bndhelp");
	    break;
    }
    if ((fp = fopen(path, "r")) == null(FILE *)) {
	put_it("*** No help available on %s", name);
	free(path);
	return;
    }
    free(path);
    if (ptr = index(name, '.'))
	*ptr = null(char);
    put_it("*** Help on %s", name);
    cnt = 6;
    while (fgets(line, 80, fp)) {
	if (*(line + strlen(line) - 1) == '\n')
	    *(line + strlen(line) - 1) = null(char);
	put_it(line);
	if (cnt >= LI) {
	    char c;

	    put_it("******** Hit the spacebar for more, any other key to stop ********");
	    read(0, &c, 1);
	    if (c != ' ')
		break;
	    cnt = 4;
	}
	cnt++;
    }
    fclose(fp);
}

/*
 * help_var: given a variable name, reports help information on that
 * variable.  It does all the right things, displaying choices for ambigous
 * names, reports errors, etc 
 */
static void help_var(name)
char *name;
{
    struct direct **namelist;
    char *ptr;
    int cnt,
        i,
        entries,
        free_cnt;

    upper(name);
    help_args = name;
    help_type = VAR_TYPE;
    free_cnt = entries = scandir(".", &namelist, selectent, compar);
    /* special case to handle stuff like LOG and LOGFILE */
    if (entries > 1) {
	if (stricmp(namelist[0]->d_name, help_args) == 0)
	    entries = 1;
    }
    switch (entries) {
	case 0:
	    put_it("*** No help available on variable %s", name);
	    break;
	case 1:
	    help_topic(namelist[0]->d_name, VAR_TYPE);
	    set_variable(null(char *), name);
	    break;
	default:
	    put_it("*** Variable Choices:");
	    *buffer = null(char);
	    cnt = 0;
	    for (i = 0; i < entries; i++) {
		if (ptr = index(namelist[i]->d_name, '.'))
		    *ptr = null(char);
		strmcat(buffer, namelist[i]->d_name, BUFFER_SIZE);
		if (++cnt == 2) {
		    put_it(buffer);
		    *buffer = null(char);
		    cnt = 0;
		} else {
		    int x,
		        l;

		    l = strlen(namelist[i]->d_name);
		    for (x = l; x < VAR_COLUMN_WIDTH; x++)
			strmcat(buffer, " ", BUFFER_SIZE);
		}
	    }
	    if (*buffer)
		put_it(buffer);
	    break;
    }
    for (i = 0; i < free_cnt; i++)
	free(namelist[i]);
    free(namelist);
}

/*
 * help_bind: given a variable name, reports help information on that
 * variable.  It does all the right things, displaying choices for ambigous
 * names, reports errors, etc 
 */
static void help_bind(name)
char *name;
{
    struct direct **namelist;
    char *ptr;
    int cnt,
        i,
        entries,
        free_cnt;

    upper(name);
    help_args = name;
    help_type = BIND_TYPE;
    free_cnt = entries = scandir(".", &namelist, selectent, compar);
    /* special case to handle stuff like LOG and LOGFILE */
    if (entries > 1) {
	if (stricmp(namelist[0]->d_name, help_args) == 0)
	    entries = 1;
    }
    switch (entries) {
	case 0:
	    put_it("*** No help available on function %s", name);
	    break;
	case 1:
	    help_topic(namelist[0]->d_name, BIND_TYPE);
	    break;
	default:
	    put_it("*** Function Choices:");
	    *buffer = null(char);
	    cnt = 0;
	    for (i = 0; i < entries; i++) {
		if (ptr = index(namelist[i]->d_name, '.'))
		    *ptr = null(char);
		strmcat(buffer, namelist[i]->d_name, BUFFER_SIZE);
		if (++cnt == 2) {
		    put_it(buffer);
		    *buffer = null(char);
		    cnt = 0;
		} else {
		    int x,
		        l;

		    l = strlen(namelist[i]->d_name);
		    for (x = l; x < BIND_COLUMN_WIDTH; x++)
			strmcat(buffer, " ", BUFFER_SIZE);
		}
	    }
	    if (*buffer)
		put_it(buffer);
	    break;
    }
    for (i = 0; i < free_cnt; i++)
	free(namelist[i]);
    free(namelist);
}

/*
 * help: the HELP command, gives help listings for any and all topics out
 * there 
 */
void help(command, args)
char *command,
    *args;
{
    char path[PATH_LEN + 1];
    char *ptr;
    struct direct **namelist;
    int entries,
        free_cnt,
        cnt,
        i;

    if ((help_args = next_arg(args, &args)) == null(char *))
	help_args = "";

    upper(help_args);
    help_type = IRC_TYPE;
    getwd(path);
    chdir(IRC_HELP_PATH);
    free_cnt = entries = scandir(".", &namelist, selectent, compar);
    /* special case to handle stuff like LOG and LOGFILE */
    if (entries > 1) {
	if (stricmp(namelist[0]->d_name, help_args) == 0)
	    entries = 1;
    }
    switch (entries) {
	case 0:
	    put_it("*** No help available on %s", help_args);
	case 1:
	    if (stricmp(namelist[0]->d_name, "SET") == 0) {
		if ((help_args = next_arg(args, &args)) == null(char *)) {
		    help_topic(namelist[0]->d_name, IRC_TYPE);
		    help_var("");
		} else
		    help_var(help_args);
	    } else if (stricmp(namelist[0]->d_name, "BIND") == 0) {
		if ((help_args = next_arg(args, &args)) == null(char *)) {
		    help_topic(namelist[0]->d_name, IRC_TYPE);
		    help_bind("");
		} else {
		    char *more_args;

		    if (more_args = next_arg(args, &args))
			help_bind(more_args);
		    else
			help_bind(help_args);
		}
	    } else
		help_topic(namelist[0]->d_name, IRC_TYPE);
	    break;
	default:
	    put_it("*** Help choices:");
	    *buffer = null(char);
	    cnt = 0;
	    for (i = 0; i < entries; i++) {
		if (ptr = index(namelist[i]->d_name, '.'))
		    *ptr = null(char);
		strmcat(buffer, namelist[i]->d_name, BUFFER_SIZE);
		if (++cnt == 5) {
		    put_it(buffer);
		    *buffer = null(char);
		    cnt = 0;
		} else {
		    int x,
		        l;

		    l = strlen(namelist[i]->d_name);
		    for (x = l; x < COLUMN_WIDTH; x++)
			strmcat(buffer, " ", BUFFER_SIZE);
		}
	    }
	    if (*buffer)
		put_it(buffer);
	    put_it("*** Type \"/HELP <topic>\" for info on any given topic.");
	    break;
    }
    for (i = 0; i < free_cnt; i++)
	free(namelist[i]);
    free(namelist);
    chdir(path);
}

/* help_line: the help function used when a ? is hit on a command line */
void help_line(line)
char *line;
{
    if (*line == get_int_var(CMDCHAR_VAR)) {
	char *ptr = null(char *);

	if (*(line + 1) == ' ')
	    return;
	malloc_strcpy(&ptr, line + 1);
	help(null(char *), ptr);
	free(ptr);
    } else
	help(null(char *), "");
    cursor_to_input();	
}
