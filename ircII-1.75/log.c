/*
 * log.c: handles the irc session loggin functions 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <stdio.h>
#include "irc.h"

static FILE *fp = null(FILE *);

/* logger: if flag is 0, logging is turned of, else it's turned on */
void logger(flag)
int flag;
{
    long t;

    t = time(0);
    if (flag) {
	if (fp)
	    put_it("*** Logging is already on");
	else {
	    put_it("*** Log file %s started",
		   get_string_var(LOGFILE_VAR));
	    if (fp = fopen(get_string_var(LOGFILE_VAR), "a")) {
		fprintf(fp, "IRC Log started %.16s\n", ctime(&t));
		fflush(fp);
	    } else{
		put_it("*** Couldn't open log file %s: %s",
		       get_string_var(LOGFILE_VAR),sys_errlist[errno]);
		set_var_value(LOG_VAR, "OFF");
	    }
	}
    } else {
	if (fp) {
	    put_it("*** Log file ended");
	    fprintf(fp, "IRC Log ended %.16s\n", ctime(&t));
	    fflush(fp);
	    close(fp);
	    fp = null(FILE *);
	} 
    }
}

void set_log_file(filename)
char *filename;
{
    if (strcmp(filename, get_string_var(LOGFILE_VAR)))
	set_string_var(LOGFILE_VAR, expand_twiddle(filename));
    else
	set_string_var(LOGFILE_VAR, expand_twiddle(get_string_var(LOGFILE_VAR)));
    if (fp) {
	logger(0);
	logger(1);
    }
}

void add_to_log(line)
char *line;
{
    if (fp) {
	fprintf(fp, "%s\n", line);
	fflush(fp);
    }
}
