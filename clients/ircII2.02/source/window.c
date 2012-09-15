/*
 * window.c: Handles the Main Window stuff for irc.  This includes proper
 * scrolling, saving of screen memory, refreshing, clearing, etc. 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include <stdio.h>
#include "irc.h"
#include "window.h"
#include "vars.h"
#include "term.h"
#include "names.h"
#include "ircaux.h"

#define BOLD_OFF '\017'
#define BOLD_TOGGLE '\002'

Window *current_window;		/* The "input focus" window, you know? */
static Window *window_list = null(Window *);	/* List of all windows */
static Window *cursor_window = null(Window *);	/* Last window to have
						 * something written to it */
static Window *to_window = null(Window *);	/* Where the next message
						 * should be displayed (if
						 * not current_window */
static int visible_windows = 0;	/* total number of windows */
static char *who_from = null(char *);	/* nick of person who's message is
					 * being displayed */
static int who_level = LOG_CRAP;/* Log level of message being displayed */
static char *save_from = null(char *);
static int save_level = LOG_CRAP;

/* display_highlight: turns off and on the display highlight.  */
char display_highlight(flag)
int flag;
{
    static char highlight = OFF;

    fflush(stdout);
    if (flag == highlight)
	return (flag);
    switch (flag) {
	case ON:
	    highlight = ON;
	    if (get_int_var(INVERSE_VIDEO_VAR))
		term_standout_on();
	    return (OFF);
	case OFF:
	    highlight = OFF;
	    if (get_int_var(INVERSE_VIDEO_VAR))
		term_standout_off();
	    return (ON);
	case TOGGLE:
	    if (highlight == ON) {
		highlight = OFF;
		if (get_int_var(INVERSE_VIDEO_VAR))
		    term_standout_off();
		return (ON);
	    } else {
		highlight = ON;
		if (get_int_var(INVERSE_VIDEO_VAR))
		    term_standout_on();
		return (OFF);
	    }
    }
}

/*
 * set_scroll_lines: called by /SET SCROLL_LINES to check the scroll lines
 * value 
 */
void set_scroll_lines(size)
int size;
{
    if (size == 0) {
	set_var_value(SCROLL_VAR, var_settings[0], 0);
	if (current_window)
	    current_window->scroll = 0;
    } else if (size > current_window->display_size) {
	put_it("*** Maximum lines that may be scrolled is %d", current_window->display_size);
	set_int_var(SCROLL_LINES_VAR, current_window->display_size);
    }
}

/*
 * set_scroll: called by /SET SCROLL to make sure the SCROLL_LINES variable
 * is set correctly 
 */
void set_scroll(value)
int value;
{
    if (value && (get_int_var(SCROLL_LINES_VAR) == 0)) {
	put_it("You must set SCROLL_LINES to a positive value first!");
	if (current_window)
	    current_window->scroll = 0;
    } else {
	if (current_window)
	    current_window->scroll = value;
    }
}

/*
 * reset_line_cnt: called by /SET HOLD_MODE to reset the line counter so we
 * always get a held screen after the proper number of lines 
 */
void reset_line_cnt(value)
int value;
{
    current_window->hold_mode = value;
    current_window->hold_on_next_rite = 0;
    current_window->line_cnt = 0;
}

/*
 * set_continued_line: checks the value of CONTINUED_LINE for validity,
 * altering it if its no good 
 */
void set_continued_line(value)
char *value;
{
    if (value && (strlen(value) > (CO / 4)))
	value[CO / 4] = null(char);
}

/*
 * scroll_window: Given a pointer to a window, this determines if that window
 * should be scrolled, or the cursor moved to the top of the screen again, or
 * if it should be left alone. 
 */
static void scroll_window(window)
Window *window;
{
    if (window->cursor == window->display_size) {
	if (window->scroll) {
	    int scroll,
	        i;

	    if ((scroll = get_int_var(SCROLL_LINES_VAR)) <= 0)
		scroll = 1;

	    for (i = 0; i < scroll; i++)
		window->top_of_display = window->top_of_display->next;
	    if (term_scroll(window->top, window->top + window->cursor - 1, scroll)) {
#ifdef old
		int i;

		/*
		 * this method of sim-u-scroll seems to work fairly well 
		 */
		term_move_cursor(0, LI - 2);
		term_clear_to_eol(0, LI - 2);
		term_cr();
		term_newline();
		term_clear_to_eol(0, LI - 1);
		for (i = 0; i < scroll; i++) {
		    term_cr();
		    term_newline();
		}
		refresh_status();
		update_input(UPDATE_ALL);
#else
		redraw_window(window, 1);
#endif old
	    }
	    window->cursor -= scroll;
	    term_move_cursor(0, window->cursor + window->top);
	} else {
	    window->cursor = 0;
	    term_move_cursor(0, window->top);
	}
    } else {
	term_cr();
	term_newline();
    }
    term_clear_to_eol(0, window->cursor + window->top);
    term_flush();
}

/*
 * rite: this routine displays a line to the screen adding bold facing when
 * specified by ^Bs, etc.  It also does handles scrolling and paging, if
 * SCROLL is on, and HOLD_MODE is on, etc.  This routine assumes that str
 * already fits on one screen line.  If show is true, str is displayed
 * regardless of the hold mode state.  If redraw is true, it is assumed we a
 * redrawing the screen from the display_ip list, and we should not add what
 * we are displaying back to the display_ip list again. 
 *
 * Note that rite sets display_highlight() to what it was at then end of the
 * last rite().  Also, before returning, it sets display_highlight() to OFF.
 * This way, between susequent rites(), you can be assured that the state of
 * bold face will remain the same and the it won't interfere with anything
 * else (i.e. status line, input line). 
 */
int rite(window, str, show, redraw)
Window *window;
char *str;
char show,
     redraw;
{
    static int high = OFF;
    int written = 0,
        len;
    char *ptr,
         c;

    if (window->hold_mode && window->hold_on_next_rite && !redraw) {
	/* stops output from going to the window */
	window->hold_on_next_rite = 0;
	hold_mode(window, ON, 1);
	if (show)
	    return (1);
    }
    if (!show && (hold_output(window) || hold_queue(window)))
	/* sends window output to the hold list for that window */
	add_to_hold_list(window, str);
    else {
	if (!redraw) {
	    /*
	     * This isn't a screen refresh, so add the line to the display
	     * list for the window 
	     */
	    malloc_strcpy(&(window->display_ip->line), str);
	    window->display_ip = window->display_ip->next;
	    malloc_strcpy(&(window->display_ip->line), empty_string);
	}
	/* make sure the cursor is in the appropriate window */
	if (cursor_window != window) {
	    cursor_window = window;
	    if (redraw)
		term_move_cursor(0, window->top);
	    else
		term_move_cursor(0, window->cursor + window->top);
	}
	ptr = str;
	display_highlight(high);
	/* do processing on the string, handle inverse and bells */
	while (*ptr) {
	    switch (*ptr) {
		case BOLD_TOGGLE:
		    /*
		     * what do we do?  Display everything so far up to the
		     * inverse video, the toggle inverse 
		     */
		    c = *ptr;
		    *ptr = null(char);
		    len = strlen(str);
		    written += len;
		    if (written > CO)
			len = len - (written - CO);
		    fwrite(str, len, 1, stdout);
		    display_highlight(TOGGLE);
		    *ptr = c;
		    str = ++ptr;
		    break;
		case BOLD_OFF:
		    /*
		     * same as above, except after we display everything so
		     * far, we turn inverse off 
		     */
		    c = *ptr;
		    *ptr = null(char);
		    len = strlen(str);
		    written += len;
		    if (written > CO)
			len = len - (written - CO);
		    fwrite(str, len, 1, stdout);
		    display_highlight(OFF);
		    *ptr = c;
		    str = ++ptr;
		    break;
		case '\007':
		    /*
		     * same as above, except after we display everything so
		     * far, we beep the terminal 
		     */
		    c = *ptr;
		    *ptr = null(char);
		    len = strlen(str);
		    written += len;
		    if (written > CO)
			len = len - (written - CO);
		    fwrite(str, len, 1, stdout);
		    term_beep();
		    *ptr = c;
		    str = ++ptr;
		    break;
		default:
		    ptr++;
		    break;
	    }
	}
	len = strlen(str);
	written += len;
	if (written > CO)
	    len = len - (written - CO);
	fwrite(str, len, 1, stdout);
	term_clear_to_eol();
	if (!redraw) {
	    window->cursor++;
	    window->line_cnt++;
	    scroll_window(window);
	    if (window->scroll) {
		if (window->line_cnt >= (window->display_size - 1)) {
		    window->hold_on_next_rite = 1;
		    window->line_cnt = 0;
		}
	    } else if (window->cursor == (window->display_size - 1))
		window->hold_on_next_rite = 1;
	} else {
	    term_cr();
	    term_newline();
	}
	high = display_highlight(OFF);
	term_flush();
	return (0);
    }
}

/*
 * cursor_not_in_display: This forces the cursor out of the display by
 * setting the cursor window to null.  This doesn't actually change the
 * physical position of the cursor, but it will force rite() to reset the
 * cursor upon its next call 
 */
void cursor_not_in_display()
{
         cursor_window = null(Window *);
}

/*
 * cursor_in_display: returns true if the cursor is in one of the windows
 * (cursor_window is not null), false otherwise 
 */
int cursor_in_display()
{
    if  (cursor_window)
	    return (1);
    else
	return (0);
}

/*
 * free_display: This frees all memory for the display list for a given
 * window.  It resets all of the structures related to the display list
 * appropriately as well 
 */
void free_display(window)
Window *window;
{
    Display *tmp,
           *next;
    int i;

    if (window == null(Window *))
	window = current_window;
    for (tmp = window->top_of_display, i = 0; i < window->display_size; i++, tmp = next) {
	next = tmp->next;
	new_free(&(tmp->line));
	new_free(&tmp);
    }
    window->top_of_display = null(Display *);
    window->display_ip = null(Display *);
    window->display_size = 0;
}

/*
 * erase_display: This effectively causes all members of the display list for
 * a window to be set to empty strings, thus "clearing" a window.  It sets
 * the cursor to the top of the window, and the display insertion point to
 * the top of the display. Note, this doesn't actually refresh the screen,
 * just cleans out the display list 
 */
void erase_display(window)
Window *window;
{
    int i;
    Display *tmp;

    if (dumb)
	return;
    if (window == null(Window *))
	window = current_window;
    for (tmp = window->top_of_display, i = 0; i < window->display_size; i++, tmp = tmp->next)
	new_free(&(tmp->line));
    window->cursor = 0;
    window->line_cnt = 0;
    window->hold_on_next_rite = 0;
    window->display_ip = window->top_of_display;
}

/*
 * resize_display: After determining that the screen has changed sizes, this
 * resizes all the internal stuff.  If the screen grew, this will add extra
 * empty display entries to the end of the display list.  If the screen
 * shrank, this will remove entries from the end of the display list.  By
 * doing this, we try to maintain as much of the display as possible. 
 */
void resize_display(window, size)
Window *window;
int size;
{
    int cnt,
        i;
    Display *tmp;

    if (dumb)
	return;
    if (window->top_of_display == null(Display *)) {
	window->top_of_display = (Display *) new_malloc(sizeof(Display));
	window->top_of_display->line = null(char *);
	window->top_of_display->next = window->top_of_display;
	window->display_ip = window->top_of_display;
	window->display_size = 1;
    }
    cnt = size - window->display_size;
    if (cnt > 0) {
	Display *new;

	/*
	 * screen got bigger: got to last display entry and link in new
	 * entries 
	 */
	for (tmp = window->top_of_display, i = 0; i < (window->display_size - 1); i++, tmp = tmp->next);
	for (i = 0; i < cnt; i++) {
	    new = (Display *) new_malloc(sizeof(Display));
	    new->line = null(char *);
	    new->next = tmp->next;
	    tmp->next = new;
	}
    } else if (cnt < 0) {
#ifdef new_resize
	Display *top,
	       *last,
	       *ptr,
	       *next;
	char reached_ip;
#else
	Display *ptr;
#endif

	/*
	 * screen shrank: find last display entry we want to keep, and remove
	 * all after that point 
	 */
#ifdef new_resize
	top = window->top_of_display;
	last = null(Display *);
	reached_ip = 0;
	for (tmp = window->top_of_display, i = 0; tmp; i++, tmp = next) {
	    next = tmp->next;
	    if ((tmp == window->display_ip) && (reached_ip == 0))
		reached_ip = 1;
	    if (i > size) {
		if (reached_ip) {
		    if (last == null(Display *))
			top = next;
		    else
			last->next = next;
		    new_free(&(tmp->line));
		    new_free(&tmp);
		    /* delete it */
		} else {
		    ptr = top->next;
		    new_free(&(top->line));
		    new_free(&top);
		    top = ptr;
		    last = tmp;
		}
	    } else
		last = tmp;
	}
	if (!reached_ip)
	    window->display_ip = last;
	window->top_of_display = top;
#else
	for (tmp = window->top_of_display, i = 0; i < (window->display_size + cnt - 1); i++, tmp = tmp->next);
	cnt *= -1;
	for (i = 0; i < cnt; i++) {
	    ptr = tmp->next;
	    tmp->next = tmp->next->next;
	    new_free(&(ptr->line));
	    new_free(&ptr);
	}
#endif new
    }
    /* find a new insertion_point, use the first empty line */
    for (tmp = window->top_of_display, i = 0; i < (size - 1); i++, tmp = tmp->next) {
	if ((tmp->line == null(char *)) || (*tmp->line == null(char)))
	    break;
    }
    window->display_size = size;
    window->display_ip = tmp;
    malloc_strcpy(&(tmp->line), empty_string);
    window->cursor = i;
}

void recalculate_windows()
{
    int base_size,
        size,
        top,
        extra;
    Window *tmp;

    if (dumb)
	return;
    base_size = ((LI - 1) / visible_windows) - 1;
    extra = (LI - 1) - ((base_size + 1) * visible_windows);
    top = 0;
    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (tmp->visible) {
	    if (extra) {
		extra--;
		size = base_size + 1;
	    } else
		size = base_size;
	    resize_display(tmp, size);
	    tmp->display_size = size;
	    tmp->top = top;
	    tmp->bottom = top + size;
	    top += size + 1;
	}
    }
    redraw_all_windows();
}


void clear_window(window)
Window *window;
{
    int i,
        cnt;

    if (dumb)
	return;
    if (window == null(Window *))
	window = current_window;
    erase_display(window);
    term_move_cursor(0, window->top);
    cnt = window->bottom - window->top;
    for (i = 0; i < cnt; i++) {
	term_clear_to_eol();
	term_newline();
    }
    term_flush();
}

void clear_all_windows()
{
    Window *tmp;

    for (tmp = window_list; tmp; tmp = tmp->next)
	clear_window(tmp);
}

void redraw_window(window, just_one)
Window *window;
char just_one;
{
    Display *tmp;
    int i;

    if (dumb)
	return;
    if (window == null(Window *))
	window = current_window;
    term_move_cursor(0, window->top);
    term_clear_to_eol();
    for (tmp = window->top_of_display, i = 0; i < window->display_size; i++, tmp = tmp->next) {
	if (tmp->line)
	    rite(window, tmp->line, 1, 1);

	else {
	    if (just_one) {
		term_clear_to_eol();
		term_newline();
	    } else
		break;
	}
    }
    if (!just_one)
	update_window_status(window, 1);
    term_flush();
}

void grow_window(window, offset)
Window *window;
int offset;
{
    Window *other,
          *tmp;
    char after;
    int window_size,
        other_size;

    if (window == null(Window *))
	window = current_window;
    if (window->next) {
	other = window->next;
	after = 1;
    } else {
	other = null(Window *);
	for (tmp = window_list; tmp; tmp = tmp->next) {
	    if (tmp == window)
		break;
	    other = tmp;
	}
	if (other == null(Window *)) {
	    put_it("*** Can't change the size of this window!");
	    return;
	}
	after = 0;
    }
    window_size = window->display_size + offset;
    other_size = other->display_size - offset;
    if ((window_size < 5) || (other_size < 5)) {
	window_size = window->display_size - offset;
	other_size = other->display_size + offset;
	put_it("*** Not enough room to resize this window!");
	return;
    }
    if (after) {
	window->bottom += offset;
	other->top += offset;
    } else {
	window->top -= offset;
	other->bottom -= offset;
    }
    resize_display(window, window_size);
    resize_display(other, other_size);
    if (after) {
	redraw_window(window, 1);
	update_window_status(window, 1);
	redraw_window(other, 1);
	update_window_status(other, 1);
    } else {
	redraw_window(other, 1);
	update_window_status(other, 1);
	redraw_window(window, 1);
	update_window_status(window, 1);
    }
    term_flush();
}

void redraw_all_windows()
{
    Window *tmp;

    if (dumb)
	return;
    term_clear_screen();
    for (tmp = window_list; tmp; tmp = tmp->next)
	redraw_window(tmp, 0);
    term_flush();
}

/*
 * add_to_display: adds the given string to the display.  No kidding. This
 * routine handles the whole ball of wax.  It keeps track of what's on the
 * screen, does the scrolling, everything... well, not quite everything...
 * The CONTINUED_LINE idea thanks to Jan L. Peterson (jlp@hamblin.byu.edu)  
 */
static void add_to_window(window, str)
Window *window;
char *str;
{
    char buffer[BUFFER_SIZE + 1];
    char *ptr,
        *cont_ptr,
        *cont = null(char *),
        *temp = null(char *),
         c;
    int pos = 0,
        col = 0,
        nd_cnt = 0,
        word_break = 0,
        start = 0,
        i,
        len,
        indent = 0;

    display_highlight(OFF);
    if (*str == null(char))
	str = " ";		/* special case to make blank lines show up */
    for (ptr = str; *ptr; ptr++) {
	if (*ptr <= 32) {
	    switch (*ptr) {
		case '\007':	/* bell */
		    if (get_int_var(AUDIBLE_BELL_VAR)) {
			buffer[pos++] = *ptr;
			nd_cnt++;
		    } else
			buffer[pos++] = *ptr + 64;
		    col++;
		    break;
		case '\011':	/* tab */
		    if (indent == 0)
			indent = -1;
		    len = 8 - (col % 8);
		    word_break = pos;
		    for (i = 0; i < len; i++)
			buffer[pos++] = ' ';
		    col += len;
		    break;
		case ' ':	/* word break */
		    if (indent == 0)
			indent = -1;
		    word_break = pos;
		    buffer[pos++] = *ptr;
		    col++;
		    break;
		case BOLD_OFF:
		case BOLD_TOGGLE:
		    buffer[pos++] = *ptr;
		    nd_cnt++;
		    break;
		default:	/* Anything else, make it displayable */
		    if (indent == -1)
			indent = pos - nd_cnt;
		    buffer[pos++] = *ptr + 64;
		    col++;
		    break;
	    }
	} else {
	    if (indent == -1)
		indent = pos - nd_cnt;
	    buffer[pos++] = *ptr;
	    col++;
	}
	if (pos == BUFFER_SIZE)
	    *ptr = null(char);
	if (col >= CO) {
	    if (word_break == 0)/* one big long line, no word breaks */
		word_break = pos - (col - CO);
	    c = buffer[word_break];
	    buffer[word_break] = null(char);
	    if (cont) {
		malloc_strcpy(&temp, cont);
		malloc_strcat(&temp, &(buffer[start]));
	    } else
		malloc_strcpy(&temp, &(buffer[start]));
	    rite(window, temp, 0, 0);
	    buffer[word_break] = c;
	    start = word_break;
	    word_break = 0;
	    while (buffer[start] == ' ')
		start++;
	    if (start > pos)
		start = pos;
	    if ((cont_ptr = get_string_var(CONTINUED_LINE_VAR)) == null(char *))
		cont_ptr = empty_string;
	    if (get_int_var(INDENT_VAR) && (indent < CO / 3)) {
		/*
		 * INDENT thanks to Carlo "lynx" v. Loesch
		 * <loesch@lan.informatik.tu-muenchen.dbp.de> 
		 */
		if (cont == null(char *)) {
		    if ((len = strlen(cont_ptr)) > indent) {
			cont = (char *) new_malloc(len + 1);
			strcpy(cont, cont_ptr);
		    } else {
			cont = (char *) new_malloc(indent + 1);
			strcpy(cont, cont_ptr);
			for (i = len; i < indent; i++)
			    cont[i] = ' ';
			cont[indent] = null(char);
		    }
		}
	    } else
		malloc_strcpy(&cont, cont_ptr);
	    col = strlen(cont) + (pos - start);
	}
    }
    buffer[pos] = null(char);
    if (buffer[start]) {
	if (cont) {
	    malloc_strcpy(&temp, cont);
	    malloc_strcat(&temp, &(buffer[start]));
	} else
	    malloc_strcpy(&temp, &(buffer[start]));
	rite(window, temp, 0, 0);
    }
    new_free(&cont);
    new_free(&temp);
    term_flush();
}

void save_message_from()
{
         malloc_strcpy(&save_from, who_from);
    save_level = who_level;
}

void restore_message_from()
{
         malloc_strcpy(&who_from, save_from);
    who_level = save_level;
}

void message_from(who, level)
char *who;
int level;
{
    malloc_strcpy(&who_from, who);
    who_level = level;
}

void message_to(refnum)
unsigned int refnum;
{
    Window *tmp;

    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (tmp->refnum == refnum) {
	    to_window = tmp;
	    return;
	}
    }
    to_window = null(Window *);
}

void add_to_screen(buffer)
char *buffer;
{
    Window *tmp;

    if (dumb) {
	add_to_lastlog(current_window, buffer);
	puts(buffer);
	return;
    }
    strmcat(buffer, "\017", 1024);	/* turns off all boldface */
    if (who_level == LOG_CURRENT) {
	add_to_lastlog(current_window, buffer);
	add_to_window(current_window, buffer);
	return;
    }
    if (to_window) {
	add_to_lastlog(to_window, buffer);
	add_to_window(to_window, buffer);
	return;
    }
    if (who_from) {
	for (tmp = window_list; tmp; tmp = tmp->next) {
	    if ((tmp->current_channel && (stricmp(who_from, tmp->current_channel) == 0)) || (tmp->query_nick && (stricmp(who_from, tmp->query_nick) == 0))) {
		add_to_lastlog(tmp, buffer);
		add_to_window(tmp, buffer);
		return;
	    }
	}
    }
    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (who_level & tmp->window_level) {
	    add_to_lastlog(tmp, buffer);
	    add_to_window(tmp, buffer);
	    return;
	}
    }
    add_to_lastlog(current_window, buffer);
    add_to_window(current_window, buffer);
}

void next_window()
{
    if   (current_window->next)
	     current_window = current_window->next;
    else
	current_window = window_list;
    update_all_status();
}

void previous_window()
{
    if   (current_window->prev)
	     current_window = current_window->prev;
    else {
	Window *tmp;

	for (tmp = window_list; tmp; tmp = tmp->next)
	    current_window = tmp;
    }
    update_all_status();
}

void free_hold(window)
Window *window;
{
    Hold *tmp,
        *next;

    for (tmp = window->hold_head; tmp; tmp = next) {
	next = tmp->next;
	new_free(&(tmp->str));
	new_free(&tmp);
    }
}

void free_lastlog(window)
Window *window;
{
    Lastlog *tmp,
           *next;

    for (tmp = window->lastlog_head; tmp; tmp = next) {
	next = tmp->next;
	new_free(&(tmp->msg));
	new_free(&tmp);
    }
}

void delete_window(window)
Window *window;
{
    Window *other = null(Window *);

    if (window == null(Window *))
	window = current_window;
    if (visible_windows == 1) {
	put_it("*** You can't kill the last window!");
	return;
    }
    new_free(&(window->status_line));
    new_free(&(window->last_sl));
    new_free(&(window->query_nick));
    new_free(&(window->current_channel));
    free_display(window);
    free_hold(window);
    free_lastlog(window);

    if (other = window->next) {
	other->prev = window->prev;
	other->top = window->top;
    }
    if (window->prev) {
	if (other == null(Window *)) {
	    other = window->prev;
	    other->bottom = window->bottom;
	}
	window->prev->next = window->next;
    } else
	window_list = window->next;
    visible_windows--;
    if (window == current_window)
	current_window = window_list;
    new_free(&window);
    resize_display(other, other->bottom - other->top);
    redraw_window(other, 1);
    redraw_all_status();
    cursor_to_input();
    term_flush();
}

/*
 * create_refnum: this generates a reference number for a new window that is
 * nor currently is use by another window.  A refnum of 0 is reserved (and
 * never returned by this routine).  Using a refnum of 0 in the message_to()
 * routine means no particular window (stuff goes to CRAP) 
 */
static unsigned int create_refnum()
{
    static unsigned int refnum = 1;
    Window *tmp;
    int done = 0;

    while (!done) {
	done = 1;
	if (refnum == 0)
	    refnum++;
	for (tmp = window_list; tmp; tmp = tmp->next) {
	    if (tmp->refnum == refnum) {
		done = 0;
		refnum++;
		break;
	    }
	}
    }
    return (refnum++);
}

void new_window()
{
    Window *new,
          *tmp,
          *old;

    if (dumb && visible_windows == 1)
	return;
    new = (Window *) new_malloc(sizeof(Window));
    new->refnum = create_refnum();
    new->line_cnt = 0;
    new->visible = 1;
    if (visible_windows == 0)
	new->window_level = LOG_ALL;
    else
	new->window_level = LOG_NONE;
    new->hold_mode = get_int_var(HOLD_MODE_VAR);
    new->scroll = get_int_var(SCROLL_VAR);
    new->lastlog_head = null(Lastlog *);
    new->lastlog_tail = null(Lastlog *);
    new->lastlog_level = real_lastlog_level();
    new->lastlog_size = 0;
    new->held = OFF;
    new->last_held = OFF;
    new->current_channel = null(char *);
    new->query_nick = null(char *);
    new->hold_on_next_rite = 0;
    new->status_line = null(char *);
    new->last_sl = null(char *);
    new->top_of_display = null(Display *);
    new->display_ip = null(Display *);
    new->hold_head = null(Hold *);
    new->hold_tail = null(Hold *);
    new->held_lines = 0;
    new->next = null(Window *);
    new->prev = null(Window *);
    new->cursor = 0;

    visible_windows++;
    old = current_window;
    if (current_window == null(Window *)) {
	window_list = current_window = new;
	if (dumb) {
	    new->display_size = 24;	/* what the hell */
	    return;
	}
	recalculate_windows();
    } else {
	/* split current window, or find a better window to split */
	if (current_window->display_size < 5) {
	    int size = 0;
	    Window *tmp,
	          *biggest = null(Window *);

	    for (tmp = window_list; tmp; tmp = tmp->next) {
		if (tmp->display_size > size) {
		    size = tmp->display_size;
		    biggest = tmp;
		}
	    }
	    if ((biggest == null(Window *)) || (size < 5)) {
		put_it("*** Not enough room to create another window!");
		new_free(&new);
		visible_windows--;
		return;
	    }
	    current_window = biggest;
	}
	new->prev = current_window->prev;
	new->next = current_window;
	if (new->prev)
	    new->prev->next = new;
	else
	    window_list = new;
	current_window->prev = new;
	new->top = current_window->top;
	new->bottom = (current_window->top + current_window->bottom) / 2;
	current_window->top = new->bottom + 1;
	resize_display(new, new->bottom - new->top);
	resize_display(current_window, current_window->bottom - current_window->top);
	tmp = current_window;
	current_window = new;
	redraw_window(new, 1);
	update_window_status(new, 1);
	redraw_window(tmp, 1);
	update_window_status(tmp, 1);
	if (old && (old != tmp)) {
	    redraw_window(old, 1);
	    update_window_status(old, 1);
	}
    }
    term_flush();
}

int unhold_windows()
{
    Window *tmp;
    char *stuff;
    int flag = 0;

    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (!hold_output(tmp) && (stuff = hold_queue(tmp))) {
	    if (rite(tmp, stuff, 1, 0) == 0) {
		remove_from_hold_list(tmp);
		flag = 1;
	    }
	}
    }
    return (flag);
}

/***** STATUS STUFF *******/

void update_window_status(window, refresh)
Window *window;
char refresh;
{
    if (dumb)
	return;
    if (window == null(Window *))
	window = current_window;
    if (refresh)
	new_free(&(window->last_sl));
    make_status(window);
    term_flush();
}

void redraw_all_status()
{
    Window *tmp;

    if (dumb)
	return;
    for (tmp = window_list; tmp; tmp = tmp->next) {
	new_free(&(tmp->last_sl));
	make_status(tmp);
    }
    term_flush();
}

void update_all_status()
{
    Window *tmp;

    if (dumb)
	return;
    for (tmp = window_list; tmp; tmp = tmp->next)
	make_status(tmp);
    term_flush();
}

int is_current_channel(channel, delete)
char *channel;
char delete;
{
    Window *tmp;
    char flag = 0;

    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (tmp->current_channel &&
	    (stricmp(channel, tmp->current_channel) == 0)) {
	    flag = 1;
	    if (delete)
		new_free(&(tmp->current_channel));
	}
    }
    return (flag);
}

char *current_channel(channel)
char *channel;
{
    if (channel) {
	if (strcmp(channel, "0") == 0)
	    channel = null(char *);
	malloc_strcpy(&(current_window->current_channel), channel);
	return (channel);
    } else
	return (current_window->current_channel);
}

unsigned int current_refnum()
{
        return (current_window->refnum);
}

char *query_nick()
{
         return (current_window->query_nick);
}

void set_query_nick(nick)
char *nick;
{
    malloc_strcpy(&(current_window->query_nick), nick);
}

static void revamp_window_levels(level)
int level;
{
    Window *tmp;

    for (tmp = window_list; tmp; tmp = tmp->next) {
	if (tmp != current_window)
	    tmp->window_level ^= (tmp->window_level & level);
    }
}

void window(command, args)
char *command,
    *args;
{
    int len;
    char *arg;
    char no_args = 1;

    message_from(null(char *), LOG_CURRENT);
    while (arg = next_arg(args, &args)) {
	no_args = 0;
	len = strlen(arg);
	if (strnicmp("NEW", arg, len) == 0)
	    new_window();
	else if (strnicmp("KILL", arg, len) == 0)
	    delete_window(current_window);
	else if (strnicmp("SHRINK", arg, len) == 0) {
	    if (arg = next_arg(args, &args))
		grow_window(current_window, -atoi(arg));
	    else
		put_it("*** You must specify the number of lines");
	} else if (strnicmp("GROW", arg, len) == 0) {
	    if (arg = next_arg(args, &args))
		grow_window(current_window, atoi(arg));
	    else
		put_it("*** You must specify the number of lines");
	} else if (strnicmp("SCROLL", arg, len) == 0) {
	    if (((arg = next_arg(args, &args)) == null(char *)) ||
		do_boolean(arg, &(current_window->scroll)))
		put_it("*** Value for SCROLL must be ON, OFF, or TOGGLE");
	    else
		put_it("*** Scrolling is %s", var_settings[current_window->scroll]);
	} else if (strnicmp("HOLD_MODE", arg, len) == 0) {
	    if (((arg = next_arg(args, &args)) == null(char *)) ||
		do_boolean(arg, &(current_window->hold_mode)))
		put_it("*** Value for HOLD_MODE must be ON, OFF, or TOGGLE");
	    else {
		reset_line_cnt(current_window->hold_mode);
		put_it("*** Hold mode is %s", var_settings[current_window->hold_mode]);
	    }
	} else if (strnicmp("LASTLOG_LEVEL", arg, len) == 0) {
	    if (arg = next_arg(args, &args)) {
		current_window->lastlog_level = parse_lastlog_level(arg);
		put_it("*** Lastlog level is %s", bits_to_lastlog_level(current_window->lastlog_level));
	    } else
		put_it("*** Level required");
	} else if (strnicmp("LEVEL", arg, len) == 0) {
	    if (arg = next_arg(args, &args)) {
		current_window->window_level = parse_lastlog_level(arg);
		put_it("*** Window level is %s", bits_to_lastlog_level(current_window->window_level));
		revamp_window_levels(current_window->window_level);
	    } else
		put_it("*** Level required");
	} else if (strnicmp("BALANCE", arg, len) == 0)
	    recalculate_windows();
	else if (strnicmp("CHANNEL", arg, len) == 0) {
	    if (arg = next_arg(args, &args)) {
		if (is_current_channel(arg, 1)) {
		    current_channel(arg);
		    update_all_status();
		} else {
		    if (is_on_channel(arg, nickname)) {
			current_channel(arg);
			update_all_status();
		    } else if ((*arg == '0') && (*(arg + 1) = null(char)))
			current_channel(null(char *));
		    else {
			if (irc2_6)
			    send_to_server("JOIN %s", arg);
			else
			    send_to_server("CHANNEL %s", arg);
		    }
		}
	    } else
		put_it("*** You must specify a channel!");
	} else
	    put_it("*** Unknown WINDOW command: %s", arg);
    }
    if (no_args) {
	put_it("*** This window");
	put_it("***\tCurrent channel: %s", (current_window->current_channel ? current_window->current_channel : "<None>"));
	put_it("***\tQuery User: %s", (current_window->query_nick ? current_window->query_nick : "<None>"));
	put_it("***\tScrolling is %s", var_settings[current_window->scroll]);
	put_it("***\tHold mode is %s", var_settings[current_window->hold_mode]);
	put_it("***\tWindow level is %s", bits_to_lastlog_level(current_window->window_level));
	put_it("***\tLastlog level is %s", bits_to_lastlog_level(current_window->lastlog_level));
    }
    message_from(null(char *), LOG_CRAP);
}

int number_of_windows()
{
        return (visible_windows);
}
