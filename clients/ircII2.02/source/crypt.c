/*
 * crypt.c: handles some encryption of messages stuff. 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>
#ifdef ISC22
#include <unistd.h>
#endif
#include "irc.h"
#include "crypt.h"
#include "vars.h"
#include "ircaux.h"
#include "list.h"

/*
 * Crypt: the crypt list structure,  consists of the nickname, and the
 * encryption key 
 */
typedef struct CryptStru {
    struct CryptStru *next;
    char *nick;
    char *key;
}         Crypt;

/* crypt_list: the list of nicknames and encryption keys */
static Crypt *crypt_list = null(Crypt *);

/*
 * add_to_crypt: adds the nickname and key pair to the crypt_list.  If the
 * nickname is already in the list, then the key is changed the the supplied
 * key. 
 */
static void add_to_crypt(nick, key)
char *nick;
char *key;
{
    Crypt *new;

    if (new = (Crypt *) remove_from_list(&crypt_list, nick)){
	new_free(&(new->nick));
	new_free(&(new->key));
	new_free(&new);
    }
    new = (Crypt *) new_malloc(sizeof(Crypt));
    new->nick = null(char *);
    new->key = null(char *);
    malloc_strcpy(&(new->nick), nick);
    malloc_strcpy(&(new->key), key);
    upper(new->nick);
    add_to_list(&crypt_list, new, sizeof(Crypt));
}

/*
 * remove_crypt: removes the given nickname from the crypt_list, returning 0
 * if successful, and 1 if not (because the nickname wasn't in the list) 
 */
static int remove_crypt(nick)
char *nick;
{
    Crypt *tmp;

    if (tmp = (Crypt *) list_lookup(&crypt_list, nick, !USE_WILDCARDS, REMOVE_FROM_LIST)) {
	new_free(&(tmp->nick));
	new_free(&(tmp->key));
	new_free(&tmp);
	return (0);
    }
    return (1);
}

/*
 * is_crypted: looks up nick in the crypt_list and returns the encryption key
 * if found in the list.  If not found in the crypt_list, null is returned. 
 */
char *is_crypted(nick)
char *nick;
{
    Crypt *tmp;

    if (crypt_list) {
	if (tmp = (Crypt *) list_lookup(&crypt_list, nick, !USE_WILDCARDS, !REMOVE_FROM_LIST))
	    return (tmp->key);
    }
    return (null(char *));
}

/*
 * encrypt_cmd: the ENCRYPT command.  Adds the given nickname and key to the
 * encrypt list, or removes it, or list the list, you know. 
 */
void encrypt_cmd(command, args)
char *command,
    *args;
{
    char *nick,
        *key;

    if (nick = next_arg(args, &args)) {
	if (key = next_arg(args, &args)) {
	    add_to_crypt(nick, key);
	    put_it("*** %s added to the crypt with key %s", nick, key);
	} else {
	    if (remove_crypt(nick))
		put_it("*** No such nickname in the crypt: %s", nick);
	    else
		put_it("*** %s removed from the crypt", nick);
	}
    } else {
	if (crypt_list) {
	    Crypt *tmp;

	    put_it("*** The crypt:");
	    for (tmp = crypt_list; tmp; tmp = tmp->next)
		put_it("%s with key %s", tmp->nick, tmp->key);
	} else
	    put_it("*** The crypt is empty");
    }
}

/*
 * nice_it: makes the given string sendable over the current irc servers.  If
 * the flag is true, all occurrence of characters that normally would be
 * interpreted by the irc server are altered so they can be sent.  When the
 * flag is false, this conversion is reversed.  The conversion basically puts
 * a . (period) before converted characters.  Any period in the text is
 * changed to two periods, any control character in the text is converted to
 * a period followed by a nice readable form of the character.  ^J and ^M are
 * thus converted to a period followed by a J or an M, respectively 
 */
static char *nice_it(str, flag)
char *str,
     flag;
{
    static char ret[BUFFER_SIZE + 1];
    char *ptr;

    if (flag) {
	strcpy(ret, CRYPT_HEADER);
	for (ptr = ret + CRYPT_HEADER_LEN; *str; str++) {
	    switch (*str) {
		case '.':
		    *(ptr++) = '.';
		    *(ptr++) = '.';
		    break;
		case '\n':
		    *(ptr++) = '.';
		    *(ptr++) = 'J';
		    break;
		case '\r':
		    *(ptr++) = '.';
		    *(ptr++) = 'M';
		    break;
		default:
		    *(ptr++) = *str;
		    break;
	    }
	}
	*ptr = null(char);
    } else {
	*ret = null(char);
	str += CRYPT_HEADER_LEN;
	while (ptr = index(str, '.')) {
	    *(ptr++) = null(char);
	    strmcat(ret, str, BUFFER_SIZE);
	    switch (*(ptr++)) {
		case 'J':
		    strmcat(ret, "\n", BUFFER_SIZE);
		    break;
		case 'M':
		    strmcat(ret, "\r", BUFFER_SIZE);
		    break;
		case '.':
		    strmcat(ret, ".", BUFFER_SIZE);
		    break;
	    }
	    str = ptr;
	}
	strmcat(ret, str, BUFFER_SIZE);
    }
    return (ret);
}

/*
 * crypt_msg: Executes the encryption program on the given string with the
 * given key.  If flag is true, the string is encrypted and the returned
 * string is ready to be sent over irc.  If flag is false, the string is
 * decrypted and the returned string should be readable 
 */
char *crypt_msg(str, key, flag)
char *str,
    *key,
     flag;
{
    int in[2],
        out[2],
        c;
    static char ret[BUFFER_SIZE + 1];
    char *ptr,
        *encrypt_program;

    in[0] = in[1] = -1;
    out[0] = out[1] = -1;
    if ((encrypt_program = get_string_var(ENCRYPT_PROGRAM_VAR)) == null(char *)) {
	put_it("*** You must first set the ENCRYPT_PROGRAM variable!");
	return (null(char *));
    }
    if (access(encrypt_program, X_OK)) {
	put_it("*** Unable to execute encryption program: %s", encrypt_program);
	return (null(char *));
    }
    if (!flag)
	ptr = nice_it(str, flag);
    else
	ptr = str;
    if (pipe(in) || pipe(out)) {
	put_it("*** Unable to start encryption process: %s", sys_errlist[errno]);
	if (in[0] != -1) {
	    close(in[0]);
	    close(in[1]);
	}
	if (out[0] != -1) {
	    close(out[0]);
	    close(out[1]);
	}
    }
    switch (fork()) {
	case -1:
	    put_it("*** Unable to start encryption process: %s", sys_errlist[errno]);
	    return (null(char *));
	case 0:
	    signal(SIGINT, SIG_IGN);
	    dup2(out[1], 1);
	    dup2(in[0], 0);
	    close(out[0]);
	    close(in[1]);
	    execl(encrypt_program, encrypt_program, key, NULL);
	    exit(0);
	default:
	    close(out[1]);
	    close(in[0]);
	    write(in[1], ptr, strlen(ptr));
	    close(in[1]);
	    c = read(out[0], ret, BUFFER_SIZE);
	    ret[c] = null(char);
	    close(out[0]);
	    break;
    }
    if (flag)
	ptr = nice_it(ret, flag);
    else
	ptr = ret;
    return (ptr);
}
