/*************************************************************************
 ** screen.c  Beta  v2.0    (22 Aug 1988)
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

char screen_id[] = "screen.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include <stdio.h>
#include <curses.h>

#ifdef TRUE
#undef TRUE
#endif
#define FALSE (0)
#define TRUE  (!FALSE)
#ifdef BUFSIZ
#undef BUFSIZ
#endif
#define BUFSIZ 240

#define FROM_START 0
#define FROM_END   1
#define RELATIVE   2

#define HIST_SIZ 1000

static char last_line[BUFSIZ+1];
static char yank_buffer[BUFSIZ+1];
static char history[HIST_SIZ][BUFSIZ+1];
static int position=0;
static int pos_in_history=0;
int insert=0;
extern int termtype;

get_char(pos)
int pos;
{
    if (pos>=BUFSIZ || pos<0)
	return 0;
    return (int)last_line[pos];
}

set_char(pos, ch)
int pos, ch;
{
    if (pos<0 || pos>=BUFSIZ)
	return;
    if (ch<0)
	ch=0;
    last_line[pos]=(char)ch;
}

get_yank_char(pos)
int pos;
{
    if (pos>=BUFSIZ || pos<0)
	return 0;
    return (int)yank_buffer[pos];
}

set_yank_char(pos, ch)
int pos, ch;
{
    if (pos<0 || pos>=BUFSIZ)
	return;
    if (ch<0)
	ch=0;
    yank_buffer[pos]=(char)ch;
}

set_position(disp, from)
int disp, from;
{
    switch (from) {
    case FROM_START:
	position=disp;
	break;
    case RELATIVE:
	position+=disp;
	break;
    case FROM_END: { 
        int i1 = 0;
	for (; get_char(i1); i1++);
	position=i1-1;
        } 
	break;
      
    default:
	position=0;
	break;
    }
}

get_position()
{
    return position;
}

toggle_ins()
{
    insert = ~insert;
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      standout();
      mvaddstr(LINES-2, 75, insert ? "INS" : "OWR");
      standend();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM)
      put_insflag(insert);
#endif
}

in_insert_mode()
{
    return insert;
}

send_this_line(sock)
{
    record_line();
    sendit(sock, last_line);
    clear_last_line();
    set_position(0, FROM_START); /* beginning of line */ 
    tulosta_viimeinen_rivi();
#ifdef DOCURSES
    if (termtype == CURSES_TERM)
      refresh();
#endif
}

record_line()
{
    static int place=0;
    int i1;

    for(i1=0; i1<BUFSIZ; i1++)
	history[place][i1]=get_char(i1);
    place++;
    if (place==HIST_SIZ)
	place=0;
    pos_in_history=place;
}
    
clear_last_line()
{
    int i1;

    for(i1=0; i1<BUFSIZ; i1++)
	set_char(i1,(int)'\0');
}

kill_eol()
{
    int i1, i2, i3;

    i1=get_position();
    set_position(0, FROM_END);
    i2=get_position();
    for(i3=0; i3<BUFSIZ; i3++)
	set_yank_char(i3,(int)'\0');
    for(i3=0; i3<=(i2-i1); i3++) {
	set_yank_char(i3,get_char(i1+i3));
	set_char(i1+i3, 0);
    }
    set_position(i1, FROM_START);
}

next_in_history()
{
    int i1=0;

    pos_in_history++;
    if (pos_in_history==HIST_SIZ)
	pos_in_history=0;
    clear_last_line();
    for (; history[pos_in_history][i1]; i1++) 
	set_char(i1, history[pos_in_history][i1]);
    
    set_position(0, FROM_START);
}

previous_in_history()
{
    int i1=0;

    pos_in_history--;
    if (pos_in_history<0)
	pos_in_history=HIST_SIZ-1;
    clear_last_line();
    for (; history[pos_in_history][i1]; i1++) 
	set_char(i1, history[pos_in_history][i1]);
    
    set_position(0, FROM_START);
}

kill_whole_line()
{
    clear_last_line();
    set_position(0, FROM_START);
}

yank()
{
    int i1=get_position();
    int i2=0;
    int i3;

    while (get_yank_char(i2))
	i2++;
    
    for(i3=BUFSIZ-1; i3>=i1+i2; i3--)
	set_char(i3, get_char(i3-i2));
    for(i3=0; i3<i2; i3++)
	set_char(i1+i3, get_yank_char(i3));
}
#define RMARGIN 78

tulosta_viimeinen_rivi()
{
    static int paikka=0;
    int i1, i2, i3;

    i1=get_position();
    /* taytyyko siirtaa puskuria */
    if (i1<(get_disp(paikka)+10) && paikka) {
	paikka--;
	i2=get_disp(paikka);
    } else if (i1>(get_disp(paikka)+70)) {
	paikka++;
	i2=get_disp(paikka);
    } else {
	i2=get_disp(paikka);
    }

#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      move(LINES-1,0);
      for(i3=0; i3<RMARGIN; i3++)
        if (get_char(i2+i3))
	  mvaddch(LINES-1, i3, get_char(i2+i3));
      clrtoeol();
      move(LINES-1, i1-get_disp(paikka));
      refresh();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM) {
      tcap_move(-1, 0);
      for(i3=0; i3<RMARGIN; i3++)
        if (get_char(i2+i3))
	  tcap_putch(LINES-1, i3, get_char(i2+i3));
      clear_to_eol();
      tcap_move(-1, i1-get_disp(paikka));
      refresh();
    }
#endif
    return (i1-get_disp(paikka));
}

get_disp(paikka)
int paikka;
{
    static int place[]={0,55,55*2,55*3,55*4};

    if (paikka>4 || paikka<0)
	return 0;
    return place[paikka];
}
