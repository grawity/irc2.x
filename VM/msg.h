/************************************************************************
 *   IRC - Internet Relay Chat, include/msg.h
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

#define MSG_PRIVATE  "PRIVMSG"
#define MSG_WHO      "WHO"
#define MSG_WHOIS    "WHOIS"
#define MSG_WHOWAS   "WHOWAS"
#define MSG_USER     "USER"
#define MSG_NICK     "NICK"
#define MSG_SERVER   "SERVER"
#define MSG_LIST     "LIST"
#define MSG_TOPIC    "TOPIC"
#define MSG_INVITE   "INVITE"
#define MSG_VERSION  "VERSION"
#define MSG_QUIT     "QUIT"
#define MSG_SQUIT    "SQUIT"
#define MSG_KILL     "KILL"
#define MSG_INFO     "INFO"
#define MSG_LINKS    "LINKS"
#define MSG_SUMMON   "SUMMON"
#define MSG_STATS    "STATS"
#define MSG_USERS    "USERS"
#define MSG_RESTART  "RESTART"
#define MSG_HELP     "HELP"
#define MSG_WHOREPLY "WHOREPLY"
#define MSG_ERROR    "ERROR"
#define MSG_AWAY     "AWAY"
#define MSG_DIE      "DIE"
#define MSG_CONNECT  "CONNECT"
#define MSG_PING     "PING"
#define MSG_PONG     "PONG"
#define MSG_OPER     "OPER"
#define MSG_PASS     "PASS"
#define MSG_WALL     "WALL"
#define MSG_WALLOPS  "WALLOPS"
#define MSG_TIME     "TIME"
#define MSG_REHASH   "REHASH"
#define MSG_NAMES    "NAMES"
#define MSG_NAMREPLY "NAMREPLY"
#define MSG_ADMIN    "ADMIN"
#define MSG_TRACE    "TRACE"
#define MSG_LINREPLY "LINREPLY"
#define MSG_NOTICE   "NOTICE"
#define MSG_JOIN     "JOIN"
#define MSG_CHANNEL  "CHANNEL"
#define MSG_PART     "PART"
#define MSG_LUSERS   "LUSERS"
#define MSG_MOTD     "MOTD"
#define MSG_MODE     "MODE"
#define MSG_KICK     "KICK"
#define MSG_SERVICE  "SERVICE"
#define MSG_SVCQUERY "SVCQUERY"
#define MSG_SVCREPLY "SVCREPLY"
#define MSG_USERHOST "USERHOST"
#define MSG_ISON     "ISON"
#define MSG_DEOP     "DEOP"
#ifdef DEBUGMODE
#ifndef CLIENT_COMPILE
#define	MSG_HASH     "HASH"
#endif
#endif

#define MAXPARA    15 

extern int m_private(), m_who(), m_whois(), m_user(), m_list();
extern int m_topic(), m_invite(), m_version(), m_quit();
extern int m_server(), m_kill(), m_info(), m_links(), m_summon(), m_stats();
extern int m_users(), m_nick(), m_error(), m_help(), m_whoreply();
extern int m_squit(), m_restart(), m_away(), m_die(), m_connect();
extern int m_ping(), m_pong(), m_oper(), m_pass(), m_wall(), m_trace();
extern int m_time(), m_rehash(), m_names(), m_namreply(), m_admin();
extern int m_linreply(), m_notice(), m_lusers(), m_umode(), m_deop();
extern int m_motd(), m_whowas(), m_wallops(), m_mode(), m_kick();
extern int m_join(), m_part(), m_service(), m_userhost(), m_ison();
#ifdef DEBUGMODE
#ifndef CLIENT_COMPILE
extern int m_hash();
#endif
#endif

struct Message {
  char *cmd;
  int (* func)();
  int count;
  int parameters;
  char flags;     /* bit 0 set means that this command is allowed to be used
                   * only on the average of once per 2 seconds -SRB */
};

#ifdef MSGTAB
struct Message msgtab[] = {
  { MSG_PRIVATE, m_private,  0, 2, 1 },
  { MSG_NICK,    m_nick,     0, 2, 1 },
  { MSG_WHOIS,   m_whois,    0, 4, 1 },
  { MSG_CHANNEL, m_join,     0, 6, 1 },
  { MSG_USER,    m_user,     0, 4, 1 },
  { MSG_QUIT,    m_quit,     0, 2, 1 },
  { MSG_MODE,    m_mode,     0, 6, 1 },
  { MSG_NOTICE,  m_notice,   0, 2, 1 },
  { MSG_PONG,    m_pong,     0, 3, 1 },
  { MSG_JOIN,    m_join,     0, 6, 1 },
  { MSG_AWAY,    m_away,     0, 1, 1 },
  { MSG_SERVER,  m_server,   0, 3, 1 },
  { MSG_WALLOPS, m_wallops,  0, 1, 1 },
  { MSG_TOPIC,   m_topic,    0, 2, 1 },
  { MSG_SQUIT,   m_squit,    0, 2, 1 },
  { MSG_OPER,    m_oper,     0, 3, 1 },
  { MSG_WHO,     m_who,      0, 2, 1 },
  { MSG_USERHOST,m_userhost, 0, 1, 1 },
  { MSG_KICK,    m_kick,     0, 3, 1 },
  { MSG_WHOWAS,  m_whowas,   0, 4, 1 },
  { MSG_LIST,    m_list,     0, 2, 1 },
  { MSG_NAMES,   m_names,    0, 1, 1 },
  { MSG_PART,    m_part,     0, 2, 1 },
  { MSG_INVITE,  m_invite,   0, 2, 1 },
  { MSG_PING,    m_ping,     0, 2, 1 },
  { MSG_ISON,    m_ison,     0, 1, 1 },
  { MSG_KILL,    m_kill,     0, 2, 1 },
  { MSG_TRACE,   m_trace,    0, 1, 1 },
  { MSG_PASS,    m_pass,     0, 2, 1 },
  { MSG_LUSERS,  m_lusers,   0, 4, 1 },
  { MSG_TIME,    m_time,     0, 1, 1 },
  { MSG_CONNECT, m_connect,  0, 3, 1 },
  { MSG_VERSION, m_version,  0, 1, 1 },
  { MSG_STATS,   m_stats,    0, 2, 1 },
  { MSG_LINKS,   m_links,    0, 2, 1 },
  { MSG_ADMIN,   m_admin,    0, 1, 1 },
  { MSG_ERROR,   m_error,    0, 1, 1 },
  { MSG_USERS,   m_users,    0, 1, 1 },
  { MSG_REHASH,  m_rehash,   0, 1, 1 },
  { MSG_SUMMON,  m_summon,   0, 1, 1 },
  { MSG_HELP,    m_help,     0, 2, 1 },
  { MSG_INFO,    m_info,     0, 1, 1 },
  { MSG_MOTD,    m_motd,     0, 2, 1 },
  { MSG_RESTART, m_restart,  0, 1, 1 },
  { MSG_NAMREPLY,m_namreply, 0, 3, 1 },
  { MSG_LINREPLY,m_linreply, 0, 2, 1 },
  { MSG_WHOREPLY,m_whoreply, 0, 7, 1 },
  { MSG_SERVICE, m_service,  0, 6, 1 },
  { MSG_DEOP,    m_deop,     0, 1, 1 },
#ifdef DEBUGMODE
#ifndef CLIENT_COMPILE
  { MSG_HASH,    m_hash,     0, 2, 1 },
#endif
#endif
  { MSG_DIE,     m_die,      0, 1, 0 },
  { (char *) 0, (int (*)()) 0 }
};
#else
extern struct Message msgtab[];
#endif
