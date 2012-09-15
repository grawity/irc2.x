/*
 * help.c: handles the help stuff for irc 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include "config.h"
#include "irc.h"
#include "term.h"
#include "vars.h"
#include "ircaux.h"
#include "input.h"
#include "window.h"

#ifndef ISC22
extern char *index();
extern char *rindex();
#else
extern char *strrchr();
#define rindex strrchr
#include <sys/dirent.h>
#define direct dirent
#endif

#define TOPIC_LIST_SIZE 255

static Window *help_window = null(Window *);
static char *this_arg;
static char *other_args;
static int entry_size;
static char topic_list[TOPIC_LIST_SIZE + 1];
static char *tmp_args = null(char *);

/* compar: used by scandir to alphabetize the help entries */
static int compar(e1, e2)
struct direct **e1,
     **e2;
{
    return (stricmp((*e1)->d_name, (*e2)->d_name));
}

/*
 * selectent: used by scandir to decide which entries to include in the help
 * listing.  
 */
static int selectent(entry)
struct direct *entry;
{
    struct stat stat_buf;

    if (*(entry->d_name) == '.')
	return (0);
    if (strnicmp(entry->d_name, this_arg, strlen(this_arg)))
	return (0);
    else {
	stat(entry->d_name, &stat_buf);
	if (operator || !(stat_buf.st_mode & S_ISUID)) {
	    int len;

	    len = strlen(entry->d_name);
	    entry_size = (len > entry_size) ? len : entry_size;
	    return (1);
	} else
	    return (0);
    }
}

static void help_sendline()
{
         malloc_strcpy(&tmp_args, get_input());
    this_arg = tmp_args;
    if (*this_arg == null(char))
	this_arg = null(char *);
    irc_io_loop = 0;
}

/*
 * help_topic: displays the given help information is a nice way.  Or so we
 * hope 
 */
static void help_topic(name)
char *name;
{
    int cnt;
    FILE *fp;
    char line[81];
    struct stat stat_buf;

    stat(name, &stat_buf);
    if (stat_buf.st_mode & S_IFDIR)
	return;

    if (name == null(char *))
	return;
    if ((fp = fopen(name, "r")) == null(FILE *)) {
	put_it("*** No help available on %s: Use ? for list of topics", name);
	return;
    }
    cnt = 3;
    put_it("*** Help on %s", name);

    while (fgets(line, 80, fp)) {
	if (*(line + strlen(line) - 1) == '\n')
	    *(line + strlen(line) - 1) = null(char);
	switch (*line) {
	    case '*':
		if (operator)
		    put_it("%s", line + 1);
		break;
	    case '-':
		if (!operator)
		    put_it("%s", line + 1);
		break;
	    default:
		put_it("%s", line);
		break;
	}
	if (cnt >= current_window->display_size) {
	    char c;

	    c = pause("******** Hit any key for more, 'q' to stop ********");
	    if ((c == 'q') || (c == 'Q'))
		break;
	    cnt = 1;
	}
	cnt++;
    }
    fclose(fp);
}

static void help_me(path, args)
char *path;
char *args;
{
    char *ptr;
    struct direct **namelist;
    int entries,
        free_cnt,
        cnt,
        i,
        cols;
    struct stat stat_buf;

    if (path) {
	if (chdir(path)) {
	    put_it("*** Cannot access help directory!");
	    return;
	}
	strmcat(topic_list, path, TOPIC_LIST_SIZE);
	strmcat(topic_list, " ", TOPIC_LIST_SIZE);
    } else
	strncpy(topic_list, "HELP ", TOPIC_LIST_SIZE);
    this_arg = args;
    while (this_arg) {
	current_window = help_window;
	message_from("*");
	if (*this_arg == null(char))
	    help_topic(path);
	if (strcmp(this_arg, "?") == 0)
	    this_arg = empty_string;
	entry_size = 0;
	free_cnt = entries = scandir(".", &namelist, selectent, compar);
	/* special case to handle stuff like LOG and LOGFILE */
	if (entries > 1) {
	    if (stricmp(namelist[0]->d_name, this_arg) == 0)
		entries = 1;
	}
	switch (entries) {
	    case -1:
		put_it("*** Error during help function: %s", sys_errlist[errno]);
		return;
	    case 0:
		put_it("*** No help available on %s: Use ? for list of topics", this_arg);
		break;
	    case 1:
		stat(namelist[0]->d_name, &stat_buf);
		if (stat_buf.st_mode & S_IFDIR) {
		    if ((args = next_arg(other_args, &other_args)) == null(char *))
			help_me(namelist[0]->d_name, empty_string);
		    else
			help_me(namelist[0]->d_name, args);
		} else
		    help_topic(namelist[0]->d_name);
		break;
	    default:
		put_it("*** %schoices:", topic_list);
		*buffer = null(char);
		cnt = 0;
		entry_size += 4;
		cols = CO / entry_size;
		for (i = 0; i < entries; i++) {
		    strmcat(buffer, namelist[i]->d_name, BUFFER_SIZE);
		    if (++cnt == cols) {
			put_it("%s", buffer);
			*buffer = null(char);
			cnt = 0;
		    } else {
			int x,
			    l;

			l = strlen(namelist[i]->d_name);
			for (x = l; x < entry_size; x++)
			    strmcat(buffer, " ", BUFFER_SIZE);
		    }
		}
		if (*buffer)
		    put_it("%s", buffer);

		break;
	}
	for (i = 0; i < free_cnt; i++)
	    new_free(&(namelist[i]));
	new_free(&namelist);
	cnt = strlen(topic_list);
	strmcat(topic_list, "Topic: ", TOPIC_LIST_SIZE);
	message_from(null(char *));
	if (irc_io(topic_list, help_sendline))
	    return;
	if (this_arg)
	    this_arg = next_arg(this_arg, &other_args);
	topic_list[cnt] = null(char);
    }
    chdir("..");
    *(topic_list + strlen(topic_list) - 1) = null(char);
    if (ptr = rindex(topic_list, ' '))
	*(ptr + 1) = null(char);
}

/*
 * help: the HELP command, gives help listings for any and all topics out
 * there 
 */
void help(command, args)
char *command,
    *args;
{
    int hold;
#ifndef HELP_SERVICE
    char path[PATH_LEN + 1];
#endif HELP_SERVICE

    if (get_int_var(HELP_WINDOW_VAR)) {
	new_window();
#ifdef HELP_SERVICE
	query("QUERY", HELP_SERVICE);
#endif HELP_SERVICE
    }
    help_window = current_window;
    hold = help_window->hold_mode;
    help_window->hold_mode = OFF;
    message_from("*");
#ifdef HELP_SERVICE
    send_to_server("PRIVMSG %s :HELP %s", HELP_SERVICE, args);
#else HELP_SERVICE

    if ((args = next_arg(args, &other_args)) == null(char *))
	args = empty_string;

    upper(args);
#ifndef ISC22
    getwd(path);
#else
    getcwd(path);
#endif
    if (chdir(IRC_HELP_PATH)) {
	put_it("*** Cannot access help directory %s", IRC_HELP_PATH);
	return;
    }
    help_me(null(char *), args);
    chdir(path);
#endif HELP_SERVICE
    help_window->hold_mode = hold;
#ifndef HELP_SERVICE
    if (get_int_var(HELP_WINDOW_VAR))
	delete_window(help_window);
#endif HELP_SERVICE
    message_from(null(char *));
}
