/*
 * term.h 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */

extern char term_reset_flag;
extern char *CM,
    *CE,
    *CL,
    *CR,
    *NL,
    *SO,
    *SE;
extern int CO,
    LI;

extern void putchar_x();
#define tputs_x(s)		(tputs(s, 0, putchar_x))

#define term_standout_on()		(tputs_x(SO))
#define term_standout_off()		(tputs_x(SE))
#define term_clear_screen()		(tputs_x(CL))
#define term_move_cursor(c, r)		(tputs_x(tgoto(CM, (c), (r))))
#define term_cr()			(tputs_x(CR))
#define term_newline()			(tputs_x(NL))

extern void term_init();
extern void term_resize();
extern void term_pause();
extern void term_putchar();
extern void term_puts();
extern int (*term_scroll) ();
extern int (*term_insert) ();
extern int (*term_delete) ();
extern int (*term_cursor_right) ();
extern int (*term_cursor_left) ();
extern int (*term_clear_to_eol) ();

/* Stuff needed for HP-UX installation                   */

#ifdef HPUX
#ifndef _TTY_CHARS_ST_
#define _TTY_CHARS_ST_
/* Structure for terminal special characters */
struct tchars {
	char	t_intrc;		/* Interrupt			*/
	char	t_quitc;		/* Quit 			*/
	char	t_startc;		/* Start output 		*/
	char	t_stopc;		/* Stop output			*/
	char	t_eofc; 		/* End-of-file (EOF)		*/
	char	t_brkc; 		/* Input delimiter (like nl)	*/
};

/* Window size structure */
struct winsize {
	unsigned short	ws_row, ws_col; 	/* Window charact. size */
	unsigned short	ws_xpixel, ws_ypixel;	/* Window pixel size	*/
};

#endif _TTY_CHARS_ST_

#define TIOCSETC	_IOW('t',17,struct tchars)	/* set special chars */
#define TIOCGETC	_IOR('t',18,struct tchars)	/* get special chars */

#define CBREAK		0x02				/* Half-cooked mode */
#define TIOCGWINSZ	_IOR('t', 104, struct winsize)	/* Get win. sz. */

#endif
