/*
 * window.h: header file for window.c 
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "hold.h"
#include "lastlog.h"

typedef struct DisplayStru {
    char *line;
    struct DisplayStru *next;
}           Display;

typedef struct WindowStru {
    int refnum;
    int top;
    int bottom;
    int cursor;
    int line_cnt;
    int visible;
    int scroll;

    char *status_line,
        *last_sl;

    int display_size;
    Display *top_of_display,
           *display_ip;

    char *query_nick;
    char *current_channel;
    int window_level;

    /* hold stuff */
    int hold_mode;
    int hold_on_next_rite;
    int held;
    int last_held;
    Hold *hold_head,
        *hold_tail;
    int held_lines;

    /* lastlog stuff */
    Lastlog *lastlog_head;
    Lastlog *lastlog_tail;
    int lastlog_level;
    int lastlog_size;
    
    struct WindowStru *next;
    struct WindowStru *prev;
}          Window;

extern Window *current_window;
extern void set_scroll_lines();
extern void set_scroll();
extern void reset_line_cnt();
extern void set_continued_line();
extern int rite();
extern void erase_display();
extern void resize_display();
extern void redraw_all_windows();
extern void add_to_screen();
extern void init_windows();
extern int unhold_windows();

/* var_settings indexes */
#define OFF 0
#define ON 1
#define TOGGLE 2

extern void erase_display();
extern void resize_display();
extern void redraw_display();
extern void add_to_display();
extern void set_scroll();
extern void set_scroll_lines();
extern void reset_line_cnt();
extern int display_hold();
extern void update_all_status();
extern void set_query_nick();
extern char *query_nick();
extern void update_window_status();
extern void window();
extern void redraw_window();
extern void redraw_all_windows();
extern void next_window();
extern void previous_window();
extern int is_current_channel();
extern void redraw_all_status();
extern void message_to();
extern void message_from();
extern unsigned int current_refnum();
