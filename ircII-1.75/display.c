/*
 * display.c: Handles the Main Window stuff for irc.  This includes proper
 * scrolling, saving of screen memory, refreshing, clearing, etc. 
 *
 * Written by Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserverd 
 */
#include <stdio.h>
#include "irc.h"
#include "term.h"

/*
 * Display: the struct of screen memory... a circularly linked list.  Why? So
 * when we scroll, we can simply move the top_of_display pointer to the next
 * screen entry 
 */
typedef struct DisplayStru {
    char *line;
    struct DisplayStru *next;
}           Display;

/*
 * top_of_display: points to the first line to be displayed at the top of the
 * physical screen 
 */
static Display *top_of_display = null(Display *);

/*
 * display_ip: the display insertion point (ip)... the display line where the
 * next thing added will go.  
 */
static Display *display_ip = null(Display *);

/*
 * cursor_line: The actualy line on the physical screen of the cursor (or
 * where we should put it before doing any writing to the screen) 
 */
static int cursor_line = 0;

/*
 * display_size: the number of line of the physical screen, also the number
 * of entries in the display buffer 
 */
static int display_size;

char cursor_in_display = 0;

/*
 * erase_display: frees all display memory, sets the cursor to the top of the
 * screen, and the display insertion point to the top of the display.  Note,
 * this doesn't actually refresh the screen 
 */
void erase_display()
{
    int i;
    Display *tmp;

    for (tmp = top_of_display, i = 0; i < display_size; i++, tmp = tmp->next) {
	if (tmp->line)
	    free(tmp->line);
	tmp->line = null(char *);
    }
    cursor_line = 0;
    display_ip = top_of_display;
}

/*
 * resize_display: After determining that the screen has changed sizes, this
 * resizes all the internal stuff.  If the screen grew, this will add extra
 * empty display entries to the end of the display list.  If the screen
 * shrank, this will remove entries from the end of the display list.  By
 * doing this, we try to maintain as much of the display as possible. 
 */
void resize_display()
{
    int cnt,
        i,
        new_display_size;
    Display *tmp;

    new_display_size = LI - 2;
    cnt = new_display_size - display_size;
    if (cnt > 0) {
	Display *new;

	/*
	 * screen got bigger: got to last display entry and link in new
	 * entries 
	 */
	for (tmp = top_of_display, i = 0; i < (display_size - 1); i++, tmp = tmp->next);
	for (i = 0; i < cnt; i++) {
	    new = (Display *) new_malloc(sizeof(Display));
	    new->line = null(char *);
	    new->next = tmp->next;
	    tmp->next = new;
	}
    } else if (cnt < 0) {
	Display *old;

	/*
	 * screen shrank: find last display entry we want to keep, and remove
	 * all after that point 
	 */
	for (tmp = top_of_display, i = 0; i < (display_size + cnt - 1); i++, tmp = tmp->next);
	cnt *= -1;
	for (i = 0; i < cnt; i++) {
	    old = tmp->next;
	    tmp->next = tmp->next->next;
	    if (old->line)
		free(old->line);
	    free(old);
	}
    }
    display_size = new_display_size;
    /* find a new insertion_point, use the first empty line */
    for (tmp = top_of_display, i = 0; i < (display_size - 1); i++, tmp = tmp->next) {
	if ((tmp->line == null(char *)) || (*tmp->line == null(char)))
	    break;
    }
    display_ip = tmp;
    cursor_line = i;
    malloc_strcpy(&(display_ip->line), "");
}

/*
 * redraw_display: Refreshes the screen from the display list.  In order to
 * speed things up, we only draw lines that have something (non-null), and
 * end when we get to the first null line.  Note that this ends on
 * null-lines, not lines that point to strings of length 0.  This distinction
 * is important, because the display_ip should always be a blank line.  In
 * non-scrolling mode, this line could be anywhere on the screen, yet we
 * still want the refresh to draw stuff below the display_ip.  It is for this
 * reason that the display_ip should always point to a string of length 0
 * ("") whenever there is something below it on the screen.  got it?  Fine. 
 */
void redraw_display()
{
    static int li = 0,
        co = 0;
    int i;
    Display *tmp;

    cursor_in_display = 1;
    if ((li != LI) || (co != CO)) {
	resize_display();
	li = LI;
	co = CO;
    }
    term_move_cursor(0, 0);
    for (tmp = top_of_display, i = 0; i < display_size; i++, tmp = tmp->next) {
	if (tmp->line) {
	    puts(tmp->line);
	    term_clear_to_eol(strlen(tmp->line), i);
	} else
	    term_clear_to_eol(0, i);
    }
}

/*
 * add_to_display: adds the given string to the display.  No kidding.  This
 * routine handles the whole ball of wax.  It keeps track of what's on the
 * screen, does the scrolling, everything... well, not quite everything...
 * this routine should handle the word wrap too.  Tomorrow I add that  
 */
void add_to_display(str)
char *str;
{
    char *ptr;
    int pos = 0, back = 0;

    if (!cursor_in_display)
	term_move_cursor(0, cursor_line);
    for (ptr = str; *ptr; ptr++,pos++) {
	int foo;
	
	if (*ptr < 32){
	    switch (*ptr){
		case '\007':
		    break;
		case '\011':
		    foo = (8 - (pos % 8));
		    pos += foo;
		    back += foo+1;
		    break;
		default:
		    *ptr += 64;
		    break;
	    }
	}
    }
    if (pos > CO)
	str[CO-back] = null(char);
    fwrite(str, strlen(str), 1, stdout);
    malloc_strcpy(&(display_ip->line), str);
    display_ip = display_ip->next;
    malloc_strcpy(&(display_ip->line), "");
    if (++cursor_line == display_size) {
	if (get_int_var(SCROLL_VAR)) {
	    top_of_display = top_of_display->next;
	    cursor_line--;
	    if (term_scroll(0, cursor_line)) {
		/* this method of sim-u-scroll seems to work fairly well */
		term_move_cursor(0, cursor_line + 1);
		term_clear_to_eol(0, cursor_line + 1);
		term_newline();
		term_newline();
		make_status();
		update_input(UPDATE_ALL);
		term_move_cursor(0, cursor_line);
	    } else
		term_cr();	/* term_scroll leaves cursor in the right
				 * place for the term_cr to work */
	} else {
	    cursor_line = 0;
	    term_move_cursor(0, 0);
	}
    } else
	term_newline();
    term_clear_to_eol(0, cursor_line);
    cursor_in_display = 1;
}

/* init_display: gets the display rolling by creating the first display line */
void init_display()
{
    if   (top_of_display == null(Display *)) {
	     top_of_display = (Display *) new_malloc(sizeof(Display));
	top_of_display->line = null(char *);
	top_of_display->next = top_of_display;
	display_ip = top_of_display;
	display_size = 1;
    }
}
