/*
 * term.c: termcap stuff... 
 *
 *
 * Written By Michael Sandrof 
 *
 * Copyright (c) 1990 
 *
 * All Rights Reserved 
 */
#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "irc.h"
#include "term.h"
#ifdef HPUX
#include <sgtty.h>
#include <sys/bsdtty.h>
#endif


#define null(foo) (foo) 0L

extern char *tgetstr();
extern int tgetent();
extern char *getenv();

extern int term_CE_clear_to_eol();
extern int term_SPACE_clear_to_eol();
extern int term_CS_scroll();
extern int term_ALDL_scroll();
extern int term_IC_insert();
extern int term_IMEI_insert();
extern int term_DC_delete();
extern int term_null_function();
extern int term_BS_cursor_left();
extern int term_LE_cursor_left();
extern int term_ND_cursor_right();

static int tty_des;		/* descriptor for the tty */

static char empty_string[] = "";

static struct ltchars oldltchars,
        newltchars = {-1, -1, -1, -1, -1, -1};
static struct tchars oldtchars,
       newtchars = {'\003', -1, -1, -1, -1, -1};

static struct sgttyb oldb,
       newb;
static char termcap[1024];

/*
 * Function variables: each returns 1 if the function is not supported on the
 * current term type, otherwise they do their thing and return 0 
 */
int (*term_scroll) ();		/* this is set to the best scroll available */
int (*term_insert) ();		/* this is set to the best insert available */
int (*term_delete) ();		/* this is set to the best delete available */
int (*term_cursor_left) ();	/* this is set to the best left available */
int (*term_cursor_right) ();	/* this is set to the best right available */
int (*term_clear_to_eol) ();	/* this is set... figure it out */

/* The termcap variables */
char *CM,
    *CE,
    *CL,
    *CR,
    *NL,
    *AL,
    *DL,
    *CS,
    *DC,
    *IC,
    *IM,
    *EI,
    *SO,
    *SE,
    *ND,
    *LE;
int CO,
    LI;

/*
 * term_reset_flag: set to true whenever the terminal is reset, thus letter
 * the calling program work out what to do 
 */
char term_reset_flag = 0;

static int li,
    co;

void term_intr(c)
char c;
{
    newtchars.t_intrc=c;
    ioctl(tty_des, TIOCSETC, &newtchars);
}

void term_susp(c)
char c;
{
    newltchars.t_suspc=c;
    ioctl(tty_des, TIOCSLTC, &newltchars);
}

/*
 * term_putchar: puts a character to the screen, and displays control
 * characters as inverse video uppercase letters.  NOTE:  Dont use this to
 * display termcap control sequences!  It won't work! 
 */
void term_putchar(c)
char c;
{

    if (c < 32) {
	term_standout_on();
	putchar(c + 64);
	term_standout_off();
    } else if (c == '\177') {
	term_standout_on();
	putchar('?');
	term_standout_off();
    } else
	putchar(c);
}

/* term_puts: uses term_putchar to print text */
void term_puts(str, len)
char *str;
int len;
{
    int i;

    for (i = 0; *str && (i < len); str++, i++)
	term_putchar(*str);
}

/* putchar_x: the putchar function used by tputs */
void putchar_x(c)
char c;
{
    putchar(c);
}

/*
 * term_reset: sets terminal attributed back to what they were before the
 * program started 
 */
void term_reset()
{
    if   (CS)
	     tputs_x(tgoto(CS, LI - 1, 0));
    ioctl(tty_des, TIOCSETP, &oldb);
    ioctl(tty_des, TIOCSLTC, &oldltchars);
    ioctl(tty_des, TIOCSETC, &oldtchars);
    term_move_cursor(0, LI - 1);
    fflush(stdout);
    term_reset_flag = 1;
}

/*
 * term_pause: sets terminal back to pre-program days, then SIGSTOPs itself.
 * When the program is restarted, the terminal is set back to program-mode
 * This function should be called whent the program receives a SIGTSTP 
 */
void term_pause()
{
	 term_reset();
    kill(getpid(), SIGSTOP);
    ioctl(tty_des, TIOCSETC, &newtchars);
    ioctl(tty_des, TIOCSLTC, &newltchars);
    ioctl(tty_des, TIOCSETP, &newb);
}

/*
 * term_init: does all terminal initialization... reads termcap info, sets
 * the terminal to CBREAK, no ECHO mode.   Chooses the best of the terminal
 * attributes to use 
 */
void term_init()
{
    char bp[1024],
        *term,
        *ptr;

    if ((tty_des = open("/dev/tty", O_RDONLY, 0)) == -1)
	tty_des = 0;

    ioctl(tty_des, TIOCGLTC, &oldltchars);
    ioctl(tty_des, TIOCGETC, &oldtchars);
    ioctl(tty_des, TIOCSLTC, &newltchars);
    ioctl(tty_des, TIOCSETC, &newtchars);

    ioctl(tty_des, TIOCGETP, &oldb);
    newb = oldb;
    newb.sg_flags |= CBREAK;
#ifndef HPUX
    newb.sg_flags &= (~ECHO);
#endif
    ioctl(tty_des, TIOCSETP, &newb);
    if ((term = getenv("TERM")) == null(char *)) {
	fprintf(stderr, "irc: No TERM variable set!\n");
	exit(1);
    }
    if (tgetent(bp, term) < 1) {
	fprintf(stderr, "No tercap entry for %s.\n", term);
	exit(1);
    }
    co = tgetnum("co");
    li = tgetnum("li");
    ptr = termcap;

    CM = tgetstr("cm", &ptr);
    CL = tgetstr("cl", &ptr);
    if ((CM == null(char *)) ||
	(CL == null(char *))) {
	fprintf(stderr, "irc: can't run irc on this terminal!\n");
	exit(1);
    }
    if ((CR = tgetstr("cr", &ptr)) == null(char *))
	CR = "\r";
    if ((NL = tgetstr("nl", &ptr)) == null(char *))
	NL = "\n";

    if (CE = tgetstr("ce", &ptr))
	term_clear_to_eol = term_CE_clear_to_eol;
    else
	term_clear_to_eol = term_SPACE_clear_to_eol;

    if ((ND = tgetstr("nd", &ptr)) || (ND = tgetstr("kr", &ptr)))
	term_cursor_right = term_ND_cursor_right;
    else
	term_cursor_right = term_null_function;

    if ((LE = tgetstr("le", &ptr)) || (LE = tgetstr("kl", &ptr)))
	term_cursor_left = term_LE_cursor_left;
    else if (tgetflag("bs"))
	term_cursor_left = term_BS_cursor_left;
    else
	term_cursor_left = term_null_function;

    if (CS = tgetstr("cs", &ptr))
	term_scroll = term_CS_scroll;
    else {
	if ((AL = tgetstr("al", &ptr)) && (DL = tgetstr("dl", &ptr)))
	    term_scroll = term_ALDL_scroll;
	else
	    term_scroll = term_null_function;
    }

    if (IC = tgetstr("ic", &ptr))
	term_insert = term_IC_insert;
    else {
	if ((IM = tgetstr("im", &ptr)) && (EI = tgetstr("ei", &ptr)))
	    term_insert = term_IMEI_insert;
	else
	    term_insert = term_null_function;
    }

    if (DC = tgetstr("dc", &ptr))
	term_delete = term_DC_delete;
    else
	term_delete = term_null_function;

    SO = tgetstr("so", &ptr);
    SE = tgetstr("se", &ptr);
    if ((SO == null(char *)) || (SE == null(char *))) {
	SO = empty_string;
	SE = empty_string;
    }
}

/*
 * term_resize: gets the terminal height and width.  Trys to get the info
 * from the tty driver about size, if it can't... uses the termcap values 
 */
void term_resize()
{
    struct winsize window;

#ifdef HPUX
    LI = li;
    CO = co;
#else
    if (ioctl(tty_des, TIOCGWINSZ, &window) < 0) {
	LI = li;
	CO = co;
    } else {
	if ((LI = window.ws_row) == 0)
	    LI = li;
	if ((CO = window.ws_col) == 0)
	    CO = co;
    }
#endif    
    CO--;
}

/*
 * term_null_function: used when a terminal is missing a particulary useful
 * feature, such as scrolling, to warn the calling program that no such
 * function exists 
 */
static int term_null_function()
{
        return (1);
}

/* term_CE_clear_to_eol(): the clear to eol function, right? */
static int term_CE_clear_to_eol()
{
        tputs_x(CE);
    return (0);
}

/*
 * term_SPACE_clear_to_eol: clear to eol using spaces, then moves the cursor
 * back to x,y.  Thus, the cursor x and y positions have to be passed to
 * term_clear_to_eol() unless you can guarantee that the CE capability will
 * always exist 
 */
static int term_SPACE_clear_to_eol(x, y)
int x,
    y;
{
    int i,
        cnt;

    cnt = CO - x;
    for (i = 0; i < cnt; i++)
	putchar(' ');
    term_move_cursor(x, y);
}

/*
 * term_CS_scroll: should be used if the terminal has the CS capability by
 * setting term_scroll equal to it 
 */
static int term_CS_scroll(line1, line2)
int line1,
    line2;
{
    tputs_x(tgoto(CS, line2, line1));	/* shouldn't do this each time */
    term_move_cursor(0, line2);	/* Use SR if it's there? duh */
    tputs_x(NL);
    return (0);
    /* note: leaves cursor (hopefully) on last line of scrolled region */
}

/*
 * term_ALDL_scroll: should be used for scrolling if the term has AL and DL
 * by setting the term_scroll function to it 
 */
static int term_ALDL_scroll(line1, line2)
int line1,
    line2;
{
    term_move_cursor(0, line1);
    tputs_x(DL);
    term_move_cursor(0, line2);
    tputs_x(AL);
    return (0);
    /* note: leaves cursor (hopefully) on last line of scrolled region */
}

/*
 * term_IC_insert: should be used for character inserts if the term has IC by
 * setting term_insert to it. 
 */
static int term_IC_insert(c)
char c;
{
    tputs_x(IC);
    term_putchar(c);
    return (0);
}

/*
 * term_IMEI_insert: should be used for character inserts if the term has IM
 * and EI by setting term_insert to it 
 */
static int term_IMEI_insert(c)
char c;
{
    tputs_x(IM);
    term_putchar(c);
    tputs_x(EI);
    return (0);
}

/*
 * term_DC_delete: should be used for character deletes if the term has DC by
 * setting term_delete to it 
 */
static int term_DC_delete()
{
        tputs_x(DC);
    return (0);
}

/* term_ND_cursor_right: got it yet? */
static int term_ND_cursor_right()
{
        tputs_x(ND);
    return (0);
}

/* term_LE_cursor_left:  shouldn't you move on to something else? */
static int term_LE_cursor_left()
{
        tputs_x(LE);
    return (0);
}

static int term_BS_cursor_left()
{
        putchar('\010');
    return (0);
}
