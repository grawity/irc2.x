/************************************************************************
 *   IRC - Internet Relay Chat, irc/irc.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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
extern int do_unkill(), do_bye(), do_server();

struct Command commands[] = {
  { (int (*)()) 0, "SIGNOFF", SERVER_CMD, "\0\0", "QUIT" },
  { do_bye,        "QUIT",    LOCAL_FUNC, "\0\0", "QUIT" },
  { do_bye,        "EXIT",    LOCAL_FUNC, "\0\0", "QUIT" },
  { do_bye,        "BYE",     LOCAL_FUNC, "\0\0", "QUIT" },
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
  { (int (*)()) 0, "WHOWAS",  SERVER_CMD, "\0\0", "WHOWAS" },
  { (int (*)()) 0, "JOIN",    SERVER_CMD, "\0\0", "CHANNEL" },
  { (int (*)()) 0, "WALL",    SERVER_CMD, "\0\0", "WALL" },
  { (int (*)()) 0, "WOPS",    SERVER_CMD, "\0\0", "WALLOPS" },
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
  { do_unkill,     "UNKILL",  LOCAL_FUNC, "\0\0", "UNKILL" },
  { do_server,     "SERVER",  LOCAL_FUNC, "\0\0", "SERVER" },
  { (int (*)()) 0, (char *) 0, 0,         "\0\0", (char *) 0 }
};

