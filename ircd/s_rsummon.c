/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_rsummon.c
 *   Copyright (C) 1990 Markku Savela
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

#include "struct.h"
#if RSUMMON /* define nonzero for /summon user%client@host */
/* #include <time.h> /* seems my version of sys/time.h inlcudes this --msa */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/errno.h>

#include "sys.h"

#define Default_UDPport 6667

extern int dummy();

rsummon(who,to)
aClient *who;
char *to;
{
    char bbb[MAXBUFLEN];
    char tonick[BUFSIZE];
    char tohost[BUFSIZE];
    int ns;

    ns=sscanf(to,"%[^%]%%%[^@]",tonick,tohost);

    if (ns==0) { /* bad format */

  sendto_one(who,"NOTICE %s :%s: internal error; received invalid summon format"
,
             who->name, me.name);
        return(-1);

    }
    else if (ns==1)
        strcpy(tohost,me.name);

    sprintf(bbb, "%s:You are being summoned to Internet Relay Chat by\n\r\
Channel %d: %s@%s (%s) %s\n\rRespond with `irc'.\n\r",tonick,
        who->user->channel, who->user->username, who->user->host,
	who->name, who->info);

  sendto_one(who, "NOTICE %s :%s: Working on summoning user %s%%%s...",
             who->name, me.name, tonick, tohost);

    return(dorsummon(who,bbb,tohost));
}


dorsummon(who,msg,host)
aClient *who;
char *msg,*host;
{
    static struct sockaddr_in myaddr;
    static struct sockaddr_in servaddr;
    int f;
    int readfds,len,i;
    char *cp;
    static struct timeval tv;
    int rretval;
    int snn;
    int retval;
    struct hostent *hp;
    struct servent *sp;

    sendto_one(who,"NOTICE %s : socket",who->name);
    if ((f = socket(AF_INET, SOCK_DGRAM, 0)) <0){
        sendto_one(who,"NOTICE %s :%s: local system error; can't create socket",
                 who->name, me.name);
        return(-1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;

    sendto_one(who,"NOTICE %s : gethostbyname",who->name);
    if ((hp=gethostbyname(host))==NULL) {
        sendto_one(who, "NOTICE %s :%s: Can't summon user%%%s: host unknown",
                 who->name, me.name, host);
        return(-1);
    }

    servaddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    sp = getservbyname("summon", "udp");
    servaddr.sin_port = sp ? sp->s_port : htonl(Default_UDPport);

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = 0;

    sendto_one(who,"NOTICE %s : bind",who->name);
    if (bind(f, &myaddr,sizeof(struct sockaddr_in)) == -1) {
        sendto_one(who,"NOTICE %s :%s: local system error; can't bind socket",
                 who->name, me.name);
        return(-1);
    }

    sendto_one(who,"NOTICE %s : sendto",who->name);
    alarm(5);
    if (sendto(f,msg,strlen(msg)+1,0,&servaddr,sizeof(struct sockaddr_in)) <0) {
	alarm(0);
        sendto_one(who,"NOTICE %s :%s: local system error; can't send summon",
                 who->name, me.name, host);
        return(-1);
/*
        if (errno==EINTR) {
                break;
        }
        else
            perror("sendto");
        exit(1);
 */
    }
    sendto_one(who,"NOTICE %s : recv",who->name);
    alarm(10);
    if ((retval = recv(f,msg,256,0)) <0) {
        alarm(0);
        if (errno==EINTR) {
        sendto_one(who, "NOTICE %s :%s: Can't summon; host %s not responding",
                 who->name, me.name, host);
        return(-1);
        }
        else {
        sendto_one(who,"NOTICE %s :%s: local system error recv'ing from %s",
                 who->name, me.name, host);
        return(-1);
        }
    }
    alarm(0);
    sendto_one(who, "NOTICE %s :%s: %s", who->name, me.name, msg);
    return(0);
}
#endif /*RSUMMON*/


