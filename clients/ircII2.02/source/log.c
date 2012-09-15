/*
 * log.c: handles the irc session logging functions 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "irc.h"
#include "log.h"
#include "vars.h"

static FILE *fp = null(FILE *);

/* logger: if flag is 0, logging is turned off, else it's turned on */
void logger(flag)
int flag;
{
    long t;
    char *logfile;


    t = time(0);
    if (flag) {
	if (fp)
	    put_it("*** Logging is already on");
	else {
#ifdef DAEMON_UID
	    if (getuid() == DAEMON_UID) {
		put_it("*** You are not permitted to use LOG");
		set_int_var(LOG_VAR, 0);
		return;
	    }
#endif DAEMON_UID
	    if ((logfile = get_string_var(LOGFILE_VAR)) == null(char *)) {
		put_it("*** You must set the LOGFILE variable first!");
		set_int_var(LOG_VAR, 0);
		return;
	    }
	    put_it("*** Starting logfile %s", logfile);
	    if (fp = fopen(logfile, "a")) {
		fprintf(fp, "IRC log started %.16s\n", ctime(&t));
		fflush(fp);
	    } else {
		put_it("*** Couldn't open logfile %s: %s", logfile, sys_errlist[errno]);
		set_int_var(LOG_VAR, 0);
	    }
	}
    } else {
	if (fp) {
	    put_it("*** Logfile ended");
	    fprintf(fp, "IRC Log ended *** %.16s\n", ctime(&t));
	    fflush(fp);
	    fclose(fp);
	    fp = null(FILE *);
	}
    }
}

/*
 * set_log_file: sets the log file name.  If logging is on already, this
 * closes the last log file and reopens it with the new name.  This is called
 * automatically when you SET LOGFILE. 
 */
void set_log_file(filename)
char *filename;
{
    if (filename) {
	if (strcmp(filename, get_string_var(LOGFILE_VAR)))
	    set_string_var(LOGFILE_VAR, expand_twiddle(filename));
	else
	    set_string_var(LOGFILE_VAR, expand_twiddle(get_string_var(LOGFILE_VAR)));
	if (fp) {
	    logger(0);
	    logger(1);
	}
    }
}

/*
 * add_to_log: add the given line to the log file.  If no log file is open
 * this function does nothing. 
 */
void add_to_log(line)
char *line;
{
    if (fp) {
	fprintf(fp, "%s\n", line);
	fflush(fp);
    }
}
