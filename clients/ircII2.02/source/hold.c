/*
 * hold.c: handles buffering of display output. 
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "irc.h"
#include "window.h"
#include "vars.h"

/* add_to_hold_list: adds str to the hold list queue */
void add_to_hold_list(window, str)
Window *window;
char *str;
{
    Hold *new;
    unsigned int max;

    new = (Hold *) new_malloc(sizeof(Hold));
    new->str = null(char *);
    malloc_strcpy(&(new->str), str);
    window->held_lines++;
    if (max = get_int_var(HOLD_MODE_MAX_VAR)) {
	if (window->held_lines > max)
	    hold_mode(window, OFF, 1);
    }
    new->next = window->hold_head;
    new->prev = null(Hold *);
    if (window->hold_tail == null(Hold *))
	window->hold_tail = new;
    if (window->hold_head)
	window->hold_head->prev = new;
    window->hold_head = new;
}

/* remove_from_hold_list: pops the next element off the hold list queue. */
void remove_from_hold_list(window)
Window *window;
{
    Hold *crap;

    if (window->hold_tail) {
	window->held_lines--;
	new_free(&window->hold_tail->str);
	crap = window->hold_tail;
	window->hold_tail = window->hold_tail->prev;
	if (window->hold_tail == null(Hold *))
	    window->hold_head = window->hold_tail;
	else
	    window->hold_tail->next = null(Hold *);
	new_free(&crap);
    }
}

/*
 * hold_mode: sets the "hold mode".  Really.  If the update flag is true,
 * this will also update the status line, if needed, to display the hold mode
 * state.  If update is false, only the internal flag is set.  
 */
void hold_mode(window, flag, update)
Window *window;
int flag,
     update;
{
    if (window == null(Window *))
	window = current_window;
    if (flag == TOGGLE) {
	if (window->held == OFF)
	    window->held = ON;
	else
	    window->held = OFF;
    } else
	window->held = flag;
    if (update) {
	if (window->held != window->last_held) {
	    window->last_held = window->held;
	    update_window_status(window,0);
	}
    } else
	window->last_held = -1;
}

/*
 * hold_output: returns the state of the window->held, which is set in
 * the hold_mode() routine. 
 */
int hold_output(window)
Window *window;
{
    if (window)
	return (window->held == ON);
    else
	return (current_window->held == ON);
}

/*
 * hold_queue: returns the string value of the next element on the hold
 * quere.  This does not change the hold queue 
 */
char *hold_queue(window)
Window *window;
{
    if (window->hold_tail)
	return (window->hold_tail->str);
    else
	return (null(char *));
}


