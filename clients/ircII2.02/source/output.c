/*
 * output.c: handles a variety of tasks dealing with the output from the irc
 * program 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include "irc.h"
#include "output.h"
#include "vars.h"
#include "input.h"
#include "term.h"
#include "lastlog.h"
#include "window.h"

/*
 * refresh_screen: Whenever the REFRESH_SCREEN function is activated, this
 * swoops into effect 
 */
void refresh_screen()
{
    if   (term_resize())
	     recalculate_windows();
    else
	redraw_all_windows();
    update_input(UPDATE_ALL);
}

/* init_windows:  */
void init_screen()
{
         term_init();
    term_resize();
    new_window();
    init_input();
    term_move_cursor(0, 0);
}

/* put_file: uses put_it() to display the contents of a file to the display */
void put_file(filename)
char *filename;
{
    FILE *fp;
    char line[256];		/* too big?  too small?  who cares? */
    int len;

    if ((fp = fopen(filename, "r")) != null(FILE *)) {
	while (fgets(line, 256, fp)) {
	    len = strlen(line);
	    if (*(line + len - 1) == '\n')
		*(line + len - 1) = null(char);
	    put_it("%s", line);
	}
	fclose(fp);
    }
}

/*
 * put_it: the irc display routine.  Use this routine to display anything to
 * the main irc window.  It handles sending text to the display or stdout as
 * needed, add stuff to the lastlog and log file, etc.  Things NOT to do:
 * Dont send any text that contains \n, very unpredictable.  Tabs will also
 * screw things up.  The calling routing is responsible for not overwriting
 * the 1K buffer allocated.  
 */
void put_it(format, arg1, arg2, arg3, arg4, arg5,
	         arg6, arg7, arg8, arg9, arg10)
char *format;
int arg1,
    arg2,
    arg3,
    arg4,
    arg5,
    arg6,
    arg7,
    arg8,
    arg9,
    arg10;
{
    char buffer[1025];		/* make this buffer *much* bigger than needed */

    if (get_int_var(DISPLAY_VAR)) {
	sprintf(buffer, format, arg1, arg2, arg3, arg4, arg5,
		arg6, arg7, arg8, arg9, arg10);
	add_to_log(buffer);
	add_to_screen(buffer);
    }
    fflush(stdout);
}
