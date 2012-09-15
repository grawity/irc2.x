/*
 * ircaux.c: some extra routines... not specific to irc... that I needed 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <netdb.h>
#include "irc.h"
#include "ircaux.h"

/*
 * new_free:  Why do this?  Why not?  Saves me a bit of trouble here and
 * there 
 */
void new_free(ptr)
char **ptr;
{
    if (*ptr) {
	free(*ptr);
	*ptr = null(char *);
    }
}

char *new_realloc(ptr, size)
char *ptr;
int size;
{
    char *new_ptr;

    if ((new_ptr = (char *) realloc(ptr, size)) == null(char *)) {
	fprintf(stderr, "Malloc failed: %s\nIrc Aborted!\n", sys_errlist[errno]);
	exit(1);
    }
    return (new_ptr);
}

char *new_malloc(size)
int size;
{
    char *ptr;

    if ((ptr = (char *) malloc(size)) == null(char *)) {
	fprintf(stderr, "Malloc failed: %s\nIrc Aborted!\n", sys_errlist[errno]);
	exit(1);
    }
    return (ptr);
}

void malloc_strcpy(ptr, src)
char **ptr;
char *src;
{
    if (*ptr)
	free(*ptr);
    if (src) {
	*ptr = new_malloc(strlen(src) + 1);
	strcpy(*ptr, src);
    }else
      *ptr = null(char *);
}

/* malloc_strcat: Yeah, right */
void malloc_strcat(ptr, src)
char **ptr;
char *src;
{
    char *new;

    new = (char *) new_malloc(strlen(*ptr) + strlen(src) + 1);
    strcpy(new, *ptr);
    strcat(new, src);
    free(*ptr);
    *ptr = new;
}

char *upper(str)
char *str;
{
    char *ptr = null(char *);

    if (str) {
	ptr = str;
	for (; *str; str++) {
	    if (islower(*str))
		*str = toupper(*str);
	}
    }
    return (ptr);
}

/*
 * Connect_By_Number Performs a connecting to socket 'service' on host
 * 'host'.  Host can be a hostname or ip-address.  If 'host' is null, the
 * local host is assumed.   The parameter full_hostname will, on return,
 * contain the expanded hostname (if possible).  Note that full_hostname is a
 * pointer to a char *, and is allocated by connect_by_numbers() 
 *
 * Errors: 
 *
 * -1 get service failed 
 *
 * -2 get host failed 
 *
 * -3 socket call failed 
 *
 * -4 connect call failed 
 */
int connect_by_number(service, host)
int service;
char *host;
{
    int s;
    char buf[100];
    struct sockaddr_in server;
    struct hostent *hp;

    if (host == null(char *)) {
	gethostname(buf, sizeof(buf));
	host = buf;
    }
    if ((server.sin_addr.s_addr = inet_addr(host)) == -1) {
	if (hp = gethostbyname(host)) {
	    bzero((char *) &server, sizeof(server));
	    bcopy(hp->h_addr, (char *) &server.sin_addr, hp->h_length);
	    server.sin_family = hp->h_addrtype;
	} else
	    return (-2);
    } else
	server.sin_family = AF_INET;
    server.sin_port = (unsigned short) htons(service);
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return (-3);
    setsockopt(s, SOL_SOCKET, ~SO_LINGER, 0, 0);
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, 0, 0);
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, 0, 0);
    if (connect(s, &server, sizeof(server)) < 0) {
	close(s);
	return (-4);
    }
    return (s);
}

char *next_arg(str, new_ptr)
char *str,
   **new_ptr;
{
    char *ptr;

    if (ptr = sindex(str, "^ \t")) {
	if (str = sindex(ptr, " \t"))
	    *(str++) = null(char);
	else
	    str = empty_string;
    } else
	str = empty_string;
    if (new_ptr)
	*new_ptr = str;
    return (ptr);
}

/* stricmp: case insensitive version of strcmp */
int stricmp(str1, str2)
char *str1,
    *str2;
{
    register xor;

    for (; (*str1 != null(char)) || (*str2 != null(char)); str1++, str2++) {
	if ((*str1 == null(char)) || (*str2 == null(char)))
	    return (*str1 - *str2);
	if (isalpha(*str1) && isalpha(*str2)) {
	    xor = *str1 ^ *str2;
	    if ((xor != 32) && (xor != 0))
		return (*str1 - *str2);
	} else {
	    if (*str1 != *str2)
		return (*str1 - *str2);
	}
    }
    return (0);
}

/* strnicmp: case insensitive version of strncmp */
int strnicmp(str1, str2, n)
char *str1,
    *str2;
int n;
{
    register i,
             xor;

    for (i = 0; i < n; i++, str1++, str2++) {
	if (isalpha(*str1) && isalpha(*str2)) {
	    xor = *str1 ^ *str2;
	    if ((xor != 32) && (xor != 0))
		return (*str1 - *str2);
	} else {
	    if (*str1 != *str2)
		return (*str1 - *str2);
	}
    }
    return (0);
}

/*
 * strmcat: like strcat, but truncs the dest string to maxlen (thus the dest
 * should be able to handle maxlen+1 (for the null)) 
 */
void strmcat(dest, src, maxlen)
char *dest,
    *src;
int maxlen;
{
    int srclen,
        len;

    srclen = strlen(src);
    if ((len = strlen(dest) + srclen) > maxlen)
	strncat(dest, src, srclen - (len - maxlen));
    else
	strcat(dest, src);
}

/*
 * scanstr: looks for an occurrence of str in source.  If not found, returns
 * 0.  If it is found, returns the position in source (1 being the first
 * position).  Not the best way to handle this, but what the hell 
 */
int scanstr(source, str)
char *str,
    *source;
{
    int i,
        max,
        len;

    len = strlen(str);
    max = strlen(source) - len;
    for (i = 0; i < max; i++, source++) {
	if (strnicmp(source, str, len) == 0)
	    return (i + 1);
    }
    return (0);
}

/* expand_twiddle: expands ~ in pathnames. */
char *expand_twiddle(str)
char *str;
{
    /*
     * uses the global 'buffer', change it if you don't like it.  So, I'm
     * limiting path lengths to BUFFER_SIZE... so sue me 
     */
    if (*str == '~') {
	str++;
	if ((*str == '/') || (*str == null(char))) {
	    strncpy(buffer, my_path, BUFFER_SIZE);
	    strmcat(buffer, str, BUFFER_SIZE);
	} else {
	    char *rest;
	    struct passwd *entry;

	    if (rest = index(str, '/')) {
		*(rest++) = null(char);
		if (entry = getpwnam(str)) {
		    strncpy(buffer, entry->pw_dir, BUFFER_SIZE);
		    strmcat(buffer, "/", BUFFER_SIZE);
		    strmcat(buffer, rest, BUFFER_SIZE);
		} else
		    return (null(char *));
	    } else
		return (null(char *));
	}
    } else
	strncpy(buffer, str, BUFFER_SIZE);
    return (buffer);
}

/* isfirst: true if c is a legal first char for a nickname */
#define isfirst(c) (((c) >= 'A') && ((c) <= '}'))
/* islegal: true if c is a legal nickname char anywhere but first char */
#define islegal(c) ((((c) >= '0') && ((c) <= '}')) || ((c) == '-'))

/*
 * check_nickname: checks is a nickname is legal.  If the first character is
 * bad new, null is returned.  If the first character is bad, the string is
 * truncd down to only legal characters and returned 
 */
char *check_nickname(str)
char *str;
{
    char *ptr;


    if (str) {
	if (isfirst(*str)) {
	    for (ptr = str; *ptr; ptr++) {
		if (!(islegal(*ptr))) {
		    *ptr = null(char);
		    break;
		}
	    }
	    return (str);
	} else
	    return (null(char *));
    } else
	return (null(char *));
}

/*
 * sindex: much like index(), but it looks for a match of any character in
 * the group, and returns that position.  If the first character is a ^, then
 * this will match the first occurence not in that group. 
 */
char *sindex(string, group)
char *string,
    *group;
{
    char *ptr;

    if (*group == '^') {
	group++;
	for (; *string; string++) {
	    for (ptr = group; *ptr; ptr++) {
		if (*ptr == *string)
		    break;
	    }
	    if (*ptr == null(char))
		return (string);
	}
    } else {
	for (; *string; string++) {
	    for (ptr = group; *ptr; ptr++) {
		if (*ptr == *string)
		    return (string);
	    }
	}
    }
    return (null(char *));
}

/* is_number: returns true if the given string is a number, false otherwise */
int is_number(str)
char *str;
{
    while (*str == ' ')
	str++;
    if (*str == '-')
	str++;
    if (*str) {
	for (; *str; str++) {
	    if (!isdigit((*str)))
		return (0);
	}
	return (1);
    } else
	return (0);
}

/* rfgets: exactly like fgets, cept it works backwards through a file!  */
char *rfgets(buffer, size, file)
char *buffer;
int size;
FILE *file;
{
    char *ptr;
    long pos;

    if (fseek(file, -2L, 1))
	return (NULL);
    do {
	switch (fgetc(file)) {
	    case EOF:
		return (NULL);
	    case '\n':
		pos = ftell(file);
		ptr = fgets(buffer, size, file);
		fseek(file, pos, 0);
		return (ptr);
	}
    } while (fseek(file, -2L, 1) == 0);
    rewind(file);
    pos = 0L;
    ptr = fgets(buffer, size, file);
    fseek(file, pos, 0);
    return (ptr);
}
