/*************************************************************************
 ** r_msg.c    Beta v2.0    (22 Aug 1988)
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
 
char r_msg_id[] = "r_msg.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "msg.h"

char mybuf[513];
char abuf1[20], abuf2[20], abuf3[20], abuf4[20];

extern char *center(), *mycncmp();
extern aClient *client;

m_restart() {
  putline("*** Oh boy... somebody wants *me* to restart... exiting...\n");
  sleep(5);
  exit(0);
}

m_time() {
  putline("*** Received time message..!");
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

m_wall(sptr, cptr, sender, message) 
aClient *sptr, *cptr;
char *sender, *message;
{
  sprintf(abuf1, "*** #%s# %s", sender, message);
  putline(abuf1);
}

m_connect() {
  putline("*** Received Connect message !");
}

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
  putline("*** Received PONG message: %s %s", para1, (para2) ? para2 : "");
}

m_nick(sptr, cptr, sender, nickname)
aClient *sptr, *cptr;
char *sender, *nickname;
{
  sprintf(mybuf,"*** Change: %s is now known as %s", sender, nickname);
  putline(mybuf);
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
  putline("*** Oh boy... server telling me the topic... exiting...");
}

m_channel(sptr, cptr, sender, ch)
aClient *cptr, *sptr;
char *sender, *ch;
{
  if (ch == (char *) 0 || *ch == '\0' || atoi(ch) == 0)
    sprintf(mybuf,"*** Change: %s has left this Channel", sender);
  else
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
  exit(-1);    
}

m_quit(sptr, cptr, sender)
aClient *sptr, *cptr;
char *sender;
{
  sprintf(mybuf,"*** Signoff: %s", sender);
  putline(mybuf);
}

m_kill() {
  putline("*** Received KILL message");
}

m_info(sptr, cptr, sender, info)
aClient *cptr, *sptr;
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
  putline(mybuf);
}

m_mytext(sptr, cptr, sender, buf)
aClient *sptr, *cptr;
char *sender, *buf;
{
  if (sender)
    sprintf(mybuf,"<%s> %s", sender, buf);
  else
    sprintf(mybuf,"> %s",buf);
  putline(mybuf);
}

m_text(sptr, cptr, sender, buf)
aClient *sptr, *cptr;
char *buf, *sender;
{
  if (sender && sender[0]) {
    sprintf(mybuf,"<%s> %s", sender, buf);
    putline(mybuf);
  } else
    putline(buf);
}

m_myprivate(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  sprintf(mybuf,"-> *%s* %s",buf,buf2);
  putline(mybuf);
}

m_private(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  char *ptr;
  if (sender && sender[0]) {
    if (myncmp(sender, client->nickname, NICKLEN))
      sprintf(mybuf,"*%s* %s", sender, buf2);
    else
      sprintf(mybuf,"-> *%s* %s", buf, buf2);
    putline(mybuf);
    
    if (ptr = mycncmp(buf2,"DEAL", 1)) {
      sendto_one(client,"MSG :Deal requested by %s", sender);
      deal();
    } else if (ptr = mycncmp(buf2, "PLAYER", 1)) {
      sendto_one(client,"MSG :%s requested to be added to game",
		 sender);
      player(sender);
    } else if (ptr = mycncmp(buf2, "INIT", 1)) {
      sendto_one(client,"MSG :%s requested a new game with %d cards",
		 sender, atoi(ptr));
      if (atoi(ptr) > 0)
	init_game(atoi(ptr));
    } else if (ptr = mycncmp(buf2, "NAMES", 1)) {
      sendto_one(client,"MSG :Revealing names of players");
      names();
    } else if (ptr = mycncmp(buf2, "HAND", 1)) {
      hand(sender);
    } else if (ptr = mycncmp(buf2, "SHUFFLE", 1)) {
      shuffle(sender);
    } else {
      sendto_one(client,"PRIVMSG %s :Commands available are:", sender);
      sendto_one(client,"PRIVMSG %s :INIT PLAYER NAMES DEAL HAND SHUFFLE",
		 sender);
    }
  }
  else
    putline(buf2);
}

m_invite(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  sprintf(mybuf,"*** %s Invites you to channel %s", sender, buf2);
  putline(mybuf);
  sendto_one(client,"MSG :%s invited me away, Byebye...", sender);
  sendto_one(client,"CHANNEL %s", buf2);
  sendto_one(client,"MSG :Hello %s, always ready to serve you...", sender);
}

m_error(sptr, cptr, sender, buf, buf2)
aClient *sptr, *cptr;
char *buf, *buf2, *sender;
{
  sprintf(mybuf,"*** Error: %s %s", buf, (buf2) ? buf2 : "");
  putline(mybuf);
}
