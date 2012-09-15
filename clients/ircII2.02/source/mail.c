/*
 * mail.c: Ok, so I gave in.  I added mail checking.  So sue me. 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */

#include <sys/file.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include "irc.h"
#include "config.h"
#include "mail.h"
#include "hook.h"
#include "vars.h"

/*
 * count_files: counts all the visible files in the specified directory and
 * returns that number as the function value 
 */
static unsigned int count_files(dir_name)
char *dir_name;
{
    DIR *dir;
    struct direct *dirbuf;
    unsigned int cnt;
    
    if ((dir = opendir(dir_name)) == null(DIR *))
	return (0);
    cnt = 0;
    while ((dirbuf = readdir(dir)) != null(struct direct *)) {
	if (*(dirbuf->d_name) != '.')
	    cnt++;
    }
    closedir(dir);
    return (cnt);
}

/*
 * check_mail: This here thing counts up the number of pieces of mail and
 * returns it as static string.  If there are no mail messages, null is
 * returned. 
 */
char *check_mail()
{
    static time_t old_stat = 0L;
    static unsigned int cnt = 0;
    static char ret_str[8];
    struct stat stat_buf;
    unsigned int new_cnt;
    char tmp[8];
#ifdef UNIX_MAIL
    int des;
#endif UNIX_MAIL

    if (!get_int_var(MAIL_VAR)) {
	old_stat = 0L;
	cnt = 0;
	return (null(char *));
    }
#ifdef UNIX_MAIL
    sprintf(buffer, "%s/%s", UNIX_MAIL, username);

    if (stat(buffer, &stat_buf) == -1)
	return (null(char *));
    if (stat_buf.st_mtime > old_stat) {
	if (des = open(buffer, O_RDONLY, 0)) {
	    new_cnt = 0;
	    while (dgets(buffer, BUFFER_SIZE, des)) {
		if (strnicmp(MAIL_DELIMITER, buffer, sizeof(MAIL_DELIMITER) - 1) == 0)
		    new_cnt++;
	    }
	    new_close(des);
	}
#else
#ifdef AMS_MAIL
    sprintf(buffer, "%s/%s", my_path, AMS_MAIL);
    if (stat(buffer, &stat_buf) == -1)
	return (null(char *));
    if (stat_buf.st_mtime > old_stat) {
	old_stat = stat_buf.st_mtime;
	new_cnt = count_files(buffer);
#endif AMS_MAIL
#endif UNIX_MAIL
	/* yeeeeack */
	sprintf(tmp, "%d", new_cnt - cnt);
	sprintf(buffer, "%d", new_cnt);
	if (new_cnt > cnt) {
	    if (do_hook(MAIL_REAL_LIST, tmp, buffer))
		echo(null(char *), "*** You have new mail.");
	}
	cnt = new_cnt;
    }
    if (cnt && (cnt < 65536)) {
	sprintf(ret_str, "%d", cnt);
	return (ret_str);
    } else
	return (null(char *));
}
