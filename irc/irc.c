/************************************************************************
 *   IRC - Internet Relay Chat, irc/irc.c
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

/* -- Jto -- 20 Jun 1990
 * Changed debuglevel to have a default value
 */

/* -- Jto -- 03 Jun 1990
 * Channel string changes...
 */

/* -- Jto -- 24 May 1990
 * VMS version changes from LadyBug (viljanen@cs.helsinki.fi)
 */

char irc_id[]="irc.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#define DEPTH 10
#define KILLMAX 10  /* Number of kills to accept to really die */
                    /* this is to prevent looping with /unkill */
#include "struct.h"
#include <sys/types.h>
#if VMS
#include stdlib
#if MAIL50
#include "maildef.h"

struct itmlst
{
  short buffer_length;
  short item_code;
  long buffer_address;
  long return_length_address;
};
#endif
#endif
#include <stdio.h>

#ifdef AUTOMATON
#ifdef DOCURSES
#undef DOCURSES
#endif
#ifdef DOTERMCAP
#undef DOTERMCAP
#endif
char *a_myname();
char *a_myreal();
char *a_myuser();
#endif /* AUTOMATON */

#ifdef DOCURSES
#include <curses.h>
#endif
#include <signal.h>

#include "common.h"
#include "msg.h"
#include "sys.h"
#include "irc.h"

extern char *HEADER;
int timeout();
char buf[BUFSIZE];
aChannel *channel = (aChannel *) 0;
static char currserver[HOSTLEN + 1];
char *mycncmp(), *real_name(), *last_to_me(), *last_from_me();
aClient me;
aClient *client = &me;
anUser meUser; /* User block for 'me' --msa */
int portnum, termtype = CURSES_TERM;
int debuglevel = DEBUG_ERROR;
FILE *logfile = (FILE *)0;
int intr();
int unkill_flag = 0, cchannel = 0;
static int apu = 0;  /* Line number we're currently on screen */
static int sock;     /* Server socket fd */
int QuitFlag = 0;
static int KillCount = 0;

#if HPUX
char logbuf[BUFSIZ]; 
#endif

#if VMS
struct spasswd {
  char *pw_name;
  char *passwd;
  int pw_uid;
  int pw_gid;
  char *pw_comment;
  char *pw_gecos;
  char *pw_dir;
  char *pw_shell;
} temppassword = {
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL,
  NULL
};

char username[L_cuserid];
#if MAIL50
int ucontext = 0, perslength;
char persname[81];

struct itmlst
  null_list[] = {{0,0,0,0}},
  gplist[] = {{80, MAIL$_USER_PERSONAL_NAME,
	       &persname, &perslength}};
#endif
#endif

main(argc, argv)
     int argc;
     char *argv[];
{
  int channel = 0;
  int length;
  struct passwd *userdata;
  char *cp, *argv0=argv[0], *nickptr, *servptr, *getenv(), ch;
#if !VMS
  extern int exit();
#endif
  static char usage[] =
    "Usage: %s [ -c channel ] [ -p port ] [ nickname [ server ] ]\n";
  if ((cp = rindex(argv0, '/')) != NULL)
    argv0 = ++cp;
  portnum = PORTNUM;
/*  signal(SIGTSTP, SIG_IGN); */
  buf[0] = currserver[0] = '\0';
  me.user = &meUser;
  me.from = &me;
  initconf(currserver, me.passwd, me.sockhost, &meUser.channel);
  setuid(getuid());
  while (argc > 1 && argv[1][0] == '-') {
    switch(ch = argv[1][1]) {
    case 'p':
    case 'c':
      length = 0;
      if (argv[1][2] != '\0') {
	length = atoi(&argv[1][2]);
      } else {
	if (argc > 2) {
	  length = atoi(argv[2]);
	  argv++; argc--;
	}
      }
      if (ch == 'p') {
	if (length <= 0) {
	  printf(usage, argv0);
	  exit(1);
	}
	cchannel = length;
      } else {
	if (length == 0) {
	  printf(usage, argv0);
	  exit(1);
	} else
	  channel = length;
      }
      break;
#ifdef DOTERMCAP
      case 's':
	termtype = TERMCAP_TERM;
	break;
#endif
    }
    argv++; argc--;
  }
  me.buffer[0] = '\0'; me.next = NULL;
  me.status = STAT_ME;
  if (servptr = getenv("IRCSERVER"))
    strncpyzt(currserver, servptr, HOSTLEN);
  if (argc > 2)
    strncpyzt(currserver, argv[2], HOSTLEN);
  do {
    QuitFlag = 0;
    if (unkill_flag < 0)
      unkill_flag += 2;
#ifdef UPHOST
    sock = client_init(((currserver[0]) ? currserver : UPHOST),
		       (cchannel > 0) ? (int) cchannel : portnum);
#else
    sock = client_init(((currserver[0]) ? currserver : me.sockhost),
		       (cchannel > 0) ? (int) cchannel : portnum);
#endif
    if (sock < 0) {
      printf("sock < 0\n");
      exit(1);
    }
#if VMS
    userdata = &temppassword;
    cuserid(username);
    {
      int p = 0;
      
      while (username[p] != '\0') {
	username[p] = tolower(username[p]);
	p++;
      }
    }
    
    userdata->pw_name = &username[0];
    userdata->pw_gecos = &username[0];
    
#else
    userdata = getpwuid(getuid());
#endif
    if (strlen(userdata->pw_name) >= USERLEN) {
      userdata->pw_name[USERLEN-1] = '\0';
    }
    if (strlen(userdata->pw_gecos) >= REALLEN) {
      userdata->pw_gecos[REALLEN-1] = '\0';
    }
    /* FIX:    jtrim@orion.cair.du.edu -- 3/14/88 
       & jto@tolsun.oulu.fi */
    if (argc >= 2) {
      strncpy(me.name, argv[1], NICKLEN);
    } else if (nickptr = getenv("IRCNICK")) {
      strncpy(me.name, nickptr, NICKLEN);
    } else
#ifdef AUTOMATON
      strncpy(me.name,a_myname(), NICKLEN);
#else
      strncpy(me.name,userdata->pw_name,NICKLEN);
#endif

    me.name[NICKLEN] = '\0';
    /* END FIX */

    if (argv0[0] == ':') {
      strcpy(me.sockhost,"OuluBox");
      strncpy(me.info, &argv0[1], REALLEN);
      strncpy(meUser.username, argv[1], USERLEN);
    }
    else {
      if (cp = getenv("IRCNAME"))
	strncpy(me.info, cp, REALLEN);
      else if (cp = getenv("NAME"))
	strncpy(me.info, cp, REALLEN);
      else
#if (VMS && MAIL50)
	{
	  mail$user_begin(&ucontext,null_list,null_list);
	  mail$user_get_info(&ucontext,null_list,gplist);
	  mail$user_end(&ucontext,null_list,null_list);
	  
	  persname[perslength+1] = '\0';
	  
	  if ((perslength != 0) && (perslength <= REALLEN))
	    strncpy(me.info,persname,REALLEN);
	  else
	    strncpy(me.info,real_name(userdata),REALLEN);
	}
#else
      {
#ifdef AUTOMATON
	strncpy(me.info, a_myreal(), REALLEN);
#else
	strncpy(me.info,real_name(userdata),REALLEN);
#endif
	if (me.info[0] == '\0')
	  strcpy(me.info, "*real name unknown*");
      }
#endif
#ifdef AUTOMATON
      strncpy(meUser.username, a_myuser(), USERLEN);
#else
      strncpy(meUser.username,userdata->pw_name,USERLEN);
#endif
    }
    strcpy(meUser.server,me.sockhost);
    meUser.username[USERLEN] = '\0';
    me.info[REALLEN] = '\0';
    me.fd = sock;
#ifdef AUTOMATON
    a_init();
#endif
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      initscr();
      noecho();
      crmode();
      clear();
      refresh();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM) {
      printf("Io on !\n");
      io_on(1);
      clearscreen();
    }
#endif
    if (me.passwd[0])
      sendto_one(&me, "PASS %s", me.passwd);
    sendto_one(&me, "NICK %s", me.name);
    sendto_one(&me, "USER %s %s %s %s", meUser.username, me.sockhost,
	       meUser.server, me.info);
    if (channel)
      sendto_one(&me, "CHANNEL %d", channel);
/*  signal(SIGTSTP, SIG_IGN); */
    myloop(sock);
    if (logfile != (FILE *) 0)
      do_log(NULL);
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      echo();
      nocrmode();
      endwin();
    } 
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM) {
      io_off();
    }
#endif
    apu = 0;
  } while (unkill_flag && KillCount++ < KILLMAX);
  exit(0);
}

intr()
{
  if (logfile != (FILE *) 0) {
    do_log(NULL);
  }
#ifdef DOCURSES
  if (termtype == CURSES_TERM) {
    echo();
    nocrmode();
    endwin();
  }
#endif
#ifdef DOTERMCAP
  if (termtype == TERMCAP_TERM) {
    io_off();
  }
#endif
  exit(0);
}

myloop(sock)
int sock;
{
#ifdef DOCURSES
  char header[HEADERLEN];
  if (termtype == CURSES_TERM) {
    sprintf(header,HEADER,version);
    standout();
    mvaddstr(LINES - 2, 0, header);
    standend();
  }
#endif
#ifdef DOTERMCAP
  if (termtype == TERMCAP_TERM) {
    put_statusline();
  }
#endif
  client_loop(sock);
}

#define QUERYLEN 50

static char cmdch = '/';
static char queryuser[QUERYLEN+2] = "";

int
do_cmdch(ptr, temp)
char *ptr, *temp;
{
  if (BadPtr(ptr)) {
    putline("Error: Command character not changed");
    return (-1);
  }
  cmdch = *ptr;
  return (0);
}

int
do_quote(ptr, temp)
char *ptr, *temp;
{
  if (BadPtr(ptr)) {
    putline("*** Error: Empty command");
    return (-1);
  }
  sendto_one(&me,"%s", ptr);
  return (0);
}

int
do_query(ptr, temp)
char *ptr, *temp;
{
  if (BadPtr(ptr)) {
    sprintf(buf, "*** Ending a private chat with %s", queryuser);
    putline(buf);
    queryuser[0] = '\0';
  }
  else {
    strncpyzt(queryuser, ptr, QUERYLEN);
    sprintf(buf, "*** Beginning a private chat with %s", queryuser);
    putline(buf);
  }
  return (0);
}

int
do_mypriv(buf1, buf2)
char *buf1, *buf2;
{
  char *tmp = index(buf1, ' ');
  if (tmp == NULL) {
    putline("*** Error: Empty message not sent");
    return (-1);
  }
  if (buf1[0] == ',' && buf1[1] == ' ') {
    sendto_one(&me, "PRIVMSG %s %s", last_to_me((char *) 0), &buf1[2]);
    last_from_me(last_to_me((char *) 0));
    *(tmp++) = '\0';
    sprintf(buf,"-> !!*%s* %s", last_to_me((char *) 0), tmp);
    putline(buf);
  } else if (buf1[0] == '.' && buf1[1] == ' ') {
    sendto_one(&me, "PRIVMSG %s %s", last_from_me((char *) 0), &buf1[2]);
    *(tmp++) = '\0';
    sprintf(buf,"-> ##*%s* %s", last_from_me((char *) 0), tmp);
    putline(buf);
  } else {
    sendto_one(&me, "PRIVMSG %s", buf1);
    *(tmp++) = '\0';
    last_from_me(buf1);
    if (*buf1 == '#' || *buf1 == '+' || atoi(buf1))
      sprintf(buf, "%s> %s", buf1, tmp);
    else
      sprintf(buf,"->%s> %s", buf1, tmp);
    putline(buf);
  }
  return (0);
}

int
do_myqpriv(buf1, buf2)
char *buf1, *buf2;
{
  if (BadPtr(buf1)) {
    putline("*** Error: Empty message not sent");
    return (-1);
  }
  sendto_one(&me, "PRIVMSG %s :%s", queryuser, buf1);

  sprintf(buf,"-> *%s* %s", queryuser, buf1);
  putline(buf);
  return (0);
}

static char *querychannel = "0";

int
do_mytext(buf1, temp)
char *buf1, *temp;
{
  sendto_one(&me, "PRIVMSG %s :%s", querychannel, buf1);
  sprintf(buf,"%s> %s", querychannel, buf1);
  putline(buf);
  return (0);
}

int
do_unkill(buf, temp)
char *buf, *temp;
{
  if (unkill_flag)
    unkill_flag = 0;
  else
    unkill_flag = 1;
  sprintf(buf, "*** Unkill feature turned %s", (unkill_flag) ? "on" : "off");
  putline(buf);
  return (0);
}

int
do_bye(buf, tmp)
char *buf, *tmp;
{
  unkill_flag = 0;
  sendto_one(&me,"QUIT");
#if VMS
#ifdef DOCURSES
  if (termtype == CURSES_TERM) {
    echo();
    nocrmode();
    endwin();
  } 
#endif
#ifdef DOTERMCAP
  if (termtype == TERMCAP_TERM) {
    io_off();
  }
#endif
  exit(0);
#endif
  return (0);
}

int
do_server(buf, tmp)
char *buf, *tmp;
{
  strncpyzt(currserver, buf, HOSTLEN);
  unkill_flag -= 2;
  sendto_one(&me,"QUIT");
  close(sock);
  QuitFlag = 1;
  return (-1);
}

int
sendit(sock,line)
int sock;
char *line;
{
  char *ptr;
  struct Command *cmd = commands;
  KillCount = 0;
  if (line[0] != cmdch) {
    if (*queryuser) {
      do_myqpriv(line, NULL);
    }
    else {
      do_mytext(line, NULL);
    }
  }
  else {
    if (line[1] == ' ') {
      do_mytext(&line[2], NULL);
    } else {
      if (line[1]) {
	for ( ; cmd->name; cmd++) {
	  if (ptr = mycncmp(&line[1], cmd->name)) {
	    switch (cmd->type) {
	    case SERVER_CMD:
	      sendto_one(&me,"%s %s", cmd->extra, ptr);
	      break;
	    case LOCAL_FUNC:
	      return ((*cmd->func)(ptr, cmd->extra));
	      break;
	    default:
	      putline("*** Error: Data error in irc.h");
	      break;
	    }
	    break;
	  }
	}
        if (!cmd->name)
	  putline("*** Error: Unknown command");
      }
    }
  }
}

char *mycncmp(str1, str2)
char *str1, *str2;
{
  int flag = 0;
  char *s1;
  for (s1 = str1; *s1 != ' ' && *s1 && *str2; s1++, str2++) {
    if (!isascii(*s1)) return 0;
    if (islower(*s1)) *s1 = toupper(*s1);
    if (*s1 != *str2) flag = 1;
  }
  if (*s1 && *s1 != ' ' && *str2 == '\0')
    return 0;
  if (flag) return 0;
  if (*s1) return s1 + 1;
  else return s1;
}

do_clear(buf, temp)
char *buf, *temp;
{
#ifdef DOCURSES
  char header[HEADERLEN];
  if (termtype == CURSES_TERM) {
    apu = 0;
    sprintf(header,HEADER,version);
    clear();
    standout();
    mvaddstr(LINES - 2, 0, header);
    standend();
    refresh();
  }
#endif
#ifdef DOTERMCAP
  if (termtype == TERMCAP_TERM)
    clearscreen();
#endif
}

putline(line)
char *line;
{
  char *ptr, *ptr2;
#ifdef DOCURSES
  char ch='\0';
  /* int pagelen = LINES - 3; not used -Armin */
  int rmargin = COLS - 1;
#endif

  /*
   ** This is a *safe* client--filter out all possibly dangerous
   ** codes from the messages (this sets them as "_").
   */
  if (line)
    for (ptr = line; *ptr; ptr++)
      if ((*ptr < 32 && *ptr != 7 && *ptr != 9) || (*ptr > 126))
	*ptr = '_'; 

  ptr = line;
  
#ifdef DOCURSES
  if (termtype == CURSES_TERM) {
    while (ptr) {

/* first, see if we have to chop the string into pieces */

      if (strlen(ptr) > rmargin) {
	ch = ptr[rmargin];
	ptr[rmargin] = '\0';
	ptr2 = &ptr[rmargin - 1];
      } 
      else
	ptr2 = NULL;

/* move cursor to correct position and place line */

      move(apu,0);
#if VMS
      clrtoeol();
#endif
      addstr(ptr);
      if (logfile)
	fprintf(logfile, "%s\n", ptr);

/* now see if we are at the end of the page, and take action */

#ifndef SCROLLINGCLIENT

/* clear one line. */

#if VMS
      addstr("\n");
      clrtoeol();
#else
      addstr("\n\n");
#endif

      if (++apu > LINES - 4) {
	apu = 0;
	move(0,0);
	clrtoeol();
      }

#else /* doesn't work, dumps core :-( */

      if(++apu > LINES - 4) {

	char header[HEADERLEN];

/* erase status line */

	move( LINES - 2, 0 );
	clrtobot();
	refresh();

/* scroll screen */

	scrollok(stdscr,TRUE);
	move( LINES - 1, 0 );
	addstr("\n\n\n\n");
	refresh();
	apu -= 4;

/* redraw status line */

	sprintf(header,HEADER,version);
	standout();
	mvaddstr(LINES - 2, 0, header);
	standend();
	tulosta_viimeinen_rivi();

      } else
	addstr( "\n\n" );

#endif

/* finally, if this is a multiple-line line, munge things up so that
   we print the next line. overwrites the end of the previous line. */

      ptr = ptr2;
      if (ptr2) {
	*ptr2++ = '+';
	*ptr2 = ch;
      }
    }
    refresh();
  }
#endif
#ifdef DOTERMCAP
  if (termtype == TERMCAP_TERM)
    tcap_putline(line);
#endif
#ifdef AUTOMATON
  puts(line);
#endif
}

int
unixuser()
{
#if VMS
  return 1;
#else
  return(!StrEq(me.sockhost,"OuluBox"));
#endif
}

aClient *
make_client()
{
  return(NULL);
}


do_log(ptr, temp)
char *ptr, *temp;
{
  long tloc;
  char buf[150];
  char *ptr2;

#if VMS
#define LOGFILEOPT  ,"rat=cr", "rfm=var"
#else
#define LOGFILEOPT 
#endif

  if (!unixuser())
    return -1;
  if (!logfile) {		          /* logging currently off */
    if (BadPtr(ptr))
      putline("*** You must specify a filename to start logging.");
    else {
      if (!(logfile = fopen(ptr, "a" LOGFILEOPT))) {
	sprintf(buf, "*** Error: Couldn't open log file %s.\n", ptr);
	putline(buf);
      } else {
#if !VMS
#if HPUX
	setvbuf(logfile, logbuf, _IOLBF, sizeof(logbuf));
#else
        setlinebuf(logfile);
#endif
#endif
	time(&tloc);
	sprintf(buf, "*** IRC session log started at %s", ctime(&tloc));
	ptr2 = rindex(buf, '\n');
	*ptr2 = '.';
	putline(buf);
      }
    }
  } else {                            /* logging currently on */
#if VMS
    if (ptr == NULL) { /* vax 'c' hates the next line.. */
#else
    if (BadPtr(ptr)) {
#endif
      time(&tloc);
      sprintf(buf, "*** IRC session log ended at %s", ctime(&tloc));
      ptr2 = rindex(buf, '\n');
      *ptr2 = '.';
      putline(buf);
      fclose(logfile);
      logfile = (FILE *)0;
    } else
      putline("*** Your session is already being logged.");
  }
}

/* remember the commas */
#define LASTKEEPLEN (MAXRECIPIENTS * (NICKLEN+1)) 

char *
last_to_me(sender)
char *sender;
{
  static char name[LASTKEEPLEN+1] = ",";

  if (sender)
    strncpyzt(name, sender, sizeof(name));

  return (name);
}

char *
last_from_me(recipient)
char *recipient;
{
  static char name[LASTKEEPLEN+1] = ".";

  if (recipient)
    strncpyzt(name, recipient, sizeof(name));

  return (name);
}

/*
 * Left out until it works with VMS as well..
 */

#ifdef GETPASS
do_oper(ptr, xtra)
char *ptr, *xtra;
{
      extern char *getmypass();

      if (BadPtr(ptr))
	ptr = getmypass("Enter nick & password: ");

      sendto_one(&me, "%s %s", xtra, ptr);
}
#endif

/* Fake routine (it's only in server...) */
int IsMember() { return 0; }

do_channel(ptr, xtra)
char *ptr, *xtra;
{
    if (BadPtr(ptr))
	{
	putline("*** Which channel do you want to join?");
	return;
	}

    if (querychannel)
	free((char *)querychannel);

    if (querychannel = (char *)malloc(strlen(ptr) + 1))
	strcpy(querychannel, ptr);
    else
	querychannel = me.name; /* kludge */

    sendto_one(&me, "%s %s", xtra, ptr);
}
