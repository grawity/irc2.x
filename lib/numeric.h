/*************************************************************************
 ** numeric.h  irc  v2.02    (16 Aug 1989)
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

#define ERR_NOSUCHNICK       401
#define ERR_NOSUCHSERVER     402
#define ERR_NOSUCHCHANNEL    403

#define ERR_NORECIPIENT      411
#define ERR_NOTEXTTOSEND     412

#define ERR_UNKNOWNCOMMAND   421

#define ERR_NONICKNAMEGIVEN  431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433

#define ERR_USERNOTINCHANNEL 441

#define ERR_NOTREGISTERED    451

#define ERR_NEEDMOREPARAMS   461
#define ERR_ALREADYREGISTRED 462
#define ERR_NOPERMFORHOST    463
#define ERR_PASSWDMISMATCH   464
#define ERR_YOUREBANNEDCREEP 465

#define ERR_CHANNELISFULL    471

#define ERR_NOPRIVILEGES     481

#define ERR_NOOPERHOST       491

#define RPL_AWAY             301

#define RPL_WHOISUSER        311
#define RPL_WHOISSERVER      312
#define RPL_WHOISOPERATOR    313

#define RPL_LISTSTART        321
#define RPL_LIST             322
#define RPL_LISTEND          323

#define RPL_NOTOPIC          331
#define RPL_TOPIC            332

#define RPL_INVITING         341

#define RPL_VERSION          351

#define RPL_KILLDONE         361

#define RPL_INFO             371
#define RPL_MOTD             372

#define RPL_YOUREOPER        381
#define RPL_REHASHING        382

#define RPL_TIME             391
