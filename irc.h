/*************************************************************************
 ** irc.h  Beta v2.0    (17 Jul 1989)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1989 Jarkko Oikarinen
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 **
 *************************************************************************/

struct Command {
  int (*func)();
  char *name;
  int type;
  char keybinding[3];
  char *extra;   /* Normally contains the command to send to irc daemon */
};

#define SERVER_CMD   0
#define LOCAL_FUNC  1

extern int do_mypriv(), do_cmdch(), do_quote(), do_query();
extern int do_ignore(), do_help(), do_log(), do_clear();

struct Command commands[] = {
  { (int (*)()) 0, "SIGNOFF", SERVER_CMD, "\0\0", "QUIT" },
  { (int (*)()) 0, "QUIT",    SERVER_CMD, "\0\0", "QUIT" },
  { (int (*)()) 0, "EXIT",    SERVER_CMD, "\0\0", "QUIT" },
  { (int (*)()) 0, "BYE",     SERVER_CMD, "\0\0", "QUIT" },
  { (int (*)()) 0, "KILL",    SERVER_CMD, "\0\0", "KILL" },
  { (int (*)()) 0, "SUMMON",  SERVER_CMD, "\0\0", "SUMMON" },
  { (int (*)()) 0, "STATS",   SERVER_CMD, "\0\0", "STATS" },
  { (int (*)()) 0, "USERS",   SERVER_CMD, "\0\0", "USERS" },
  { (int (*)()) 0, "TIME",    SERVER_CMD, "\0\0", "TIME" },
  { (int (*)()) 0, "DATE",    SERVER_CMD, "\0\0", "TIME" },
  { (int (*)()) 0, "NAMES",   SERVER_CMD, "\0\0", "NAMES" },
  { (int (*)()) 0, "NICK",    SERVER_CMD, "\0\0", "NICK" },
  { (int (*)()) 0, "WHO",     SERVER_CMD, "\0\0", "WHO" },
  { (int (*)()) 0, "WHOIS",   SERVER_CMD, "\0\0", "WHOIS" },
  { (int (*)()) 0, "JOIN",    SERVER_CMD, "\0\0", "CHANNEL" },
  { (int (*)()) 0, "WALL",    SERVER_CMD, "\0\0", "WALL" },
  { (int (*)()) 0, "CHANNEL", SERVER_CMD, "\0\0", "CHANNEL" },
  { (int (*)()) 0, "AWAY",    SERVER_CMD, "\0\0", "AWAY" },
  { do_mypriv,     "MSG",     LOCAL_FUNC, "\0\0", "PRIVMSG" },
  { (int (*)()) 0, "TOPIC",   SERVER_CMD, "\0\0", "TOPIC" },
  { do_cmdch,      "CMDCH",   LOCAL_FUNC, "\0\0", "CMDCH" },
  { (int (*)()) 0, "INVITE",  SERVER_CMD, "\0\0", "INVITE" },
  { (int (*)()) 0, "INFO",    SERVER_CMD, "\0\0", "INFO" },
  { (int (*)()) 0, "LIST",    SERVER_CMD, "\0\0", "LIST" },
  { (int (*)()) 0, "KILL",    SERVER_CMD, "\0\0", "KILL" },
  { (int (*)()) 0, "OPER",    SERVER_CMD, "\0\0", "OPER" },
  { do_quote,      "QUOTE",   LOCAL_FUNC, "\0\0", "QUOTE" },
  { (int (*)()) 0, "LINKS",   SERVER_CMD, "\0\0", "LINKS" },
  { (int (*)()) 0, "ADMIN",   SERVER_CMD, "\0\0", "ADMIN" },
  { do_ignore,     "IGNORE",  LOCAL_FUNC, "\0\0", "IGNORE" },
  { (int (*)()) 0, "TRACE",   SERVER_CMD, "\0\0", "TRACE" },
  { do_help,       "HELP",    LOCAL_FUNC, "\0\0", "HELP" },
  { do_log,        "LOG",     LOCAL_FUNC, "\0\0", "LOG" },
  { (int (*)()) 0, "VERSION", SERVER_CMD, "\0\0", "VERSION" },
  { do_clear,      "CLEAR",   LOCAL_FUNC, "\0\0", "CLEAR" },
  { (int (*)()) 0, "REHASH",  SERVER_CMD, "\0\0", "REHASH" },
  { do_query,      "QUERY",   LOCAL_FUNC, "\0\0", "QUERY" },
  { (int (*)()) 0, "LUSERS",  SERVER_CMD, "\0\0", "LUSERS" },
  { (int (*)()) 0, "MOTD",    SERVER_CMD, "\0\0", "MOTD" },
  { (int (*)()) 0, "WOPER",    SERVER_CMD, "\0\0","WOPER" },
  { (int (*)()) 0, (char *) 0, 0,         "\0\0", (char *) 0 }
};

