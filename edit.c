/*************************************************************************
 ** edit.c  Beta  v2.0    (22 Aug 1988)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen 
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 ** 
 *************************************************************************/

char edit_id[] = "edit.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <curses.h>
#include <signal.h>
#include "struct.h"

#ifdef TRUE
#undef TRUE
#endif
#define FALSE (0)
#define TRUE  (!FALSE)

#define FROM_START 0
#define FROM_END   1
#define RELATIVE   2

extern int termtype;
static int esc=0;
static int literal=0;

do_char(ch, sock)
char ch;
{
    static int first_time=0;

    if (!first_time) {
	toggle_ins();
	toggle_ins();
	first_time=1;
#ifdef DOCURSES
	if (termtype == CURSES_TERM)
	  refresh();
#endif
    }
    if (esc==1) {
	do_after_esc(ch);
	return tulosta_viimeinen_rivi(); /* print last line */
    }
    switch (ch) {
    case '\000':		/* NULL */
	break;
    case '\001':		/* ^A */
        set_position(0, FROM_START); /* beginning of line */ 
        break;
    case '\002':		/* ^B */
        set_position(-1, RELATIVE);		/* backward char */
	break;
    case '\003':		/* ^C */
	rev_line();		/* reverse line */
	break;
    case '\004':		/* ^D */
	del_ch_right();		/* delete char from right */
	break;
    case '\005':		/* ^E */
        set_position(0, FROM_END); /* end of line */
        set_position(1, RELATIVE);
	break;
    case '\006':		/* ^F */
        set_position(1, RELATIVE); /* forward char */
	break;
    case '\007':		/* ^G */
	add_ch(ch);		/* bell */
	break;
    case '\010':		/* ^H */
	del_ch_left();		/* delete char to left */
	break;
    case '\011':		/* TAB */
	toggle_ins();		/* toggle insert mode */
	break;
    case '\012':		/* ^J */
	send_this_line(sock);	/* send this line */
	break;
    case '\013':		/* ^K */
	kill_eol();		/* kill to end of line */
	break;
    case '\014':		/* ^L */
	refresh_screen();	/* refresh screen */
	break;
    case '\015':		/* ^M */
	send_this_line(sock);	/* send this line */
	break;
    case '\016':		/* ^N */
	next_in_history();	/* next in history */
	break;
    case '\017':		/* ^O */
	break;
    case '\020':		/* ^P */
	previous_in_history();	/* previous in history */
	break;
    case '\021':		/* ^Q */
	break;
    case '\022':		/* ^R */
    case '\023':		/* ^S */
    case '\024':		/* ^T */
	break;
    case '\025':		/* ^U */
	kill_whole_line();	/* kill whole line */
	break;
    case '\026':		/* ^V */
        esc = 1; 		/* literal next */
        literal=1;
	break;
    case '\027':		/* ^W */
	del_word_left();        /* delete word left */
	break;
    case '\030':		/* ^X */
	break;
    case '\031':		/* ^Y */
	yank();			/* yank */
	break;
    case '\032':		/* ^Z */
	suspend_irc();		/* suspend irc */
	break;
    case '\033':		/* ESC */
        esc = 1;	       /* got esc */
	break;
    case '\177':		/* DEL */
	del_ch_left();		/* delete char to left */
	break;
    default:
	add_ch(ch);
	break;
    }
    return tulosta_viimeinen_rivi();
}

rev_line()
{
    int i1, i2, i3, i4;
    
    i4=get_position();
    set_position(0, FROM_START);
    i1=get_position();
    set_position(0, FROM_END);
    i1=get_position()-i1;
    set_position(i4, FROM_START);

    for( i2=0; i2>i1/2; i2++) {
	i3=get_char(i2);
	set_char(i2, get_char(i1-i2-1));
	set_char(i1-i2-1, i3);
    }
}

del_ch_right()
{
    int i1, i2, i3;

    i1=get_position();
    
    if (!get_char(i1))
	return;			/* last char in line */
    set_position(0, FROM_END);
    i2=get_position();
    for (i3=i1; i3<i2; i3++)
	set_char(i3, get_char(i3+1));
    set_char(i3, 0);
    set_position(i1, FROM_START);
}

del_ch_left()
{
    int i1, i2, i3;

    i1=get_position();
    
    if (!i1)
	return;			/* first pos in line */
    set_position(0, FROM_END);
    i2=get_position();
    for (i3=i1-1; i3<i2; i3++)
	set_char(i3, get_char(i3+1));
    set_char(i3, 0);
    set_position(i1, FROM_START);
    set_position(-1, RELATIVE);
}

suspend_irc()
{
#if HPUX
#ifdef SIGSTOP
  kill(getpid(), SIGSTOP);
#endif
#else
#ifndef VMS
  tstp(); 
#endif
#endif
}

do_after_esc(ch)
char ch;
{
    if (literal) {
	literal=0;
	add_ch(ch);
	return;
    }
    esc = 0;
    switch (ch) {
    case 'b':
	word_back();
	break;
    case 'd':
	del_word_right();
	break;
    case 'f':
	word_forw();
	break;
    case 'y':
	yank();
	break;
    case '\177':
	del_word_left();
	break;
    default:
	break;
    }
}

refresh_screen()
{
#ifdef DOCURSES
  if (termtype == CURSES_TERM) {
    clearok(curscr, TRUE);
    refresh();
  }
#endif
}

add_ch(ch)
int ch;
{
    int i1, i2, i3;
    if (in_insert_mode()) {
	i1=get_position();
	set_position(0, FROM_END);
	i2=get_position();
	for (i3=i2; i3>=0; i3--)
	    set_char(i1+i3+1, get_char(i3+i1));
	set_char(i1, ch);
	set_position(i1, FROM_START);
	set_position(1, RELATIVE);
    } else {
	i1=get_position();
	set_char(i1, ch);
	set_position(i1, FROM_START);
	set_position(1, RELATIVE);
    }
}

literal_next()
{
    esc = 1; 
    literal=1;
}

word_forw()
{
    int i1,i2;

    i1=get_position();
    while( i2=get_char(i1) )
	if ((i2==(int)' ') ||
	    (i2==(int)'\t') ||
	    (i2==(int)'_') ||
	    (i2==(int)'-'))
	    i1++;
	else
	    break;
    while( i2=get_char(i1) )
	if ((i2==(int)' ') ||
	    (i2==(int)'\t') ||
	    (i2==(int)'_') ||
	    (i2==(int)'-'))
	    break;
	else
	    i1++;
    set_position(i1, FROM_START);
}

#define MagicChar(x) \
( (x) == (int)' ' || (x) == (int)'\t' || (x) == (int)'_' || (x) == (int)'-')
  
word_back()
{
    int  i1,i2;

    i1=get_position();
    if (i1!=0)
	i1--;
    while (i2=get_char(i1))
	if (MagicChar(i2)) 
	    i1--;
	else
	    break;
    while( i2=get_char(i1) )
	if (MagicChar(i2))
	    break;
	else
	    i1--;
    if (i1<=0)
	i1=0;
    else
	i1++;
    set_position(i1, FROM_START);
}

del_word_left()
{
    int i1, i2, i3, i4;

    i1=get_position();
    word_back();
    i2=get_position();
    set_position(0, FROM_END);
    i3=get_position();
    for(i4=i2; i4<=i3-(i1-i2); i4++)
	set_char(i4, get_char(i4+(i1-i2)));
    for(; i4<=i3; i4++)
	set_char(i4, (int)'\0');
    set_position(i2, FROM_START);
}

del_word_right()
{
    int i1, i2, i3, i4;

    i2=get_position();
    word_forw();
    i1=get_position();
    set_position(0, FROM_END);
    i3=get_position();
    for(i4=i2; i4<=i3-(i1-i2); i4++)
	set_char(i4, get_char(i4+(i1-i2)));
    for(; i4<=i3; i4++)
	set_char(i4, (int)'\0');
    set_position(i2, FROM_START);
}
