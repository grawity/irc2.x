/************************************************************************
 *   IRC - Internet Relay Chat, VM/socket.c
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
#include "cmallcl.h"
#include "socket.h"
#include "sys.h"
 
#undef  DEBUG
 
#ifndef NULL
#define NULL 0
#endif
 
#define EVER ;;
 
extern char a2e_table[], e2a_table[];
 
#define asc2ebc(x, y, l) tlate(x, y, l, a2e_table)
#define ebc2asc(x, y, l) tlate(x, y, l, e2a_table)
#define min(a, b)       ((a) > (b) ? (b) : (a))
 
int env = 0;  /* program environment tracking var */
 
#ifdef onexit_t
static onexit_t TCP_exit = NULL;        /* exit function chain */
#endif
 
int mystrncpy(s1, s2, n)
register char *s1, *s2;
register int n;
{
  register int cnt=0;
 
  while (n-- && (*s1++ = *s2++))
      cnt++;
    *s1='\0';
  return(cnt);
}
 
/*
** Socket table. Simply access to structures by
** using the socket descriptor as table index.
*/
 
static aSocket *socktab[MAXSOCKET] = { NULL } ;
 
/*
** Socket ptr for fullscreen usage. Prevents complete lookup
** in socket table for fullscreen socket.
*/
 
static aSocket *sockfs = NULL;
 
/*
** Exit routine; closes and frees all sockets,
** ends TCP/IP and exits.
*/
 
#ifdef onexit_t
static onexit_t TCP_cleanup()
#else
int exit()
#endif
{
    int rc, i;
 
    if (env)
        {
        for (i=MINSOCKET; i < MAXSOCKET; i++)
             if (socktab[i])
                close(socktab[i]);
        inter(cendtcpip, &env, &rc);
        inter(cdropemulation, &env, &rc);
        }
#ifdef onexit_t
    return(TCP_exit);
#else
    _exit();
#endif
}
 
static checkinit() /* check if TCP usage is already set up. If not, do it. */
{
    int rc;
 
    if (env == 0)
        {
        inter(cinitemulation, &env, &rc);
#ifdef DEBUG
debug(0, "initemulation: rc = %d", rc);
#endif
        if (rc)
            {
            fprintf(stderr, "*** Fatal error: Not enough core.\n");
            exit(-1);
            }
        inter(cbegintcpip, &env, &rc);
#ifdef DEBUG
debug(0, "begintcpip: rc = %d", rc);
#endif
        if (rc)
            {
            fprintf(stderr, "*** Fatal error: No TCP possible.\n");
            exit(-1);
            }
#ifdef onexit_t
        TCP_exit = (onexit_t) onexit(TCP_cleanup);
#endif
        init_e2atable();
        loadpfile();
        }
}
 
debug(level, pat, a, b, c, d, e, f, g, h)
int level;
char *pat, *a, *b, *c, *d, *e, *f, *g, *h;
{
    char buf[512];
    extern int debuglevel;
 
    if (debuglevel && level <= debuglevel)
        {
     checkinit();
        sprintf(buf, pat, a, b, c, d, e, f, g, h);
        inter(coutstrln, &env, buf); /* Notice this, no putline() here !! */
        }
}
 
/*
** Hostaddr resolution by given domain name.
** Name server is used.
*/
 
int gethostbyname(hostname)
char *hostname;
{
    int hostaddr = 0, hostlen=strlen(hostname);
 
    checkinit();
    inter(chostresol, &env, hostname, &hostlen, &hostaddr);
#ifdef DEBUG
debug(0, "hostresol: hostaddr = %d", hostaddr);
#endif
    return(hostaddr ? hostaddr : -1);
}
 
#define findsocket(socket) ((MINSOCKET <= (socket) \
&& (socket) < MAXSOCKET) ? socktab[(socket)] : NULL)
 
/*
** making a socket -- by allocating memory for a socket structure
**                    and updating the socket table
*/
 
static aSocket * makesocket()
{
    register aSocket *sptr;
    register int i;
 
    for(i=MINSOCKET; i < MAXSOCKET; i++)
        if (socktab[i] == NULL)
            break;
    if (i >= MAXSOCKET)
        return(NULL);
    if ((sptr = (aSocket *)calloc(1, sizeof *sptr)) != NULL)
        {
        sptr->socket = i;
        socktab[i]=sptr;
#ifdef DEBUG
       debug(0, "makesocket: %x\n", sptr->socket);
#endif
        }
    return(sptr);
}
 
/*
** killing a socket -- freeing memory, deleting socket
**                     from the socket table
*/
 
static killsocket(socket)
register int socket;
{
    register aSocket *sptr;
    register int i;
 
    if ((sptr = findsocket(socket)) != NULL)
        {
#ifdef DEBUG
        debug(0, "killing socket %x", sptr->socket);
#endif
        free((char *)sptr);
        socktab[socket] = NULL;
        }
}
 
/*
** returning appropriate socket structure pointer
** for a given TCP/IP connection number
*/
 
static aSocket * issocket(connnum)
register int connnum;
{
    register int i;
 
    for (i=MINSOCKET; i < MAXSOCKET; i++)
        if (socktab[i] && socktab[i]->connnum == connnum)
            {
#ifdef DEBUG
            debug(0, "issocket: conn %d: socket ==> %x",
            connnum, socktab[i]->socket);
#endif
            return(socktab[i]);
            }
#ifdef DEBUG
            debug(0, "issocket: conn %d: Socket does not exist",
                  connnum);
#endif
    return(NULL);
}
 
/*
** socket -- Creating a new socket structure,
**           and filling the correct members for later use
*/
 
int socket(domain, type, protocol)
int domain, type, protocol;
{
    aSocket *sptr;
 
    if ((sptr=makesocket()) != NULL)
        {
        sptr->type     = domain;
        sptr->hostaddr = UNSPECIFIEDaddress;
        sptr->lcladdr  = UNSPECIFIEDaddress;
        sptr->hostport = UNSPECIFIEDport;
        sptr->lclport  = UNSPECIFIEDport;
        sptr->connstat = ACTIVE;
        sptr->state    = SOCK_CLOSE;
        sptr->timeout  = 10;  /* 10 seconds are enough (when trying to open) */
        sptr->connptr  = 0;
        sptr->connnum  = 0;
        sptr->push     = 1;
        sptr->urgent   = 0;
        sptr->buf[0]   = '\0';
        sptr->bufp     = &sptr->buf[0];
        sptr->buflen   = sizeof(sptr->buf);
        return(sptr->socket);
        }
    return(-1);
}
 
/*
** closing a previously opened (allocated and maybe opened)
** socket by closing the TCP/IP line and freeing socket structure
*/
 
int close(socket)
int socket;
{
    aSocket *sptr;
    int rc;
 
    if ((sptr=findsocket(socket)) != NULL)
        {
        switch (sptr->type)
            {
            case AF_INET:
                if (sptr->state == SOCK_OPEN)
                   inter(ctcpclose, &env, &sptr->connnum, &rc);
                else if (sptr->state != SOCK_CLOSE)
                   inter(ctcpabort, &env, &sptr->connnum, &rc);
                break;
            case AF_FS:
                if (sockfs)
                    inter(cfsstop, &env);
                sockfs = NULL;
                break;
            case AF_IUCV:
            default:
                break;
            }
#ifdef DEBUG
        debug(0, "Closing socket %x, rc=%d, connnum=%d",
                sptr->socket, rc, sptr->connnum);
#endif
        killsocket(socket);
        return(0);
        }
#ifdef DEBUG
        debug(0, "Closing socket %x: Does not exist!", socket);
#endif
    return(1);
}
 
/*
** connect -- connecting socket to a specified hostaddr and portnumber,
**            and setting up TCP/IP for interrupt handling
*/
 
connect(socket, hostaddr, port)
int socket, hostaddr, port;
{
    aSocket *sptr;
    int rc;
    static int notemask = 0;
 
    if (!notemask)
        {
        notemask =  DATAdelivered |
                CONNECTIONstateCHANGED |
                EXTERNALinterrupt |
                USERdeliversLINE |
                SMSGreceived |
                IUCVinterrupt |
                USERwantsATTENTION;
        inter(chandle, &env, &notemask, &rc);
 
#ifdef DEBUG
        debug(0, "connect: handle: rc = %d", rc);
#endif
 
        if (rc != OK)
            return(-1);
        }
 
    if ((sptr=findsocket(socket)) != NULL)
        {
 
#ifdef DEBUG
        debug(0, "connect: opening socket %x, hostaddr %x, port %x",
               socket, hostaddr, port);
#endif
 
        sptr->hostaddr = hostaddr;
        sptr->hostport = port;
        switch (sptr->type)
            {
            case AF_INET:
               inter(ctcpwatopen, &env, &sptr->connstat, &sptr->timeout,
               &sptr->lcladdr, &sptr->lclport, &sptr->hostaddr,
               &sptr->hostport, &sptr->connptr, &sptr->connnum,
               &rc);
               if (rc == 0)
                   {
                   inter(ctcpreceive, &env, &sptr->connnum,
                   &sptr->bufp, &sptr->buflen, &rc);
                   sptr->state = SOCK_OPEN;
                   }
               break;
            default:
               break;
            }
#ifdef DEBUG
        debug(0, "opening, rcvbuf socket %x: rc=%d",
              sptr->socket, rc);
#endif
        return(rc);
        }
    return(-1);
}
 
/*
** nextevent - wait for the next event, either fullscreen input,
**             or TCP/IP events (maybe later for IUCV/VMCF also)
**
** kind of select, but UNIX semantics aren't possible here, there
** is too much difference.
**
** return code is the condition of the socket ..
*/
 
int nextevent(sock, info, block)
int *sock;
int *info;
int block;
{
 
    aSocket *sptr;
    struct coninfo c;
    int rc;
 
    for (EVER)
    {
    inter(cmynextnote, &env,
        &block,     &c.notetype,&c.connnum, &c.urgbytes,&c.urgspan,
        &c.bytesdel,&c.lasturg, &c.push,    &c.amtspc,  &c.newstat,
        &c.reas,    &c.datm,    &c.assctmr, &c.rupt,    &c.datalen,
        &c.foraddr, &c.forport, &c.iucvbf,  &rc);
 
      switch(c.notetype)
        {
        case USERwantsATTENTION:
            if (!sockfs)
               break;
            sprintf(sockfs->buf, "\001\001\001%c", c.reas);
            *sock = sockfs->socket;
            *info = 4;
            return(SOCK_READ);
        case USERdeliversLINE:
            if (!sockfs)
               break;
            sockfs->buf[c.bytesdel] = '\0';
            *sock = sockfs->socket;
            *info = c.bytesdel;
            return(SOCK_READ);
        case CONNECTIONstateCHANGED:
            if ((sptr=issocket(c.connnum)) != NULL)
                {
                switch(c.newstat)
                    {
                    case CONNECTIONclosing:
                    case RECEIVINGonly:
                    case SENDINGonly:
                        sptr->state = SOCK_CLOSE;
                        *sock = sptr->socket;
                        *info = SOCK_CLOSE;
                        return(SOCK_CLOSE);
                    case OPEN: /* should already be opened !! */
                    default:
                        *sock = sptr->socket;
                        *info = c.newstat;
                        return(SOCK_ERRORSTATE);
                    }
                }
            break;
        case DATAdelivered:
            if ((sptr=issocket(c.connnum)))
                {
                sptr->buf[c.bytesdel] = '\0';
                *sock = sptr->socket;
                *info = c.bytesdel;
                return(SOCK_READ);
                }
            break;
        case EXTERNALinterrupt:
            *sock = -1;
            *info = c.rupt;
            return(SOCK_EXTINTR);
        default:
            *sock = -1;
            *info = c.notetype;
            return(SOCK_ERROR);
        }
    }
}
 
/*
** bind -- binding a socket to a specified address and port
**         for listening (cannot check if it is free here)
*/
 
bind(socket, addr, port)
int socket, addr, port;
{
    aSocket *sptr;
 
    if ((sptr=findsocket(socket)) != NULL)
        {
        sptr->lcladdr  = addr;
        sptr->lclport  = port;
        sptr->connstat = PASSIVE;
        return(0);
        }
    return(-1);
}
 
/*
** write -- writing to the TCP/IP connection associated to
**          the specified socket, or to fullscreen
*/
 
write(socket, buf, len)
int socket, len;
char *buf;
{
    int rc, tmplen, maxlen;
    aSocket *sptr;
 
    if ((sptr=findsocket(socket)) != NULL && sptr->state == SOCK_OPEN)
        {
        switch(sptr->type)
            {
            case AF_INET:
                len = tmplen = ebc2asc(buf, buf, len);
#ifdef DEBUG
                debug(0, "write on socket %x, len = %d", sptr->socket,
                       len);
#endif
                while (tmplen)
                        {
                        maxlen = min(TCP_BUFSIZ, tmplen);
 
                        do {
                        inter(ctcpwatsend, &env, &sptr->connnum, &buf, &maxlen,
                        &sptr->push, &sptr->urgent, &rc);
                        } while (rc == NObufferSPACE);
 
                        if (rc)
                                break;
 
                        buf += maxlen;
                        tmplen -= maxlen;
                        }
                break;
            case AF_FS:
                inter(coutstrln, &env, buf);
                rc = 0;
                break;
            default:
                break;
            }
        return(rc ? -1 : len);
        }
    return(-1);
}
 
/*
** read -- reading from a TCP/IP line of a specified socket, or
**         from fullscreen
*/
 
read(socket, buf, len)
int socket, len;
char *buf;
{
    int rc;
    aSocket *sptr;
    char *pftext();
 
    if ((sptr=findsocket(socket)) != NULL && sptr->state == SOCK_OPEN)
        {
        switch(sptr->type)
            {
            case AF_INET:
                len = asc2ebc(buf, sptr->buf, len);
#ifdef DEBUG
                debug(0, "Read from socket %x, %d bytes",
                      sptr->socket, len);
#endif
                inter(ctcpreceive, &env, &sptr->connnum, &sptr->bufp,
                &sptr->buflen, &rc);
                return(rc ? -1 : len);
            case AF_FS:
                len = mystrncpy(buf, sptr->buf, len);
                if ((len > 3) && (strncmp("\001\001\001", buf, 3) == 0))
                    {
                    len = mystrncpy(buf, pftext(buf[3]), len);
                    return len;
                    }
                inter(cfsreceive, &env, sptr->buf, &rc);
                return(rc ? -1 : len);
            case AF_IUCV:
            default:
                break;
            }
        }
    return(-1);
}
 
/*
** opening fullscreen with a specified banner
*/
 
openfs(banner)
char *banner;
{
    int rc, fs=0; /* line mode */
    aSocket *sptr;
 
    if (sockfs == NULL)
        {
        if ((sptr = makesocket()) != NULL)
            {
#ifdef DEBUG
            debug(0, "opening fullscreen..");
#endif
            checkinit();
            inter(cfsstartup, &env, &fs, banner);
            inter(cfsreceive, &env, sptr->buf, &rc);
#ifdef DEBUG
            debug(0, "fsreceive: rc = %d", rc);
#endif
            sptr->type = AF_FS;
            sptr->state = SOCK_OPEN;
            sptr->connnum = -1; /* this is no TCP/IP connection ! */
            sockfs = sptr;
            return(sptr->socket);
            }
        }
    return(-1);
}
 
/*
** setline -- place a string on the command line
**
** first  integer value: protect first field
** second integer value: add second field
*/
setline(buf, f, s)
char *buf;
int f, s;
{
     inter(cfsuserline, &env, buf, f, s);
     inter(cfsflush, &env);
}
 
/*
** seeline - command line inputs are visible
*/
 
seeline()
{
   inter(cfsseeline, &env);
   inter(cfsflush, &env);
}
 
/*
** hideline - command line inputs are hidden
*/
 
hideline()
{
   inter(cfsblankline, &env);
   inter(cfsflush, &env);
}
 
/*
** table translation
*/
 
int tlate(s1, s2, len, tab)
register char *s1, *s2, tab[];
register int len;
{
     register int cnt=0;
 
     while(s2[cnt] && len--)
          {
          s1[cnt]=(char) tab[(int) s2[cnt]];
          cnt++;
          }
     s1[cnt] = '\0';
     return(cnt);
}
 
int sleep(sec)
int sec;
{
   char buf[32];
 
   sprintf(buf, "CP SLEEP %d SEC", sec);
   return(system(buf));
}
 
/*
** Not yet implemented
*/
 
static int
pfkeys(key)
int key;
{
    char buf[512];
    register int i;
    static int map[] = { 0x0,
           0xf1 , 0xf2 , 0xf3 , 0xf4 , 0xf5 , 0xf6 ,
           0xf7 , 0xf8 , 0xf9 , 0x7a , 0x7b , 0x7c ,
           0xc1 , 0xc2 , 0xc3 , 0xc4 , 0xc5 , 0xc6 ,
           0xc7 , 0xc8 , 0xc9 , 0x4a , 0x4b , 0x4c ,
           0x0 };
 
    for (i=1; map[i]; i++)
            if (map[i] == key)
               break;
    return(map[i] ? i : 0);
}
 
static char *texts[25];
 
#define IFCASE(k)  if (strncmp(k, texts[i], tmp - texts[i]) == 0)
 
char *
pftext(key)
int key;
{
    int i = pfkeys(key);
    char *tmp;
 
    if (!(tmp = index(texts[i], ' ')))
        return "Error in IRC PFKEYS file.";
 
    IFCASE("TEST")
        {
        setline(tmp + 1, 1, 0);
        return "";
        }
    IFCASE("DELAY")
        {
        setline(tmp + 1, 0, 0);
        return "";
        }
    IFCASE("IMMED")
        return tmp + 1;
 
    return "Error in IRC PFKEYS file.";
}
 
static char *
strdup(s)
char *s;
{
    char *tmp = (char *)malloc(strlen(s) + 1);
 
    if (tmp)
        strcpy(tmp, s);
 
    return tmp;
}
 
loadpfile()
{
    static char *t = \
    "DELAY Functionkeybindings may be defined in file IRC PFKEYS";
    int i;
    char buf[512], *s, *nl;
    FILE *fd = fopen("IRC.PFKEYS", "r");
 
    for (i=0; i < 24; i++)
        texts[i] = t;
 
    if (fd)
        while (fgets(buf, sizeof buf, fd))
            {
            if (!(s = index(buf, ' ')))
        continue;
            if (nl = index(buf, '\n'))
                *nl = ' ';
            if ((i = atoi(buf)) == 0 || i > 24)
                continue;
            texts[i] = strdup(s+1);
            }
}
