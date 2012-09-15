/*
 * reg.c: handles matching of very simple wildcard expressions, where an
 * asterisk is a wildcard character that can match anything 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "reg.h"

#define WILD_CARD -1
#define null(foo) (foo) 0L

/*
 * parse_it: parse the next valid expression from the given string.  Because
 * we have terrible trivial expressions (either a series of characters or an
 * asterisk) this is a damn easy thing to do.  If a wild card is found, the
 * function value is null, otherwise, it is the next series of characters to
 * be matched.  An asterisk preceeded by a backslash is considered quoted and
 * treated as a character rather than an asterisk 
 */
static char *parse_it(str, space)
char *str;
char *space;
{
    char *ptr,
        *start;

    if (*str == '*') {
	strcpy(str, str + 1);
	return (null(char *));
    }
    ptr = space;
    start = str;
    *space = null(char);
    while (*str) {
	if (*str == '\\')
	    str++;
	else if (*str == '*')
	    break;
	*(ptr++) = *(str++);
    }
    *ptr = null(char);
    strcpy(start, str);
    return (space);
}

/*
 * wild_match: matches the expression 'match' which may contain wildcards,
 * with the str.  Returns 1 on match, 0 otherwise 
 */
int wild_match(match, str)
char *match,
    *str;
{
    char *ptr,
        *match_str = null(char *),
        *space;
    int len;

    if ((match == null(char *)) || (str == null(char *)))
	return (0);
    space = (char *) new_malloc(strlen(match) + 1);	/* never need more than
							 * this */
    malloc_strcpy(&match_str, match);
    while (strlen(match_str)) {
	if (ptr = parse_it(match_str, space)) {
	    len = strlen(ptr);
	    if (strnicmp(ptr, str, len)) {
		new_free(&space);
		new_free(&match_str);
		return (0);
	    }
	    str += len;
	} else {
	    while ((ptr = parse_it(match_str, space)) == null(char *));
	    if ((len = strlen(ptr)) == 0) {
		new_free(&space);
		new_free(&match_str);
		return (1);
	    }
	    while (1) {
		if (strnicmp(ptr, str, len) == 0) {
		    if (wild_match(match_str, str + len)) {
			new_free(&match_str);
			new_free(&space);
			return (1);
		    }
		    str++;
		} else {
		    if (*(str++) == null(char)) {
			new_free(&match_str);
			new_free(&space);
			return (0);
		    }
		}
	    }
	}
    }
    new_free(&match_str);
    new_free(&space);
    if (strlen(str))
	return (0);
    else
	return (1);
}
