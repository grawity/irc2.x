/*
 * output.c: handles a variety of tasks dealing with the output from the irc
 * program 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */

#include <sys/ioctl.h>
#include <sys/file.h>
#include <stdio.h>
#include "irc.h"
#include "term.h"

/*
 * make_status: changes the information in the status line to the current
 * stuff.  Probably should make this updating more effecient.  Next time 
 */
void make_status()
{
    if   (!dumb) {
	     term_move_cursor(0, LI - 2);
	term_standout_on();
	if (query_nick)
	    sprintf(buffer,
		 " *** IRC II %s: %chelp for info.  %s%c  Query %s %s%s ***",
		    irc_version, get_int_var(CMDCHAR_VAR), nickname,
		    operator ? '*' : ' ', query_nick,
		    (away_message == null(char *)) ? "" : "(Away) ",
		    (get_int_var(INSERT_MODE_VAR)) ? "(I)" : "(O)");
	else
	    sprintf(buffer,
	       " *** IRC II %s: %chelp for info.  %s%c  Channel %d %s%s ***",
		    irc_version, get_int_var(CMDCHAR_VAR), nickname,
		    operator ? '*' : ' ', current_channel,
		    (away_message == null(char *)) ? "" : "(Away) ",
		    (get_int_var(INSERT_MODE_VAR)) ? "(I)" : "(O)");
#ifdef FULL_STATUS_LINE
	{
	    int len,
	        i;

	    len = strlen(buffer);
	    for (i = len; i < CO; i++)
		strcat(buffer, " ");
	}
#endif FULL_STATUS_LINE
	if (strlen(buffer) > CO)
	    buffer[CO] = null(char);
	fwrite(buffer, strlen(buffer), 1, stdout);
	term_standout_off();
	term_clear_to_eol(strlen(buffer),LI-2);
	update_input(UPDATE_JUST_CURSOR);
    }
}

void refresh_screen()
{
         term_resize();
    term_clear_screen();
    redraw_display();
    make_status();
    update_input(UPDATE_ALL);
    fflush(stdout);
}

/* init_windows:  */
void init_screen()
{
         term_init();
    term_resize();
    init_display();
    init_input();
    term_clear_screen();
    redraw_display();
    make_status();
    update_input(UPDATE_ALL);
    fflush(stdout);
}

/*
 * put_it: the irc display routine.  Use this routine to display anything to
 * the main irc window.  It handles scrolling, line-wrap, word-wrap, etc. 
 * Things NOT to do:  Dont send any text that contains \n, very
 * unpredictable.  Tabs will also screw things up.  The calling routing is
 * responsible for not overwriting the 1K buffer allocated 
 */
void put_it(format, arg1, arg2, arg3, arg4, arg5, arg6)
char *format;
int arg1,
    arg2,
    arg3,
    arg4,
    arg5,
    arg6;
{
    char buffer[1024],		/* make this buffer *much* bigger than needed */
        *ptr,
         c = 0;
    int len,
        pos;

    if (get_int_var(DISPLAY_VAR)) {
	sprintf(buffer, format, arg1, arg2, arg3, arg4, arg5, arg6);
	add_to_lastlog(buffer);
	add_to_log(buffer);
	if (dumb)
	    puts(buffer);
	else {
	    ptr = buffer;
	    while (1) {
		len = strlen(ptr);
		if (len > CO) {
		    pos = CO;
		    while (ptr[pos] != ' ') {
			if (--pos == 10) {
			    pos = 0;
			    break;
			}
		    }
		    if (pos) {
			ptr[pos] = null(char);
			c = ptr[++pos];
		    } else {
			pos = CO;
			c = ptr[pos];
			ptr[pos] = null(char);
		    }
		}
		add_to_display(ptr);
		if (c) {
		    ptr[pos] = c;
		    c = 0;
		    ptr[pos - 1] = '+';
		    ptr = &(ptr[pos - 1]);
		} else
		    break;
	    }
	}
    }
    fflush(stdout);
}

/* send_to_server: sends the given info the the server */
void send_to_server(format, arg1, arg2, arg3, arg4, arg5)
char *format;
int arg1,
    arg2,
    arg3,
    arg4,
    arg5;
{
    char buffer[1024];		/* make this buffer *much* bigger than needed */

    if (server_des != -1) {
	sprintf(buffer, format, arg1, arg2, arg3, arg4, arg5);
	write(server_des, buffer, strlen(buffer));
	write(server_des, "\n", 1);
    }
}

