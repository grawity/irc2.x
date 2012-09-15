/*
 * term.h: header file for term.c
 *
 *
 * Written By Michael Sandrof <ms5n@andrew.cmu.edu> 
 *
 * Copyright(c) 1990 
 *
 * See the COPYRIGHT file, or do a HELP IRCII COPYRIGHT 
 */
#include "config.h"
#ifdef MUNIX
#include <sys/ttold.h>
#endif

extern char term_reset_flag;
extern char *CM,
    *DO,
    *CE,
    *CL,
    *CR,
    *NL,
    *SO,
    *SE,
    *BL;
extern int CO,
    LI,
    SG;

extern void putchar_x();
#define tputs_x(s)		(tputs(s, 0, putchar_x))

#define term_standout_on()		(tputs_x(SO))
#define term_standout_off()		(tputs_x(SE))
#define term_clear_screen()		(tputs_x(CL))
#define term_move_cursor(c, r)		(tputs_x(tgoto(CM, (c), (r))))
#define term_cr()			(tputs_x(CR))
#define term_newline()			(tputs_x(NL))
#define term_beep()			(tputs_x(BL),fflush(stdout))

extern void term_cont();
extern char term_echo();
extern void term_init();
extern int term_resize();
extern void term_pause();
extern void term_putchar();
extern void term_puts();
extern void term_flush();
extern int (*term_scroll) ();
extern int (*term_insert) ();
extern int (*term_delete) ();
extern int (*term_cursor_right) ();
extern int (*term_cursor_left) ();
extern int (*term_clear_to_eol) ();

#if defined(ISC22) || defined(MUNIX)
/* Structure for terminal special characters */
struct tchars {
	char	t_intrc;		/* Interrupt			*/
	char	t_quitc;		/* Quit 			*/
	char	t_startc;		/* Start output 		*/
	char	t_stopc;		/* Stop output			*/
	char	t_eofc; 		/* End-of-file (EOF)		*/
	char	t_brkc; 		/* Input delimiter (like nl)	*/
};
struct ltchars {
	char	t_suspc;	/* stop process signal */
	char	t_dsuspc;	/* delayed stop process signal */
	char	t_rprntc;	/* reprint line */
	char	t_flushc;	/* flush output (toggles) */
	char	t_werasc;	/* word erase */
	char	t_lnextc;	/* literal next character */
};
#endif /* ISC22 */

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

#define    SIGWINCH    SIGWINDOW

#endif

#ifdef mips
/* well, it works */
#define fputc(c,f) write(1,&(c),1)
#define fwrite(buffer,len,cnt,f) write(1,buffer,len)
#endif mips
