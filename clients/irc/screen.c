/************************************************************************
 *   IRC - Internet Relay Chat, irc/screen.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/*
 * -- Gonzo -- Sat Jul  8 1990
 * Added getmypass() [used in m_oper()]
 */

char screen_id[] = "screen.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#include <stdio.h>
#include <curses.h>
#ifdef GETPASS
#include <signal.h>
#include <sgtty.h>
#include <pwd.h>
#endif

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

int insert=1;  /* default to insert mode */
               /* I want insert mode, thazwhat emacs does ! //jkp */

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
    int i1;

    switch (from) {
    case FROM_START:
	position=disp;
	break;
    case RELATIVE:
	position+=disp;
	break;
    case FROM_END:
	for (i1=0; get_char(i1); i1++);
	position=i1-1;
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
    insert = !insert;
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      standout();
      if (insert)
	mvaddstr(LINES-2, 75, "INS");
      else
	mvaddstr(LINES-2, 75, "OWR");
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
    bol();
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
    int i1;

    pos_in_history++;
    if (pos_in_history==HIST_SIZ)
	pos_in_history=0;
    clear_last_line();

    for (i1 = 0; history[pos_in_history][i1]; i1++) 
	set_char(i1, history[pos_in_history][i1]);

    set_position(0, FROM_START);
}

previous_in_history()
{
    int i1;

    pos_in_history--;
    if (pos_in_history<0)
	pos_in_history=HIST_SIZ-1;
    clear_last_line();
    for (i1=0; history[pos_in_history][i1]; i1++) 
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
    int i1, i2, i3;
    
    i1=get_position();
    i2=0;
    while (get_yank_char(i2))
	i2++;
    
    for(i3=BUFSIZ-1; i3>=i1+i2; i3--)
	set_char(i3, get_char(i3-i2));
    for(i3=0; i3<i2; i3++)
	set_char(i1+i3, get_yank_char(i3));
}

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
      for(i3=0; i3<78; i3++)
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
      for(i3=0; i3<78; i3++)
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
    static int place[]={0,55,110,165,220};

    if (paikka>4 || paikka<0)
	return 0;
    return place[paikka];
}

#ifdef GETPASS
/*
** getmypass -        read password from /dev/tty
**            backspace - space - backspace must make sense
**            environment is preserved across interrupts
**            ag 10/86
**            rev ag 7/90 (adapted for Internet Relay Chat)
*/

static void (*oldsig[NSIG])();        /* save signal handlers */
static struct sgttyb ttyb;    /* ioctl needs this */
static int flags;             /* save old tty settings */
static FILE *fi;

static reset()
{
  register int i;
  
  ioctl(fileno(fi), TIOCFLUSH);
  putc('\r', stderr);
  for (i = 1; i < COLS; i++)
    putc(' ', stderr);
  putc('\r', stderr);
  ttyb.sg_flags = flags;
  ioctl(fileno(fi), TIOCSETP, &ttyb);
  if (fi != stdin)
    fclose(fi);
  for (i=1; i < NSIG; i++)
    if (i != SIGKILL)
      signal(i, oldsig[i]);
}

static interrupt(sig)
     int sig;
{
  if (oldsig[sig] != SIG_IGN)
    {
      reset();
      kill(getpid(), sig);
    }
}

char *getmypass(prompt)
     char *prompt;
{
  static char pbuf[PASSWDLEN + NICKLEN + 1];
  register int p, c;
  char lbuf[PASSWDLEN + NICKLEN + 1];
  
  if ((fi = fopen("/dev/tty", "r")) == NULL)
    fi = stdin;
  else
    setbuf(fi, (char *)NULL);
  
  ioctl(fileno(fi), TIOCGETP, &ttyb);
  
  for (c=1; c < NSIG; c++)
    if (c != SIGKILL)
      oldsig[c] = signal(c, interrupt);
  
  flags = ttyb.sg_flags;
  ttyb.sg_flags &= ~ECHO;
  ttyb.sg_flags |= CBREAK;
  
  ioctl(fileno(fi), TIOCSETP, &ttyb);
  ioctl(fileno(fi), TIOCFLUSH);
  putc('\r', stderr);
  fputs(prompt, stderr);
  srand(time(0));
  for (p=0; p <PASSWDLEN + NICKLEN;)
    {
      switch (c = getc(fi)){
      case EOF:
      case '\0':
      case '\004':
      case '\n':
	break;
      default:
	if (c == ttyb.sg_erase)
	  {
	    if (p)
	      for (--p, c=0; c<lbuf[p];++c)
		fputs("\b \b", stderr);
	    continue;
	  }
	if (c == ttyb.sg_kill)
	  {
	    putc('\r', stderr);
	    for (c = 1; c < COLS; c++)
	      putc(' ', stderr);
	    putc('\r', stderr);
	    fputs(prompt, stderr);
	    p = 0;
	    continue;
	  }
	pbuf[p] = c;
	for (lbuf[p] = c = 1 + rand() % 2; c--;)
	  putc(' '+1+rand()%('~'-' '), stderr);
	++p;
	continue;
      }
      break;
    }
  pbuf[p] = '\0';
  reset();
  return pbuf;
}
#endif
