/*************************************************************************
 ** s_msg.c  Beta  v2.1    (22 Aug 1988)
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

extern aClient *client, me;
extern aClient *find_client();
extern aClient *find_server();
extern aConfItem *find_conf(), *find_admin();
extern aConfItem *conf;
extern aChannel *find_channel();
extern char *center();
extern char *uphost;
aChannel *channel = (aChannel *) 0;
extern int maxusersperchannel;

char buf[BUFSIZE];
char buf2[80];

#define BadChar(x) ((x) < 65 || (x) > 125)

int m_nick(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{
  aClient *acptr;
  char *ch;
  int flag = 0;
  if (para == NULL || *para == '\0') {
    sendto_one(sptr,":%s %d %s :No nickname given",
	       myhostname, ERR_NONICKNAMEGIVEN, sptr->nickname);
  } else {
    ch = para;
    if (isdigit(*ch) || *ch == '-' || *ch == '+')
      *ch = '\0';
    for (; *ch; ch++) 
      if (BadChar(*ch) && !isdigit(*ch) && *ch != '_' && *ch != '-') 
        *ch = '\0';
    
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
      if (ISSERVER(cptr)) {
	sendto_serv_butone(NULL, "KILL %s :%s", acptr->nickname, myhostname);
	m_bye(acptr, acptr, 0);
      }
    } else {
      if (ISSERVER(sptr)) {
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

#define CheckRegister(x, host) \
  if ((x)->status < 0) { \
    sendto_one((x), ":%s %d * :You have not registered yourself yet", \
	       (host), ERR_NOTREGISTERED); \
    return (-1); \
  }

#define CheckText(p, s, h) \
  if ((p) == NULL || *(p) == '\0') {\
    sendto_one((s),":%s %d %s :No text to send", (h), \
      ERR_NOTEXTTOSEND, (s)->nickname); \
    return -1; \
  }

#define CheckJoin(s, h) \
  if (!(s)->channel) { \
    sendto_one((s),":%s %d %s :You have not joined any channel", (h), \
      ERR_USERNOTINCHANNEL, (s)->nickname); \
    return -1; \
  }
     
int m_text(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{
  CheckRegister(sptr, myhostname);
  CheckText(para, sptr, myhostname);
  CheckJoin(sptr, myhostname);

  sendto_channel_butone(cptr, sptr->channel, ":%s MSG :%s", 
			  sptr->nickname, para);
  return(0);
}

#define CheckRecipient(p, s, h) \
  if ((p) == NULL || *(p) == '\0') { \
    sendto_one((s),":%s %d %s :No recipient given", \
	       (h), ERR_NORECIPIENT, (s)->nickname); \
    return (-1); \
  }

int m_private(cptr, sptr, sender, para, para2)
aClient *cptr, *sptr;
char *para, *para2, *sender;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;

  CheckRegister(sptr, myhostname);
  CheckRecipient(para, sptr, myhostname); 
  CheckText(para2, sptr, myhostname);

  tmp = para;
  do {
    nick = tmp;
    if (tmp = index(nick, ',')) {
      *(tmp++) = '\0';
    }
    acptr = find_client(nick, NULL);
    if (acptr) {
      fprintf(stderr,"Found client '%s' on '%s'\n",acptr->nickname,acptr->host);
      if (acptr->away) 
	sendto_one(sptr,":%s %d %s %s :%s", myhostname, RPL_AWAY,
		   sptr->nickname, acptr->nickname, acptr->away);
      
      sendto_one(acptr, ":%s PRIVMSG %s :%s", sptr->nickname, nick, para2);
    } 
    else if (chptr = find_channel(nick, NULL)) {
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

int m_xtra(cptr, sptr, sender, para, para2)
aClient *cptr, *sptr;
char *para, *para2, *sender;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;
  CheckRegister(sptr, myhostname);
  CheckRecipient(para, sptr, myhostname);  
  CheckText(para2, sptr, myhostname);  
  
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

int m_grph(cptr, sptr, sender, para, para2)
aClient *cptr, *sptr;
char *para, *para2, *sender;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;
   
  CheckRegister(sptr, myhostname);
  CheckRecipient(para, sptr, myhostname);
  CheckText(para2, sptr, myhostname);
  
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

int m_voice(cptr, sptr, sender, para, para2)
aClient *cptr, *sptr;
char *para, *para2, *sender;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;

  CheckRegister(sptr, myhostname);
  CheckRecipient(para, sptr, myhostname); 
  CheckText(para2, sptr, myhostname); 
  
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
    } 
    else if (chptr = find_channel(nick, NULL)) {
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

int m_notice(cptr, sptr, sender, para, para2)
aClient *cptr, *sptr;
char *para, *para2, *sender;
{
  aClient *acptr;
  aChannel *chptr;
  char *nick, *tmp;
  CheckRegister(sptr, myhostname);
  CheckRecipient(para, sptr, myhostname);
  CheckText(para2, sptr, myhostname);
  
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

int m_who(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{
  int i;
  aClient *acptr = client;
  if (para == NULL || para[0] == '\0')
    i = 0;
  else if (index(para,'*'))
    i = sptr->channel;
  else
    i = atoi(para);
  sendto_one(sptr,"WHOREPLY * User Host Server Nickname S :Name");
  for (; acptr; acptr = acptr->next) {
    int chan = acptr->channel;

    if (i != 0 && acptr->channel != i)
      continue;
    if (!ISCLIENT(acptr) && !ISOPER(acptr))
      continue;

    if (!ISOPER(sptr))		/* dont reveal if not oper */
      if (chan < 0 && chan != sptr->channel)
        continue; 

    {
      char awaych = acptr->away ? 'G' : 'H';
      char operch = ISOPER(acptr) ? '*' : ' ';
      int channel =  VisibleChannel(chan, sptr->channel) ? chan : 0;
                   
#ifdef NOETHICS
      if (ISOPER(sptr))
         channel = chan;
#endif
      sendto_one(sptr, "WHOREPLY %d %s %s %s %s %c%c :%s",
	   	channel, acptr->username, acptr->host, acptr->server, 
                acptr->nickname, awaych, operch, acptr->realname);
    }
  }
  return(0);
}

int m_whois(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{
  char *cp;
  aClient *acptr, *a2cptr;
  
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
    if (cp)
      *cp = '\0';
    acptr = find_client(para,NULL);
    if (acptr == (aClient *) 0)
      sendto_one(sptr, ":%s %d %s %s :No such nickname",
		 myhostname, ERR_NOSUCHNICK, sptr->nickname, para);
    else {
      a2cptr = find_server(acptr->server);
      if (VisibleChannel(acptr->channel, sptr->channel))
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
      if (ISOPER(acptr))
	sendto_one(sptr, ":%s %d %s %s :is an operator", myhostname,
		   RPL_WHOISOPERATOR, sptr->nickname,
		   acptr->nickname);
    }
    if (cp != NULL)
      para = ++cp;
  } while (cp != NULL && *para != '\0');
  return(0);
}

int m_user(cptr, sptr, sender, username, host, server, realname)
aClient *cptr, *sptr;
char *username, *server, *host, *realname, *sender;
{
  aConfItem *aconf;
  if (!username  || !server  || !host  || !realname || *username == '\0' || 
       *server == '\0' || *host == '\0' || *realname == '\0')
    sendto_one(sptr,":%s %d * :Not enough parameters",
	       myhostname, ERR_NEEDMOREPARAMS);
  else if (sptr->status != STAT_UNKNOWN && !ISOPER(sptr))
    sendto_one(sptr,":%s %d * :Identity problems, eh ?",
	       myhostname, ERR_ALREADYREGISTRED);
  else {
    if (sptr->fd >= 0) {
      if (!(aconf = find_conf(sptr->sockhost, NULL, NULL, CONF_CLIENT))) {
	if ((aconf = find_conf("", NULL, NULL, CONF_CLIENT)) == 0) {
	  sendto_one(sptr,"%s %d * :Your host isn't among the privileged..",
		     myhostname, ERR_NOPERMFORHOST);
	  m_bye(sptr, sptr, 0);
	  return(FLUSH_BUFFER);
	} 
      } else 
	if (!Equal(sptr->passwd, aconf->passwd)) {
	  sendto_one(sptr,"%s %d * :Only correct words will open this gate..",
		     myhostname, ERR_PASSWDMISMATCH);
	  m_bye(sptr, sptr, 0);
	  return(FLUSH_BUFFER);
	}
      host = sptr->sockhost;
      if (find_conf(host, NULL, username, CONF_KILL)) {
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
    if (sptr->host[0] == '\0') 
      CopyFixLen(sptr->host, host, HOSTLEN+1);
    
    strcpy(sptr->realname, realname);
    sptr->realname[REALLEN] = '\0';
    if (sptr->status == STAT_CLIENT)
      sendto_serv_butone(cptr,":%s USER %s %s %s :%s", sptr->nickname,
			 username, sptr->host, sptr->server, realname);
  }
  return(0);
}

int m_list(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{
  aChannel *chptr = channel;
  sendto_one(sptr,":%s %d %s :  Channel  : Users  Name",
	     myhostname, RPL_LISTSTART, sptr->nickname);
  for (; chptr; chptr = chptr->nextch) {
    if (VisibleChannel(chptr->channo, sptr->channel))
      sprintf(buf2,"%d",chptr->channo);
    else
      strcpy(buf2,"*");
    sendto_one(sptr,":%s %d %s %s %d :%s", myhostname, RPL_LIST, 
               sptr->nickname, buf2, chptr->users, chptr->name);
  }
  return(0);
}

int m_topic(cptr, sptr, sender, topic)
aClient *cptr, *sptr;
char *topic, *sender;
{
  aChannel *chptr = channel;
  CheckRegister(sptr, myhostname);
  
  if (topic != NULL && *topic != '\0')
    sendto_serv_butone(cptr,":%s TOPIC %s",sptr->nickname,topic);
  for (; chptr; chptr = chptr->nextch) {
    if (chptr->channo != sptr->channel)
      continue; 
      if (topic == NULL || topic[0] == '\0') {
	if (chptr->name[0] == '\0')
	  sendto_one(sptr, ":%s %d %s :No topic is set.", 
		     myhostname, RPL_NOTOPIC, sptr->nickname);
	else
	  sendto_one(sptr, ":%s %d %s :%s",
		     myhostname, RPL_TOPIC, sptr->nickname, chptr->name);
	return (0);
      } else {
	CopyFixLen(chptr->name, topic, CHANNELLEN);
	sendto_channel_butserv(chptr->channo, ":%s TOPIC %s", sptr->nickname,
			       chptr->name);
      }
  }
  return(0);
}

int m_invite(cptr, sptr, sender, user, channel)
aClient *cptr, *sptr;
char *user, *channel, *sender;
{
  aClient *acptr;
  int i;
  CheckRegister(sptr, myhostname);

  if (!user || *user == '\0') {
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
  if (!acptr) 
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

int m_channel(cptr, sptr, sender, ch)
aClient *cptr, *sptr;
char *ch, *sender;
{
  aChannel *chptr, *pchptr;
  int i;
  CheckRegister(sptr, myhostname);
  
  chptr = channel;
  if (!ch) {
    sendto_one(sptr, "NOTICE %s :*** What channel do you want to join?",
	       sptr->nickname);
    return (0);
  }
  else
    i = atoi(ch);
  pchptr = (aChannel *) 0;
  if (i > 0) {
    for ( ; chptr; chptr = chptr->nextch) 
      if (chptr->channo == i)
	break;
    
    if (!chptr) {
      chptr = (aChannel *) MyMalloc(sizeof(aChannel)); 
      chptr->nextch = channel;
      chptr->channo = i;
      chptr->name[0] = '\0';
      chptr->users = 0;
      channel = chptr;
    } 
      else if (cptr == sptr && is_full(i, chptr->users)) {
      sendto_one(sptr,":%s %d %s %d :Sorry, Channel is full.",
		 myhostname, ERR_CHANNELISFULL, sptr->nickname, i);
      return(0);
    }
    chptr->users++;
  }
  for (chptr = channel ; chptr; pchptr = chptr, chptr = chptr->nextch) {
    if (chptr->channo == sptr->channel && chptr->users > 0) 
      --chptr->users;
    if (chptr->users <= 0) {
      if (pchptr)
	pchptr->nextch = chptr->nextch;
      else
	channel = chptr->nextch;
      free(chptr);
    }
  }
  sendto_serv_butone(cptr, ":%s CHANNEL %d", sptr->nickname, i);
  if (sptr->channel != 0)
    sendto_channel_butserv(sptr->channel, ":%s CHANNEL 0", sptr->nickname);
  sptr->channel = i;
  if (sptr->channel != 0)
    sendto_channel_butserv(i, ":%s CHANNEL %d", sptr->nickname, i);
  return(0);
}

int m_version(cptr, sptr, sender, vers)
aClient *sptr, *cptr;
char *sender, *vers;
{
  aClient *acptr = (aClient *) 0;
  if (vers == NULL || *vers == '\0' || !(acptr = find_server(vers, NULL)))
    sendto_one(sptr,":%s %d %s %s %s", myhostname, RPL_VERSION,
	       sptr->nickname, version, myhostname);
  if (acptr)
    sendto_one(acptr, ":%s VERSION %s", sptr->nickname, vers); 
  return(0);
}

int m_quit(cptr, sptr)
aClient *cptr, *sptr;
{
/*  if (sptr->fd >= 0 && !Equal(sptr->fromhost, cptr->fromhost))
    return(FLUSH_BUFFER); */  /* Fix 12 Mar 1989 by Jto */

  if (sptr->status != STAT_SERVER) {
    m_bye(cptr, sptr, 0);
    return cptr == sptr ? FLUSH_BUFFER : 0; 
  }
  return(0);
}

int m_squit(cptr, sptr, dummy, server)
aClient *cptr, *sptr;
char *dummy, *server;
{
  aClient *acptr = find_server(server, NULL);
  if (sptr->status != STAT_SERVER && !ISOPER(sptr)) {
    sendto_one(sptr,":%s %d %s :'tis is no game for mere mortal souls",
	       myhostname, ERR_NOPRIVILEGES, sptr->nickname);
    return(0);
  }
  if (acptr)
    m_bye(cptr, acptr, 0);
  return(0);
}

int m_server(cptr, sptr, user, host, server)
aClient *cptr, *sptr;
char *host, *server, *user;
{
  aClient *acptr;
  aConfItem *aconf, *bconf;
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
  if (ISSERVER(sptr)) {
    if (server == NULL || *server == '\0') {
      sendto_one(cptr,"ERROR :No servername specified");
      return(0);
    }
    acptr = make_client();
    strncpy(acptr->host,host,HOSTLEN); 
    strncpy(acptr->server,server,HOSTLEN);
    acptr->host[HOSTLEN] = '\0';   acptr->server[HOSTLEN] = '\0';
    acptr->status = STAT_SERVER;
    acptr->next = client;
    strncpy(acptr->fromhost,sptr->host,HOSTLEN);
    client = acptr;
    sendto_serv_butone(cptr,"SERVER %s %s",acptr->host, acptr->server);
    return(0);
  } else if ((sptr->status == STAT_CLIENT) || (ISOPER(sptr))) {
    sendto_one(cptr,"ERROR :Client may not currently become server");
    return(0);
  } else if (sptr->status == STAT_UNKNOWN || sptr->status == STAT_HANDSHAKE) {
    if (!(aconf=find_conf(sptr->sockhost, NULL, host, CONF_NOCONNECT_SERVER))){
      sendto_one(sptr,"ERROR :Access denied (no such server enabled) (%s@%s)",
		 host, (server) ? (server) : "Master");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    if (!(bconf=find_conf(sptr->sockhost, NULL,host, CONF_CONNECT_SERVER))) {
      sendto_one(sptr, "ERROR :Only C field for server.");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    if (*(aconf->passwd) && !Equal(aconf->passwd, sptr->passwd)) {
      sendto_one(sptr,"ERROR :Access denied (passwd mismatch)");
      m_bye(sptr, sptr, 0);
      return(FLUSH_BUFFER);
    }
    strncpy(sptr->host, host, HOSTLEN);   sptr->host[HOSTLEN] = '\0';
    if (server && *server)
      CopyFixLen(sptr->server, server, HOSTLEN);
    else 
      CopyFixLen(sptr->server, myhostname, HOSTLEN);
    
    CopyFixLen(sptr->fromhost, host, HOSTLEN);
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
    for (; acptr; acptr = acptr->next) {
      if ((ISCLIENT(acptr)) ||
	  (ISOPER(acptr))) {
	sendto_one(cptr,"NICK %s",acptr->nickname);
	sendto_one(cptr,":%s USER %s %s %s :%s",acptr->nickname,
		   acptr->username, acptr->host, acptr->server,
		   acptr->realname);
	sendto_one(cptr,":%s CHANNEL %d",acptr->nickname, acptr->channel);
	if (ISOPER(acptr)) 
	  sendto_one(cptr,":%s OPER", acptr->nickname);
      } else if (ISSERVER(acptr) && acptr != sptr) {
	  sendto_one(cptr,"SERVER %s %s",acptr->host, acptr->server);
      }
    }
    return(0);
  }
  return(0);
}

int m_kill(cptr, sptr, sender, user, path)
aClient *cptr, *sptr;
char *sender, *user, *path;
{
  aClient *acptr;
  char *oldpath;
  debug(DEBUG_DEBUG,"%s: m_kill: %s", sptr->nickname, user);
  if ((user == NULL) || (*user == '\0')) {
    sendto_one(sptr,":%s %d %s :No user specified",
	       myhostname, ERR_NEEDMOREPARAMS, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if (!ISOPER(sptr) && sptr->status != STAT_SERVER) {
    sendto_one(sptr,":%s %d %s :Death before dishonor ?",
	       myhostname, ERR_NOPRIVILEGES, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if ((acptr = find_client(user, NULL)) == (aClient *) 0) {
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

int m_info(cptr, sptr)
aClient *cptr, *sptr;
{
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO, 
	     sptr->nickname, info1);
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO,
	     sptr->nickname, info2);
  sendto_one(sptr,":%s %d %s :%s", myhostname, RPL_INFO,
	     sptr->nickname, info3);
  return(0);
}

int m_links(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *sender, *para;
{
  aClient *acptr = client;
  if (!ISCLIENT(sptr) && !ISOPER(sptr)) {
    sendto_one(sptr,":%s %d * :You have not registered yourself yet",
	       myhostname, ERR_NOTREGISTERED);
  }
  while (acptr) {
    if ((ISSERVER(acptr) || acptr->status == STAT_ME) &&
	(para == NULL || *para == '\0' || matches(para, acptr->host) == 0))
      sendto_one(sptr,"LINREPLY %s %s", acptr->host,
		 (acptr->server[0] ? acptr->server : "(Unknown Location)"));
    acptr = acptr->next;
  }
  return(0);
}

int m_summon(cptr, sptr, sender, user)
aClient *sptr, *cptr;
char *user, *sender;
{
  char namebuf[10],linebuf[10],hostbuf[17],*host;
  int fd, flag;
  if (!ISCLIENT(sptr) && !ISOPER(sptr)) {
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
  if (host == NULL || *host == '\0' || MyEqual(host,myhostname,HOSTLEN)) {
    if ((fd = utmp_open()) == -1) {
      sendto_one(sptr,"NOTICE %s Cannot open %s",sptr->nickname,UTMP);
      return(0);
    }
    while (!(flag = utmp_read(fd, namebuf, linebuf, hostbuf))) 
      if (Equal(namebuf,user))
	break;
    utmp_close(fd);
    if (flag == -1)
      sendto_one(sptr,"NOTICE %s :%s: No such user found",
		 sptr->nickname, myhostname);
    else
      summon(sptr, namebuf, linebuf);
    return(0);
  }
  {
    aClient *acptr = client;
  for (; acptr; acptr = acptr->next) 
    if (!(ISSERVER(acptr) && strcmp(acptr->host, host)))
      break;
  if (!acptr)  {
    sendto_one(sptr,"ERROR No such host found");
    return 0;
  }
   }    
  {
    aClient *acptr = client;
    for(; acptr; acptr = acptr->next)
    if (ISSERVER(acptr) && Equal(acptr->fromhost, acptr->host))
	break;
  if (!acptr)
   sendto_one(sptr,"ERROR Internal Error. Contact Administrator");
  else 
   sendto_one(acptr,":%s SUMMON %s@%s",sptr->nickname, user, host);
  } 
  return(0);
}

int m_stats(cptr, sptr)
aClient *cptr, *sptr;
{
  aMessage *mptr = msgtab;
  for (; mptr->cmd; mptr++)
    sendto_one(sptr,"NOTICE %s :%s has been used %d times after startup",
	       sptr->nickname, mptr->cmd, mptr->count);
  return(0);
}

int m_users(cptr, sptr, from, host)
aClient *cptr, *sptr;
char *from, *host;
{
  aClient *acptr=client, *a2cptr=client;
  char namebuf[10],linebuf[10],hostbuf[17];
  int fd, flag = 0;
  if (sptr->nickname[0] == '\0') {
    sendto_one(sptr,"ERROR Nickname not specified yet");
    return(0);
  }
  if (host == NULL || *host == '\0' || Equal(host,myhostname)) {
    if ((fd = utmp_open()) == -1) {
      sendto_one(sptr,"NOTICE %s Cannot open %s",sptr->nickname,UTMP);
      return(0);
    }
    sendto_one(sptr,"NOTICE %s :UserID   Terminal Host", sptr->nickname);
    while (!utmp_read(fd, namebuf, linebuf, hostbuf)) {
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
  for (; acptr; acptr = acptr->next) 
    if (ISSERVER(acptr) && Equal(acptr->host, host))
      break;
  if (!acptr) 
    sendto_one(sptr,"ERROR No such host found");
  else {
    for(; a2cptr; a2cptr = a2cptr->next)
      if (ISSERVER(a2cptr) && Equal(acptr->fromhost, a2cptr->host))
	break;
    
    if (!a2cptr)
      sendto_one(sptr,"ERROR Internal Error. Contact Administrator");
    else 
      sendto_one(a2cptr,":%s USERS %s",sptr->nickname, host);
  }
  return(0);
}

int m_bye(cptr, sptr, from) 
aClient *cptr;
aClient *sptr;
char *from;
{
  aClient *acptr;
  close(sptr->fd); sptr->fd = -20;
  if (from == NULL)
    from = "";
  if (ISSERVER(sptr) && sptr == cptr) {

    for (acptr = client; acptr; acptr = acptr->next) 
      if ((ISCLIENT(acptr) || ISOPER(acptr)) &&
	  Equal(acptr->fromhost, sptr->host) && sptr != acptr) {
	exit_user(cptr, acptr, from);
	acptr = client;
      } 

    for (acptr = client; acptr; acptr = acptr->next) 
      if (ISSERVER(acptr) &&
	  Equal(acptr->fromhost, sptr->host) && sptr != acptr) {
	exit_user(cptr, acptr, from);
	acptr = client;
      } 
  }
  exit_user(cptr, sptr, from);
  return(0);
}

exit_user(cptr, sptr, from)
aClient *sptr;
aClient *cptr;
char *from;
{
  aClient *acptr;
  aChannel *chptr = channel, *pchptr;
  int i;
  if ((sptr->status == STAT_CLIENT || ISOPER(sptr)) &&
      ((i = sptr->channel) != 0)) {
    sptr->channel = 0;
    sendto_channel_butserv(i,":%s QUIT :%s!%s", sptr->nickname,
			   from, myhostname);
  } else
    i = 0;
  if (ISSERVER(sptr))
    sendto_serv_butone(cptr,"SQUIT %s :%s!%s",sptr->host, from, myhostname);
  else if (sptr->nickname[0])
    sendto_serv_butone(cptr,":%s QUIT :%s!%s",sptr->nickname, from, myhostname);
  pchptr = (aChannel *) 0;
  for ( ; chptr; pchptr = chptr, chptr = chptr->nextch) {
    if (chptr->channo == i && chptr->users > 0) 
      --chptr->users;
    if (chptr->users <= 0) {
      if (pchptr)
	pchptr->nextch = chptr->nextch;
      else
	channel = chptr->nextch;
      free(chptr);
    }
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
  for (acptr = client; acptr && acptr->next != sptr; acptr = acptr->next) ;
    
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

int m_error(cptr, sptr, sender, para1, para2, para3, para4)
aClient *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4;
{
  debug(DEBUG_ERROR,"Received ERROR message from %s: %s %s %s %s",
	sptr->nickname, para1, para2, para3, para4);
  return(0);
}

int m_help(cptr, sptr)
aClient *cptr, *sptr;
{
  int i;
  for (i = 0; msgtab[i].cmd; i++)
    sendto_one(sptr,"NOTICE %s :%s",sptr->nickname,msgtab[i].cmd);
  return(0);
}

int m_whoreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
aClient *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Whoreply", para1, para2, para3);
}

int m_restart(cptr, sptr)
aClient *cptr, *sptr;
{
  if (ISOPER(sptr))
    restart();
}

int m_die(cptr, sptr)
aClient *cptr, *sptr;
{
  if (ISOPER(sptr))
    exit(-1);
}

dowelcome(sptr)
aClient *sptr;
{
  sendto_one(sptr,"NOTICE %s :*** %s%s", sptr->nickname, welcome1,version);
  m_lusers(sptr, sptr, NULL, NULL);
}

int m_lusers(cptr, sptr, sender)
aClient *cptr, *sptr;
char *sender;
{
  int s_count = 0, c_count = 0, u_count = 0, i_count = 0, o_count = 0;
  aClient *acptr;
  acptr = client;
  for (; acptr; acptr = acptr->next) {
    switch (acptr->status) {
    case STAT_SERVER:
    case STAT_ME:
      s_count++;
      break;
    case STAT_OPER:
#ifdef never
      if (acptr->channel >= 0 || ISOPER(sptr))
#endif 
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
  }
  if (ISOPER(sptr) && i_count)
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

int m_away(cptr, sptr, sender, msg)
aClient *cptr, *sptr;
char *msg;
{
  if (!ISCLIENT(sptr) && sptr->status != STAT_SERVICE &&
      !ISOPER(sptr)) {
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

int m_connect(cptr, sptr, sender, host, portstr)
aClient *cptr, *sptr;
char *sender, *host, *portstr;
{
  int port, tmpport, retval;
  aConfItem *aconf;
  extern char *sys_errlist[];
  if (sptr->status != STAT_SERVER && !ISOPER(sptr)) {
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
  for (aconf = conf; aconf; aconf = aconf->next) {
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
  }
  if (!aconf)
    sendto_one(cptr, "ERROR :Connect: Host %s not listed in irc.conf.",host);
  return(0);
}

int m_ping(cptr, sptr, sender, origin, dest)
aClient *cptr, *sptr;
char *sender, *origin, *dest;
{
  aClient *acptr;
  if (origin == NULL || *origin == '\0') {
    sendto_one(sptr,"ERROR :No origin specified.");
    return(-1);
  }
  if (dest && *dest && strcmp(dest, myhostname)) {
    if (acptr = find_server(dest,NULL)) {
      sendto_one(acptr,"PING %s %s", origin, dest);
      return(0);
    } else {
      sendto_one(sptr,"ERROR :No such host found");
      return(-1);
    }
  } else {
    sendto_one(sptr,"PONG %s %s", 
	       (dest) ? dest : myhostname, origin);
    return(0);
  }
}

int m_pong(cptr, sptr, sender, origin, dest)
aClient *cptr, *sptr;
char *sender, *origin, *dest;
{
  aClient *acptr;
  if (origin == NULL || *origin == '\0') {
    sendto_one(sptr,"ERROR :No origin specified.");
    return(-1);
  }
  cptr->flags &= ~FLAGS_PINGSENT;
  sptr->flags &= ~FLAGS_PINGSENT;
  if (dest && *dest && strcmp(dest, myhostname)) {
    if (acptr = find_server(dest,NULL)) {
      sendto_one(acptr,"PONG %s %s", origin, dest);
      return(0);
    } else {
      sendto_one(sptr,"ERROR :No such host found");
      return(-1);
    }
  } else {
    debug(DEBUG_NOTICE, "PONG: %s %s", origin, dest);
    return(0);
  }
}

/**************************************************************************
 * m_oper() added Sat, 4 March 1989
 **************************************************************************/

int m_oper(cptr, sptr, sender, name, password)
aClient *cptr, *sptr;
char *sender, *name, *password;
{
  aConfItem *aconf;
  if (!ISCLIENT(sptr) || 
      (name == NULL || *name == '\0' || password == NULL ||
      *password == '\0') && cptr == sptr ) {
    sendto_one(sptr, ":%s %d %s :Dave, don't do that...",
	       myhostname, ERR_NEEDMOREPARAMS, (sptr->nickname[0] == '\0') ?
	       "*" : sptr->nickname);
    return(0);
  }
  if (ISSERVER(cptr)) {
    sptr->status = STAT_OPER;
    sendto_serv_butone(cptr, ":%s OPER", sptr->nickname);
    return(0);
  }
  if (!(aconf = find_conf(cptr->sockhost, NULL, name, CONF_OPERATOR)) &&
      !(aconf = find_conf(cptr->host, NULL, name, CONF_OPERATOR))) {
    sendto_one(sptr, ":%s %d %s :Only few of mere mortals may try to %s",
	       myhostname, ERR_NOOPERHOST, sptr->nickname,
	       "enter twilight zone");
    return(0);
  } 
  if (aconf->status == CONF_OPERATOR &&
      Equal(password, aconf->passwd)) {
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

int m_pass(cptr, sptr, sender, password)
aClient *cptr, *sptr;
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

int m_wall(cptr, sptr, sender, message)
aClient *cptr, *sptr;
char *sender, *message;
{
  char *which = (sender && *sender) ? sender : sptr->nickname;

  if (!ISOPER(sptr)) {
    sendto_one(sptr,":%s %d %s :Only real wizards know the correct %s",
	       myhostname, ERR_NOPRIVILEGES, 
	       (sptr->nickname[0] == '\0') ? "*" : sptr->nickname,
	       "spells to open this gate");
    return(0);
  }
  sendto_all_butone(cptr == sptr ? NULL : cptr,":%s WALL :%s",which, message);
  
  return(0);
}


/**************************************************************************
 * woper() - Command added 16 October 1989 (mike) 
 **************************************************************************/

int m_woper (cptr, sptr, sender, message)
aClient *cptr, *sptr;
char *sender, *message;
{
  char *which = (sender && *sender) ? sender : sptr->nickname;

  sendto_ops("%s: %s",which, message);
  
  return(0);
}

/**************************************************************************
 * time() - Command added 23 March 1989
 **************************************************************************/

int m_time(cptr, sptr, sender, dest)
aClient *cptr, *sptr;
char *sender, *dest;
{
  aClient *acptr;
  if (!ISCLIENT(sptr) && !ISOPER(sptr)) {
    sendto_one(sptr, ":%s %d %s :You haven't registered yourself yet",
	       myhostname, ERR_NOTREGISTERED, "*");
    return (-1);
  }
  if (dest && *dest && matches(dest, myhostname)) {
    if ((acptr = find_server(dest,NULL)) &&
	(acptr->status == (short) STAT_SERVER)) {
      sendto_one(acptr,":%s TIME %s", sptr->nickname, dest);
      return(0);
    } else {
      sendto_one(sptr,":%s %d %s %s :No such host found",
		 myhostname, ERR_NOSUCHSERVER, sptr->nickname, dest);
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

int m_rehash(cptr, sptr)
aClient *cptr, *sptr;
{
  if (!ISOPER(sptr)) {
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

#define PrivChar(ch) (chan_isprivate(chptr) ? '*' : '=')
	
int m_names(cptr, sptr, sender, para)
aClient *cptr, *sptr;
char *para, *sender;
{ 
  aChannel *chptr = channel;
  aClient *c2ptr;
  int idx, flag;

  for (; chptr ; chptr = chptr->nextch) {
    if (!(!para || *para == '\0' || chan_match(chptr, chan_conv(para))))
      continue; 
    if (chan_isprivate(chptr) && !chan_match(chptr, sptr->channel))
      continue; 
    sprintf(buf, "%c %d ", PrivChar(chptr), chptr->channo);
    idx = strlen(buf);
    flag = 0;
    for (c2ptr = client ; c2ptr; c2ptr = c2ptr->next) {
      if (!((ISOPER(c2ptr) || c2ptr->status == STAT_CLIENT) &&
          chan_match(chptr, c2ptr->channel)))
        continue; 
      strncat(buf, c2ptr->nickname, NICKLEN);
      idx += strlen(c2ptr->nickname) + 1;
      flag = 1;
      strcat(buf," ");
      if (idx + NICKLEN > BUFSIZE - 2) {
        sendto_one(sptr, "NAMREPLY %s", buf);
               sprintf(buf, "%c %d ", PrivChar(channel), chptr->channo);
        idx = strlen(buf);
        flag = 0;
      }
    }
    if (flag)
      sendto_one(sptr, "NAMREPLY %s", buf);
  }

  if (para != NULL && *para)
    return(1);
  strcpy(buf, "* * ");
  idx = strlen(buf);
  c2ptr = client;
  flag = 0;
  for (c2ptr = client ; c2ptr; c2ptr = c2ptr->next) {
    if (!((ISOPER(c2ptr) || c2ptr->status == STAT_CLIENT) &&
	((c2ptr->channel > 999 && (sptr->channel != c2ptr->channel)) ||
	c2ptr->channel == 0))) 
     continue;
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
  if (flag)
    sendto_one(sptr, "NAMREPLY %s", buf);
  return(1);
}

int m_namreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
aClient *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Namreply", para1, para2, para3);
}

int m_linreply(cptr, sptr, sender, para1, para2, para3, para4, para5, para6)
aClient *cptr, *sptr;
char *sender, *para1, *para2, *para3, *para4, *para5, *para6;
{
  m_error(cptr, sptr, sender, "Linreply", para1, para2, para3);
}

int m_admin(cptr, sptr, sender, para1)
aClient *cptr, *sptr;
char *sender, *para1;
{
  aConfItem *aconf;
  aClient *acptr;
  if (para1 && *para1 && matches(para1, myhostname)) {
    /* Isn't for me... */
    if ((acptr = find_server(para1, (aClient *) 0)) &&
	(ISSERVER(acptr))) {
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

int m_trace(cptr, sptr, sender, para1)
aClient *cptr, *sptr;
char *sender, *para1;
{
  aClient *acptr;
  if (!ISOPER(sptr)) {
    sendto_one(sptr,
       "NOTICE %s :*** Error: %s", sptr->nickname,
	       "No mere mortals may trace the nets of the universe");
    return(-1);
  }
  if (para1 && *para1 && !MyEqual(para1, myhostname, HOSTLEN) &&
      (acptr = find_server(para1, (aClient *) 0))) {
    sendto_one(acptr, ":%s TRACE :%s", sptr->nickname, para1);
    sendto_one(sptr, "NOTICE %s :*** Link %s ==> %s",
	       sptr->nickname, myhostname, para1);
    return(0);
  }
  for (acptr = client; acptr; acptr = acptr->next) 
    if (ISSERVER(acptr) && acptr->fd >= 0)
      sendto_one(sptr, "NOTICE %s :*** Connection %s ==> %s",
		 sptr->nickname, myhostname, acptr->host);
  
  return(0);
}

int m_motd(cptr, sptr, sender, para1)
aClient *cptr, *sptr;
char *sender, *para1;
{
  aClient *acptr;
  FILE *fptr;
  char line[80], *tmp;

  if (para1 && *para1 && matches(para1, myhostname)) {
    /* Isn't for me... */
    if ((acptr = find_server(para1, (aClient *) 0)) && (ISSERVER(acptr))) {
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
  if (!(fptr = fopen(MOTD, "r"))) {
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
