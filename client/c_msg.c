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
#ifdef AUTOMATON
#include <ctype.h>
#else
#include <curses.h>
#endif

char mybuf[513];
char abuf1[20], abuf2[20], abuf3[20], abuf4[20];

extern struct Ignore *find_ignore();
extern char *center(), *last_to_me();
extern struct Client *client;

m_restart() {
  putline("*** Oh boy... somebody wants *me* to restart... exiting...\n");
  sleep(5);
  exit(0);
}

m_time() {
  putline("*** Received time message..!");
}

m_motd() {
  putline("*** Received time message..!");
}

m_admin() {
  putline("*** Received time message..!");
}

m_trace() {
  putline("*** Received trace message..!");
}

m_rehash() {
  putline("*** Received rehash message..!");
}

m_die() {
  exit(-1);
}

m_pass() {
  putline("*** Received Pass message !");
}

m_oper() {
  putline("*** Received Oper message !");
}

m_names() {
  putline("*** Received Names message !");
}

m_lusers() {
  putline("*** Received Lusers message !");
}

m_wall(sptr, cptr, sender, message) 
struct Client *sptr, *cptr;
char *sender, *message;
{
  sprintf(abuf1, "*** #%s# %s", sender, message);
  putline(abuf1);
}

m_connect() {
  putline("*** Received Connect message !");
}

m_ping(sptr, cptr, sender, para1, para2) 
struct Client *sptr, *cptr;
char *sender, *para1, *para2;
{
  if (para2 && *para2)
    sendto_one(client, "PONG %s@%s %s", client->username, client->host, para2);
  else
    sendto_one(client, "PONG %s@%s", client->username, client->host);
}

m_pong(sptr, cptr, sender, para1, para2)
struct Client *sptr, *cptr;
char *sender, *para1, *para2;
{
  sprintf(mybuf, "*** Received PONG message: %s %s",
	  para1, (para2) ? para2 : "");
  putline(mybuf);
}

m_nick(sptr, cptr, sender, nickname)
struct Client *sptr, *cptr;
char *sender, *nickname;
{
  sprintf(mybuf,"*** Change: %s is now known as %s", sender, nickname);
  putline(mybuf);
#ifdef AUTOMATON
  a_nick(sender, nickname);
#endif
}

m_away(sptr, cptr, sender, text)
struct Client *sptr, *cptr;
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
struct Client *cptr, *sptr;
char *sender, *serv;
{
  sprintf(mybuf,"*** New server: %s", serv);
  putline(mybuf);
}

m_list() {
  putline("*** Oh boy... server asking me channel lists... exiting...");
}

m_topic(sptr, cptr, sender, topic)
struct Client *sptr, *cptr;
char *sender, *topic;
{
  sprintf(mybuf, "*** %s changed the topic to %s", sender, topic);
  putline(mybuf);
}

m_channel(sptr, cptr, sender, ch)
struct Client *cptr, *sptr;
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
struct Client *cptr, *sptr;
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
struct Client *sptr, *cptr;
char *sender;
{
  sprintf(mybuf,"*** Signoff: %s", sender);
  putline(mybuf);
#ifdef AUTOMATON
  a_quit(sender);
#endif
}

m_kill(cptr, sptr, sender, user, path)
struct Client *cptr, *sptr;
char *sender, *user, *path;
{
  sprintf(mybuf,"*** Kill: %s (%s)", sender, path);
  putline(mybuf);
}

m_info(sptr, cptr, sender, info)
struct Client *cptr, *sptr;
char *sender, *info;
{
  sprintf(mybuf,"*** Info: %s", info);
  putline(mybuf);
}

m_links() { 
  putline("*** Received LINKS message");
}

m_summon() {
  putline("*** Received SUMMON message");
}

m_stats() {
  putline("*** Received STATS message");
}

m_users() {
  putline("*** Received USERS message");
}

m_help() {
  putline("*** Received HELP message");
}

m_squit(sptr, cptr, sender, server)
struct Client *cptr, *sptr;
char *sender, *server;
{
  sprintf(mybuf,"*** Server %s has died. Snif.", server);
  putline(mybuf);
}

m_whoreply(sptr, cptr, sender, channel, username, host, server,
	   nickname, away, realname)
struct Client *sptr, *cptr;
char *sender, *channel, *username, *host, *server, *nickname, *away;
char *realname;
{
  int i = atoi(channel);
#ifdef OLDWHOREPLY
  if (username)
    center(abuf1, username, 8);
  else
    abuf1[0] = '\0';
  if (host)
    center(abuf2, host, 8);
  else
    abuf2[0] = '\0';
  if (server)
    center(abuf3, server, 8);
  else
    abuf3[0] = '\0';
  if (nickname)
    center(abuf4, nickname, 8);
  else
    abuf4[0] = '\0';
  if (i != 0)
    sprintf(mybuf,"Channel %3d: %8s@%8s %8s (%8s) %s %s",i ,abuf1, abuf2,
	    abuf3, abuf4, away, realname);
  else
    sprintf(mybuf,"* Private *: %8s@%8s %8s (%8s) %s %s", abuf1, abuf2,
	    abuf3, abuf4, away, realname);
#else  /* Fix: wisner@mica.berkeley.edu */
  if (i != 0)
    sprintf(mybuf, "%3d:  %-10s %s %s@%s (%s)", i, nickname, away, username,
	    host, realname);
  else if (*away == 'S')
    sprintf(mybuf, "Chn:  Nickname   S  User@Host, Name");
  else 
    sprintf(mybuf, "Prv:  %-10s %s %s@%s (%s)", nickname, away, username,
	    host, realname);
#endif /* End Fix */
  putline(mybuf);
}

m_text(sptr, cptr, sender, buf)
struct Client *sptr, *cptr;
char *buf, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PUBLIC))
      return(0);
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
struct Client *sptr, *cptr;
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
struct Client *sptr, *cptr;
char *buf, *sender, *buf2;
{
  sprintf(mybuf,"*** Server: %s (%s)", buf, buf2);
  putline(mybuf);
}

m_private(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE)) {
	sendto_one(client,
		   "NOTICE %s :*** Automatic reply: You have been ignored");
	return(0);
      }
  if (sender && sender[0]) {
#ifdef AUTOMATON
    a_privmsg(sender, buf2);
#else
    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')
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
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE)) {
	sendto_one(client,
		   "NOTICE %s :*** Automatic reply: You have been ignored");
	return(0);
      }
  if (sender && sender[0]) {
    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')
      sprintf(mybuf,"(%s) %s", sender, buf2);
    else {
      sprintf(mybuf,"*%s* %s", sender, buf2);
      last_to_me(sender);
    }
    putline(mybuf);
  }
  else
    putline(buf2);
}

m_grph(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE)) {
	sendto_one(client,
		   "NOTICE %s :*** Automatic reply: You have been ignored");
	return(0);
      }
  if (sender && sender[0]) {
    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')
      sprintf(mybuf,"(%s) %s", sender, buf2);
    else {
      sprintf(mybuf,"*%s* %s", sender, buf2);
      last_to_me(sender);
    }
    putline(mybuf);
  }
  else
    putline(buf2);
}

m_xtra(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE)) {
	sendto_one(client,
		   "NOTICE %s :*** Automatic reply: You have been ignored");
	return(0);
      }
  if (sender && sender[0]) {
    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')
      sprintf(mybuf,"(%s) %s", sender, buf2);
    else {
      sprintf(mybuf,"*%s* %s", sender, buf2);
      last_to_me(sender);
    }
    putline(mybuf);
  }
  else
    putline(buf2);
}

m_notice(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  if (sender && sender[0]) {
    if ((buf[0] >= '0' && buf[0] <= '9') || buf[0] == '-')
      sprintf(mybuf,"(%s) %s", sender, buf2);
    else
      sprintf(mybuf,"*%s* %s", sender, buf2);
    putline(mybuf);
  }
  else
    putline(buf2);
}

m_invite(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  struct Ignore *iptr;
  if ((iptr = find_ignore(sender, (struct Ignore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE) && (iptr->flags & IGNORE_PUBLIC)) {
	sendto_one(client,
		   "NOTICE %s :*** Automatic reply: You have been ignored");
	return(0);
      }
#ifdef AUTOMATON
  a_invite(sender, buf2);
#else
  sprintf(mybuf,"*** %s Invites you to channel %s", sender, buf2);
  putline(mybuf);
#endif
}

m_error(sptr, cptr, sender, buf, buf2)
struct Client *sptr, *cptr;
char *buf, *buf2, *sender;
{
  sprintf(mybuf,"*** Error: %s %s", buf, (buf2) ? buf2 : "");
  putline(mybuf);
}
