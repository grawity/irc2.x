/*************************************************************************
 ** card.c  Beta v2.0    (22 Aug 1988)
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

char card_id[]="card.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include "sys.h"

#define MAIN
#define DEPTH 10
#include "struct.h"
#include "msg.h"
#undef MAIN

#define NICKNAME "Bartender"
#define USERNAME "ron"
#define REALNAME "Ron the Bartender"

typedef struct command {
  char *name;
  int  minlen;
} aCommand;

char *makeclientbuf();

int timeout();
char buf[BUFSIZE];
char *mycncmp(), *real_name();
aClient me;
aClient *client = &me;
int portnum;
int debuglevel;

main(argc, argv)
     int argc;
     char *argv[];
{
  int sock,length, streamfd, msgsock, i;
  struct passwd *userdata;
  char *ch, *argv0=argv[0], *nickptr, *servptr, *getenv();
  extern int exit();
  portnum = PORTNUM;
/*  signal(SIGTSTP, SIG_IGN); */
  buf[0] = '\0';
  initconf(buf, me.passwd, me.host, &me.channel);
  while (argc > 1 && argv[1][0] == '-') {
    switch(argv[1][1]) {
      case 'p':
        if ((length = atoi(&argv[1][2])) > 0)
          portnum = length;
        break;
    }
    argv++; argc--;
  }
  me.buffer[0] = '\0'; me.next = NULL;
  me.status = STAT_ME;
  if (servptr = getenv("IRCSERVER")) {
    strncpy(buf, servptr, HOSTLEN);
    buf[HOSTLEN] = '\0';
  }
  sock = client_init((argc > 2) ? argv[2] : ((buf[0]) ? buf : me.host),
		     (me.channel > 0) ? me.channel : portnum);
  userdata = getpwuid(getuid());
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
    strncpy(me.nickname,NICKNAME,NICKLEN);

  me.nickname[NICKLEN] = '\0';
  /* END FIX */

  if (argv0[0] == ':') {
    strcpy(me.host,"OuluBox");
    strncpy(me.realname, &argv0[1], REALLEN);
    strncpy(me.username, argv[1], USERLEN);
  }
  else {
    strncpy(me.realname,REALNAME,REALLEN);
    strncpy(me.username,USERNAME,USERLEN);
  }
  strcpy(me.server,me.host);
  me.username[USERLEN] = '\0';
  me.realname[REALLEN] = '\0';
  me.fd = sock;
  if (me.passwd[0])
    sendto_one(&me, "PASS %s", me.passwd);
  sendto_one(&me, "NICK %s", me.nickname);
  sendto_one(&me, "USER %s %s %s %s", me.username, me.host,
	     me.server, me.realname);
  myloop(sock,streamfd);
}

myloop(sock,fd)
int sock,fd;
{
  client_loop(sock);
}

/*
sendit(sock,line)
int sock;
char *line;
{
  static char queryuser[NICKLEN+2];
  static char cmdch = '/';
  int i;
  char *ptr, *ptr2;
  if (line[0] != cmdch) {
    if (*queryuser) {
      m_myprivate(NULL, NULL, NULL, queryuser, line);
      sendto_one(&me, "PRIVMSG %s :%s", queryuser, line);
    }
    else {
      sendto_one(&me, "MSG :%s", line);
      m_mytext(NULL, NULL, NULL, line);
    }
  }
  else {
    if (mycncmp(&line[1],"SIGNOFF", 1))
      sendto_one(&me, "QUIT");
    else if (mycncmp(&line[1],"BYE", 1))
      sendto_one(&me, "QUIT");
    else if (mycncmp(&line[1],"EXIT", 1))
      sendto_one(&me, "QUIT");
    else if (mycncmp(&line[1],"QUIT", 1))
      sendto_one(&me, "QUIT");
    else if (ptr=mycncmp(&line[1],"KILL", 2)) {
      if (unixuser())
	sendto_one(&me, "KILL %s", ptr);
      else
	putline("`tis is no game for mere mortal souls...");
    }
    else if (ptr=mycncmp(&line[1],"SUMMON", 2)) {
	sendto_one(&me, "SUMMON %s", ptr);
    }
    else if (ptr=mycncmp(&line[1],"STATS", 2)) 
      sendto_one(&me, "STATS %s", ptr);
    else if (ptr=mycncmp(&line[1],"USERS", 1)) 
      sendto_one(&me, "USERS %s", ptr);
    else if (ptr=mycncmp(&line[1],"TIME", 1))
      sendto_one(&me, "TIME %s", ptr);
    else if (ptr=mycncmp(&line[1],"DATE", 1))
      sendto_one(&me, "TIME %s", ptr);
    else if (ptr=mycncmp(&line[1],"NICK", 1)) 
      sendto_one(&me, "NICK %s", ptr);
    else if (ptr=mycncmp(&line[1],"WHOIS", 4)) 
      sendto_one(&me, "WHOIS %s", ptr);
    else if (ptr=mycncmp(&line[1],"WHO", 1)) 
      sendto_one(&me, "WHO %s", ptr);
    else if (ptr=mycncmp(&line[1],"JOIN", 1))
      sendto_one(&me, "CHANNEL %s", ptr);
    else if (ptr=mycncmp(&line[1],"WALL", 2))
      sendto_one(&me, "WALL %s", ptr);
    else if (ptr=mycncmp(&line[1],"CHANNEL", 1))
      sendto_one(&me, "CHANNEL %s", ptr);
    else if (ptr=mycncmp(&line[1],"AWAY", 1))
      sendto_one(&me, "AWAY %s", ptr);
    else if (ptr=mycncmp(&line[1],"MSG", 1)) {
      if ((ptr2 = index(ptr, ' ')) == NULL)
	putline("ERROR: No message");
      else {
	*ptr2 = '\0';
	sendto_one(&me, "PRIVMSG %s :%s", ptr, ++ptr2);
	m_myprivate(NULL, NULL, NULL, ptr, ptr2);
      }
    }
    else if (ptr=mycncmp(&line[1],"TOPIC", 1))
      sendto_one(&me, "TOPIC :%s", ptr);
    else if (ptr=mycncmp(&line[1],"CMDCH", 2)) {
      if (ptr && *ptr) {
	sprintf(buf, "*** Command character changed from '%c' to '%c'",
		cmdch, *ptr);
	cmdch = *ptr;
	putline(buf);
      } else {
	putline("*** Error: Command character not changed");
      }
    }
    else if (ptr=mycncmp(&line[1],"INVITE", 2))
      sendto_one(&me, "INVITE %s", ptr);
    else if (ptr=mycncmp(&line[1],"INFO", 2))
      sendto_one(&me, "INFO");
    else if (ptr=mycncmp(&line[1],"LIST", 1))
      sendto_one(&me, "LIST %s",ptr);
    else if (ptr=mycncmp(&line[1],"KILL", 1))
      sendto_one(&me, "KILL %s",ptr);
    else if (ptr=mycncmp(&line[1],"OPER", 1))
      sendto_one(&me, "OPER %s",ptr);
    else if (ptr=mycncmp(&line[1],"QUOTE", 1))
      sendto_one(&me, "%s",ptr);
    else if (ptr=mycncmp(&line[1],"LINKS", 2)) 
      sendto_one(&me, "LINKS %s", ptr);
    else if (ptr=mycncmp(&line[1],"HELP", 1))
      help(ptr);
    else if (mycncmp(&line[1],"VERSION", 1))
      sendto_one(&me, "VERSION");
    else if (mycncmp(&line[1],"CLEAR", 1))
      doclear();
    else if (mycncmp(&line[1],"REHASH", 1))
      sendto_one(&me, "REHASH");
    else if (ptr=mycncmp(&line[1],"QUERY", 2)) {
      if (ptr == NULL || *ptr == '\0') {
	sprintf(buf,"NOTE: Finished chatting with %s",queryuser);
	putline(buf);
        queryuser[0] = '\0';
      }
      else {
	strncpy(queryuser,ptr,NICKLEN);
	queryuser[NICKLEN] = '\0';
	if (ptr = index(queryuser,' ')) *ptr = '\0';
	sprintf(buf,"NOTE: Beginning private chat with %s",queryuser);
	putline(buf);
      }
    }
    else
      putline("* Illegal Command *");
  } 
} */

char *mycncmp(str1, str2, len)
char *str1, *str2;
int len;
{
  int flag = 0;
  char *s1;
  for (s1 = str1; *s1 != ' ' && *s1 && *str2; s1++, str2++) {
    if (!isascii(*s1)) return 0;
    if (islower(*s1)) *s1 = toupper(*s1);
    if (*s1 != *str2) flag = 1;
  }
  if (flag) return 0;
  if (len != 0 && s1 - str1 < len)
    return 0;
  if (*s1) return s1 + 1;
  else return s1;
}
/*
static int apu = 0;

doclear()
{
  char header[HEADERLEN];
  apu = 0;
  sprintf(header,HEADER,version);
  clear();
  standout();
  mvaddstr(LINES - 2, 0, header);
  standend();
  refresh();
}

putline(line)
char *line;
{
  char *ptr = line, *ptr2 = NULL;
  char ch='\0';
  static char blanko[] = "                                        ";
  while (ptr) {
    if (strlen(ptr) > COLS) {
      ch = ptr[COLS];
      ptr[COLS] = '\0';
      ptr2 = &ptr[COLS-1];
    } 
    else
      ptr2 = NULL;
    move(apu++,0);
    if (apu > LINES - 4) apu = 0;
    addstr(ptr);
    if (apu == 0) 
      mvaddstr(0,0,"\n\n");
    else if (apu == LINES - 4) {
      mvaddstr(LINES - 4, 0, "\n");
      mvaddstr(0,0,"\n");
    }
    else {
      addstr(blanko); addstr(blanko);
      addstr("\n");
      addstr(blanko); addstr(blanko);
    }
    ptr = ptr2;
    if (ptr2) {
      *ptr2++ = '+';
      *ptr2 = ch;
    }
  }
  refresh();
}
*/
int
unixuser()
{
  return(strcmp(me.host,"OuluBox"));
}

aClient *
make_client()
{
  return(NULL);
}

putline(str)
char *str;
{
  printf("%s\n",str);
}
