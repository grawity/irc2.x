/************************************************************************
 *   IRC - Internet Relay Chat, VM/socket.h
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
 
/* some definitions for TCP/IP usage */
 
#define TCP_BUFSIZ 8192 /* maximum of data that TCP/IP can handle at one time */
 
struct coninfo  {
                int notetype;   /* set to event occured */
                int connnum;    /* set to connection in which event occured */
                int urgbytes;   /* urgent data not delivered to client */
                int urgspan;    /* number of undelivered data */
                int bytesdel;   /* number of data delivered to you */
                int lasturg;    /* num of bytes remaining */
                int push;       /* set != 0 if received data was urgent */
                int amtspc;     /* set to buf size TCPIP has avail. for me */
                int newstat;    /* new state of connection */
                int reas;       /* reason for the new stat of con */
                int datm;       /* set to timer data spec. when (set)timer */
                int assctmr;    /* set to address of (set)timer */
                int rupt;       /* interrupt type */
                int datalen;    /* size of UDP data */
                int foraddr;    /* source of UDP datagram */
                int forport;    /* foreign port of UDP diagram */
                char *iucvbf;   /* addr where intr data has been placed */
                };
 
struct timer    {
                int timerpt;    /* integer containing timer address */
                int amttime;    /* time in seconds before we expire */
                int dat;        /* data noted when expiring */
                };
 
typedef struct aSocket  {
                int socket;
                int state;
                int type;          /*
                                   ** currently only for TCP/IP,
                                   ** maybe in future VMCF/IUCV
                                   */
                int hostaddr;      /* resolved number */
                int hostport;
                int lcladdr;
                int lclport;
                int connstat;      /* avtice or passive open */
                int timeout;       /* open attempt */
                int connptr;       /* set to statusinfo addr */
                                   /* zero for the first call */
                int connnum;       /* thats my connnum for that line */
                int push;          /* immediate passthru */
                int urgent;        /* and urgent !! */
                char buf[TCP_BUFSIZ];
                char *bufp;
                int buflen;
        } aSocket;
 
extern  int asc2ebc();
extern  int ebc2asc();
 
/* some extensions to Pascal<->C Interface */
 
extern int cenableeyeucv(), cdisableeyeucv();
extern int cfsstartup(), cfsstop(), coutstrln(), cfsreceive();
extern int cfsflush(), cfsuserline(), cfsseeline(), cfsblankline();
extern int cmynextnote(), chostresol();
 
