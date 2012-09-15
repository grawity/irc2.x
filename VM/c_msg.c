/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_msg.c
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

/* -- Jto -- 03 Jun 1990
 * Fixes to allow channels to be strings
 */
 
char c_msg_id[] = "c_msg.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "config.h"
#ifdef AUTOMATON
#include <ctype.h>
#else
#ifndef VMSP
#include <curses.h>
#endif
#endif

#include "common.h"
#include "struct.h"
#include "msg.h"

char mybuf[513];

extern anIgnore *find_ignore();
extern char *last_to_me();
extern aClient *client;

m_restart() {
  putline("*** Oh boy... somebody wants *me* to restart... exiting...\n");
  sleep(5);
  exit(0);
}

#ifdef MSG_MAIL
m_mail() {
  putline("*** Received base message..!");
}
#endif

m_time() {
  putline("*** Received time message..!");
}

m_motd() {
  putline("*** Received motd message..!");
}

m_admin() {
  putline("*** Received admin message..!");
}

m_service() {
  putline("*** Received service message..!");
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

m_mode(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  strcpy(mybuf, "*** Mode change ");
  while (parc--) {
    strcat(mybuf, parv[0]);
    strcat(mybuf, " ");
    parv++;
  }
  putline(mybuf);
}

m_wall(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** #%s# %s", parv[0], parv[1]);
  putline(mybuf);
}

m_wallops(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** =%s= %s", parv[0], parv[1]);
  putline(mybuf);
}

m_connect() {
  putline("*** Received Connect message !");
}

m_ping(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  if (parv[2] && *parv[2])
    sendto_one(client, "PONG %s@%s %s", client->user->username,
               client->sockhost, parv[2]);
  else
    sendto_one(client, "PONG %s@%s", client->user->username, client->sockhost);
}

m_pong(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** Received PONG message: %s %s",
          parv[1], (parv[2]) ? parv[2] : "");
  putline(mybuf);
}

m_nick(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Change: %s is now known as %s", parv[0], parv[1]);
  putline(mybuf);
#ifdef AUTOMATON
  a_nick(parv[0], parv[1]);
#endif
}

m_away(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** %s is away: \"%s\"",parv[0], parv[1]);
  putline(mybuf);
}

m_who() { 
  putline("*** Oh boy... server asking who from client... exiting...");
}

m_whois() {
  putline("*** Oh boy... server asking whois from client... exiting...");
}

m_whowas() {
  putline("*** Oh boy... server asking whowas from client... exiting...");
}

m_user() {
  putline("*** Oh boy... server telling me user messages... exiting...");
}

m_server(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** New server: %s", parv[1]);
  putline(mybuf);
}

m_list() {
  putline("*** Oh boy... server asking me channel lists... exiting...");
}

m_topic(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** %s changed the topic to %s", parv[0], parv[1]);
  putline(mybuf);
}

m_join(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Change: %s has joined channel %s", 
          parv[0], parv[1]);
  putline(mybuf);
}

m_part(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Change: %s has left channel %s", 
          parv[0], parv[1]);
  putline(mybuf);
}

m_version(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Version: %s", parv[1]);
  putline(mybuf);
}

m_bye()
{
#if !defined(AUTOMATON) && !defined(VMSP)
  echo();
  nocrmode();
  clear();
  refresh();
#endif
  exit(-1);    
}

m_quit(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Signoff: %s", parv[0]);
  putline(mybuf);
#ifdef AUTOMATON
  a_quit(sender);
#endif
}

m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Kill: %s (%s)", parv[0], parv[2]);
  putline(mybuf);
}

m_info(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Info: %s", parv[1]);
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

m_squit(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Server %s has died. Snif.", parv[1]);
  putline(mybuf);
}

m_whoreply(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  /* ...dirty hack, just assume all parameters present.. --msa */

  char *channel = parv[1],
       *username = parv[2],
       *host = parv[3],
  /*   *server = parv[4], not used .. --Armin */
       *nickname = parv[5],
       *away = parv[6],
       *realname = parv[7];

  if (channel[0] != '*')
    sprintf(mybuf, "%3s:  %-10s %s %s@%s (%s)", channel, nickname, away,
            username, host, realname);
  else if (*away == 'S')
    sprintf(mybuf, "Chn:  Nickname   S  User@Host, Name");
  else 
    sprintf(mybuf, "Prv:  %-10s %s %s@%s (%s)", nickname, away, username,
            host, realname);
  putline(mybuf);
}

m_text(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  anIgnore *iptr;
  if ((iptr = find_ignore(parv[0], (anIgnore *) 0)) &&
      (iptr->flags & IGNORE_PUBLIC))
      return(0);
  if (!BadPtr(parv[0])) {
    sprintf(mybuf,"<%s> %s", parv[0], parv[1]);
#ifdef AUTOMATON
    a_msg(mybuf);
#else
    putline(mybuf);
#endif
  } else
    putline(parv[1]);
}

m_namreply(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  if (parv[1]) {
    switch (*parv[1]) {
      case '*':
        sprintf(mybuf,"Prv: %-3s %s", parv[2], parv[3]);
        break;
      case '=':
        sprintf(mybuf,"Pub: %-3s %s", parv[2], parv[3]);
        break;
      case '@':
        sprintf(mybuf,"Sec: %-3s %s", parv[2], parv[3]);
        break;
      default:
        sprintf(mybuf,"???: %-3s %s", parv[2], parv[3]);
        break;
    }
  } else
    sprintf(mybuf, "*** Internal Error: namreply");
  putline(mybuf);
}

m_linreply(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Server: %s (%s)", parv[1], parv[2]);
  putline(mybuf);
}

m_private(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  anIgnore *iptr;
  int ignoreflag = 0;
  if ((iptr = find_ignore(parv[0], (anIgnore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE)) {
    ignoreflag = 1;
  }
  if (parv[0] && parv[0][0]) {
#ifdef AUTOMATON
    a_privmsg(sender, parv[2]);
#else
    if ((parv[1][0] >= '0' && parv[1][0] <= '9') || parv[1][0] == '-'
        || parv[1][0] == '+' || parv[1][0] == '#' || parv[1][0] == '$') {
      if (ignoreflag)
        return (0);
      sprintf(mybuf,"(%s:%s) %s", parv[0], parv[1], parv[2]);
    } else {
      if (ignoreflag) {
        sendto_one(client,
                   "NOTICE %s :*** Automatic reply: You have been ignored",
                   parv[0]);
        return(0);
      }
      sprintf(mybuf,"*%s* %s", parv[0], parv[2]);
      last_to_me(parv[0]);
    }
    putline(mybuf);
#endif
  }
  else
    putline(parv[2]);
}


m_kick(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** %s Kicked %s off channel %s",
          (parv[0]) ? parv[0] : "*Unknown*",
          (parv[2]) ? parv[2] : "*Unknown*",
          (parv[1]) ? parv[1] : "*Unknown*");
  putline(mybuf);
}

m_voice(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
    m_private(sptr,cptr,parc,parv);
}

m_grph(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
    m_private(sptr,cptr,parc,parv);
}

m_xtra(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
    m_private(sptr,cptr,parc,parv);
}

m_notice(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  if (parv[0] && parv[0][0]) {
    if ((parv[1][0] >= '0' && parv[1][0] <= '9') || parv[1][0] == '-')
      sprintf(mybuf,"(%s) %s", parv[0], parv[2]);
    else
      sprintf(mybuf,"*%s* %s", parv[0], parv[2]);
    putline(mybuf);
  }
  else
    putline(parv[2]);
}

m_invite(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  anIgnore *iptr;
  if ((iptr = find_ignore(parv[0], (anIgnore *) 0)) &&
      (iptr->flags & IGNORE_PRIVATE) && (iptr->flags & IGNORE_PUBLIC)) {
        sendto_one(client,
                   "NOTICE %s :*** Automatic reply: You have been ignored",
                   parv[0]);
        return(0);
      }
#ifdef AUTOMATON
  a_invite(parv[0], parv[2]);
#else
  sprintf(mybuf,"*** %s Invites you to channel %s", parv[0], parv[2]);
  putline(mybuf);
#endif
}

m_error(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Error: %s %s", parv[1], (parv[2]) ? parv[2] : "");
  putline(mybuf);
}
