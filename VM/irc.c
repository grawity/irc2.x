/************************************************************************
 *   IRC - Internet Relay Chat, VM/irc.c
 *   Copyright (C) 1990 Armin Gruner
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
 
#include <stdio.h>
#include <stdlib.h>
#include <stdefs.h>
#include <string.h>
#include <ctype.h>
#include "sys.h"
#include "msg.h"
#include "struct.h"
#include "patchlev.h"
 
#define USAGE \
fprintf(stderr, "usage: %s \
<-p portnum> <-c channelnum> <nickname <host>>\n"), exit(100);
 
static int port = PORTNUM;
 
char *version = VERSION;
int patchlevel = PATCHLEVEL;
 
int fsd, debuglevel = DEBUG_ERROR;
int hold = 1, unkill = 0;
 
aClient me, *client;
anUser meUser;
 
#ifdef IRCPASS
static char *ircpass;
static int uphostaddr;
#endif
 
extern unsigned long gethostbyname();
 
main(argc, argv)
int argc;
char **argv;
{
    extern int mystrncpy();
    char buf[BUFFERLEN];
    char *argv0 = *argv, *tmp;
    int sock, info, rc;
 
#ifdef IRCPASS
    ircpass = IRCPASS;
    uphostaddr = gethostbyname(UPHOST);
#endif
 
    client = &me;
    me.user = &meUser;
    me.from = NULL;
    me.next = NULL;
 
    me.buffer[0] = '\0';
    me.name[0] = '\0';
    meUser.username[0] = '\0';
    meUser.host[0] = '\0';
    meUser.server[0] = '\0';
    strcpy(meUser.channel, "0");
    me.status=STAT_ME;
    me.fd=0;
    me.count=0;
    me.sendM=0;
    me.sendB=0;
    me.receiveM=0;
    me.receiveB=0;
 
    mystrncpy(me.name, (tmp=getenv("IRCNICK")) ? tmp : "", NICKLEN);
    mystrncpy(meUser.server, (tmp=getenv("IRCSERVER")) ? tmp : UPHOST, HOSTLEN);
    mystrncpy(me.info, (tmp=getenv("IRCNAME")) ? tmp : "", REALLEN);
 
    info = 0;
    while (--argc && *++argv)
            {
            if (**argv == '-' || **argv == '(')
                    switch(*++*argv)
                            {
                            case 'c':
                                if (*++*argv)
                                   strncpy(meUser.channel, *argv,
                                           sizeof (meUser.channel));
                                else  {
                                   strncpy(meUser.channel, *++argv,
                                           sizeof (meUser.channel));
                                      --argc;
                                      }
                                    break;
                            case 'p':
                                    if (*++*argv)
                                            port=atoi(*argv);
                                    else  {
                                          port=atoi(*++argv);
                                          --argc;
                                          }
                                    break;
                            case '?':
                            case 'h':
                                    sprintf(buf, "HELP %s", argv0);
                                    exit(system(buf));
                            default:
                                    USAGE;
                            }
            else if (**argv == '?')
                       {
                       sprintf(buf, "HELP %s", argv0);
                       exit(system(buf));
                       }
            else if (!info)
                       {
                       mystrncpy(me.name, *argv, NICKLEN);
                       info++;
                       }
            else    strncpy(meUser.server, *argv, HOSTLEN);
        }
 
    system("QUERY USERID (STACK");
    gets(buf);
    sscanf(buf, "%8s %*s %8s", meUser.username, meUser.host);
    strncpy(me.sockhost, meUser.host, sizeof me.sockhost);
    meUser.username[8] = meUser.host[8] = '\0';
    me.sockhost[8] = '\0';
 
    if (!me.name[0] || !me.info[0])
        {
        sprintf(buf, "NAMEFIND :USERID %s :NICK :NAME (STACK",
                meUser.username);
        if (!system(buf))
                {
                gets(buf);
                if (!me.name[0])
                        mystrncpy(me.name, buf, NICKLEN);
                gets(buf);
                if (!me.info[0])
                        mystrncpy(me.info, buf, REALLEN);
                }
 
        if (!me.name[0])
             mystrncpy(me.name, meUser.username, NICKLEN);
 
        if (!me.info[0])
             sprintf(me.info, "%s at %s",
                meUser.username, meUser.host);
        }
 
    fsd = openfs(BANNER);              /* Full Screen Setup */
 
    putline(WELCOME);
 
    if (do_server(meUser.server))
       putline("*** Use /server to connect to a server.");
 
    do {
        switch (nextevent(&sock, &info, 1))
            {
            case SOCK_READ:
                if ((me.fd == sock) &&
                    (info = read(me.fd, buf, sizeof buf)))
                        dopacket(&me, buf, info);
                else if ((fsd == sock) &&
                         (read(fsd, buf, sizeof buf) > 0))
                            sendit(buf);
                break;
 
            case SOCK_CLOSE:
            case SOCK_ERRORSTATE:
                if (me.fd == sock)
                    {
                    putline("*** Lost server connection.");
                    if (!unkill || do_server(meUser.server))
                        putline("*** Use /server to connect to a server.");
                    if (unkill)
                            unkill--;
                    }
                break;
 
            case SOCK_EXTINTR:
                hold = 0;
                break;
 
            default:
                putline("unknown event error. Inform author.");
                break;
            }
        } while (hold);
 
    close(me.fd);
    close(fsd);         /* close full screen; myexit() would do ... */
    exit(0);            /* drops TCP/IP usage */
}
 
do_server(server)
char *server;
{
    extern int port;
    char buf[512];
    int sock, hostaddr;
    extern int socket();
 
    if (!server[0])
        {
        putline("*** Error: No server specified.");
        return(-1);
        }
    if ((hostaddr = gethostbyname(server)) == NULL)
        {
        sprintf(buf, "*** %s: Unknown host.", server);
        putline(buf);
        return(-1);
        }
    sprintf(buf, "*** Connecting to server %s", server);
    putline(buf);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)
        if (connect(sock, hostaddr, port) < 0)
                {
                close(sock);
                sprintf(buf, "*** Unable to connect to server %s", server);
                putline(buf);
          return(-1);
                }
    if (me.fd)
        {
        sprintf(buf, "*** Closing connection to %s", meUser.server);
        putline(buf);
        sendto_one(&me, MSG_QUIT);
        close(me.fd);
        sleep(3); /* time to recover */
        }
    me.fd = sock;
    strncpy(meUser.server, server, HOSTLEN);
 
#ifdef IRCPASS
    if (hostaddr == uphostaddr)
        sendto_one(&me, "%s %s", MSG_PASS, ircpass);
#endif
 
    sendto_one(&me, "%s %s", MSG_NICK, me.name);
    sendto_one(&me, "%s %s %s %s %s", MSG_USER,
        meUser.username, meUser.host, meUser.server, me.info);
    if (strcmp(meUser.channel, "0") != NULL)
        sendto_one(&me, "%s %s", MSG_CHANNEL, meUser.channel);
 
    return(0);
}
