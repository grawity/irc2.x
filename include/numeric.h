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

#define RPL_WHOWASUSER       314

#define RPL_ENDOFWHO         315

#define RPL_LISTSTART        321
#define RPL_LIST             322
#define RPL_LISTEND          323

#define RPL_NOTOPIC          331
#define RPL_TOPIC            332

#define RPL_INVITING         341

#define RPL_VERSION          351

#define RPL_KILLDONE         361

#define RPL_ENDOFLINKS       365
#define RPL_ENDOFNAMES       366

#define RPL_INFO             371
#define RPL_MOTD             372

#define RPL_YOUREOPER        381
#define RPL_REHASHING        382

#define RPL_TIME             391
