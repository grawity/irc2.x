/*************************************************************************
 ** c_msg.c    Beta v2.0    (22 Aug 1988)
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
 
char c_msg_id[] = "c_msg.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "msg.h"
#include <ctype.h>
#ifndef AUTOMATON
#include <curses.h>
#endif

#define CheckIgnorePriv(sender, client) \
 { struct Ignore *iptr = find_ignore(sender, (struct Ignore *) 0); \
   if (iptr && ISIGNORE_PRIVATE(iptr)) { \
     sendto_one(client, \
       "NOTICE %s :*** Automatic reply: You have been ignored"); \
     return(0); \
    } \
  }

#define CheckIgnorePub(sender) \
 { struct Ignore *iptr = find_ignore(sender, (struct Ignore *) 0); \
   if (iptr && ISIGNORE_PUBLIC(iptr))  \
     return(0); \
 }

#define DoSomething(sender, buf, buf2, mybuf) \
 { \
  if (sender && sender[0]) { \
    if (DigitORMinus(buf[0])) \
      sprintf(mybuf,"(%s) %s", sender, buf2); \
    else { \
      sprintf(mybuf,"*%s* %s", sender, buf2); \
      last_to_me(sender); \
    } \
    putline(mybuf); \
  } \
  else \
    putline(buf2); \
 }

char mybuf[513];
char abuf1[20], abuf2[20], abuf3[20], abuf4[20];

extern struct Ignore *find_ignore();
extern char *center(), *last_to_me();
extern aClient *client;
static char *rec_time_message = "*** Received time message..!";

m_restart() {
  putline("*** Oh boy... somebody wants *me* to restart... exiting...\n");
  sleep(5);
  exit(0);
}

m_time() { putline(rec_time_message); }

m_motd() { putline(rec_time_message); }

m_admin() { putline(rec_time_message); }

m_trace() { putline(rec_time_message); }

m_rehash() { putline("*** Received rehash message..!"); }

m_die() { exit(-1); }

m_pass() { putline("*** Received Pass message !"); }

m_oper() { putline("*** Received Oper message !"); }

m_names() { putline("*** Received Names message !"); }

m_lusers() { putline("*** Received Lusers message !"); }

m_wall(sptr, cptr, sender, message) 
aClient *sptr, *cptr;
char *sender, *message;
{
  sprintf(abuf1, "*** #%s# %s", sender, message);
  putline(abuf1);
}

m_woper() { putline("*** Received Woper message!"); }


m_connect() { putline("*** Received Connect message !"); }

m_ping(sptr, cptr, sender, para1, para2) 
aClient *sptr, *cptr;
char *sender, *para1, *para2;
{
  if (para2 && *para2)
    sendto_one(client, "PONG %s@%s %s", client->username, client->host, para2);
  else
    sendto_one(client, "PONG %s@%s", client->username, client->host);
}

m_pong(sptr, cptr, sender, para1, para2)
aClient *sptr, *cptr;
char *sender, *para1, *para2;
{
  sprintf(mybuf, "*** Received PONG message: %s %s",
	  para1, (para2) ? para2 : "");
  putline(mybuf);
}

m_nick(sptr, cptr, sender, nickname)
aClient *sptr, *cptr;
char *sender, *nickname;
{
  sprintf(mybuf,"*** Change: %s is now known as %s", sender, nickname);
  putline(mybuf);
#ifdef AUTOMATON
  a_nick(sender, nickname);
#endif
}

m_away(sptr, cptr, sender, text)
aClient *sptr, *cptr;
char *sender, *text;
{
  sprintf(mybuf,"*** %s is away: \"%s\"",sender,text);
  putline(mybuf);
}

m_who() { 
  putline("*** Oh boy... server asking who from client... exiting...");
}

m_whois() {
  putline("*** Oh boy... server asking whois from client... exiting...");
}

m_user() {
  putline("*** Oh boy... server telling me user messages... exiting...");
}

m_server(sptr, cptr, sender, serv) 
aClient *cptr, *sptr;
char *sender, *serv;
{
  sprintf(mybuf,"*** New server: %s", serv);
  putline(mybuf);
}

m_list() {
  putline("*** Oh boy... server asking me channel lists... exiting...");
}

m_topic(sptr, cptr, sender, topic)
aClient *sptr, *cptr;
char *sender, *topic;
{
  sprintf(mybuf, "*** %s changed the topic to %s", sender, topic);
  putline(mybuf);
}

m_channel(sptr, cptr, sender, ch)
aClient *cptr, *sptr;
char *sender, *ch;
{
  if (ch == (char *) 0 || *ch == '\0' || atoi(ch) == 0) {
    sprintf(mybuf,"*** Change: %s has left this Channel", sender);
#ifdef AUTOMATON
    a_leave(sender);
#endif
  } else
    sprintf(mybuf,"*** Change: %s has joined this Channel (%d)", 
	    sender, atoi(ch));
  putline(mybuf);
}

m_version(sptr, cptr, sender, version)
aClient *cptr, *sptr;
char *sender, *version;
{
  sprintf(mybuf,"*** Version: %s", version);
  putline(mybuf);
}

m_bye()
{
#ifndef AUTOMATON
  echo();
  nocrmode();
  clear();
  refresh();
#endif
  exit(-1);    
}

m_quit(sptr, cptr, sender)
aClient *sptr, *cptr;
char *sender;
{
  sprintf(mybuf,"*** Signoff: %s", sender);
  putline(mybuf);
#ifdef AUTOMATON
  a_quit(sender);
#endif
}

m_kill(cptr, sptr, sender, user, path)
aClient *cptr, *sptr;
char *sender, *user, *path;
{
  sprintf(mybuf,"*** Kill: %s (%s)", sender, path);
  putline(mybuf);
}

m_info(sptr, cptr, sender, info)
aClient *cptr, *sptr;
char *sender, *info;
{
  sprintf(mybuf,"*** Info: %s", info);
  putline(mybuf);
}

m_links() { putline("*** Received LINKS message"); }

m_summon() { putline("*** Received SUMMON message"); }

m_stats() { putline("*** Received STATS message"); }

m_users() { putline("*** Received USERS message"); }

m_help() { putline("*** Received HELP message"); }

m_squit(sptr, cptr, sender, server)
aClient *cptr, *sptr;
char *sender, *server;
{
  sprintf(mybuf,"*** Server %s has died. Snif.", server);
  putline(mybuf);
}

m_whoreply(sptr, cptr, sender, channel, username, host, server,
	   nickname, away, realname)
aClient *sptr, *cptr;
char *sender, *channel, *username, *host, *server, *nickname, *away;
char *realname;
{
  int i = atoi(channel);
  if (i != 0)
    sprintf(mybuf, "%3d:  %-10s %s %s@%s (%s)", i, nickname, away, username,
	    host, realname);
  else if (*away == 'S')
    sprintf(mybuf, "Chn:  Nickname   S  User@Host, Name");
  else 
    sprintf(mybuf, "Prv:  %-10s %s %s@%s (%s)", nickname, away, username,
	    host, realname);
  putline(mybuf);
}

m_text(sptr, cptr, sender, buf)
aClient *sptr, *cptr;
char *buf, *sender;
{
  CheckIgnorePub(sender);

  if (sender && sender[0]) {
    sprintf(mybuf,"<%s> %s", sender, buf);
#ifdef AUTOMATON
    a_msg(mybuf);
#else
    putline(mybuf);
#endif
  } else
    putline(buf);
}

m_namreply(sptr, cptr, sender, buf, buf2, buf3)
aClient *sptr, *cptr;
char *buf, *sender, *buf2, *buf3;
{
  if (buf) {
    switch (*buf) {
      case '*':
	sprintf(mybuf,"Prv: %-3s %s", buf2, buf3);
        break;
      case '=':
        sprintf(mybuf,"Pub: %-3s %s", buf2, buf3);
        break;
      case '@':
        sprintf(mybuf,"Sec: %-3s %s", buf2, buf3);
        break;
      default:
        sprintf(mybuf,"???: %-3s %s", buf2, buf3);
        break;
    }
  } else
    sprintf(mybuf, "*** Internal Error: namreply");
  putline(mybuf);
}

m_linreply(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *sender, *buf2;
{
  sprintf(mybuf,"*** Server: %s (%s)", buf, buf2);
  putline(mybuf);
}

m_private(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  CheckIgnorePriv(sender, client);
      
  if (sender && sender[0]) {
#ifdef AUTOMATON
    a_privmsg(sender, buf2);
#else
    if (DigitORMinus(buf[0]))
      sprintf(mybuf,"(%s) %s", sender, buf2);
    else {
      sprintf(mybuf,"*%s* %s", sender, buf2);
      last_to_me(sender);
    }
    putline(mybuf);
#endif
  }
  else
    putline(buf2);
}

m_voice(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  CheckIgnorePriv(sender, client);
  DoSomething(sender, buf, buf2, mybuf);
}


m_grph(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  CheckIgnorePriv(sender, client);
  DoSomething(sender, buf, buf2, mybuf);
}

m_xtra(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  CheckIgnorePriv(sender, client);
  DoSomething(sender, buf, buf2, mybuf);
}

m_notice(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  DoSomething(sender, buf, buf2, mybuf);
}

m_invite(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  CheckIgnorePriv(sender, client);
#ifdef AUTOMATON
  a_invite(sender, buf2);
#else
  sprintf(mybuf,"*** %s Invites you to channel %s", sender, buf2);
  putline(mybuf);
#endif
}

m_error(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  sprintf(mybuf,"*** Error: %s %s", buf, (buf2) ? buf2 : "");
  putline(mybuf);
}
