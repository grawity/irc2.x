/************************************************************************
 *   IRC - Internet Relay Chat, irc/edit_ext.h
 *   Copyright (C) 1997 Alain Nissen
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*  This file contains external definitions for global variables and functions
    defined in irc/edit.c.
 */

/*  External definitions for global variables.
 */
#ifndef EDIT_C
extern char edit_id[];
#endif /* EDIT_C */

/*  External definitions for global functions.
 */
#ifndef EDIT_C
#define EXTERN extern
#else /* EDIT_C */
#define EXTERN
#endif /* EDIT_C */
EXTERN int do_char (char ch);
EXTERN void bol(void);
EXTERN void eol(void);
EXTERN void back_ch(void);
EXTERN void forw_ch(void);
EXTERN void rev_line(void);
EXTERN void del_ch_right(void);
EXTERN void del_ch_left(void);
EXTERN RETSIGTYPE suspend_irc (int s);
EXTERN void got_esc(void);
EXTERN void do_after_esc (char ch);
EXTERN void refresh_screen(void);
EXTERN void add_ch (int ch);
EXTERN void literal_next(void);
EXTERN void word_forw(void);
EXTERN void word_back(void);
EXTERN void del_word_left(void);
EXTERN void del_word_right(void);
#undef EXTERN
