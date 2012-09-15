/*
 * alias.c Handles command aliases for irc.c 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "irc.h"
#include "alias.h"
#include "edit.h"
#include "vars.h"
#include "ircaux.h"
#include "server.h"
#include "window.h"

#define LEFT_BRACKET '['
#define RIGHT_BRACKET ']'
#define LEFT_PAREN '('
#define RIGHT_PAREN ')'
#define DOUBLE_QUOTE '"'

extern int strnicmp();
extern int stricmp();

/* Alias: structure of each alias entry */
typedef struct AliasStru {
    char *name;			/* name of alias */
    char *stuff;		/* what the alias is */
    char mark;			/* used to prevent recursive aliasing */
    struct AliasStru *next;	/* pointer to next alias in list */
}         Alias;

/* aliases: pointer to the top of the alias list */
static Alias *aliases = null(Alias *);

/* alias_string: the thing that gets replaced by the $"..." construct */
static char *alias_string = null(char *);

/*
 * find_alias: looks up name in in alias list.  Returns the Alias entry if
 * found, or null if not found.   If unlink is set, the found entry is
 * removed from the list as well.  If match is null, only perfect matches
 * will return anything.  Otherwise, the number of matches will be returned. 
 */
static Alias *find_alias(name, unlink, match)
char *name;
char unlink;
int *match;
{
    Alias *tmp,
         *last = null(Alias *);
    int cmp,
        len;
    int (*cmp_func) ();

    if (match) {
	*match = 0;
	cmp_func = strnicmp;
    } else
	cmp_func = stricmp;
    len = strlen(name);
    if (name) {
	for (tmp = aliases; tmp; tmp = tmp->next) {
	    if ((cmp = cmp_func(name, tmp->name, len)) == 0) {
		if (unlink) {
		    if (last)
			last->next = tmp->next;
		    else
			aliases = tmp->next;
		}
		if (match) {
		    (*match)++;
		    if (strlen(tmp->name) == len) {
			*match = 0;
			return (tmp);
		    }
		} else
		    return (tmp);
	    } else if (cmp < 0)
		break;
	    last = tmp;
	}
    }
    if (match && (*match == 1))
	return (last);
    else
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
    if ((tmp = find_alias(name, 1, null(int *))) == null(Alias *)) {
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

/* alias_arg: a special version of next_arg for aliases */
static char *alias_arg(str, pos)
char **str;
int *pos;
{
    char *ptr;

    *pos = 0;
    ptr = *str;
    while ((*ptr == ' ') || (*ptr == '\t')) {
	*ptr++;
	(*pos)++;
    }
    if (*ptr == null(char)) {
	*str = empty_string;
	return (null(char *));
    }
    if (*str = sindex(ptr, " \t"))
	*((*str)++) = null(char);
    else
	*str = empty_string;
    return (ptr);
}

/*
 * arg_number: Returns the argument 'num' from 'str', or, if 'num' is
 * negative, returns from argument 'num' to the end of 'str'.  You might be
 * wondering what's going on down there... here goes.  First we copy 'str' to
 * malloced space.  Then, using next_arg(), we strip out each argument ,
 * putting them in arg_list, and putting their position in the original
 * string in arg_list_pos.  Note that this parsing is only done once, as long
 * as time remains the same.  Thus, multiple call to arg_number() with the
 * same time will result in the parsing only occuring the first time. Anyway,
 * once parsing is done, the arguments are returned directly from the
 * arg_list array... or in the case of negative 'num', the arg_list_pos is
 * used to return the postion of the rest of the args in the original
 * string... got it?  Anyway, the bad points of the routine:  1) Always
 * parses out everything, even if only one arg is used.  2) The malloced
 * stuff remains around until arg_number is called with a different string.
 * Even then, some malloced stuff remains around.  This can be fixed. 
 */
static char *arg_number(lower_lim, upper_lim, str, eval)
int lower_lim,
    upper_lim;
char *str;
int *eval;
{
    char *ptr,
        *arg,
         use_full = 0,
         c;
    unsigned int pos,
        start_pos;
    static char *last_args = null(char *);
    static char *last_range = null(char *);
    static char **arg_list = null(char **);
    static unsigned int *arg_list_pos = null(int unsigned *);
    static unsigned int *arg_list_end_pos = null(int unsigned *);
    static int arg_list_size;

    if (*eval) {
	int arg_list_limit;

	*eval = 0;
	new_free(&arg_list);
	new_free(&arg_list_pos);
	new_free(&arg_list_end_pos);
	arg_list_size = 0;
	arg_list_limit = 10;
	arg_list = (char **) new_malloc(sizeof(char *) * arg_list_limit);
	arg_list_pos = (unsigned int *) new_malloc(sizeof(unsigned int) * arg_list_limit);
	arg_list_end_pos = (unsigned int *) new_malloc(sizeof(unsigned int) * arg_list_limit);
	malloc_strcpy(&last_args, str);
	ptr = last_args;
	pos = 0;
	while (arg = alias_arg(&ptr, &start_pos)) {
	    arg_list_pos[arg_list_size] = pos;
	    pos += start_pos + strlen(arg);
	    arg_list_end_pos[arg_list_size] = pos++;
	    arg_list[arg_list_size++] = arg;
	    if (arg_list_size == arg_list_limit) {
		arg_list_limit += 10;
		arg_list = (char **) new_realloc(arg_list, sizeof(char *) * arg_list_limit);
		arg_list_pos = (unsigned int *) new_realloc(arg_list_pos, sizeof(unsigned int) * arg_list_limit);
		arg_list_end_pos = (unsigned int *) new_realloc(arg_list_end_pos, sizeof(unsigned int) * arg_list_limit);
	    }
	}
    }
    if (arg_list_size == 0)
	return (empty_string);
    if ((upper_lim >= arg_list_size) || (upper_lim < 0)) {
	use_full = 1;
	upper_lim = arg_list_size - 1;
    }
    if (upper_lim < lower_lim)
	return (empty_string);
    if (lower_lim >= arg_list_size)
	lower_lim = arg_list_size - 1;
    else if (lower_lim < 0)
	lower_lim = 0;
    if ((use_full == 0) && (lower_lim == upper_lim))
	return (arg_list[lower_lim]);
    c = *(str + arg_list_end_pos[upper_lim]);
    *(str + arg_list_end_pos[upper_lim]) = null(char);
    malloc_strcpy(&last_range, str + arg_list_pos[lower_lim]);
    *(str + arg_list_end_pos[upper_lim]) = c;
    return (last_range);
}

/*
 * parse_number: returns the next number found in a string and moves the
 * string pointer beyond that point in the string.  Here's some examples: 
 *
 * "123harhar"  returns 123 and str as "harhar" 
 *
 * while: 
 *
 * "hoohar"     returns -1  and str as "hoohar" 
 */
static int parse_number(str)
char **str;
{
    int ret;
    char *ptr;

    ptr = *str;
    if (isdigit(*ptr)) {
	ret = atoi(ptr);
	for (; isdigit(*ptr); ptr++);
	*str = ptr;
    } else
	ret = -1;
    return (ret);
}

void do_alias_string()
{
         malloc_strcpy(&alias_string, get_input());
    irc_io_loop = 0;
}

static void expander_addition(buff, add, length)
char *buff,
    *add;
int length;
{
    char format[40];

    if (length) {
	sprintf(format, "%%%ds", -length);
	sprintf(buffer, format, add);
	if (strlen(buffer) > abs(length))
	    buffer[abs(length)] = null(char);
	add = buffer;
    }
    strmcat(buff, add, BUFFER_SIZE);
}

/*
 * expand_alias: Expands inline variables in the given string and returns the
 * expanded string in a new string which is malloced by expand_alias().
 * Inline variables are one of the following: 
 *
 * $"prompt"      prompts the user for input and replaces the result 
 *
 * $.	       expands to nick of last person to whom you sent a MSG 
 *
 * $B          expands to the body of the last message you sent. 
 *
 * $,	       expands to nick of last person who sent you a MSG 
 *
 * $:	       expands to nick of last person to join a channel 
 *
 * $; 	       expands to last person to send a public message 
 *
 * $S          expands to your server name 
 *
 * $+	       expands to the current channel 
 *
 * $=          expands to channel of last INVITE 
 *
$*          expands to all arguments 
 *
 * $^          expands to NO arguments.  Prevents arguments from automatically
 * being appended to the end. 
 *
 * $/          expands to the current setting of CMDCHAR 
 *
 * $Q          expands to current query nick 
 *
 * $T          expands to the 'target' of your input line (either query user of
 * channel) 
 *
 * $N	       expands to your nick 
 *
 * $S	       expands to your server 
 *
 * $n          expands to nth argument, where n is a positive integer 
 *
 * $n-m        expands to the range of args between n and m inclusive 
 *
 */
char *expand_alias(name, string, args)
char *name,
    *string,
    *args;
{
    char buffer[BUFFER_SIZE],
        *ptr,
        *stuff = null(char *),
        *free_stuff,
         c,
         args_not_used = 1;
    int upper,
        lower,
        eval,
        length;

    malloc_strcpy(&stuff, string);
    free_stuff = stuff;
    *buffer = null(char);
    eval = 1;
    while (ptr = index(stuff, '$')) {
	length = 0;
	*ptr = null(char);
	if ((c = *(++ptr)) == LEFT_BRACKET) {
	    char *rest;

	    ptr++;
	    if (rest = index(ptr, RIGHT_BRACKET)) {
		*(rest++) = null(char);
		if (ptr = expand_alias(null(char *), ptr, empty_string)) {
		    length = atoi(ptr);
		    new_free(&ptr);
		}
		ptr = rest;
		c = *ptr;
	    } else {
		put_it("*** Missing %c", RIGHT_BRACKET);
		return (empty_string);
	    }
	}
	strmcat(buffer, stuff, BUFFER_SIZE);
	switch (c) {
	    case LEFT_PAREN:
		{
		    char *var_name;

		    var_name = ptr + 1;
		    if (ptr = index(var_name, RIGHT_PAREN))
			*(ptr++) = null(char);
		    if (var_name = make_string_var(var_name)) {
			expander_addition(buffer, var_name, length);
			new_free(&var_name);
		    } else
			put_it("*** Unknown or ambiguous variable");
		}
		stuff = ptr;
		break;
	    case DOUBLE_QUOTE:
		{
		    char *prompt;

		    if (name && mark_alias(name, 1)) {
			new_free(&free_stuff);
			ptr = null(char *);
			malloc_strcpy(&ptr, empty_string);
			return (ptr);	/* tricky... make the parser do nada */
		    }
		    prompt = ptr + 1;
		    if (ptr = index(prompt, DOUBLE_QUOTE))
			*(ptr++) = null(char);
		    alias_string = null(char *);
		    if (irc_io(prompt, do_alias_string)) {
			put_it("Illegal recursive edit");
			new_free(&free_stuff);
			ptr = null(char *);
			malloc_strcpy(&ptr, empty_string);
			if (name)
			    mark_alias(name, 0);
			return (ptr);	/* tricky... make the parser do nada */
		    }
		    if (name)
			mark_alias(name, 0);
		    expander_addition(buffer, alias_string, length);
		    new_free(&alias_string);
		    stuff = ptr;
		}
		break;
	    case '.':
		if (sent_nick)
		    expander_addition(buffer, sent_nick, length);
		stuff = ptr + 1;
		break;
	    case 'B':
		if (sent_body)
		    expander_addition(buffer, sent_body, length);
		stuff = ptr + 1;
		break;
	    case ',':
		if (recv_nick)
		    expander_addition(buffer, recv_nick, length);
		stuff = ptr + 1;
		break;
	    case ':':
		if (joined_nick)
		    expander_addition(buffer, joined_nick, length);
		stuff = ptr + 1;
		break;
	    case ';':
		if (public_nick)
		    expander_addition(buffer, public_nick, length);
		stuff = ptr + 1;
		break;
	    case '$':
		expander_addition(buffer, "$", length);
		stuff = ptr + 1;
		break;
	    case '*':
		expander_addition(buffer, args, length);
		args_not_used = 0;
		stuff = ptr + 1;
		break;
	    case '^':
		args_not_used = 0;
		stuff = ptr + 1;
		break;
	    case '+':
		if (current_channel(null(char *)))
		    expander_addition(buffer, current_channel(null(char *)), length);
		else
		    expander_addition(buffer, "0", length);
		stuff = ptr + 1;
		break;
	    case 'S':
		if (current_server_name(null(char *)))
		    expander_addition(buffer, current_server_name(null(char *)), length);
		stuff = ptr + 1;
		break;
	    case 'Q':
		if (query_nick())
		    expander_addition(buffer, query_nick(), length);
		stuff = ptr + 1;
		break;
	    case 'T':
		if (query_nick())
		    expander_addition(buffer, query_nick(), length);
		else if (current_channel(null(char *)))
		    expander_addition(buffer, current_channel(null(char *)), length);
		stuff = ptr + 1;
		break;
	    case 'N':
		/* Thanks to lynx for this */
		expander_addition(buffer, nickname, length);
		stuff = ptr + 1;
		break;
	    case '=':
		if (invite_channel)
		    expander_addition(buffer, invite_channel, length);
		stuff = ptr + 1;
		break;
	    case '/':
		{
		    char thing[2],
		        *cmdchars;

		    if ((cmdchars = get_string_var(CMDCHARS_VAR)) == null(char *))
			cmdchars = DEFAULT_CMDCHARS;
		    thing[0] = cmdchars[0];
		    thing[1] = null(char);
		    expander_addition(buffer, thing, length);
		    stuff = ptr + 1;
		}
		break;
	    default:
		if (isdigit(c) || (c == '-')) {
		    lower = parse_number(&ptr);
		    if (*ptr == '-') {
			ptr++;
			upper = parse_number(&ptr);
		    } else
			upper = lower;
		    stuff = ptr;
		    expander_addition(buffer, arg_number(lower, upper, args, &eval), length);
		    args_not_used = 0;
		} else
		    stuff = ptr;
		break;
	}
    }
    strmcat(buffer, stuff, BUFFER_SIZE);
    if (args_not_used && args && *args) {
	strmcat(buffer, " ", BUFFER_SIZE);
	strmcat(buffer, args, BUFFER_SIZE);
    }
    ptr = null(char *);
    new_free(&free_stuff);
    malloc_strcpy(&ptr, buffer);
    return (ptr);
}

/*
 * get_alias: returns the alias matching 'name' as the function value. 'args'
 * are expanded as needed, etc.  If no matching alias is found, null is
 * returned, cnt is 0, and full_name is null.  If one matching alias is
 * found, it is retuned, with cnt set to 1 and full_name set to the full name
 * of the alias.  If more than 1 match are found, null is returned, cnt is
 * set to the number of matches, and fullname is null. NOTE: get_alias()
 * mallocs the space for the returned alias and for full_name, so free it
 * when done! 
 */
char *get_alias(name, args, cnt, full_name)
char *name,
    *args,
   **full_name;
int *cnt;
{
    Alias *tmp;

    *full_name = null(char *);
    if ((name == null(char *)) || (*name == null(char))) {
	*cnt = 0;
	return (null(char *));
    }
    if (tmp = find_alias(name, 0, cnt)) {
	if (*cnt < 2) {
	    malloc_strcpy(full_name, tmp->name);
	    return (expand_alias(tmp->name, tmp->stuff, args));
	}
    }
    return (null(char *));
}

/*
 * match_alias: this returns a list of alias names that match the given name.
 * This is used for command completion etc.  Note that the returned array is
 * malloced in this routine.  Returns null if no matches are found 
 */
char **match_alias(name, cnt)
char *name;
int *cnt;
{
    Alias *tmp;
    char **matches = null(char **);
    int matches_size = 5;
    int len;

    len = strlen(name);
    *cnt = 0;
    matches = (char **) new_malloc(sizeof(char *) * matches_size);
    for (tmp = aliases; tmp; tmp = tmp->next) {
	if (strncmp(name, tmp->name, len) == 0) {
	    matches[*cnt] = null(char *);
	    malloc_strcpy(&(matches[*cnt]), tmp->name);
	    if (++(*cnt) == matches_size) {
		matches_size += 5;
		matches = (char **) new_realloc(matches, sizeof(char *) * matches_size);
	    }
	} else if (*cnt)
	    break;
    }
    if (*cnt) {
	matches = (char **) new_realloc(matches, sizeof(char *) * (*cnt + 1));
	matches[*cnt] = null(char *);
    } else
	new_free(&matches);
    return (matches);
}

/* delete_alias: The alias name is removed from the alias list. */
void delete_alias(name)
char *name;
{
    Alias *tmp;

    upper(name);
    if (tmp = find_alias(name, 1, null(int *))) {
	new_free(&(tmp->name));
	new_free(&(tmp->stuff));
	new_free(&tmp);
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

    put_it("*** Aliases:");
    if (name) {
	upper(name);
	len = strlen(name);
    }
    for (tmp = aliases; tmp; tmp = tmp->next) {
	if (name) {
	    if (strncmp(tmp->name, name, len) == 0)
		put_it("***\t%s\t%s", tmp->name, tmp->stuff);
	} else
	    put_it("***\t%s\t%s", tmp->name, tmp->stuff);
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
    int match;

    if (tmp = find_alias(name, 0, &match)) {
	if (match < 2) {
	    old_mark = tmp->mark;
	    tmp->mark = flag;
	    return (old_mark);
	}
    }
    return (-1);
}

/*
 * inline_aliases: converts any aliases found in line... really it does. The
 * returned string is allocated by the inline_aliases function, so release it
 * later 
 */
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
	if (alias = find_alias(start_ptr, 0, null(int *)))
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
	    line = empty_string;
    }
    strmcat(new_line, line, BUFFER_SIZE);
    start_ptr = null(char *);
    malloc_strcpy(&start_ptr, new_line);
    return (start_ptr);
}

/*
 * execute_alias: After an alias has been identified and expanded, it is sent
 * here for proper execution.  This routine mainly prevents recursive
 * aliasing.  The name is the full name of the alias, and the alias is
 * already expanded alias (both of these parameters are returned by
 * get_alias()) 
 */
void execute_alias(alias_name, alias)
char *alias_name,
    *alias;
{
    if (mark_alias(alias_name, 1))
	put_it("*** Illegal recursive alias: %s", alias_name);
    else {
	parse_command(alias, 0);
	mark_alias(alias_name, 0);
    }
}

/*
 * save_aliases: This will write all of the aliases to the FILE pointer fp in
 * such a way that they can be read back in using LOAD or the -l switch 
 */
void save_aliases(fp)
FILE *fp;
{
    Alias *tmp;
    char *cmdchars;
    char *stuff,
        *ptr;

    if ((cmdchars = get_string_var(CMDCHARS_VAR)) == null(char *))
	cmdchars = DEFAULT_CMDCHARS;
    for (tmp = aliases; tmp; tmp = tmp->next) {
	for (ptr = buffer, stuff = tmp->stuff; *stuff; stuff++) {
	    if (*stuff == QUOTE_CHAR)
		*(ptr++) = QUOTE_CHAR;
	    *(ptr++) = *stuff;
	}
	*ptr = null(char);
	fprintf(fp, "%cALIAS %s %s\n", cmdchars[0], tmp->name, buffer);
    }
}
