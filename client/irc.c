/*************************************************************************
 ** irc.c  Beta v2.0    (22 Aug 1988)
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

char irc_id[]="irc.c v2.0 (c) 1988 University of Oulu, Computing Center";


#define DEPTH 10
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

#include "msg.h"
#include "sys.h"
#include "irc.h"


char *makeclientbuf();

int timeout();
char buf[BUFSIZE];
char *mycncmp(), *real_name(), *last_to_me(), *last_from_me();
struct Client me;
struct Client *client = &me;
int portnum, termtype = CURSES_TERM;
int debuglevel;
FILE *logfile = (FILE *)0;
int intr();

#ifdef HPUX
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
  null_lst[] = {{0,0,0,0}},
  gplist[] = {{80, MAIL$_USER_PERSONAL_NAME,
	       &persname, &perslength}};
#endif
#endif

main(argc, argv)
     int argc;
     char *argv[];
{
  int sock, length, channel = 0;
  struct passwd *userdata;
  char *cp, *argv0=argv[0], *nickptr, *servptr, *getenv(), ch;
#ifndef VMS
  extern int exit();
#endif
  static char usage[] =
    "Usage: %s [ -c channel ] [ -p port ] [ nickname [ server ] ]\n";
  if ((cp = rindex(argv0, '/')) != NULL)
    argv0 = ++cp;
  portnum = PORTNUM;
/*  signal(SIGTSTP, SIG_IGN); */
  buf[0] = '\0';
  initconf(buf, me.passwd, me.host, &me.channel);
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
	me.channel = length;
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
  if (servptr = getenv("IRCSERVER")) {
    strncpy(buf, servptr, HOSTLEN);
    buf[HOSTLEN] = '\0';
  }
#ifdef UPHOST
  sock = client_init((argc > 2) ? argv[2] : ((buf[0]) ? buf : UPHOST),
		     (me.channel > 0) ? me.channel : portnum);
#else
  sock = client_init((argc > 2) ? argv[2] : ((buf[0]) ? buf : me.host),
		     (me.channel > 0) ? me.channel : portnum);
#endif
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
    strncpy(me.nickname, argv[1], NICKLEN);
  } else if (nickptr = getenv("IRCNICK")) {
    strncpy(me.nickname, nickptr, NICKLEN);
  } else
#ifdef AUTOMATON
    strncpy(me.nickname,a_myname(), NICKLEN);
#else
    strncpy(me.nickname,userdata->pw_name,NICKLEN);
#endif

  me.nickname[NICKLEN] = '\0';
  /* END FIX */

  if (argv0[0] == ':') {
    strcpy(me.host,"OuluBox");
    strncpy(me.realname, &argv0[1], REALLEN);
    strncpy(me.username, argv[1], USERLEN);
  }
  else {
    if (cp = getenv("IRCNAME"))
      strncpy(me.realname, cp, REALLEN);
    else if (cp = getenv("NAME"))
      strncpy(me.realname, cp, REALLEN);
    else
#if (VMS && MAIL50)
      {
	mail$user_begin(&ucontext,null_list,null_list);
	mail$user_get_info(&ucontext,null_list,gplist);
	mail$user_end(&ucontext,null_list,null_list);

	persname[perslength+1] = '\0';

	if ((perslength != 0) && (perslength <= REALLEN))
	  strncpy(me.realname,persname,REALLEN);
	else
	  strncpy(me.realname,real_name(userdata),REALLEN);
      }
#else
    {
#ifdef AUTOMATON
      strncpy(me.realname, a_myreal(), REALLEN);
#else
      strncpy(me.realname,real_name(userdata),REALLEN);
#endif
      if (me.realname[0] == '\0')
	strcpy(me.realname, "*real name unknown*");
    }
#endif
#ifdef AUTOMATON
    strncpy(me.username, a_myuser(), USERLEN);
#else
    strncpy(me.username,userdata->pw_name,USERLEN);
#endif
  }
  strcpy(me.server,me.host);
  me.username[USERLEN] = '\0';
  me.realname[REALLEN] = '\0';
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
  sendto_one(&me, "NICK %s", me.nickname);
  sendto_one(&me, "USER %s %s %s %s", me.username, me.host,
	     me.server, me.realname);
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

static char cmdch = '/';
static char queryuser[NICKLEN+2] = "";

do_cmdch(ptr, temp)
char *ptr, *temp;
{
  if (ptr == NULL || *ptr == '\0') {
    putline("Error: Command character not changed");
    return (-1);
  }
  cmdch = *ptr;
  return (0);
}

do_quote(ptr, temp)
char *ptr, *temp;
{
  if (ptr == NULL || *ptr == '\0') {
    putline("*** Error: Empty command");
    return (-1);
  }
  return (sendto_one(&me,"%s", ptr));
}

do_query(ptr, temp)
char *ptr, *temp;
{
  if (ptr == NULL || *ptr == '\0') {
    sprintf(buf, "*** Ending a private chat with %s", queryuser);
    putline(buf);
    queryuser[0] = '\0';
  }
  else {
    strncpy(queryuser, ptr, NICKLEN);
    queryuser[NICKLEN] = '\0';
    sprintf(buf, "*** Beginning a private chat with %s", queryuser);
    putline(buf);
  }
  return (0);
}

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
    sprintf(buf,"-> *%s* %s", buf1, tmp);
    putline(buf);
  }

}

do_myqpriv(buf1, buf2)
char *buf1, *buf2;
{
  if (buf1 == NULL || *buf1 == '\0') {
    putline("*** Error: Empty message not sent");
    return (-1);
  }
  sendto_one(&me, "PRIVMSG %s :%s", queryuser, buf1);

  sprintf(buf,"-> *%s* %s", queryuser, buf1);
  putline(buf);
}

do_mytext(buf1, temp)
char *buf1, *temp;
{
  sendto_one(&me, "MSG :%s", buf1);
  sprintf(buf,"> %s",buf1);
  putline(buf);
}

sendit(sock,line)
int sock;
char *line;
{
  char *ptr;
  struct Command *cmd = commands;
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
	while (cmd->name) {
	  if (ptr = mycncmp(&line[1], cmd->name)) {
	    switch (cmd->type) {
	    case SERVER_CMD:
	      sendto_one(&me,"%s %s", cmd->extra, ptr);
	      break;
	    case LOCAL_FUNC:
	      (*cmd->func)(ptr, cmd->extra);
	      break;
	    default:
	      putline("*** Error: Data error in irc.h");
	      break;
	    }
	    break;
	  }
	  cmd++;
	}
        if (cmd->name == NULL)
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

static int apu = 0;

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
#ifdef DOCURSES
  char *ptr = line, *ptr2;
  char ch='\0';
  static char blanko[] = "                                        ";
  if (termtype == CURSES_TERM) {
    while (ptr) {
#if VMS
      if (strlen(ptr) > COLS) {
	ch = ptr[COLS];
	ptr[COLS] = '\0';
	ptr2 = &ptr[COLS-1];
      } 
#else
      if (strlen(ptr) > COLS - 1) {
	ch = ptr[COLS - 1];
	ptr[COLS - 1] = '\0';
	ptr2 = &ptr[COLS - 2];
      } 
#endif
      else
	ptr2 = NULL;
      move(apu++,0);
      if (apu > LINES - 4) apu = 0;
      addstr(ptr);
#if VMS
      addstr("\n");
#endif
      if (logfile != (FILE *) 0)
	fprintf(logfile, "%s\n", ptr);
      if (apu == 0) {
	move(0,0);
	clrtoeol();
	addstr("\n");
	clrtoeol();
      }
      else if (apu == LINES - 4) {
	mvaddstr(LINES - 4, 0, "\n");
	move(0,0);
	clrtoeol();
	addstr("\n");
	clrtoeol();
      }
      else {
	addstr("\n");
	clrtoeol();
	addstr("\n");

/*	addstr(blanko); addstr(blanko);
	addstr("\n");
	addstr(blanko); addstr(blanko);
 */
      }
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
  return(strcmp(me.host,"OuluBox"));
#endif
}

struct Client *
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

  if (!unixuser())
    return -1;
  if (logfile == (FILE *)0) {         /* logging currently off */
    if (ptr == NULL || *ptr == '\0')
      putline("*** You must specify a filename to start logging.");
    else {
#if VMS
      if ((logfile = fopen(ptr, "a", "rat=cr", "rfm=var")) == (FILE *) 0) {
#else
      if ((logfile = fopen(ptr, "a")) == (FILE *)0) {
#endif
	sprintf(buf, "*** Error: Couldn't open log file %s.\n", ptr);
	putline(buf);
      } else {
#ifndef VMS
#ifdef HPUX
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
    if (ptr == NULL || *ptr == '\0') {
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

char *
last_to_me(sender)
char *sender;
{
  static char name[NICKLEN + 1] = ",";

  if (sender) {
    strncpy(name, sender, NICKLEN);
    name[NICKLEN] = '\0';
  }

  return (name);
}

char *
last_from_me(recipient)
char *recipient;
{
  static char name[NICKLEN + 1] = ".";

  if (recipient) {
    strncpy(name, recipient, NICKLEN);
    name[NICKLEN] = '\0';
  }

  return (name);
}
