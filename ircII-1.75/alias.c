/*
 * alias.c Handles command aliases for irc.c 
 *
 *
 * Written by Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserverd 
 */
#include <ctype.h>
#include "irc.h"

/* Alias: structure of each alias entry */
typedef struct AliasStru {
    char *name;			/* name of alias */
    char *stuff;		/* what the alias is */
    char mark;			/* used to prevent recursive aliasing */
    struct AliasStru *next;	/* pointer to next alias in list */
}         Alias;

/* aliases: pointer to the top of the alias list */
static Alias *aliases = null(Alias *);

/*
 * find_alias: looks up name in in alias list.  Returns the Alias entry if
 * found, or null if not found.   If unlink is set, the found entry is
 * removed from the list as well 
 */
static Alias *find_alias(name, unlink)
char *name;
char unlink;
{
    Alias *tmp,
         *last = null(Alias *);
    int cmp;

    for (tmp = aliases; tmp; tmp = tmp->next) {
	if ((cmp = stricmp(name, tmp->name)) == 0) {
	    if (unlink) {
		if (last)
		    last->next = tmp->next;
		else
		    aliases = tmp->next;
	    }
	    return (tmp);
	} else if (cmp < 0)
	    break;
	last = tmp;
    }
    return (null(Alias *));
}

/*
 * insert_alias: adds the given alias to the alias list.  The alias list is
 * alphabetized by name 
 */
static void insert_alias(alias)
Alias *alias;
{
    Alias *tmp,
         *last,
         *foo;

    last = null(Alias *);
    for (tmp = aliases; tmp; tmp = tmp->next) {
	if (strcmp(alias->name, tmp->name) < 0)
	    break;
	last = tmp;
    }
    if (last) {
	foo = last->next;
	last->next = alias;
	alias->next = foo;
    } else {
	alias->next = aliases;
	aliases = alias;
    }
}

/*
 * add_alias: given the alias name and "stuff" that makes up the alias,
 * add_alias() first checks to see if the alias name is already in use... if
 * so, the old alias is replaced with the new one.  If the alias is not
 * already in use, it is added. 
 */
void add_alias(name, stuff)
char *name,
    *stuff;
{
    Alias *tmp;

    upper(name);
    if ((tmp = find_alias(name, 1)) == null(Alias *)) {
	tmp = (Alias *) malloc(sizeof(Alias));
	if (tmp == null(Alias *)) {
	    put_it("*** Couldn't allocate memory for new alias!");
	    return;
	}
	tmp->name = null(char *);
	tmp->stuff = null(char *);
    }
    malloc_strcpy(&(tmp->name), name);
    malloc_strcpy(&(tmp->stuff), stuff);
    tmp->mark = 0;
    insert_alias(tmp);
    put_it("*** Alias %s added", name);
}

/*
 * arg_number: Returns the argument 'num' from 'str', or, if 'num' is
 * negative, returns from argument 'num' to the end of 'str'.  You might be
 * wondering what's going on down there... here goes.  First we copy 'str' to
 * malloced space.  Then, using next_arg(), we strip out each argument ,
 * putting them in arg_list, and putting their position in the original
 * string in arg_list_pos.  Note that this parsing is only done once, as long
 * as time remains the same.  Thus, multiple call to arg_number() with the
 * same time will result in the parsing only occuring the first time. 
 * Anyway, once parsing is done, the arguments are returned directly from the
 * arg_list array... or in the case of negative 'num', the arg_list_pos is
 * used to return the postion of the rest of the args in the original
 * string... got it?  Anyway, the bad points of the routine:  1) Always
 * parses out everything, even if only one arg is used.  2) The malloced
 * stuff remains around until arg_number is called with a different string. 
 * Even then, some malloced stuff remains around.  This can be fixed. 
 */
static char *arg_number(num, str, time)
int num;
char *str;
long time;
{
    char *ptr,
        *arg,
         just_one = 1;
    int pos;
    static long last_time = 0L;
    static char *last_args = null(char *);
    static char **arg_list = null(char **);
    static int *arg_list_pos = null(int *);
    static int arg_list_size;

    if (num < 0) {
	just_one = 0;
	num *= -1;
    }
    if (time != last_time) {
	int arg_list_limit;

	last_time = time;
	if (arg_list)
	    free(arg_list);
	if (arg_list_pos)
	    free(arg_list_pos);
	arg_list_size = 0;
	arg_list_limit = 10;
	arg_list = (char **) new_malloc(sizeof(char *) * arg_list_limit);
	arg_list_pos = (int *) new_malloc(sizeof(int) * arg_list_limit);
	malloc_strcpy(&last_args, str);
	ptr = last_args;
	pos = 0;
	while (arg = next_arg(ptr, &ptr)) {
	    arg_list_pos[arg_list_size] = pos;
	    pos = (int) (ptr - last_args);
	    arg_list[arg_list_size++] = arg;
	    if (arg_list_size == arg_list_limit) {
		arg_list_limit += 10;
		arg_list = (char **) new_realloc(arg_list, sizeof(char *) * arg_list_limit);
		arg_list_pos = (int *) new_realloc(arg_list_pos, sizeof(int) * arg_list_limit);
	    }
	}
    }
    if (num < arg_list_size) {
	if (just_one)
	    return (arg_list[num]);
	else
	    return (str + arg_list_pos[num]);
    } else
	return ("");
}

/*
 * get_alias: returns the alias matching 'name' as the function value. 'args'
 * are expanded as needed, etc.  If no matching alias is found, null is
 * returned.  Otherwise, the alias is returned.  NOTE: get_alias() mallocs
 * the space for the returned alias, so free it when done! 
 *
 */
char *get_alias(name, args)
char *name,
    *args;
{
    Alias *tmp;
    char *ptr,
        *stuff,
         c,
         args_not_used = 1;
    long t;

    if (name == null(char *))
	return (null(char *));
    if (tmp = find_alias(name, 0)) {
	stuff = tmp->stuff;
	*buffer = null(char);
	t = time(0);
	while (ptr = index(stuff, '$')) {
	    *ptr = null(char);
	    c = *(++ptr);
	    strmcat(buffer, stuff, BUFFER_SIZE);
	    if (c == '.') {
		strmcat(buffer, sent_nick, BUFFER_SIZE);
		stuff = ptr + 1;
	    } else if (c == ',') {
		strmcat(buffer, recv_nick, BUFFER_SIZE);
		stuff = ptr + 1;
	    } else if (c == '$') {
		strmcat(buffer, "$", BUFFER_SIZE);
		stuff = ptr + 1;
	    } else if (c == '*') {
		strmcat(buffer, args, BUFFER_SIZE);
		args_not_used = 0;
		stuff = ptr + 1;
	    } else if (isdigit(c)) {
		strmcat(buffer, arg_number(atoi(ptr), args, t), BUFFER_SIZE);
		args_not_used = 0;
		for (stuff = ptr + 1; isdigit((*stuff)); stuff++);
	    } else if (c == '-') {
		strmcat(buffer, arg_number(atoi(ptr), args, t), BUFFER_SIZE);
		args_not_used = 0;
		for (stuff = ptr + 1; isdigit((*stuff)); stuff++);
	    } else
		stuff = ptr;
	    *(ptr - 1) = '$';
	}
	strmcat(buffer, stuff, BUFFER_SIZE);
	if (args_not_used && args && *args) {
	    strmcat(buffer, " ", BUFFER_SIZE);
	    strmcat(buffer, args, BUFFER_SIZE);
	}
	ptr = null(char *);
	malloc_strcpy(&ptr, buffer);
	return (ptr);
    }
    return (null(char *));
}

/* delete_alias: The alias name is removed from the alias list. */
void delete_alias(name)
char *name;
{
    Alias *tmp;

    upper(name);
    if (tmp = find_alias(name, 1)) {
	if (tmp->name)
	    free(tmp->name);
	if (tmp->stuff)
	    free(tmp->stuff);
	free(tmp);
	put_it("*** Alias %s removed", name);
    } else
	put_it("*** No such alias: %s", name);
}

/*
 * list_aliases: Lists all aliases matching 'name'.  If name is null, all
 * aliases are listed 
 */
void list_aliases(name)
char *name;
{
    Alias *tmp;
    int len;

    put_it("*** Aliases");
    if (name) {
	upper(name);
	len = strlen(name);
    }
    for (tmp = aliases; tmp; tmp = tmp->next) {
	if (name) {
	    if (strncmp(tmp->name, name, len) == 0)
		put_it("%s %s", tmp->name, tmp->stuff);
	} else
	    put_it("%s %s", tmp->name, tmp->stuff);
    }
}

/*
 * mark_alias: sets the mark field of the given alias to 'flag', and returns
 * the previous value of the mark.  If the name is not found, -1 is returned.
 * This is used to prevent recursive aliases by marking and unmarking
 * aliases, and not reusing an alias that has previously been marked.  I'll
 * explain later 
 */
char mark_alias(name, flag)
char *name;
char flag;
{
    char old_mark;
    Alias *tmp;

    if (tmp = find_alias(name, 0)) {
	old_mark = tmp->mark;
	tmp->mark = flag;
	return (old_mark);
    }
    return (-1);
}

/* inline_aliases: converts any aliases found in line... really it does.
The returned string is allocated by the inline_aliases function, so release it later */
char *inline_aliases(line)
char *line;
{
    char new_line[BUFFER_SIZE + 1],
        *start_ptr,
        *end_ptr,
         space;
    Alias *alias;

    *new_line = null(char);
    while (start_ptr = index(line, '%')) {
	*(start_ptr++) = null(char);
	strmcat(new_line, line, BUFFER_SIZE);
	if (end_ptr = sindex(start_ptr, " %")) {
	    space = (*end_ptr == ' ');
	    *end_ptr = null(char);
	}
	if (alias = find_alias(start_ptr, 0))
	    strmcat(new_line, alias->stuff, BUFFER_SIZE);
	else {
	    strmcat(new_line, "%", BUFFER_SIZE);
	    strmcat(new_line, start_ptr, BUFFER_SIZE);
	}
	if (end_ptr) {
	    if (space) {
		*end_ptr = ' ';
		line = end_ptr;
	    } else
		line = end_ptr + 1;
	} else
	    line = "";
    }
    strmcat(new_line, line, BUFFER_SIZE);
    start_ptr = null(char *);
    malloc_strcpy(&start_ptr, new_line);
    return (start_ptr);
}
