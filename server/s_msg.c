/*************************************************************************
 ** s_msg.c  Beta  v2.0    (22 Aug 1988)
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

char s_msg_id[] = "s_msg.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "sys.h"
#include <sys/types.h>
#include <sys/file.h>
#if APOLLO
#include <sys/types.h>
#endif
#include <utmp.h>
#include <ctype.h>
#include <stdio.h>
#include "msg.h"
#include "numeric.h"

#ifndef NULL
#define NULL ((char *) 0)
#endif

extern struct Client *client, me;
extern struct Client *find_client();
extern struct Client *find_server();
extern struct Confitem *find_conf(), *find_admin();
extern struct Confitem *conf;
extern struct Channel *find_channel();
extern char *center();
extern char *uphost;
struct Channel *channel = (struct Channel *) 0;
extern int maxusersperchannel;

char buf[BUFSIZE];
char buf2[80];

m_nick(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{
  struct Client *acptr;
  char *ch;
  int flag = 0;
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No nickname given",
	       myhostname, ERR_NONICKNAMEGIVEN, sptr->nickname);
  } else {
    ch = para;
    if ((*ch >= '0' && *ch <= '9') || *ch == '-' || *ch == '+')
      *ch = '\0';
    while (*ch) {
      if ((*ch < 65 || *ch > 125) && (*ch < '0' || *ch > '9') &&
	  *ch != '_' && *ch != '-') *ch = '\0';
      ch++;
    }
    if (strlen(para) >= NICKLEN)
      para[NICKLEN] = '\0';
    if (*para == '\0') {
      sendto_one(sptr,":%s %d %s :Erroneus Nickname", myhostname,
		 ERR_ERRONEUSNICKNAME, sptr->nickname);
      return(0);
    }
    if ((acptr = find_client(para,NULL)) &&
	(!strncmp(acptr->nickname, para, NICKLEN - 1) || acptr != sptr)) {
      sendto_one(sptr, ":%s %d %s %s :Nickname is already in use.",
		 myhostname, ERR_NICKNAMEINUSE, sptr->nickname, para);
      if (cptr->status == STAT_SERVER) {
	sendto_serv_butone(NULL, "KILL %s :%s", acptr->nickname, myhostname);
	m_bye(acptr, acptr, 0);
      }
    } else {
      if (sptr->status == STAT_SERVER) {
	sptr = make_client();
	strcpy(sptr->fromhost, cptr->host);
	flag = 1;
      }
      else if (sptr->realname[0]) {
	if (sptr->fd >= 0 && sptr->status == STAT_UNKNOWN) {
	  sptr->status = STAT_CLIENT;
	  dowelcome(sptr);
	}
	if (sptr->realname[0] && sptr->nickname[0] == '\0')
	  flag = 2;
      }
      if (sptr->channel != 0)
	sendto_channel_butserv(sptr->channel, ":%s NICK %s",
			       sptr->nickname, para);
      if (sptr->nickname[0])
	sendto_serv_butone(cptr, ":%s NICK %s",sptr->nickname, para);
      else
	sendto_serv_butone(cptr, "NICK %s", para);
      strncpy(sptr->nickname, para, NICKLEN-1);
      sptr->nickname[NICKLEN-1] = '\0';
      if (flag == 2) {
	sendto_serv_butone(cptr,":%s USER %s %s %s :%s", sptr->nickname,
			   sptr->username, sptr->host, sptr->server,
			   sptr->realname);
      }
      if (flag == 1) {
	sptr->next = client;
	client = sptr;
      } 
    }
  }
  return(0);
}

m_text(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') 
    sendto_one(sptr,":%s %d %s :No text to send", myhostname, ERR_NOTEXTTOSEND,
	       sptr->nickname);
  else if (sptr->channel != 0) 
    sendto_channel_butone(cptr,
			  sptr->channel, ":%s MSG :%s", 
			  sptr->nickname, para);
  else
    sendto_one(sptr,":%s %d %s :You have not joined any channel",
	       myhostname, ERR_USERNOTINCHANNEL, sptr->nickname);
  return(0);
}

m_private(cptr, sptr, sender, para, para2)
struct Client *cptr, *sptr;
char *para, *para2, *sender;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return (-1);
  }
  if (para2 == NULL || *para2 == '\0') {
    sendto_one(sptr,":%s %d %s :No text to send",
	       myhostname, ERR_NOTEXTTOSEND, sptr->nickname);
    return (-1);
  }
  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      if (acptr->away) {
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      }
      sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->nickname, nick, para2);
    } else if (chptr = find_channel(nick, NULL)) {
      sendto_channel_butone(cptr, chptr->channo,
			    ":%s PRIVMSG %d :%s", sptr->nickname,
			    chptr->channo, para2);
    } else {
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, nick);
    }
  } while (tmp && *tmp);
  return (0);
}

m_xtra(cptr, sptr, sender, para, para2)
struct Client *cptr, *sptr;
char *para, *para2, *sender;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return (-1);
  }
  if (para2 == NULL || *para2 == '\0') {
    sendto_one(sptr,":%s %d %s :No text to send",
	       myhostname, ERR_NOTEXTTOSEND, sptr->nickname);
    return (-1);
  }
  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      if (acptr->away) {
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      }
      sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->nickname, nick, para2);
    } else if (chptr = find_channel(nick, NULL)) {
      sendto_channel_butone(cptr, chptr->channo,
			    ":%s PRIVMSG %d :%s", sptr->nickname,
			    chptr->channo, para2);
    } else {
      sendto_one(sptr, ":%s %d %s :No such nickname (%s)",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, nick);
    }
  } while (tmp && *tmp);
  return (0);
}

m_grph(cptr, sptr, sender, para, para2)
struct Client *cptr, *sptr;
char *para, *para2, *sender;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return (-1);
  }
  if (para2 == NULL || *para2 == '\0') {
    sendto_one(sptr,":%s %d %s :No text to send",
	       myhostname, ERR_NOTEXTTOSEND, sptr->nickname);
    return (-1);
  }
  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      if (acptr->away) { 
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      }
      sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->nickname, nick, para2);
    } else if (chptr = find_channel(nick, NULL)) {
      sendto_channel_butone(cptr, chptr->channo,
			    ":%s PRIVMSG %d :%s", sptr->nickname,
			    chptr->channo, para2);
    } else {
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, nick);
    }
  } while (tmp && *tmp);
  return (0);
}

m_voice(cptr, sptr, sender, para, para2)
struct Client *cptr, *sptr;
char *para, *para2, *sender;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return (-1);
  }
  if (para2 == NULL || *para2 == '\0') {
    sendto_one(sptr,":%s %d %s :No text to send",
	       myhostname, ERR_NOTEXTTOSEND, sptr->nickname);
    return (-1);
  }
  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      if (acptr->away) {
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      }
      sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->nickname, nick, para2);
    } else if (chptr = find_channel(nick, NULL)) {
      sendto_channel_butone(cptr, chptr->channo,
			    ":%s PRIVMSG %d :%s", sptr->nickname,
			    chptr->channo, para2);
    } else {
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, nick);
    }
  } while (tmp && *tmp);
  return (0);
}

m_notice(cptr, sptr, sender, para, para2)
struct Client *cptr, *sptr;
char *para, *para2, *sender;
{
  struct Client *acptr;
  struct Channel *chptr;
  char *nick, *tmp;
  if (sptr->status < 0) {
    sendto_one(sptr, ":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return (-1);
  }
  if (para2 == NULL || *para2 == '\0') {
    sendto_one(sptr,":%s %d %s :No text to send",
	       myhostname, ERR_NOTEXTTOSEND, sptr->nickname);
    return (-1);
  }
  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      sendto_one(acptr, ":%s NOTICE %s :%s", sptr->nickname, nick, para2);
    } else if (chptr = find_channel(nick, NULL)) {
      sendto_channel_butone(cptr, chptr->channo,
			    ":%s NOTICE %d :%s", sptr->nickname,
			    chptr->channo, para2);
    } else {
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, nick);
    }
  } while (tmp && *tmp);
  return (0);
}

m_who(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{
  int i;
  struct Client *acptr = client;
  if (para == NULL || para[0] == '\0')
    i = 0;
  else if (index(para,'*'))
    i = sptr->channel;
  else
    i = atoi(para);
  sendto_one(sptr,"WHOREPLY * User Host Server Nickname S :Name");
  while (acptr) {
    if ((i == 0 || acptr->channel == i) && 
	(acptr->status == STAT_CLIENT || acptr->status == STAT_OPER) &&
	(acptr->channel >= 0 || acptr->channel == sptr->channel)) {
      if ((acptr->channel < 1000 && acptr->channel > 0) ||
	  (sptr->channel == acptr->channel && sptr->channel != 0))
	sendto_one(sptr, "WHOREPLY %d %s %s %s %s %c%c :%s",
		   acptr->channel, acptr->username, acptr->host,
		   acptr->server, acptr->nickname, (acptr->away) ? 'G' : 'H',
		   (acptr->status == STAT_OPER) ? '*' : ' ', acptr->realname);
      else
	sendto_one(sptr,"WHOREPLY 0 %s %s %s %s %c%c :%s",
		   acptr->username, acptr->host, acptr->server,
		   acptr->nickname, (acptr->away) ? 'G' : 'H',
		   (acptr->status == STAT_OPER) ? '*' : ' ', acptr->realname);
    }
    acptr = acptr->next;
  }
  return(0);
}

m_whois(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{
  char *cp;
  struct Client *acptr, *a2cptr;
  
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No nickname specified",
	       myhostname, ERR_NONICKNAMEGIVEN, sptr->nickname);
    return (0);
  }
  do {
    while (*para == ' ')
      para++;
    if (*para == '\0')
      break;
    cp = index(para, ',');
    if (cp != NULL)
      *cp = '\0';
    acptr = find_client(para,NULL);
    if (acptr == (struct Client *) 0)
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, para);
    else {
      a2cptr = find_server(acptr->server);
      if ((acptr->channel > 0 && acptr->channel < 1000) ||
	  (acptr->channel == sptr->channel && acptr->channel != 0))
	sendto_one(sptr,":%s %d %s %s %s %s %d :%s", myhostname,
		   RPL_WHOISUSER, sptr->nickname, acptr->nickname,
		   acptr->username, acptr->host, acptr->channel,
		   acptr->realname);
      else
	sendto_one(sptr,":%s %d %s %s %s %s * :%s", myhostname,
		   RPL_WHOISUSER, sptr->nickname, acptr->nickname,
		   acptr->username, acptr->host, acptr->realname);
      if (a2cptr)
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, 
		   RPL_WHOISSERVER, sptr->nickname,
		   a2cptr->host, a2cptr->server);
      else
	sendto_one(sptr,":%s %d %s * *", myhostname, 
		   RPL_WHOISSERVER, sptr->nickname);
      if (acptr->away) 
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      if (acptr->status == STAT_OPER)
	sendto_one(sptr, ":%s %d %s %s :is an operator", myhostname,
		   RPL_WHOISOPERATOR, sptr->nickname,
		   acptr->nickname);
    }
    if (cp != NULL)
      para = ++cp;
  } while (cp != NULL && *para != '\0');
  return(0);
}

m_user(cptr, sptr, sender, username, host, server, realname)
struct Client *cptr, *sptr;
char *username, *server, *host, *realname, *sender;
{
  struct Confitem *aconf;
  if (username == NULL || server == NULL || host == NULL || realname == NULL
      || *username == '\0' || *server == '\0' || *host == '\0' ||
      *realname == '\0')
    sendto_one(sptr,":%s %d * :Not enough parameters",
	       myhostname, ERR_NEEDMOREPARAMS);
  else if (sptr->status != STAT_UNKNOWN && sptr->status != STAT_OPER)
    sendto_one(sptr,":%s %d * :Identity problems, eh ?",
	       myhostname, ERR_ALREADYREGISTRED);
  else {
    if (sptr->fd >= 0) {
      if ((aconf = find_conf(sptr->sockhost, NULL, NULL, CONF_CLIENT)) == 0) {
	if ((aconf = find_conf("", NULL, NULL, CONF_CLIENT)) == 0) {
	  sendto_one(sptr,"%s %d * :Your host isn't among the privileged..",
		     myhostname, ERR_NOPERMFORHOST);
	  m_bye(sptr, sptr, 0);
	  return(FLUSH_BUFFER);
	} 
      } else 
	if (strcmp(sptr->passwd, aconf->passwd)) {
	  sendto_one(sptr,"%s %d * :Only correct words will open this gate..",
		     myhostname, ERR_PASSWDMISMATCH);
	  m_bye(sptr, sptr, 0);
	  return(FLUSH_BUFFER);
	}
      host = sptr->sockhost;
      if (find_conf(host, NULL, username,
		    CONF_KILL)) {
	sendto_one(sptr,"%s %d * :Ghosts are not allowed on irc...",
		   myhostname, ERR_YOUREBANNEDCREEP);
	m_bye(sptr, sptr, 0);
	return(FLUSH_BUFFER);
      }
    }
    if (sptr->nickname[0]) {
      sptr->status = STAT_CLIENT;
      if (sptr->fd >= 0)
	dowelcome(sptr);
    }
    strncpy(sptr->username, username, USERLEN);
    sptr->username[USERLEN] = '\0';
    if (cptr != sptr)
      strncpy(sptr->server, server, HOSTLEN);
    else
      strcpy(sptr->server, myhostname);
    sptr->server[HOSTLEN] = '\0';
    if (sptr->host[0] == '\0') {
      strncpy(sptr->host, host, HOSTLEN);
      sptr->host[HOSTLEN] = '\0';
    }
    strcpy(sptr->realname, realname);
    sptr->realname[REALLEN] = '\0';
    if (sptr->status == STAT_CLIENT)
      sendto_serv_butone(cptr,":%s USER %s %s %s :%s", sptr->nickname,
			 username, sptr->host, sptr->server, realname);
  }
  return(0);
}

m_list(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{
  struct Channel *chptr = channel;
  sendto_one(sptr,":%s %d %s :  Channel  : Users  Name",
	     myhostname, RPL_LISTSTART, sptr->nickname);
  while (chptr) {
    if ((chptr->channo < 1000 && chptr->channo > 0) ||
	(chptr->channo == sptr->channel && sptr->channel != 0))
      sprintf(buf2,"%d",chptr->channo);
    else
      strcpy(buf2,"*");
    sendto_one(sptr,":%s %d %s %s %d :%s",
	       myhostname, RPL_LIST, sptr->nickname,
	       buf2, chptr->users, chptr->name);
    chptr = chptr->nextch;
  }
  return(0);
}

m_topic(cptr, sptr, sender, topic)
struct Client *cptr, *sptr;
char *topic, *sender;
{
  struct Channel *chptr = channel;
  if (sptr->status == STAT_UNKNOWN) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return(0);
  }
  if (topic != NULL && *topic != '\0')
    sendto_serv_butone(cptr,":%s TOPIC %s",sptr->nickname,topic);
  while (chptr) {
    if (chptr->channo == sptr->channel) {
      if (topic == NULL || topic[0] == '\0') {
	if (chptr->name[0] == '\0')
	  sendto_one(sptr, ":%s %d %s :No topic is set.", 
		     myhostname, RPL_NOTOPIC, sptr->nickname);
	else
	  sendto_one(sptr, ":%s %d %s :%s",
		     myhostname, RPL_TOPIC, sptr->nickname,
		     chptr->name);
	return (0);
      } else {
	strncpy(chptr->name, topic, CHANNELLEN);
	chptr->name[CHANNELLEN] = '\0';
	sendto_channel_butserv(chptr->channo, ":%s TOPIC %s", sptr->nickname,
			       chptr->name);
      }
    }	
    chptr = chptr->nextch;
  }
  return(0);
}

m_invite(cptr, sptr, sender, user, channel)
struct Client *cptr, *sptr;
char *user, *channel, *sender;
{
  struct Client *acptr;
  int i;
  if (sptr->status < 0) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  if (user == NULL || *user == '\0') {
    sendto_one(sptr,":%s %d %s :Not enough parameters", myhostname,
	       ERR_NEEDMOREPARAMS, sptr->nickname);
    return (-1);
  }
  acptr = find_client(user,NULL);
  if (channel == NULL)
    i = 0;
  else 
    i = atoi(channel);
  if (i == 0)
    i = sptr->channel;
  if (acptr == (struct Client *) 0) 
    sendto_one(sptr,":%s %d %s %s :No such nickname",
	       myhostname, ERR_NOSUCHNICK, sptr->nickname, user);
  else {
    if (sptr->fd >= 0) {
      sendto_one(sptr,":%s %d %s %s %d", myhostname, RPL_INVITING,
		 sptr->nickname, acptr->nickname, i);
    }
    if (acptr->away)
      sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		 sptr->nickname, acptr->nickname, acptr->away);
    sendto_one(acptr,":%s INVITE %s %d",sptr->nickname,
	       acptr->nickname, i);
  }
  return(0);
}

m_channel(cptr, sptr, sender, ch)
struct Client *cptr, *sptr;
char *ch, *sender;
{
  struct Channel *chptr, *pchptr, *ch2ptr;
  int i;
  if ((sptr->status != STAT_CLIENT) && (sptr->status != STAT_OPER)) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return (-1);
  }
  chptr = channel;
  if (ch == NULL) {
    sendto_one(sptr, "NOTICE %s :*** What channel do you want to join?",
	       sptr->nickname);
    return (0);
  }
  else
    i = atoi(ch);
  pchptr = (struct Channel *) 0;
  if (i > 0) {
    while (chptr) {
      if (chptr->channo == i)
	break;
      chptr = chptr->nextch;
    }
    if (chptr == (struct Channel *) 0) {
      if ((chptr = (struct Channel *) malloc(sizeof(struct Channel))) == 
	  (struct Channel *) 0)
	{
	  perror("malloc");
	  debug(DEBUG_FATAL,"Out of memory. Cannot allocate channel");
	  restart();
	}
      chptr->nextch = channel;
      chptr->channo = i;
      chptr->name[0] = '\0';
      chptr->users = 0;
      channel = chptr;
    } else if (cptr == sptr && is_full(i, chptr->users)) {
      sendto_one(sptr,":%s %d %s %d :Sorry, Channel is full.",
		 myhostname, ERR_CHANNELISFULL, sptr->nickname, i);
      return(0);
    }
    chptr->users++;
  }
  chptr = channel;
  while (chptr) {
    if (chptr->channo == sptr->channel && chptr->users > 0) 
      --chptr->users;
    if (chptr->users <= 0) {
      if (pchptr)
	pchptr->nextch = chptr->nextch;
      else
	channel = chptr->nextch;
      free(chptr);
    }
    pchptr = chptr;
    chptr = chptr->nextch;
  }
  sendto_serv_butone(cptr, ":%s CHANNEL %d",
			sptr->nickname, i);
  if (sptr->channel != 0)
    sendto_channel_butserv(sptr->channel, ":%s CHANNEL 0", sptr->nickname);
  sptr->channel = i;
  if (sptr->channel != 0)
    sendto_channel_butserv(i, ":%s CHANNEL %d", sptr->nickname, i);
  return(0);
}

m_version(cptr, sptr, sender, vers)
struct Client *sptr, *cptr;
char *sender, *vers;
{
  struct Client *acptr = (struct Client *) 0;
  if (vers == NULL || *vers == '\0' ||
      (acptr = find_server(vers, NULL)) == (struct Client *) 0)
    sendto_one(sptr,":%s %d %s %s %s", myhostname, RPL_VERSION,
	       sptr->nickname, version, myhostname);
  if (acptr)
    sendto_one(acptr, ":%s VERSION %s", sptr->nickname, vers); 
  return(0);
}

m_quit(cptr, sptr)
struct Client *cptr, *sptr;
{
/*  if (sptr->fd >= 0 && strcmp(sptr->fromhost, cptr->fromhost))
    return(FLUSH_BUFFER); */  /* Fix 12 Mar 1989 by Jto */
  if (sptr->status != STAT_SERVER) {
    m_bye(cptr, sptr, 0);
    if (cptr == sptr)
      return(FLUSH_BUFFER);
    else
      return(0);
  }
  return(0);
}

m_squit(cptr, sptr, dummy, server)
struct Client *cptr, *sptr;
char *dummy, *server;
{
  struct Client *acptr = find_server(server, NULL);
  if (sptr->status != STAT_SERVER && sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d %s :'tis is no game for mere mortal souls",
	       myhostname, ERR_NOPRIVILEGES, sptr->nickname);
    return(0);
  }
  if (acptr)
    m_bye(cptr, acptr, 0);
  return(0);
}

m_server(cptr, sptr, user, host, server)
struct Client *cptr, *sptr;
char *host, *server, *user;
{
  struct Client *acptr;
  struct Confitem *aconf, *bconf;
  if (host == NULL || *host == '\0') {
    sendto_one(cptr,"ERROR :No hostname");
    return(0);
  }
  if (find_server(host, NULL)) {
    sendto_one(sptr,"ERROR :Such name already exists... ");
    if (cptr == sptr) {
      m_bye(cptr, sptr, 0);
      return(FLUSH_BUFFER);
    } else
      return(0);
  }
  if (sptr->status == STAT_SERVER) {
    if (server == NULL || *server == '\0') {
      sendto_one(cptr,"ERROR :No servername specified");
      return(0);
    }
    acptr = make_client();
    strncpy(acptr->host,host,HOSTLEN); strncpy(acptr->server,server,HOSTLEN);
    acptr->host[HOSTLEN] = '\0';   acptr->server[HOSTLEN] = '\0';
    acptr->status = STAT_SERVER;
    acptr->next = client;
    strncpy(acptr->fromhost,sptr->host,HOSTLEN);
    client = acptr;
    sendto_serv_butone(cptr,"SERVER %s %s",acptr->host, acptr->server);
    return(0);
  } else if ((sptr->status == STAT_CLIENT) || (sptr->status == STAT_OPER)) {
    sendto_one(cptr,"ERROR :Client may not currently become server");
    return(0);
  } else if (sptr->status == STAT_UNKNOWN || sptr->status == STAT_HANDSHAKE) {
    if ((aconf = find_conf(sptr->sockhost, NULL, host, CONF_NOCONNECT_SERVER))
	== 0) {
      sendto_one(sptr,"ERROR :Access denied (no such server enabled) (%s@%s)",
		 host, (server) ? (server) : "Master");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    if ((bconf = find_conf(sptr->sockhost, NULL, host, CONF_CONNECT_SERVER))
	== 0) {
      sendto_one(sptr, "ERROR :Only C field for server.");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    if (*(aconf->passwd) && strcmp(aconf->passwd, sptr->passwd)) {
      sendto_one(sptr,"ERROR :Access denied (passwd mismatch)");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    strncpy(sptr->host, host, HOSTLEN);   sptr->host[HOSTLEN] = '\0';
    if (server && *server) {
      strncpy(sptr->server, server, HOSTLEN); sptr->server[HOSTLEN] = '\0';
    }
    else {
      strncpy(sptr->server, myhostname, HOSTLEN); sptr->server[HOSTLEN] = '\0';
    }
    strncpy(sptr->fromhost, host, HOSTLEN); sptr->fromhost[HOSTLEN] = '\0';
    acptr = client;
    if (sptr->status == STAT_UNKNOWN) {
      if (aconf->passwd[0])
	sendto_one(cptr,"PASS %s",bconf->passwd);
      sendto_one(cptr,"SERVER %s %s", myhostname, 
		 (me.server[0]) ? (me.server) : "Master");
    }
    sptr->status = STAT_SERVER;
    sendto_ops("Link with %s established.", sptr->host);
    sendto_serv_butone(cptr,"SERVER %s %s",sptr->host, sptr->server);
    while (acptr) {
      if ((acptr->status == STAT_CLIENT) ||
	  (acptr->status == STAT_OPER)) {
	sendto_one(cptr,"NICK %s",acptr->nickname);
	sendto_one(cptr,":%s USER %s %s %s :%s",acptr->nickname,
		   acptr->username, acptr->host, acptr->server,
		   acptr->realname);
	sendto_one(cptr,":%s CHANNEL %d",acptr->nickname, acptr->channel);
	if (acptr->status == STAT_OPER) 
	  sendto_one(cptr,":%s OPER", acptr->nickname);
      } else if (acptr->status == STAT_SERVER && acptr != sptr) {
	  sendto_one(cptr,"SERVER %s %s",acptr->host, acptr->server);
      }
      acptr = acptr->next;
    }
    return(0);
  }
  return(0);
}

m_kill(cptr, sptr, sender, user, path)
struct Client *cptr, *sptr;
char *sender, *user, *path;
{
  struct Client *acptr;
  char *oldpath;
  debug(DEBUG_DEBUG,"%s: m_kill: %s", sptr->nickname, user);
  if ((user == NULL) || (*user == '\0')) {
    sendto_one(sptr,":%s %d %s :No user specified",
	       myhostname, ERR_NEEDMOREPARAMS, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if (sptr->status != STAT_OPER && sptr->status != STAT_SERVER) {
    sendto_one(sptr,":%s %d %s :Death before dishonor ?",
	       myhostname, ERR_NOPRIVILEGES, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if ((acptr = find_client(user, NULL)) == (struct Client *) 0) {
    sendto_one(sptr,":%s %d %s :Hunting for ghosts ?",
	       myhostname, ERR_NOSUCHNICK, sptr->nickname);
    return(0);
  }
  if (sptr->fd >= 0)
    sendto_one(sptr,":%s %d %s %s :May his soul rest in peace...",
	       myhostname, RPL_KILLDONE, sptr->nickname, acptr->nickname);
  if (path)
    oldpath = path;
  else
    oldpath = (sptr->nickname[0]) ? sptr->nickname : sptr->host;
  sendto_ops("Received KILL message for %s. Path: %s!%s", user, cptr->host,
	     oldpath);
  sendto_serv_butone(cptr,":%s KILL %s :%s!%s",
		     (sptr->nickname[0]) ? sptr->nickname : sptr->host,
		     user, cptr->host, oldpath);
  sendto_one(acptr,":%s KILL %s :%s!%s", 
	     (sptr->nickname[0]) ? sptr->nickname : sptr->host,
	     user, cptr->host, oldpath);
  m_bye(acptr, acptr, 0);
  return(0);
}

m_info(cptr, sptr)
struct Client *cptr, *sptr;
{
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO, 
	     sptr->nickname, info1);
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO,
	     sptr->nickname, info2);
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO,
	     sptr->nickname, info3);
  return(0);
}

m_links(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *sender, *para;
{
  struct Client *acptr = client;
  if (sptr->status != STAT_CLIENT && sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
  }
  while (acptr) {
    if ((acptr->status == STAT_SERVER || acptr->status == STAT_ME) &&
	(para == NULL || *para == '\0' || matches(para, acptr->host) == 0))
      sendto_one(sptr,"LINREPLY %s %s", acptr->host,
		 (acptr->server[0] ? acptr->server : "(Unknown Location)"));
    acptr = acptr->next;
  }
  return(0);
}

m_summon(cptr, sptr, sender, user)
struct Client *sptr, *cptr;
char *user, *sender;
{
  struct Client *acptr=client, *a2cptr=client;
  char namebuf[10],linebuf[10],hostbuf[17],*host;
  int fd, flag;
  if (sptr->status != STAT_CLIENT && sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
  }
  if (user == NULL || *user == '\0') {
    sendto_one(sptr,":%s %d %s (Summon) No recipient given",
	       myhostname, ERR_NORECIPIENT,
	       sptr->nickname);
    return(0);
  }
  if ((host = index(user,'@')) == NULL)
    host = myhostname;
  else 
    *(host++) = '\0';
  if (host == NULL || *host == '\0' || myncmp(host,myhostname,HOSTLEN) == 0) {
    if ((fd = utmp_open()) == -1) {
      sendto_one(sptr,"NOTICE %s Cannot open %s",sptr->nickname,UTMP);
      return(0);
    }
    while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf)) == 0) 
      if (strcmp(namebuf,user) == 0)
	break;
    utmp_close(fd);
    if (flag == -1)
      sendto_one(sptr,"NOTICE %s :%s: No such user found",
		 sptr->nickname, myhostname);
    else
      summon(sptr, namebuf, linebuf);
    return(0);
  }
  while (acptr) 
    if (acptr->status == STAT_SERVER && strcmp(acptr->host, host) == 0)
      break;
    else 
      acptr = acptr->next;
  if (acptr == (struct Client *) 0) 
    sendto_one(sptr,"ERROR No such host found");
  else {
    while(a2cptr)
      if (a2cptr->status == STAT_SERVER &&
	  strcmp(acptr->fromhost, a2cptr->host) == 0)
	break;
    else
      a2cptr = a2cptr->next;
    if (a2cptr == (struct Client *) 0)
      sendto_one(sptr,"ERROR Internal Error. Contact Administrator");
    else 
      sendto_one(a2cptr,":%s SUMMON %s@%s",sptr->nickname, user, host);
  }
  return(0);
}

m_stats(cptr, sptr)
struct Client *cptr, *sptr;
{
  struct Message *mptr = msgtab;
  for (; mptr->cmd; mptr++)
    sendto_one(sptr,"NOTICE %s :%s has been used %d times after startup",
	       sptr->nickname, mptr->cmd, mptr->count);
  return(0);
}

m_users(cptr, sptr, from, host)
struct Client *cptr, *sptr;
char *from, *host;
{
  struct Client *acptr=client, *a2cptr=client;
  char namebuf[10],linebuf[10],hostbuf[17];
  int fd, flag = 0;
  if (sptr->nickname[0] == '\0') {
    sendto_one(sptr,"ERROR Nickname not specified yet");
    return(0);
  }
  if (host == NULL || *host == '\0' || strcmp(host,myhostname) == 0) {
    if ((fd = utmp_open()) == -1) {
      sendto_one(sptr,"NOTICE %s Cannot open %s",sptr->nickname,UTMP);
      return(0);
    }
    sendto_one(sptr,"NOTICE %s :UserID   Terminal Host", sptr->nickname);
    while (utmp_read(fd, namebuf, linebuf, hostbuf) == 0) {
      flag = 1;
      sendto_one(sptr,"NOTICE %s :%-8s %-8s %-8s",
		 sptr->nickname, namebuf, linebuf, hostbuf);
    }
    if (flag == 0) 
      sendto_one(sptr,"NOTICE %s :Nobody logged in on %s",
		 sptr->nickname, host);
    utmp_close(fd);
    return(0);
  }
  while (acptr) 
    if (acptr->status == STAT_SERVER && strcmp(acptr->host, host) == 0)
      break;
    else 
      acptr = acptr->next;
  if (acptr == (struct Client *) 0) 
    sendto_one(sptr,"ERROR No such host found");
  else {
    while(a2cptr)
      if (a2cptr->status == STAT_SERVER &&
	  strcmp(acptr->fromhost, a2cptr->host) == 0)
	break;
    else
      a2cptr = a2cptr->next;
    if (a2cptr == (struct Client *) 0)
      sendto_one(sptr,"ERROR Internal Error. Contact Administrator");
    else 
      sendto_one(a2cptr,":%s USERS %s",sptr->nickname, host);
  }
  return(0);
}

m_bye(cptr, sptr, from) 
struct Client *cptr;
struct Client *sptr;
char *from;
{
  struct Client *acptr;
  close(sptr->fd); sptr->fd = -20;
  if (from == NULL)
    from = "";
  if (sptr->status == STAT_SERVER && sptr == cptr) {
    acptr = client;
    while (acptr) {
      if ((acptr->status == STAT_CLIENT || acptr->status == STAT_OPER) &&
	  strcmp(acptr->fromhost, sptr->host) == 0 &&
	  sptr != acptr) {
	exit_user(cptr, acptr, from);
	acptr = client;
      } else 
	acptr = acptr->next;
    }
    acptr = client;
    while (acptr) {
      if (acptr->status == STAT_SERVER &&
	  strcmp(acptr->fromhost, sptr->host) == 0 &&
	  sptr != acptr) {
	exit_user(cptr, acptr, from);
	acptr = client;
      } else 
	acptr = acptr->next;
    }
  }
  exit_user(cptr, sptr, from);
  return(0);
}

exit_user(cptr, sptr, from)
struct Client *sptr;
struct Client *cptr;
char *from;
{
  struct Client *acptr;
  struct Channel *chptr = channel, *pchptr;
  int i;
  if ((sptr->status == STAT_CLIENT || sptr->status == STAT_OPER) &&
      ((i = sptr->channel) != 0)) {
    sptr->channel = 0;
    sendto_channel_butserv(i,":%s QUIT :%s!%s", sptr->nickname,
			   from, myhostname);
  } else
    i = 0;
  if (sptr->status == STAT_SERVER)
    sendto_serv_butone(cptr,"SQUIT %s :%s!%s",sptr->host,
		       from, myhostname);
  else if (sptr->nickname[0])
    sendto_serv_butone(cptr,":%s QUIT :%s!%s",sptr->nickname,
		       from, myhostname);
  pchptr = (struct Channel *) 0;
  while (chptr) {
    if (chptr->channo == i && chptr->users > 0) 
      --chptr->users;
    if (chptr->users <= 0) {
      if (pchptr)
	pchptr->nextch = chptr->nextch;
      else
	channel = chptr->nextch;
      free(chptr);
    }
    pchptr = chptr;
    chptr = chptr->nextch;
  }
  if (sptr->fd >= 0)
    close (sptr->fd);
  if (sptr == client) {
    client = client->next;
    if (sptr->away)
      free(sptr->away);
    free(sptr);
    return(0);
  }
  acptr = client;
  while (acptr && acptr->next != sptr) 
    acptr = acptr->next;
  if (acptr) {
    acptr->next = sptr->next;
  } else {
    debug(DEBUG_FATAL, "List corrupted");
    restart();
  }
  if (sptr->away)
    free(sptr->away);
  free(sptr);
}

m_error(cptr, sptr, sender, para1, para2, para3, para4)
struct Client *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4;
{
  debug(DEBUG_ERROR,"Received ERROR message from %s: %s %s %s %s",
	sptr->nickname, para1, para2, para3, para4);
  return(0);
}

m_help(cptr, sptr)
struct Client *cptr, *sptr;
{
  int i;
  for (i = 0; msgtab[i].cmd; i++)
    sendto_one(sptr,"NOTICE %s :%s",sptr->nickname,msgtab[i].cmd);
  return(0);
}

m_whoreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
struct Client *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Whoreply", para1, para2, para3);
}

m_restart(cptr, sptr)
struct Client *cptr, *sptr;
{
  if (sptr->status == STAT_OPER)
    restart();
}

m_die(cptr, sptr)
struct Client *cptr, *sptr;
{
  if (sptr->status == STAT_OPER)
    exit(-1);
}

dowelcome(sptr)
struct Client *sptr;
{
  sendto_one(sptr,"NOTICE %s :*** %s%s", sptr->nickname, welcome1,version);
  m_lusers(sptr, sptr, NULL, NULL);
}

m_lusers(cptr, sptr, sender)
struct Client *cptr, *sptr;
char *sender;
{
  int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
  struct Client *acptr;
  acptr = client;
  while (acptr) {
    switch (acptr->status) {
    case STAT_SERVER:
    case STAT_ME:
      s_count++;
      break;
    case STAT_OPER:
/*      if (acptr->channel >= 0 || sptr->status == STAT_OPER) */
	o_count++;
    case STAT_CLIENT:
      if (acptr->channel >= 0)
	c_count++;
      else
	i_count++;
      break;
    default:
      u_count++;
      break;
    }
    acptr = acptr->next;
  }
  if (sptr->status == STAT_OPER && i_count)
    sendto_one(sptr,
	  "NOTICE %s :*** There are %d users (and %d invisible) on %d servers",
	       sptr->nickname, c_count, i_count, s_count);
  else
    sendto_one(sptr,"NOTICE %s :*** There are %d users on %d servers",
	       sptr->nickname, c_count, s_count);
  if (o_count)
    sendto_one(sptr,
	       "NOTICE %s :*** %d user%s connection to the twilight zone",
	       sptr->nickname, o_count, (o_count > 1) ? "s have" : " has");
  if (u_count > 0)
    sendto_one(sptr,"NOTICE %s :*** There are %d yet unknown connections",
	       sptr->nickname, u_count);
}
  
  
/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *            Not currently really working, I don't like this
 *            call at all...
 ***********************************************************************/

m_away(cptr, sptr, sender, msg)
struct Client *cptr, *sptr;
char *msg;
{
  if (sptr->status != STAT_CLIENT && sptr->status != STAT_SERVICE &&
      sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
    return(-1);
  }
  if (sptr->away) {
    free(sptr->away);
    sptr->away = NULL;
  }
  if (msg && *msg) {
    sptr->away = (char *) malloc((unsigned int) (strlen(msg)+1));
    if (sptr->away == NULL)
      sendto_one(sptr,"ERROR :Randomness of the world has proven its power!");
    else {
      sendto_one(sptr,"NOTICE %s :You have marked as being away",
	         sptr->nickname);
      strcpy(sptr->away, msg);
    }
  } else {
    sendto_one(sptr,"NOTICE %s :You are no longer marked as being away",
               sptr->nickname);
  }
  return(0);
}

/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************/

m_connect(cptr, sptr, sender, host, portstr)
struct Client *cptr, *sptr;
char *sender, *host, *portstr;
{
  int port, tmpport, retval;
  struct Confitem *aconf;
  extern char *sys_errlist[];
  if (sptr->status != STAT_SERVER && sptr->status != STAT_OPER) {
    sendto_one(cptr,"ERROR :Connect: Privileged command");
    return(-1);
  }
  if (!host || *host == '\0' || !portstr || *portstr == '\0') {
    sendto_one(cptr,"ERROR :Connect: Syntax error -- use CONNECT host port");
    return(-1);
  }
  if ((port = atoi(portstr)) <= 0) {
    sendto_one(cptr,"ERROR :Connect: Illegal port number");
    return(-1);
  }
  aconf = conf;
  while (aconf) {
    if (aconf->status == CONF_CONNECT_SERVER &&
	strcmp(aconf->name, host) == 0) {
      tmpport = aconf->port;
      aconf->port = port;
      switch (retval = connect_server(aconf)) {
      case 0:
	sendto_one(cptr, "NOTICE %s :*** Connection to %s established.",
		   cptr->nickname, aconf->host);
	break;
      case -1:
	sendto_one(cptr, "NOTICE %s :*** Couldn't connect to %s.",
		   cptr->nickname, conf->host);
	break;
      case -2:
	sendto_one(cptr, "NOTICE %s :*** Host %s is unknown.",
		   cptr->nickname, aconf->host);
	break;
      default:
	sendto_one(cptr, "NOTICE %s :*** Connection to %s failed: %s",
		   cptr->nickname, aconf->host, sys_errlist[retval]);
      }
      aconf->port = tmpport;
      break;
    }
    aconf = aconf->next;
  }
  if (aconf == (struct Confitem *) 0)
    sendto_one(cptr, "ERROR :Connect: Host %s not listed in irc.conf.",
	       host);
  return(0);
}

m_ping(cptr, sptr, sender, origin, destination)
struct Client *cptr, *sptr;
char *sender, *origin, *destination;
{
  struct Client *acptr;
  if (origin == NULL || *origin == '\0') {
    sendto_one(sptr,"ERROR :No origin specified.");
    return(-1);
  }
  if (destination && *destination && strcmp(destination, myhostname)) {
    if (acptr = find_server(destination,NULL)) {
      sendto_one(acptr,"PING %s %s", origin, destination);
      return(0);
    } else {
      sendto_one(sptr,"ERROR :No such host found");
      return(-1);
    }
  } else {
    sendto_one(sptr,"PONG %s %s", 
	       (destination) ? destination : myhostname, origin);
    return(0);
  }
}

m_pong(cptr, sptr, sender, origin, destination)
struct Client *cptr, *sptr;
char *sender, *origin, *destination;
{
  struct Client *acptr;
  if (origin == NULL || *origin == '\0') {
    sendto_one(sptr,"ERROR :No origin specified.");
    return(-1);
  }
  cptr->flags &= ~FLAGS_PINGSENT;
  sptr->flags &= ~FLAGS_PINGSENT;
  if (destination && *destination && strcmp(destination, myhostname)) {
    if (acptr = find_server(destination,NULL)) {
      sendto_one(acptr,"PONG %s %s", origin, destination);
      return(0);
    } else {
      sendto_one(sptr,"ERROR :No such host found");
      return(-1);
    }
  } else {
    debug(DEBUG_NOTICE, "PONG: %s %s", origin, destination);
    return(0);
  }
}

/**************************************************************************
 * m_oper() added Sat, 4 March 1989
 **************************************************************************/

m_oper(cptr, sptr, sender, name, password)
struct Client *cptr, *sptr;
char *sender, *name, *password;
{
  struct Confitem *aconf;
  if (sptr->status != STAT_CLIENT || 
      (name == NULL || *name == '\0' || password == NULL ||
      *password == '\0') && cptr == sptr ) {
    sendto_one(sptr, ":%s %d %s :Dave, don't do that...",
	       myhostname, ERR_NEEDMOREPARAMS, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if (cptr->status == STAT_SERVER) {
    sptr->status = STAT_OPER;
    sendto_serv_butone(cptr, ":%s OPER", sptr->nickname);
    return(0);
  }
  if ((aconf = find_conf(cptr->sockhost, NULL, name, CONF_OPERATOR)) 
      == (struct Confitem *) 0 &&
      (aconf = find_conf(cptr->host, NULL, name, CONF_OPERATOR))
      == (struct Confitem *) 0) {
    sendto_one(sptr, ":%s %d %s :Only few of mere mortals may try to %s",
	       myhostname, ERR_NOOPERHOST, sptr->nickname,
	       "enter twilight zone");
    return(0);
  } 
  if (aconf->status == CONF_OPERATOR &&
      strcmp(password, aconf->passwd) == 0) {
    sendto_one(sptr, ":%s %d %s :Good afternoon, gentleman. %s",
	       myhostname, RPL_YOUREOPER, sptr->nickname,
	       "I am a HAL 9000 computer.");
    sptr->status = STAT_OPER;
    sendto_serv_butone(cptr, ":%s OPER", sptr->nickname);
  } else {
    sendto_one(sptr, ":%s %d %s :Only real wizards do know the %s",
	       myhostname, ERR_PASSWDMISMATCH, sptr->nickname,
	       "spells to open the gates of paradise");
    return(0);
  }
  return(0);
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

m_pass(cptr, sptr, sender, password)
struct Client *cptr, *sptr;
char *sender, *password;
{
  if ((password == NULL) || (*password == '\0')) {
    sendto_one(cptr,":%s %d %s :No password is not good password",
	       myhostname, ERR_NEEDMOREPARAMS, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if (sptr != cptr || (cptr->status != STAT_UNKNOWN &&
                       cptr->status != STAT_HANDSHAKE)) {
    sendto_one(cptr,":%s %d %s :Trying to unlock the door twice, eh ?",
	       myhostname, ERR_ALREADYREGISTRED, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  strncpy(cptr->passwd, password, PASSWDLEN);
  cptr->passwd[PASSWDLEN] = '\0';
  return(0);
}

m_wall(cptr, sptr, sender, message)
struct Client *cptr, *sptr;
char *sender, *message;
{
  if (sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d %s :Only real wizards know the correct %s",
	       myhostname, ERR_NOPRIVILEGES, 
	       (sptr->nickname[0] == '\0') ? "*" : sptr->nickname,
	       "spells to open this gate");
    return(0);
  }
  if (sender && *sender) {
    if (cptr == sptr)
      sendto_all_butone(NULL, ":%s WALL :%s", sender, message);
    else
      sendto_all_butone(cptr, ":%s WALL :%s", sender, message);
  }
  else if (cptr == sptr)
    sendto_all_butone(NULL, ":%s WALL :%s", sptr->nickname, message);
  else
    sendto_all_butone(cptr, ":%s WALL :%s", sptr->nickname, message);
  return(0);
}

/**************************************************************************
 * time() - Command added 23 March 1989
 **************************************************************************/

m_time(cptr, sptr, sender, destination)
struct Client *cptr, *sptr;
char *sender, *destination;
{
  struct Client *acptr;
  if (sptr->status != STAT_CLIENT && sptr->status != STAT_OPER) {
    sendto_one(sptr, ":%s %d %s :You haven't registered yourself yet",
	       myhostname, ERR_NOTREGISTERED, "*");
    return (-1);
  }
  if (destination && *destination && matches(destination, myhostname)) {
    if ((acptr = find_server(destination,NULL)) &&
	(acptr->status == (short) STAT_SERVER)) {
      sendto_one(acptr,":%s TIME %s", sptr->nickname, destination);
      return(0);
    } else {
      sendto_one(sptr,":%s %d %s %s :No such host found",
		 myhostname, ERR_NOSUCHSERVER, sptr->nickname,
		 destination);
      return(-1);
    }
  } else {
    sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_TIME,
	       sptr->nickname, myhostname, date());
    return(0);
  }
}

/************************************************************************
 * m_rehash() - Added by Jto 20 Apr 1989
 ************************************************************************/

m_rehash(cptr, sptr)
struct Client *cptr, *sptr;
{
  if (sptr->status != STAT_OPER) {
    sendto_one(sptr,":%s %d %s :Use the force, Luke !",
	       myhostname, ERR_NOPRIVILEGES, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(-1);
  }
  sendto_one(sptr,":%s %d %s :Rereading irc.conf -file...",
	     myhostname, RPL_REHASHING, sptr->nickname);
  rehash();
}

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

m_names(cptr, sptr, sender, para)
struct Client *cptr, *sptr;
char *para, *sender;
{ 
  struct Channel *chptr = channel;
  struct Client *c2ptr;
  int idx, flag;

  while (chptr) {
    if (para == NULL || *para == '\0' ||
	chan_match(chptr, chan_conv(para))) {
      if (!chan_isprivate(chptr) || chan_match(chptr, sptr->channel)) {
	c2ptr = client;
	if (chan_isprivate(chptr))
	  sprintf(buf, "* %d ", chptr->channo);
	else
	  sprintf(buf, "= %d ", chptr->channo);
	idx = strlen(buf);
	flag = 0;
	while (c2ptr) {
	  if ((c2ptr->status == STAT_OPER || c2ptr->status == STAT_CLIENT) &&
	      chan_match(chptr, c2ptr->channel)) {
	    strncat(buf, c2ptr->nickname, NICKLEN);
	    idx += strlen(c2ptr->nickname) + 1;
	    flag = 1;
	    strcat(buf," ");
	    if (idx + NICKLEN > BUFSIZE - 2) {
	      sendto_one(sptr, "NAMREPLY %s", buf);
	      if (chan_isprivate(channel))
		sprintf(buf, "* %d ", chptr->channo);
	      else
		sprintf(buf, "= %d ", chptr->channo);
	      idx = strlen(buf);
	      flag = 0;
	    }
	  }
	  c2ptr = c2ptr->next;
	}
	if (flag)
	  sendto_one(sptr, "NAMREPLY %s", buf);
      }
    }
    chptr = chptr->nextch;
  }

  if (para != NULL && *para)
    return(1);
  strcpy(buf, "* * ");
  idx = strlen(buf);
  c2ptr = client;
  flag = 0;
  while (c2ptr) {
    if ((c2ptr->status == STAT_OPER || c2ptr->status == STAT_CLIENT) &&
	((c2ptr->channel > 999 && (sptr->channel != c2ptr->channel)) ||
	c2ptr->channel == 0)) {
      strncat(buf, c2ptr->nickname, NICKLEN);
      idx += strlen(c2ptr->nickname) + 1;
      strcat(buf," ");
      flag = 1;
      if (idx + NICKLEN > BUFSIZE - 2) {
	sendto_one(sptr, "NAMREPLY %s", buf);
	strcpy(buf, "* * ");
	idx = strlen(buf);
	flag = 0;
      }
    }
    c2ptr = c2ptr->next;
  }
  if (flag)
    sendto_one(sptr, "NAMREPLY %s", buf);
  return(1);
}

m_namreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
struct Client *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Namreply", para1, para2, para3);
}

m_linreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
struct Client *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Linreply", para1, para2, para3);
}

m_admin(cptr, sptr, sender, para1)
struct Client *cptr, *sptr;
char *sender, *para1;
{
  struct Confitem *aconf;
  struct Client *acptr;
  if (para1 && *para1 && matches(para1, myhostname)) {
    /* Isn't for me... */
    if ((acptr = find_server(para1, (struct Client *) 0)) &&
	(acptr->status == STAT_SERVER)) {
      sendto_one(acptr, ":%s ADMIN :%s", sptr->nickname, para1);
      return(0);
    } else {
      sendto_one(sptr, "NOTICE %s :*** No such server (%s).",
		 sptr->nickname, para1);
      return (-1);
    }
  }
  if (aconf = find_admin()) {
    sendto_one(sptr, "NOTICE %s :### Administrative info about %s",
	       sptr->nickname, myhostname);
    sendto_one(sptr, "NOTICE %s :### %s", sptr->nickname, aconf->host);
    sendto_one(sptr, "NOTICE %s :### %s", sptr->nickname, aconf->passwd);
    sendto_one(sptr, "NOTICE %s :### %s", sptr->nickname, aconf->name);
  } else
    sendto_one(sptr, 
	    "NOTICE %s :### No administrative info available about server %s",
	       sptr->nickname, myhostname);
  return(0);
}

m_trace(cptr, sptr, sender, para1)
struct Client *cptr, *sptr;
char *sender, *para1;
{
  struct Client *acptr;
  if (sptr->status != STAT_OPER) {
    sendto_one(sptr,
       "NOTICE %s :*** Error: %s", sptr->nickname,
	       "No mere mortals may trace the nets of the universe");
    return(-1);
  }
  if (para1 && *para1 && myncmp(para1, myhostname, HOSTLEN) &&
      (acptr = find_server(para1, (struct Client *) 0))) {
    sendto_one(acptr, ":%s TRACE :%s", sptr->nickname, para1);
    sendto_one(sptr, "NOTICE %s :*** Link %s ==> %s",
	       sptr->nickname, myhostname, para1);
    return(0);
  }
  acptr = client;
  while (acptr) {
    if (acptr->status == STAT_SERVER && acptr->fd >= 0)
      sendto_one(sptr, "NOTICE %s :*** Connection %s ==> %s",
		 sptr->nickname, myhostname, acptr->host);
    acptr = acptr->next;
  }
  return(0);
}

m_motd(cptr, sptr, sender, para1)
struct Client *cptr, *sptr;
char *sender, *para1;
{
  struct Confitem *aconf;
  struct Client *acptr;
  FILE *fptr;
  char line[80], *tmp;

  if (para1 && *para1 && matches(para1, myhostname)) {
    /* Isn't for me... */
    if ((acptr = find_server(para1, (struct Client *) 0)) &&
	(acptr->status == STAT_SERVER)) {
      sendto_one(acptr, ":%s MOTD :%s", sptr->nickname, para1);
      return(0);
    } else {
      sendto_one(sptr, "NOTICE %s :*** No such server (%s).",
		 sptr->nickname, para1);
      return (-1);
    }
  }
#ifndef MOTD
  sendto_one(sptr,
	     ":%s %d %s :No message-of-today available in server %s",
	     myhostname, RPL_MOTD, sptr->nickname, myhostname);
#else
  if ((fptr = fopen(MOTD, "r")) == (FILE *) 0) {
    sendto_one(sptr,
	       ":%s %d %s :Message-of-today not found in server %s",
	       myhostname, RPL_MOTD, sptr->nickname, myhostname);
  } else {
    sendto_one(sptr, ":%s %d %s :start at server %s:", myhostname,
	       RPL_MOTD, sptr->nickname, myhostname);
    while (fgets(line, 80, fptr)) {
      if (tmp = index(line,'\n'))
	*tmp = '\0';
      if (tmp = index(line,'\r'))
	*tmp = '\0';
      sendto_one(sptr, ":%s %d %s :%s", myhostname, RPL_MOTD,
		 sptr->nickname, line);
    }
    sendto_one(sptr, ":%s %d %s :end at server %s:", myhostname,
	       RPL_MOTD, sptr->nickname, myhostname);
  }
#endif
  return(0);
}

