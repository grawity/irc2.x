/*************************************************************************
 ** c_numeric.c  irc  v2.02    (16 Aug 1989)
 **
 ** This file is part of Internet Relay Chat v2.02
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

char numeric_id[] = "numeric.c (c) 1989 Jarkko Oikarinen";

#include "sys.h" 
#include "struct.h"
#include "numeric.h"

#define NULL (char *) 0

extern aClient *find_client();
extern aChannel *find_channel();



extern char mybuf[];
do_numeric(numeric, cptr, sptr, sender, para, para1, para2, para3, para4,
	   para5, para6)
int numeric;
aClient *cptr, *sptr;
char *sender, *para, *para1, *para2, *para3, *para4, *para5, *para6;
{
  if (!para)  para = "";
  if (!para1) para1 = "";
  if (!para2) para2 = "";
  if (!para3) para3 = "";
  if (!para4) para4 = "";
  if (!para5) para5 = "";
  if (!para6) para6 = "";

  switch (numeric) {
  case ERR_NOSUCHNICK:
    sprintf(mybuf, "*** Error: %s: No such nickname (%s)", sender, para1);
    break;
  case ERR_NOSUCHSERVER:
    sprintf(mybuf, "*** Error: %s: No such server (%s)", sender, para1);
    break;
  case ERR_NOSUCHCHANNEL:
    sprintf(mybuf, "*** Error: %s: No such channel (%s)", sender, para1);
    break;
  case ERR_NORECIPIENT:
    sprintf(mybuf, "*** Error: %s: Message had no recipient", sender);
    break;
  case ERR_NOTEXTTOSEND:
    sprintf(mybuf, "*** Error: %s: Empty messages cannot be sent", sender);
    break;
  case ERR_UNKNOWNCOMMAND:
    sprintf(mybuf, "*** Error: %s: Unknown command (%s)", sender, para1);
    break;
  case ERR_NONICKNAMEGIVEN:
    sprintf(mybuf, "*** Error: %s: No nickname given", sender);
    break;
  case ERR_ERRONEUSNICKNAME:
    sprintf(mybuf, "*** Error: %s: Some special characters cannot be used %s",
	    sender, "in nicknames");
    break;
  case ERR_NICKNAMEINUSE:
    sprintf(mybuf, "*** Error: %s: Nickname %s is already in use. %s",
	    sender, para1, "Please choose another.");
    break;
  case ERR_USERNOTINCHANNEL:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 : 
	    "You have not joined any channel");
    break;
  case ERR_NOTREGISTERED:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 :
	    "You have not registered yourself yet");
    break;
  case ERR_NEEDMOREPARAMS:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 :
	    "Not enough parameters");
    break;
  case ERR_ALREADYREGISTRED:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 :
	    "Identity problems, eh ?");
    break;
  case ERR_NOPERMFORHOST:
    sprintf(mybuf, "*** Error: %s: %s", sender,
	    (para1[0]) ? para1 : "Your host isn't among the privileged");
    break;
  case ERR_PASSWDMISMATCH:
    sprintf(mybuf, "*** Error: %s: %s", sender, 
	    (para1[0]) ? para1 : "Incorrect password");
    break;
  case ERR_YOUREBANNEDCREEP:
    sprintf(mybuf, "*** Error: %s: %s", sender, 
	    (para1[0]) ? para1 : "You're banned from irc...");
    break;
  case ERR_CHANNELISFULL:
    sprintf(mybuf, "*** Error: %s: Channel %s is full", sender, para1);
    break;
  case ERR_NOPRIVILEGES:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 :
	    "Only few and chosen are granted privileges. You're not one.");
    break;
  case ERR_NOOPERHOST:
    sprintf(mybuf, "*** Error: %s: %s", sender, (para1[0]) ? para1 :
	    "Only few of mere mortals may try to enter the twilight zone..");
    break;
  case RPL_AWAY:
    sprintf(mybuf, "*** %s: %s is away: %s", sender, 
	    (para1[0]) ? para1 : "*Unknown*",
	    (para2[0]) ? para2 : "*No message (strange)*");
    break;
  case RPL_WHOISUSER:
    sprintf(mybuf, "*** %s is %s@%s (%s) on channel %s", 
	    para1, para2, para3, para5, (*para4 == '*') ? "*Private*" : para4);
    break;
  case RPL_WHOISSERVER:
    sprintf(mybuf, "*** On irc via server %s (%s)", para1, para2);
    break;
  case RPL_WHOISOPERATOR:
    sprintf(mybuf, "*** %s has a connection to the twilight zone",
	    para1);
    break;
  case RPL_LISTSTART:
    sprintf(mybuf, "*** %s: Chn Users  Name", sender);
    break;
  case RPL_LIST:
    sprintf(mybuf, "*** %s: %3s %5s  %s", sender,
	    (para1[0] == '*') ? "Prv" : para1, para2, para3);
    break;
  case RPL_LISTEND:
    sprintf(mybuf, "*** %s: End of channel listing", sender);
    break;
  case RPL_NOTOPIC:
    sprintf(mybuf, "*** %s: No Topic is set", sender);
    break;
  case RPL_TOPIC:
    sprintf(mybuf, "*** %s: Topic is %s", sender, para1);
    break;
  case RPL_INVITING:
    sprintf(mybuf, "*** %s: Inviting user %s into channel %s",
	    sender, para1, para2);
    break;
  case RPL_VERSION:
    sprintf(mybuf, "*** %s: Host %s runs irc version %s", sender,
	    para2, para1);
    break;
  case RPL_KILLDONE:
    sprintf(mybuf, "*** %s: May %s rest in peace", sender, para1);
    break;
  case RPL_INFO:
    sprintf(mybuf, "*** %s: Info: %s", sender, para1);
    break;
  case RPL_MOTD:
    sprintf(mybuf, "*** %s: Motd: %s", sender, para1);
    break;
  case RPL_YOUREOPER:
    sprintf(mybuf, "*** %s: %s", sender, (para1[0] == '\0') ?
	    "You have operator privileges now. Be nice to mere mortal souls" :
	    para1);
    break;
  case RPL_REHASHING:
    sprintf(mybuf, "*** %s: %s", sender, (para1[0] == '\0') ?
	    "Rereading configuration file.." : para1);
    break;
  case RPL_TIME:
    sprintf(mybuf, "*** Time on host %s is %s",
	    para1, para2);
    break;
  default:
    sprintf(mybuf, "*** %s: Numeric message %d: %s %s %s %s %s %s",
	    sender, numeric, sender, para1, para2, para3, para4, para5, para6);
    break;
  }
  putline(mybuf);
}
