/************************************************************************
 *   IRC - Internet Relay Chat, include/numeric.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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

/*
 * $Id: numeric.h,v 6.1 1991/07/04 21:04:31 gruner stable gruner $
 *
 * $Log: numeric.h,v $
 * Revision 6.1  1991/07/04  21:04:31  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:03  gruner
 * frozen beta revision 2.6.1
 *
 */

/* -- Jto -- 16 Jun 1990
 * A couple of new numerics added...
 */

/* -- Jto -- 03 Jun 1990
 * Added ERR_YOUWILLBEBANNED and Check defines (sigh, had to put 'em here..)
 * Added ERR_UNKNOWNMODE...
 * Added ERR_CANNOTSENDTOCHAN...
 */

#define ERR_NOSUCHNICK       401
#define ERR_NOSUCHSERVER     402
#define ERR_NOSUCHCHANNEL    403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_TOOMANYCHANNELS  405
#define ERR_WASNOSUCHNICK    406
#define ERR_TOOMANYTARGETS   407

#define ERR_NORECIPIENT      411
#define ERR_NOTEXTTOSEND     412

#define ERR_UNKNOWNCOMMAND   421

#define ERR_NONICKNAMEGIVEN  431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433

#define ERR_USERNOTINCHANNEL 441
#define ERR_NOTONCHANNEL     442

#define ERR_NOTREGISTERED    451

#define ERR_NEEDMOREPARAMS   461
#define ERR_ALREADYREGISTRED 462
#define ERR_NOPERMFORHOST    463
#define ERR_PASSWDMISMATCH   464
#define ERR_YOUREBANNEDCREEP 465
#define ERR_YOUWILLBEBANNED  466

#define ERR_CHANNELISFULL    471
#define ERR_UNKNOWNMODE      472
#define ERR_INVITEONLYCHAN   473

#define ERR_NOPRIVILEGES     481
#define ERR_CHANOPRIVSNEEDED 482

#define ERR_NOOPERHOST       491
#define ERR_NOSERVICEHOST    492

#define RPL_AWAY             301
#define RPL_USERHOST         302
#define RPL_ISON             303

#define RPL_WHOISUSER        311
#define RPL_WHOISSERVER      312
#define RPL_WHOISOPERATOR    313

#define RPL_WHOWASUSER       314
#define RPL_ENDOFWHO         315

#define RPL_WHOISCHANOP      316
#define RPL_WHOISIDLE        317

#define RPL_ENDOFWHOIS       318
#define RPL_WHOISCHANNELS    319

#define RPL_LISTSTART        321
#define RPL_LIST             322
#define RPL_LISTEND          323
#define RPL_CHANNELMODEIS    324

#define RPL_NOTOPIC          331
#define RPL_TOPIC            332

#define RPL_INVITING         341

#define RPL_VERSION          351

#define RPL_KILLDONE         361

#define RPL_LINKS            364
#define RPL_ENDOFLINKS       365
#define RPL_ENDOFNAMES       366

#define RPL_INFO             371
#define RPL_MOTD             372

#define RPL_YOUREOPER        381
#define RPL_REHASHING        382
#define RPL_YOURESERVICE     383
#define RPL_MYPORTIS         384

#define RPL_TIME             391


/*
** CheckRegisteredUser is a macro to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/

#define CheckRegisteredUser(x) do { \
  if (!IsRegisteredUser(x)) \
    { \
	sendto_one(sptr, ":%s %d * :You have not registered as a user", \
		   me.name, ERR_NOTREGISTERED); \
	return -1;\
    } \
  } while (0)

/*
** CheckRegistered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/

#define CheckRegistered(x) do { \
  if (!IsRegistered(x)) \
    { \
	sendto_one(sptr, ":%s %d * :You have not registered yourself yet", \
		   me.name, ERR_NOTREGISTERED); \
	return -1;\
    } \
  } while (0)
