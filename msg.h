/*************************************************************************
 ** msg.h  Beta  v2.0    (22 Aug 1988)
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

#define MSG_TEXT     "MSG"
#define MSG_PRIVATE  "PRIVMSG"
#define MSG_WHO      "WHO"
#define MSG_WHOIS    "WHOIS"
#define MSG_USER     "USER"
#define MSG_NICK     "NICK"
#define MSG_SERVER   "SERVER"
#define MSG_LIST     "LIST"
#define MSG_TOPIC    "TOPIC"
#define MSG_INVITE   "INVITE"
#define MSG_CHANNEL  "CHANNEL"
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
#define MSG_TIME     "TIME"
#define MSG_REHASH   "REHASH"
#define MSG_NAMES    "NAMES"
#define MSG_NAMREPLY "NAMREPLY"
#define MSG_ADMIN    "ADMIN"
#define MSG_TRACE    "TRACE"
#define MSG_LINREPLY "LINREPLY"
#define MSG_NOTICE   "NOTICE"
#define MSG_JOIN     "JOIN"
#define MSG_PART     "PART"
#define MSG_LUSERS   "LUSERS"
#define MSG_VOICE    "VOICE"
#define MSG_GRPH     "GRPH"
#define MSG_XTRA     "XTRA"
#define MSG_MOTD     "MOTD"
#define MSG_WOPER     "WOPER"

#define MAXPARA    15 

extern int m_text(), m_private(), m_who(), m_whois(), m_user(), m_list();
extern int m_topic(), m_invite(), m_channel(), m_version(), m_quit();
extern int m_server(), m_kill(), m_info(), m_links(), m_summon(), m_stats();
extern int m_users(), m_nick(), m_error(), m_help(), m_whoreply();
extern int m_squit(), m_restart(), m_away(), m_die(), m_connect();
extern int m_ping(), m_pong(), m_oper(), m_pass(), m_wall(), m_trace();
extern int m_time(), m_rehash(), m_names(), m_namreply(), m_admin();
extern int m_linreply(), m_notice(), m_lusers(), m_voice(), m_grph();
extern int m_xtra(), m_motd(), m_woper();

typedef struct Message {
  char *cmd;
  int (* func)();
  int count;
  int parameters;
} aMessage;

#ifdef MSGTAB
struct Message msgtab[] = {
  { MSG_NICK,    m_nick,     0, 1 },
  { MSG_TEXT,    m_text,     0, 1 },
  { MSG_PRIVATE, m_private,  0, 2 },
  { MSG_WHO,     m_who,      0, 1 },
  { MSG_WHOIS,   m_whois,    0, 1 },
  { MSG_USER,    m_user,     0, 4 },
  { MSG_SERVER,  m_server,   0, 2 },
  { MSG_LIST,    m_list,     0, 1 },
  { MSG_TOPIC,   m_topic,    0, 1 },
  { MSG_INVITE,  m_invite,   0, 2 },
  { MSG_CHANNEL, m_channel,  0, 1 },
  { MSG_VERSION, m_version,  0, 1 },
  { MSG_QUIT,    m_quit,     0, 2 },
  { MSG_SQUIT,   m_squit,    0, 2 },
  { MSG_KILL,    m_kill,     0, 2 },
  { MSG_INFO,    m_info,     0, 1 },
  { MSG_LINKS,   m_links,    0, 1 },
  { MSG_SUMMON,  m_summon,   0, 1 },
  { MSG_STATS,   m_stats,    0, 1 },
  { MSG_USERS,   m_users,    0, 1 },
  { MSG_RESTART, m_restart,  0, 1 },
  { MSG_HELP,    m_help,     0, 2 },
  { MSG_ERROR,   m_error,    0, 1 },
  { MSG_AWAY,    m_away,     0, 1 },
  { MSG_DIE,     m_die,      0, 1 },
  { MSG_CONNECT, m_connect,  0, 3 },
  { MSG_PING,    m_ping,     0, 2 },
  { MSG_PONG,    m_pong,     0, 3 },
  { MSG_OPER,    m_oper,     0, 3 },
  { MSG_PASS,    m_pass,     0, 2 },
  { MSG_WALL,    m_wall,     0, 1 },
  { MSG_TIME,    m_time,     0, 1 },
  { MSG_REHASH,  m_rehash,   0, 1 },
  { MSG_NAMES,   m_names,    0, 1 },
  { MSG_NAMREPLY,m_namreply, 0, 3 },
  { MSG_ADMIN,   m_admin,    0, 1 },
  { MSG_TRACE,   m_trace,    0, 1 },
  { MSG_LINREPLY,m_linreply, 0, 2 },
  { MSG_NOTICE,  m_notice,   0, 2 },
  { MSG_LUSERS,  m_lusers,   0, 1 },
  { MSG_VOICE,   m_voice,    0, 2 },
  { MSG_GRPH,    m_grph,     0, 2 },
  { MSG_XTRA,    m_xtra,     0, 2 },
  { MSG_WOPER,    m_woper,   0, 1 },
  { MSG_MOTD,    m_motd,     0, 2 },
  { MSG_WHOREPLY,m_whoreply, 0, 7 },
  { (char *) 0, (int (*)()) 0 }  
};
#else
extern struct Message msgtab[];
#endif
