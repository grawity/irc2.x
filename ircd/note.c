/************************************************************************
 *   IRC - Internet Relay Chat, ircd/note.c
 *   Copyright (C) 1990 Jarle Lyngaas
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
 *        Author: Jarle Lyngaas
 *        E-mail: jarle@stud.cs.uit.no
 *        On IRC: Wizible
 */

#include "struct.h"
#ifdef MSG_NOTE
#include "numeric.h"
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#define VERSION "v1.7a"

#define NOTE_SAVE_FREQUENCY 60 /* Frequency of save time in minutes */
#define NOTE_MAXSERVER_TIME 24*60 /* Max hours for a message in the server */
#define NOTE_MAXSERVER_MESSAGES 2000 /* Max number of messages in the server */
#define NOTE_MAXUSER_MESSAGES 200 /* Max number of messages for each user */
#define NOTE_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
#define NOTE_MAXUSER_WILDCARDS 20 /* Max number of user toname wildcards */

#define LAST_CLIENTS 20    
#define GET_CHANNEL_TIME 5
#define BUF_LEN 256
#define MSG_LEN 512
#define REALLOC_SIZE 1024

#define FLAGS_OPER (1<<0)
#define FLAGS_LOCALOPER (1<<1)
#define FLAGS_NAME (1<<2)
#define FLAGS_LOG (1<<3)
#define FLAGS_NOT_SAVE (1<<4)
#define FLAGS_REPEAT_ONCE (1<<5)
#define FLAGS_REPEAT_UNTIL_TIMEOUT (1<<6)
#define FLAGS_ALL_NICK_VALID (1<<7)
#define FLAGS_SERVER_GENERATED_NOTICE (1<<8)
#define NOT_USED_1 (1<<9)
#define FLAGS_SERVER_GENERATED_DESTINATION (1<<10)
#define FLAGS_DISPLAY_IF_RECEIVED (1<<11)
#define FLAGS_DISPLAY_IF_CORRECT_FOUND (1<<12)
#define FLAGS_DISPLAY_IF_DEST_REGISTER (1<<13)
#define FLAGS_SEND_ONLY_IF_SENDER_ON_IRC (1<<14)
#define FLAGS_SEND_ONLY_IF_NOTE_CHANNEL (1<<15)
#define FLAGS_SEND_ONLY_IF_LOCALSERVER (1<<16)
#define FLAGS_SEND_ONLY_IF_THIS_SERVER (1<<17)
#define FLAGS_SEND_ONLY_IF_DESTINATION_OPER (1<<18)
#define FLAGS_SEND_ONLY_IF_NICK_NOT_NAME (1<<19)
#define FLAGS_SEND_ONLY_IF_NOT_EXCEPTION (1<<20)
#define FLAGS_KEY_TO_OPEN_SECRET_PORTAL (1<<21)
#define FLAGS_FIND_CORRECT_DEST_SEND_ONCE (1<<22)
#define FLAGS_DISPLAY_CHANNEL_DEST_REGISTER (1<<23)
#define FLAGS_DISPLAY_SERVER_DEST_REGISTER (1<<24)
#define FLAGS_SEND_ONLY_IF_SIGNONOFF (1<<25)
#define FLAGS_REGISTER_NEWNICK (1<<26)
#define FLAGS_NOTICE_RECEIVED_MESSAGE (1<<27)
#define FLAGS_RETURN_CORRECT_DESTINATION (1<<28)
#define FLAGS_NOT_QUEUE_REQUESTS (1<<29)
#define FLAGS_NEWNICK_DISPLAYED (1<<30)
#define NOT_USED_2 (1<<31)

#define DupString(x,y) do { x = MyMalloc(strlen(y)+1); strcpy(x,y); } while (0)
#define DupNewString(x,y) if (!StrEq(x,y)) { free(x); DupString(x,y); }  
#define MyEq(x,y) (!myncmp(x,y,strlen(x)))
#define Usermycmp(x,y) mycmp(x,y)
#define Key(sptr) KeyFlags(sptr,-1)
#define IsOperHere(sptr) (IsOper(sptr) && MyConnect(sptr))

typedef struct MsgClient {
            char *fromnick, *fromname, *fromhost, *tonick,
                 *toname, *tohost, *message,*passwd;
            long timeout, time, flags;
            int id, InToName, InFromName;
          } aMsgClient;
  
static int note_mst = NOTE_MAXSERVER_TIME,
           note_mum = NOTE_MAXUSER_MESSAGES,
           note_msm = NOTE_MAXSERVER_MESSAGES,
           note_msw = NOTE_MAXSERVER_WILDCARDS,
           note_muw = NOTE_MAXUSER_WILDCARDS,
           note_msf = NOTE_SAVE_FREQUENCY*60,
           wildcard_index = 0,
           toname_index = 0,
           fromname_index = 0,
           max_toname,
           max_wildcards,
           max_fromname,
           old_clock = 0,
           m_id = 0,
           changes_to_save = 0;

static aMsgClient **ToNameList, **FromNameList, **WildCardList;
extern aClient *client;
extern aChannel *channel;
extern char *MyMalloc(), *MyRealloc(), *myctime();
static char *ptr = "IRCnote", *note_save_filename_tmp;

static numeric(string)
char *string;
{
 register char *c = string;

 while (*c) if (!isdigit(*c)) return 0; else c++;
 return 1;
}

static char *wildcards(string)
char *string;
{
 register char *c;

 for (c = string; *c; c++) if (*c == '*' || *c == '?') return(c);
 return 0;
}

static only_wildcards(string)
char *string;
{
 register char *c;

 for (c = string;*c;c++) 
      if (*c != '*' && *c != '?') return 0;
 return 1;
}

static number_fromname()
{
 register int t, nr = 0;
 long clock;

 time (&clock);
 for (t = 1;t <= fromname_index; t++) 
      if (FromNameList[t]->timeout > clock
          && !(FromNameList[t]->flags & FLAGS_SERVER_GENERATED_NOTICE)
          && !(FromNameList[t]->flags & FLAGS_SERVER_GENERATED_DESTINATION)) 
       nr++;
 return nr;         
}

static strcasecmp(s1, s2)
char *s1, *s2;
{
 static unsigned char charmap[256];
 register unsigned char *cm = charmap, *us1 = (unsigned char *)s1,
                        *us2 = (unsigned char *)s2;
 static int generated = 0;
 register int t = 0,t1 = 0;

 if (!generated) {
     generated = 1;while (t < 65) charmap[t++] = t1++;
     t1 = 97;while (t < 91) charmap[t++] = t1++;
     t1 = 91;while (t < 193) charmap[t++] = t1++;
     t1 = 225;while (t < 219) charmap[t++] = t1++;
     t1 = 219;while (t < 256) charmap[t++] = t1++;
   }
 while (cm[*us1] == cm[*us2++])
        if (*us1++ == '\0') return 0;
 return(cm[*us1] - cm[*--us2]);
}


static first_tnl_indexnode(name)
char *name;
{
 register int s, t = toname_index+1, b = 0, tname;
 aMsgClient *msgclient;

 if (!t) return 0;
 while ((s = b+t >> 1) != b) {
       msgclient = ToNameList[s];
       tname = (msgclient->flags & FLAGS_NAME) ? 1 : 0;
       if (strcasecmp(tname ? msgclient->toname : msgclient->tonick, name) < 0)
        b = s; else t = s;
  }
 return t;
}

static last_tnl_indexnode(name)
char *name;
{
 register int s, t = toname_index+1, b = 0, tname;
 aMsgClient *msgclient;

 if (!t) return 0;
 while ((s = b+t >> 1) != b) {
       msgclient = ToNameList[s];
       tname = (msgclient->flags & FLAGS_NAME) ? 1 : 0;
       if (strcasecmp(tname ? msgclient->toname : msgclient->tonick, name) > 0)
        t = s; else b = s;
   }
 return b;
}

static first_fnl_indexnode(fromname)
char *fromname;
{
 register int s, t = fromname_index+1, b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(FromNameList[s]->fromname,fromname)<0) b = s; else t = s;
 return t;
}

static last_fnl_indexnode(fromname)
char *fromname;
{
 register int s, t = fromname_index+1, b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(FromNameList[s]->fromname,fromname)>0) t = s; else b = s;
 return b;
}
 
static void new(passwd,fromnick,fromname,fromhost,tonick,
                toname,tohost,flags,timeout,time,message)
char *passwd,*fromnick,*fromname,*fromhost,
     *tonick,*toname,*tohost,*message;
long timeout,time,flags;
{
 register aMsgClient **index_p;
 register int t;
 int allocate,first,last,n;
 aMsgClient *msgclient;
 char *c;
            
 if (number_fromname() > note_msm) return; 
 if (fromname_index == max_fromname-1) {
    max_fromname += REALLOC_SIZE;
    allocate = max_fromname*sizeof(FromNameList)+1;
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (wildcard_index == max_wildcards-1) {
    max_wildcards += REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList)+1;
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (toname_index == max_toname-1) {
    max_toname += REALLOC_SIZE;
    allocate = max_toname*sizeof(ToNameList)+1;
    ToNameList = (aMsgClient **) MyRealloc((char *)ToNameList,allocate);
  }
 if (!wildcards(toname) && wildcards(tonick)) 
  flags |= FLAGS_NAME; else flags &= ~FLAGS_NAME;           
 msgclient = (aMsgClient *)MyMalloc(sizeof(struct MsgClient));
 DupString(msgclient->passwd,passwd);
 DupString(msgclient->fromnick,fromnick);
 DupString(msgclient->fromname,fromname);
 DupString(msgclient->fromhost,fromhost);
 DupString(msgclient->tonick,tonick);
 DupString(msgclient->toname,toname);
 DupString(msgclient->tohost,tohost);
 DupString(msgclient->message,message);
 msgclient->flags = flags;
 msgclient->timeout = timeout;
 msgclient->time = time;
 if (flags & FLAGS_NAME || !wildcards(tonick)) {
    n = last_tnl_indexnode(flags & FLAGS_NAME ? toname : tonick) + 1;
    index_p = ToNameList+toname_index;
    toname_index++;t = toname_index-n;
    while (t--) {
          (*index_p)->InToName++; 
          index_p[1] = (*index_p);index_p--;
       }
    msgclient->InToName = n;
    ToNameList[n] = msgclient;
  } else { 
          wildcard_index++;
          WildCardList[wildcard_index] = msgclient;
          msgclient->InToName = wildcard_index;
     }
 first = first_fnl_indexnode(fromname);
 last = last_fnl_indexnode(fromname);
 if (!(n = first)) n = 1;
 index_p = FromNameList+n;
 while (n <= last && strcasecmp(msgclient->fromhost,(*index_p)->fromhost)>0) {
        index_p++;n++;
   }
 while (n <= last && strcasecmp(msgclient->fromnick,(*index_p)->fromnick)>0) { 
        index_p++;n++;
   }
 index_p = FromNameList+fromname_index;
 fromname_index++;t = fromname_index-n; 
 while(t--) {
      (*index_p)->InFromName++; 
       index_p[1] = (*index_p);index_p--;
   }
 FromNameList[n] = msgclient;
 msgclient->InFromName = n;
 changes_to_save = 1;
 msgclient->id = ++m_id;
}

static void r_code(string,fp)
register char *string;
register FILE *fp;
{
 register char *cp = ptr;

 do {
      *string = getc(fp)-*cp;
      if (!*++cp) cp = ptr;
  } while (*string++);
}

static void w_code(string,fp)
register char *string;
register FILE *fp;
{
 register char *cp = ptr;

 do {
      putc((char)(*string+*cp),fp);
      if (!*++cp) cp = ptr;
  } while (*string++);
}

static char *ltoa(i)
register long i;
{
 static unsigned char c[20];
 register unsigned char *p = c;

  do {
      *p++ = (i&127) + 1;
       i >>= 7;
    } while(i > 0);
    *p = 0;
    return (char *) c;
}

static long atol(c)
register unsigned char *c;
{
 register long a = 0;
 register unsigned char *s = c;

 while (*s != 0) ++s;
 while (--s >= c) {
        a <<= 7;
        a += (*s) - 1;
   }
  return a;
}

void init_messages()
{
 FILE *fp,*fopen();
 long timeout,atime,clock,flags;
 int t,nr,allocate;
 char passwd[20],fromnick[BUF_LEN+3],fromname[BUF_LEN+3],
      fromhost[BUF_LEN+3],tonick[BUF_LEN+3],toname[BUF_LEN+3],
      tohost[BUF_LEN+3],message[MSG_LEN+3],buf[20];

 max_fromname = max_toname = max_wildcards = REALLOC_SIZE;
 allocate = REALLOC_SIZE*sizeof(FromNameList)+1;
 ToNameList = (aMsgClient **) MyMalloc(allocate);
 FromNameList = (aMsgClient **) MyMalloc(allocate);
 WildCardList = (aMsgClient **) MyMalloc(allocate); 
 note_save_filename_tmp = MyMalloc(strlen(NOTE_SAVE_FILENAME)+6);
 sprintf(note_save_filename_tmp,"%s.tmp",NOTE_SAVE_FILENAME);
 time(&clock);old_clock = clock;
 fp = fopen(NOTE_SAVE_FILENAME,"r");
 if (!fp) return;
 r_code(buf,fp);note_msm = atol(buf);
 r_code(buf,fp);note_mum = atol(buf);
 r_code(buf,fp);note_msw = atol(buf);
 r_code(buf,fp);note_muw = atol(buf);
 r_code(buf,fp);note_mst = atol(buf);
 r_code(buf,fp);note_msf = atol(buf);
 for (;;) {
        r_code(passwd,fp);if (!*passwd) break;
        r_code(fromnick,fp);r_code(fromname,fp);
        r_code(fromhost,fp);r_code(tonick,fp);r_code(toname,fp);
        r_code(tohost,fp);r_code(buf,fp),flags = atol(buf);
        r_code(buf,fp);timeout = atol(buf);r_code(buf,fp);
        atime = atol(buf);r_code(message,fp);
        if (clock < timeout)  
            new(passwd,fromnick,fromname,fromhost,tonick,toname,
                tohost,flags,timeout,atime,message);
  }
 fclose(fp);
}

static numeric_host(host)
char *host;
{
  while (*host) {
        if (!isdigit(*host) && *host != '.') return 0;
        host++;
   } 
 return 1;
}

static elements_inhost(host)
char *host;
{
 int t = 0;
 while (*host) {
       if (*host == '.') t++;
       host++;
    }
 return t;
}

static char *local_host(host)
char *host;
{
 static char buf[BUF_LEN+3];
 char *buf_p = buf, *host_p;
 int t, e;

 e = elements_inhost(host);
 if (e < 2) return host;
 if (!numeric_host(host)) {
     if (!e) return host;
     host_p = host;
     if (e > 2) t = 2; else t = 1;
    while (t--) { 
           while (*host_p && *host_p != '.') {
                  host_p++;buf_p++;
             }
           if (t && *host_p) { 
              host_p++; buf_p++;
	    }
      }
     buf[0] = '*';
     strcpy(buf+1,host_p);
  } else {
          host_p = buf;
          strcpy(buf,host);
          while (*host_p) host_p++;    
          t = 2;
          while(t--) {
               while (host_p != buf && *host_p-- != '.'); 
	     }
           host_p+=2;
           *host_p++ = '*';
           *host_p = 0;
       }
  return buf;
}

static char *external_host(host)
char *host;
{
 static char buf[BUF_LEN+3];
 char *buf_p = buf, *host_p;
 int t;

 if (!elements_inhost(host)) return host;
 if (!numeric_host(host)) {
     host_p = host;
     while (*host_p) host_p++;
     while (host_p != host && *host_p-- != '.'); 
     buf[0] = '*';buf[1] = '.';
     if (*host_p && host_p[1] == '.') host_p += 2;
     strcpy (buf+2,host_p);
  } else {
          host_p = host;t = 0;
          while (*host_p && *host_p != '.') 
                 buf[t++] = (*host_p++);
          buf[t++] = '.';
          buf[t++] = '*';
          buf[t] = 0;
       }
  return buf;
}

static host_check(host1,host2,mode)
char *host1,*host2,mode;
{
 char buf[BUF_LEN+3];
 
 if (numeric_host(host1) != numeric_host(host2)) return 0;
 if (mode == 'l') {
     strcpy(buf,local_host(host1));
     if (!mycmp(buf,local_host(host2))) return 1;
  } else {
           strcpy(buf,external_host(host1));
	   if (!mycmp(buf,external_host(host2))) return 1;
     }
 return 0;
}

static long timegm(tm)
struct tm *tm;
{
 static int days[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
 register long mday = tm->tm_mday-1, mon = tm->tm_mon, year = tm->tm_year-70;

 mday+=((year+2) >> 2)+days[mon];
 if (mon < 2 && !((year+2) & 3)) mday--;
 return tm->tm_sec+tm->tm_min*60+tm->tm_hour*3600+mday*86400+year*31536000;
}

static void start_day(clock,mode)
long *clock;
char mode;
{
 struct tm *tm;
 long tm_gmtoff = 0;

 if (mode == 'l') {
    tm = localtime(clock);
    tm_gmtoff = timegm(tm) - *clock;
  } else tm = gmtime(clock);
 tm->tm_sec = 0;tm->tm_min = 0;tm->tm_hour = 0;
 *clock = timegm(tm) - tm_gmtoff;
}

static same_day(clock1,clock2)
long clock1,clock2;
{
 start_day(&clock1,'l');
 start_day(&clock2,'l');
 if (clock1 == clock2) return 1;
 return 0;
}

static long set_date(sptr,time_s)
aClient *sptr;
char *time_s;
{
 static char buf[BUF_LEN];
 struct tm *tm;
 long clock,tm_gmtoff;
 int t,t1,month,date,year;
 char *c = time_s,arg[3][BUF_LEN];
 static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
    }; 

 time (&clock);
 tm = localtime(&clock);
 tm_gmtoff = timegm(tm)-clock;
 tm->tm_sec = 0;
 tm->tm_min = 0;
 tm->tm_hour = 0;
 if (*time_s == '-') {
    if (!time_s[1]) return 0;
    return timegm(tm)-tm_gmtoff-3600*24*atoi(time_s+1);
  }
 for (t = 0;t<3;t++) {
      t1 = 0;
      while (*c && *c != '/' && *c != '.' && *c != '-')
             arg[t][t1++] = (*c++);
      arg[t][t1] = 0;
      if (*c) c++;
  } 

 date = atoi(arg[0]);
 if (*arg[0] && (date<1 || date>31)) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown date",sptr->name);
     return -1;
  }
 month = atoi(arg[1]);
 if (month) month--;
  else for (t = 0;t<12;t++) {
            if (MyEq(arg[1],months[t])) { 
                month = t;break;
             } 
            month = -1;
        }
 if (*arg[1] && (month<0 || month>11)) {
      sendto_one(sptr,"NOTICE %s :#?# Unknown month",sptr->name);
      return -1; 
  }       
 year = atoi(arg[2]);
 if (*arg[2] && (year<71 || year>99)) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown year",sptr->name);
     return -1;
   }
 tm->tm_mday = date;
 if (*arg[1]) tm->tm_mon = month;
 if (*arg[2]) tm->tm_year = year;
 return timegm(tm)-tm_gmtoff;
}

static local_check(sptr, msgclient, passwd, flags, tonick,
                   toname, tohost, time_l, id)
aClient *sptr;
aMsgClient *msgclient;
char *passwd, *tonick, *toname, *tohost;
long flags,time_l;
int id;
{
 if (msgclient->flags == flags 
     && (!id || id == msgclient->id)
     && !Usermycmp(sptr->user->username,msgclient->fromname)
     && (!mycmp(sptr->name,msgclient->fromnick)
         || msgclient->flags & FLAGS_ALL_NICK_VALID)
     && (!time_l || msgclient->time >= time_l 
                    && msgclient->time < time_l+24*3600)
     && (!matches(tonick,msgclient->tonick))
     && (!matches(toname,msgclient->toname))
     && (!matches(tohost,msgclient->tohost))
     && (*msgclient->passwd == '*' && !msgclient->passwd[1]
         || StrEq(passwd,msgclient->passwd))) {
     if (msgclient->flags & FLAGS_OPER && *msgclient->passwd == '*'
         && !IsOper(sptr)) return 0; 
         if (host_check(sptr->user->host,msgclient->fromhost,'l')) return 1;
  }
 return 0;
}

static send_flag(flags)
long flags;
{
  if (flags & FLAGS_RETURN_CORRECT_DESTINATION
      || flags & FLAGS_DISPLAY_IF_CORRECT_FOUND
      || flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL
      || flags & FLAGS_DISPLAY_IF_DEST_REGISTER
      || flags & FLAGS_SERVER_GENERATED_DESTINATION
      || flags & FLAGS_NOT_QUEUE_REQUESTS
      || flags & FLAGS_REPEAT_UNTIL_TIMEOUT
         && !(flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE)) return 0;
  return 1;
}

static void display_flags(flags,c)
long flags;
char *c;
{
 char t = 0;
 int send = 0;
 
 if (send_flag(flags)) send = 1;
 if (send) c[t++] = '/'; else c[t++] = '<';
 if (flags & FLAGS_OPER) c[t++] = 'O';
 if (flags & FLAGS_SERVER_GENERATED_DESTINATION) c[t++] = 'H';
 if (flags & FLAGS_ALL_NICK_VALID) c[t++] = 'C';
 if (flags & FLAGS_SERVER_GENERATED_NOTICE) c[t++] = 'G';
 if (flags & FLAGS_DISPLAY_IF_RECEIVED) c[t++] = 'D';
 if (flags & FLAGS_DISPLAY_IF_CORRECT_FOUND) c[t++] = 'L';
 if (flags & FLAGS_NOT_SAVE) c[t++] = 'S';
 if (flags & FLAGS_REPEAT_ONCE) c[t++] = 'P';
 if (flags & FLAGS_DISPLAY_IF_DEST_REGISTER) c[t++] = 'X';
 if (flags & FLAGS_DISPLAY_CHANNEL_DEST_REGISTER) c[t++] = 'J';
 if (flags & FLAGS_DISPLAY_SERVER_DEST_REGISTER) c[t++] = 'A';
 if (flags & FLAGS_REPEAT_UNTIL_TIMEOUT) c[t++] = 'R';
 if (flags & FLAGS_SEND_ONLY_IF_NOTE_CHANNEL) c[t++] = 'M';
 if (flags & FLAGS_SEND_ONLY_IF_LOCALSERVER) c[t++] = 'V';
 if (flags & FLAGS_SEND_ONLY_IF_THIS_SERVER) c[t++] = 'I';
 if (flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME) c[t++] = 'Q';
 if (flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) c[t++] = 'E';
 if (flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL) c[t++] = 'K';
 if (flags & FLAGS_SEND_ONLY_IF_SENDER_ON_IRC) c[t++] = 'Y';
 if (flags & FLAGS_NOTICE_RECEIVED_MESSAGE) c[t++] = 'N';
 if (flags & FLAGS_RETURN_CORRECT_DESTINATION) c[t++] = 'F';
 if (flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) c[t++] = 'B';
 if (flags & FLAGS_REGISTER_NEWNICK) c[t++] = 'T';
 if (flags & FLAGS_SEND_ONLY_IF_SIGNONOFF) c[t++] = 'U';
 if (flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER) c[t++] = 'W';
 if (flags & FLAGS_NOT_QUEUE_REQUESTS) c[t++] = 'Z';
 if (t == 1) c[t++] = '-';
 if (send) c[t++] = '/'; else c[t++] = '>';
 c[t] = 0;
}

static void remove_msg(msgclient)
aMsgClient *msgclient;
{
 register aMsgClient **index_p;
 register int t;
 int n,allocate;

 n = msgclient->InToName;
 if (msgclient->flags & FLAGS_NAME 
     || !wildcards(msgclient->tonick)) {
     index_p = ToNameList+n; 
     t = toname_index-n;   
     while(t--) {
                  *index_p = index_p[1];
                  (*index_p++)->InToName--;
                }
     ToNameList[toname_index] = 0;
     toname_index--;
  } else { 
          index_p = WildCardList+n; 
          t = wildcard_index-n;
          while (t--) {
                 *index_p = index_p[1];
                (*index_p++)->InToName--;
            }
          WildCardList[wildcard_index] = 0;
          wildcard_index--;
     }
 n = msgclient->InFromName;   
 index_p = FromNameList+n;t = fromname_index-n;
 while(t--) { 
       *index_p = index_p[1];
      (*index_p++)->InFromName--;
   }
 FromNameList[fromname_index] = 0;
 fromname_index--;
 free(msgclient->passwd);
 free(msgclient->fromnick);
 free(msgclient->fromname);
 free(msgclient->fromhost);
 free(msgclient->tonick);
 free(msgclient->toname);
 free(msgclient->tohost);
 free(msgclient->message);
 free((char *)msgclient);
 if (max_fromname - fromname_index > REALLOC_SIZE *2) {
    max_fromname -= REALLOC_SIZE; 
    allocate = max_fromname*sizeof(FromNameList)+1;
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (max_wildcards - wildcard_index > REALLOC_SIZE * 2) {
    max_wildcards -= REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList)+1;
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (max_toname - toname_index > REALLOC_SIZE * 2) {
    max_toname -= REALLOC_SIZE;
    allocate = max_toname*sizeof(ToNameList)+1;
    ToNameList = (aMsgClient **) MyRealloc((char *)ToNameList,allocate);
  }
}

static KeyFlags(sptr,flags) 
aClient *sptr;
long flags;
{
 int t,last, first_tnl, last_tnl, nick_list = 1;
 long clock;
 aMsgClient **index_p,*msgclient; 
 
 t = first_tnl_indexnode(sptr->name);
 last = last_tnl_indexnode(sptr->name);
 first_tnl = first_tnl_indexnode(sptr->user->username);
 last_tnl = last_tnl_indexnode(sptr->user->username);
 index_p = ToNameList;
 time (&clock);
 while (1) {
     while (last && t <= last) {
	    msgclient = index_p[t];
	    if (msgclient->flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL
		&& msgclient->flags & flags
		&& !matches(msgclient->tonick,sptr->name)
		&& !matches(msgclient->toname,sptr->user->username)
		&& !matches(msgclient->tohost,sptr->user->host)) return 1;
            t++;
	}
     if (index_p == ToNameList) {
         if (nick_list) {
             nick_list = 0; t = first_tnl; last = last_tnl;
	  } else {
                   index_p = WildCardList;
                   t = 1; last = wildcard_index;
	      }
      } else return 0;
 }
}

static set_flags(sptr, string, flags, mode, type)
aClient *sptr;
char *string, mode, *type;
long *flags;
{
 char *c,on,buf[40],cu;
 int op,uf = 0;

 op = IsOperHere(sptr) ? 1:0; 
 for (c = string; *c; c++) {
      if (*c == '+') { 
          on = 1;continue;
       } 
      if (*c == '-') { 
          on = 0;continue;
       } 
      cu = islower(*c)?toupper(*c):*c;
      switch (cu) {
              case 'S': if (on) *flags |= FLAGS_NOT_SAVE;
                         else *flags &= ~FLAGS_NOT_SAVE;
                        break;             
              case 'R': if (on) *flags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
                         else *flags &= ~FLAGS_REPEAT_UNTIL_TIMEOUT;
                        break;        
              case 'P': if (on) *flags |= FLAGS_REPEAT_ONCE;
                         else *flags &= ~FLAGS_REPEAT_ONCE;
                        break;             
              case 'M': if (on) *flags |= FLAGS_SEND_ONLY_IF_NOTE_CHANNEL;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_NOTE_CHANNEL;
                        break;             
              case 'V': if (on) *flags |= FLAGS_SEND_ONLY_IF_LOCALSERVER;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_LOCALSERVER;
                        break;             
              case 'I': if (on) *flags |= FLAGS_SEND_ONLY_IF_THIS_SERVER; 
                         else *flags &= ~FLAGS_SEND_ONLY_IF_THIS_SERVER;
                        break;             
              case 'W': if (on) *flags |= FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
                        break;             
              case 'Q': if (on) *flags |= FLAGS_SEND_ONLY_IF_NICK_NOT_NAME;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_NICK_NOT_NAME;
                        break;             
              case 'E': if (on) *flags |= FLAGS_SEND_ONLY_IF_NOT_EXCEPTION; 
                         else *flags &= ~FLAGS_SEND_ONLY_IF_NOT_EXCEPTION;
                        break;             
              case 'U': if (on) *flags |= FLAGS_SEND_ONLY_IF_SIGNONOFF;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_SIGNONOFF;
                        break;
              case 'Y': if (on) *flags |= FLAGS_SEND_ONLY_IF_SENDER_ON_IRC;
                         else *flags &= FLAGS_SEND_ONLY_IF_SENDER_ON_IRC;
                        break;             
              case 'N': if (on) *flags |= FLAGS_NOTICE_RECEIVED_MESSAGE;
                         else *flags &= ~FLAGS_NOTICE_RECEIVED_MESSAGE;
                        break;             
              case 'D': if (on) *flags |= FLAGS_DISPLAY_IF_RECEIVED;
                         else *flags &= ~FLAGS_DISPLAY_IF_RECEIVED;
                        break;             
              case 'F': if (on) *flags |= FLAGS_RETURN_CORRECT_DESTINATION;
                         else *flags &= ~FLAGS_RETURN_CORRECT_DESTINATION;
                        break;
              case 'L': if (on) *flags |= FLAGS_DISPLAY_IF_CORRECT_FOUND;
                         else *flags &= ~FLAGS_DISPLAY_IF_CORRECT_FOUND;
                        break;
              case 'X':  if (on) *flags |= FLAGS_DISPLAY_IF_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_IF_DEST_REGISTER;
                        break;
              case 'J':  if (on) *flags |= FLAGS_DISPLAY_CHANNEL_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_CHANNEL_DEST_REGISTER;
                        break;
              case 'A':  if (on) *flags |= FLAGS_DISPLAY_SERVER_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_SERVER_DEST_REGISTER;
                        break;
              case 'C': if (on) *flags |= FLAGS_ALL_NICK_VALID;
                         else *flags &= ~FLAGS_ALL_NICK_VALID;
                        break;
              case 'T': if (KeyFlags(sptr,FLAGS_REGISTER_NEWNICK) 
                            || op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_REGISTER_NEWNICK;
                             else *flags &= ~FLAGS_REGISTER_NEWNICK;
                         } else buf[uf++] = cu;
                        break;
              case 'B': if (KeyFlags(sptr,FLAGS_FIND_CORRECT_DEST_SEND_ONCE)
                            || op || mode == 'd' || !on) {
                            if (on) 
                               *flags |= FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                             else *flags &= ~FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                         } else buf[uf++] = cu;
                        break;
              case 'K': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_KEY_TO_OPEN_SECRET_PORTAL;
                             else *flags &= ~FLAGS_KEY_TO_OPEN_SECRET_PORTAL;
                         } else buf[uf++] = cu;
                        break;
              case 'O': if (mode == 'd') {
                           if (on) *flags |= FLAGS_OPER;
                            else *flags &= ~FLAGS_OPER;
       		         } else buf[uf++] = cu;
                         break;
              case 'G': if (mode == 'd') {
                           if (on) *flags |= FLAGS_SERVER_GENERATED_NOTICE;
                             else *flags &= ~FLAGS_SERVER_GENERATED_NOTICE;
		         } else buf[uf++] = cu;
                        break;
              case 'H': if (mode == 'd') {
                           if (on) 
                               *flags |= FLAGS_SERVER_GENERATED_DESTINATION;
                            else *flags &= ~FLAGS_SERVER_GENERATED_DESTINATION;
                          } else buf[uf++] = cu;
                        break;
              case 'Z': if (KeyFlags(sptr,FLAGS_NOT_QUEUE_REQUESTS)
                            || op || mode == 'd' || !on) {
                             if (on) *flags |= FLAGS_NOT_QUEUE_REQUESTS;
			      else *flags &= ~FLAGS_NOT_QUEUE_REQUESTS;
                         } else buf[uf++] = cu;
                         break;  
              default:  buf[uf++] = cu;
        } 
  }
 buf[uf] = 0;
 if (uf) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown flag%s: %s %s",sptr->name,
                uf> 1  ? "s" : "",buf,type);
     return 0;
  }
 if (mode == 's') {
    if (*flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)
        sendto_one(sptr,"NOTICE %s :### %s", sptr->name,
                   "WARNING: Recipient got keys to unlock the secret portal;");
     else if (*flags & FLAGS_NOT_QUEUE_REQUESTS)
              sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                         "WARNING: Recipient can't queue requests;");
      else if (*flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE
               && *flags & FLAGS_REPEAT_UNTIL_TIMEOUT
               && send_flag(*flags)) 
               sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                         "WARNING: Broadcast message in action;");
 }
 return 1;
}

static void split(string, nick, name, host)
char *string, *nick, *name, *host;
{
 char *np = string, *fill;

 *nick = 0; *name = 0; *host = 0;
 fill = nick;
 while (*np) { 
        *fill = *np;
        if (*np == '!') { 
           *fill = 0; fill = name;
	 } else if (*np == '@') { 
                    *fill = 0; fill = host;
 	         } else fill++;
        np++;
   } 
 *fill = 0;       
 if (!*nick) { *nick = '*'; nick[1] = 0; } 
 if (!*name) { *name = '*'; name[1] = 0; } 
 if (!*host) { *host = '*'; host[1] = 0; } 
}

void save_messages()
{
 aClient *cptr = client;
 aMsgClient *msgclient;
 long clock;
 FILE *fp,*fopen();
 int t,t1 = 0;

 for (cptr = client;cptr;cptr = cptr -> next) if (MyConnect(cptr)) t1 = 1;
 if (!t1) for (t = 1;t <= fromname_index; t++) FromNameList[t]->id = ++t1;
 if (changes_to_save) changes_to_save = 0; else return;
 time(&clock);
 fp = fopen(note_save_filename_tmp,"w");
 if (!fp) {
    sendto_ops("Can't open for write; %s", NOTE_SAVE_FILENAME);
    return;
 }
 w_code(ltoa((long)note_msm),fp);
 w_code(ltoa((long)note_mum),fp);
 w_code(ltoa((long)note_msw),fp);
 w_code(ltoa((long)note_muw),fp);
 w_code(ltoa((long)note_mst),fp);
 w_code(ltoa((long)note_msf),fp);
 t = 1;
 while (fromname_index && t <= fromname_index) {
     msgclient = FromNameList[t];
     if (clock > msgclient->timeout) { 
         remove_msg(msgclient);continue;
      }  
     if (msgclient->flags & FLAGS_NOT_SAVE) {
         t++;continue;
       }
     w_code(msgclient->passwd,fp),w_code(msgclient->fromnick,fp);
     w_code(msgclient->fromname,fp);w_code(msgclient->fromhost,fp);
     w_code(msgclient->tonick,fp),w_code(msgclient->toname,fp);
     w_code(msgclient->tohost,fp),
     w_code(ltoa(msgclient->flags),fp);
     w_code(ltoa(msgclient->timeout),fp);
     w_code(ltoa(msgclient->time),fp);
     w_code(msgclient->message,fp);
     t++;
  }
 w_code("",fp);
 fclose(fp);unlink(NOTE_SAVE_FILENAME);
 link(note_save_filename_tmp,NOTE_SAVE_FILENAME);
 unlink(note_save_filename_tmp);
 chmod(NOTE_SAVE_FILENAME, 432);
}

static filter_flag(cptr, sptr, from_ptr, nick, msgclient, mode, chn)
aClient *cptr, *sptr, *from_ptr;
char *nick, mode, *chn;
aMsgClient *msgclient;
{
 int local_server = 0;
 if (host_check(sptr->user->server, sptr->user->host,'e')) local_server = 1;
 if (!local_server && msgclient->flags & FLAGS_SEND_ONLY_IF_LOCALSERVER
     || !MyConnect(sptr) && msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER
     || (msgclient->flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME 
         && mode == 'a' && StrEq(nick,sptr->user->username))
     || (!IsOper(sptr) &&
         msgclient->flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER)
     || msgclient->flags & FLAGS_SEND_ONLY_IF_NOTE_CHANNEL &&
        (!chn || mycmp(chn, "+NOTE"))
     || msgclient->flags & FLAGS_SEND_ONLY_IF_SIGNONOFF && mode != 'a'
        && mode != 'e'
     || mode == 'v' && from_ptr == cptr) return 1;
  return 0;
 }

static char *check_flags(cptr, sptr,from_ptr,nick,newnick,destination,
                         msgclient,first_tnl,last_tnl, send,repeat,gnew,mode)
aClient *cptr, *sptr, *from_ptr;
aMsgClient *msgclient;
char *nick, *newnick, *destination, mode;
int first_tnl, last_tnl, *send, *repeat, *gnew;
{
 char *prev_nick, *new_nick, *c, mbuf[MSG_LEN+3], buf[BUF_LEN+3],
      ebuf[BUF_LEN+3], abuf[20], *sender, *message, wmode[2],
      *chn = NULL, *spy_channel = NULL, *spy_server = NULL;
 long prev_clock,new_clock,clock,time_l,gflags,timeout = 0;
 int t, t1, t2, first,last, exception, secret = 1, ss_hidden = 0,
     ss_secret = 0, hidden = 0, right_tonick = 0, show_channel = 0;
 aMsgClient **index_p, *fmsgclient;
 aChannel *chptr;

 if (mode != 'g')
    for (from_ptr = client; from_ptr; from_ptr = from_ptr->next)
         if (from_ptr->user && 
             (!mycmp(from_ptr->name,msgclient->fromnick)
              || msgclient->flags & FLAGS_ALL_NICK_VALID)
             && !Usermycmp(from_ptr->user->username,msgclient->fromname)
             && host_check(from_ptr->user->host,msgclient->fromhost,'l')) 
             break; 
 if (!mycmp(nick, msgclient->tonick)) right_tonick = 1;
 if (sptr->user->channel) chn = &(sptr->user->channel->chname[0]);
  else secret = 0;
 if (secret) for (chptr = channel; chptr; chptr = chptr->nextch)
                 if (PubChannel(chptr) && IsMember(sptr, chptr)) secret = 0;
 if (secret || mode == 'a') hidden = 1; 
 ss_secret = secret; ss_hidden = hidden;
 wmode[1] = 0; *send = 1; *repeat = 0; *gnew = 0; 
 message = msgclient->message;
 if (filter_flag(cptr, sptr, from_ptr, nick, msgclient, mode, chn)) {
     *send = 0; *repeat = 1; return message; 
  }
 if (msgclient->flags & FLAGS_SEND_ONLY_IF_SENDER_ON_IRC && !from_ptr) {
     *send = 0; *repeat = 1;
     return message;
  }
 if (!send_flag(msgclient->flags)) *send = 0;
 if (msgclient->flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) {
     c = message;
     for(;;) {
         exception = 0;t = t1 = 0 ;ebuf[0] = 0;
         while (*c != '!') {
                if (!*c || *c == ' ' || t > BUF_LEN) goto break2;
                ebuf[t++] = (*c++);
           }
         if (!*c++) goto break2;
         t1 += t;ebuf[t] = 0;
         if (!matches(ebuf,nick)) exception = 1;
         t=0;ebuf[0] = 0;            
         while (*c != '@') {
                if (!*c || *c == ' ' || t > BUF_LEN) goto break2;
                ebuf[t++] = (*c++);
           }
         if (!*c++) goto break2;
         t1 += t;ebuf[t] = 0;
         if (matches(ebuf,sptr->user->username)) exception = 0;
         t=0;ebuf[0] = 0;
         if (*c == ' ') goto break2;            
         while (*c != ' ') {
              if (!*c || t > BUF_LEN) break;
              ebuf[t++] = (*c++);
           } 
         if (*c) c++; t1 += t;ebuf[t] = 0;message += t1+3;
         if (exception && !matches(ebuf,sptr->user->host)) {
             *send = 0;*repeat = 1;
          } 
     } break2:; /* Level n breaks should be standard C :) */
     if (!*send && *repeat) return message;
 }
 if ((!ss_hidden || right_tonick) 
     && msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER) {
     time(&clock);
     msgclient->time = clock;
     msgclient->flags |= FLAGS_LOG;
     changes_to_save = 1;
   }
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER
     && from_ptr && from_ptr != sptr) {
     time(&clock);
     t = first_fnl_indexnode(from_ptr->user->username);
     last = last_fnl_indexnode(from_ptr->user->username);
     while (last && t <= last) {
            fmsgclient = FromNameList[t];
            if (fmsgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER
                && clock < fmsgclient->timeout
                && fmsgclient != msgclient
                && (!mycmp(from_ptr->name, fmsgclient->fromnick)
                    || fmsgclient->flags & FLAGS_ALL_NICK_VALID)
                && !Usermycmp(from_ptr->user->username, fmsgclient->fromname)
                && host_check(from_ptr->user->host, fmsgclient->fromhost,'l')
                && !matches(fmsgclient->tonick, nick)
                && !matches(fmsgclient->toname, sptr->user->username)
                && !matches(fmsgclient->tohost, sptr->user->host)
                && !(msgclient->flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION)
                && !(fmsgclient->flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION)
                && !filter_flag(cptr, sptr, from_ptr, nick, fmsgclient, 
                                mode, chn)) {
                t1 = wildcards(fmsgclient->tonick) ? 1 : 0;
                t2 = wildcards(msgclient->tonick) ? 1 : 0;
                if (!t1 && t2) goto break2b;
                if (t1 && !t2) { t++; continue; }
                t1 = (fmsgclient->flags & FLAGS_NAME) ? 1 : 0;
                t2 = (msgclient->flags & FLAGS_NAME) ? 1 : 0;
                if (t1 && !t2) goto break2b;
                if (!t1 && t2) { t++; continue; }
                if (fmsgclient->InFromName < msgclient->InFromName) 
                    goto break2b;
	      }
            t++;
       }
      if (mode == 'a' || mode == 'v'
          || mode == 'c' && !wildcards(msgclient->tonick)
          || mode == 'g' && (!(msgclient->flags & FLAGS_ALL_NICK_VALID)
                             || StrEq(destination, from_ptr->name)))
         msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED; 
         mbuf[0] = 0;
         for (chptr = channel; chptr; chptr = chptr->nextch)
              if (ShowChannel(from_ptr, chptr) && IsMember(sptr, chptr)) {
                  if (!IsMember(from_ptr, chptr)) show_channel = 1;
		   else if (mode != 'r') goto break2b;
	          strcat(mbuf, chptr->chname);
	 	  strcat(mbuf, " ");
	       }
              if (mode == 'r' && sptr->user->channel 
                  && !show_channel) goto break2b;
              if (mbuf[0] && PubChannel(sptr->user->channel) &&
                  msgclient->flags & FLAGS_DISPLAY_CHANNEL_DEST_REGISTER)
                  spy_channel = mbuf;
              if (msgclient->flags & FLAGS_DISPLAY_SERVER_DEST_REGISTER)
                  spy_server = sptr->user->server;
              if (spy_channel || spy_server)
                sprintf(ebuf,"%s%s%s%s%s", spy_channel ? " " : "",
                        spy_channel ? spy_channel : "",
                        spy_server ? " (" : "",
                        spy_server ? spy_server : "",
                        spy_server ? ")" : "");
               else ebuf[0] = 0;
              sprintf(buf,"%s@%s",sptr->user->username,sptr->user->host);
              switch (mode) {
		case 'r' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                              && !(only_wildcards(msgclient->tonick)
                                    && only_wildcards(msgclient->toname))
                              && !ss_secret && !right_tonick && !spy_channel) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :### %s (%s) %s%s %s",
                                           from_ptr->name, nick, buf,
                                           "signs on",ebuf,message);
				msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
		              } else 
                                 if (spy_channel && !ss_secret) {
                                     sendto_one(from_ptr,
                                     "NOTICE %s :### %s (%s) is on %s",
                                     from_ptr->name, nick, buf, spy_channel);
                                   msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
				  }
                           break;
                case 'v' :
  		case 'a' : if (!ss_hidden || right_tonick) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :### %s (%s) %s%s %s",
                                          from_ptr->name, nick, buf, 
                                          "signs on",ebuf,message);
                               msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
			    }
                           break;
		case 'c' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                               && !(only_wildcards(msgclient->tonick)
                                    && only_wildcards(msgclient->toname))
                               && (!ss_secret || right_tonick)) {
                                sendto_one(from_ptr,
                                           "NOTICE %s :### %s (%s) is %s%s %s",
					   from_ptr->name, nick,
					   buf, spy_channel ? "on" : "here",
                                           ebuf,message);
                                msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
                case 's' :
		case 'g' : if ((!ss_secret || right_tonick) &&
                               (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                                && !(only_wildcards(msgclient->tonick)
                                     && only_wildcards(msgclient->toname)))) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :### %s (%s) is %s%s %s",
			         	   from_ptr->name, nick, buf,
                                           spy_channel ? "on" : "on IRC now",
                                           ebuf,message);
                               msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
                case 'e' : if (!ss_secret || right_tonick)
                                sendto_one(from_ptr,
					   "NOTICE %s :### %s (%s) %s %s",
					   from_ptr->name, nick,
					   buf,"signs off", message);
                           break;
	        case 'n' : 
                         msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED;
                         if (ss_secret) { 
                          if (right_tonick) 
                              sendto_one(from_ptr,
                                         "NOTICE %s :### %s (%s) %s %s",
                                         from_ptr->name, nick, buf,
                                         "signs off", message);
		          } else {
                             if (mycmp(nick, newnick))
                                sendto_one(from_ptr,
                                           "NOTICE %s :### %s (%s) %s <%s> %s",
                                           from_ptr->name, nick, buf, 
                                           "changed name to", newnick, message);
                                if (!matches(msgclient->tonick,newnick))
                                    msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
			      }

               }
           break2b:;
         }

 if (*send && secret && mycmp(msgclient->tonick, sptr->name)
     || mode == 'e' || mode == 'n') { 
     *repeat = 1; *send = 0;
     return message;
  }
 while (mode != 'g' && (msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION
        || msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)) {
        if (*message && matches(message,sptr->info) || 
            ss_hidden && !right_tonick) {
           *repeat = 1; break;
	 }
        sprintf(mbuf,"Match for %s!%s@%s (%s) is: %s!%s@%s (%s)",
                msgclient->tonick,msgclient->toname,msgclient->tohost,
                *msgclient->message ? message : "*", nick,
                sptr->user->username,sptr->user->host,sptr->info);
        if (msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND && from_ptr) {
            sendto_one(from_ptr,"NOTICE %s :### %s",from_ptr->name,mbuf);
            break;
	 }
        t1 = 0; 
        t = first_tnl_indexnode(msgclient->fromname);
        last = last_tnl_indexnode(msgclient->fromname);
        while (last && t <= last) {
              if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_NOTICE &&
                  !Usermycmp(ToNameList[t]->toname, msgclient->fromname) &&
                  !mycmp(ToNameList[t]->tohost,
                         local_host(msgclient->fromhost))) {
                  t1++;
                  if (!mycmp(ToNameList[t]->message,mbuf)) {
                     t1 = note_mum; break;
		   }
	       }	  
              t++;
          }  
        if (t1 >= note_mum) break;
        gflags = 0;
        gflags |= FLAGS_OPER;
        gflags |= FLAGS_SERVER_GENERATED_NOTICE;
        if (msgclient->flags & FLAGS_OPER) 
            gflags |= FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
        time(&clock);*gnew = 1;
        new(msgclient->passwd,"SERVER","-","-",
            msgclient->flags & FLAGS_ALL_NICK_VALID ? "*":msgclient->fromnick,
            msgclient->fromname,local_host(msgclient->fromhost),
            gflags,note_mst*3600+clock,clock,mbuf);
        break;
  }
 if (msgclient->flags & FLAGS_REPEAT_UNTIL_TIMEOUT) *repeat = 1;
 while (*send && from_ptr != sptr &&
        (msgclient->flags & FLAGS_NOTICE_RECEIVED_MESSAGE
         || msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED)) {
        sprintf(buf,"%s (%s@%s) has received note queued %s",
                nick,sptr->user->username,sptr->user->host,
                myctime(msgclient->time));
        if (msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED && from_ptr) {
           sendto_one(from_ptr,"NOTICE %s :### %s", from_ptr->name,buf);
           break;
         }
       gflags = 0;
       gflags |= FLAGS_OPER;
       gflags |= FLAGS_SERVER_GENERATED_NOTICE;
       if (msgclient->flags & FLAGS_OPER) 
           gflags |= FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
       time(&clock);*gnew = 1;
       new(msgclient->passwd,"SERVER","-","-",
           msgclient->flags & FLAGS_ALL_NICK_VALID ? "*":msgclient->fromnick,
           msgclient->fromname,local_host(msgclient->fromhost),
           gflags,note_mst*3600+clock,clock,buf);
       break;
    }
 if (msgclient->flags & FLAGS_REPEAT_ONCE) {
     msgclient->flags &= ~FLAGS_REPEAT_ONCE;
     *repeat = 1;
   } 
 while (*send && msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) {
        if (mode == 'g') {
            *repeat = 1; *send = 0; break;
	  }
        if (*gnew) 
         last_tnl = last_tnl_indexnode(sptr->user->username);
        t = first_tnl;
        while (last_tnl && t <= last_tnl) {
             if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_DESTINATION
                 && (!mycmp(ToNameList[t]->fromnick,msgclient->fromnick)
                     || msgclient->flags & FLAGS_ALL_NICK_VALID)
                 && !Usermycmp(ToNameList[t]->fromname,msgclient->fromname)
                 && host_check(ToNameList[t]->fromhost,msgclient->fromhost,'l')
                 && (!(msgclient->flags & FLAGS_REGISTER_NEWNICK) 
                     || !mycmp(ToNameList[t]->tonick,nick))
                 && !Usermycmp(ToNameList[t]->toname,sptr->user->username)
                 && host_check(ToNameList[t]->tohost,sptr->user->host,'l')) {
                 *send = 0;break;
             }
             t++;
	  }
        if (!*send) break;
        gflags = 0;
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_DESTINATION;
        if (msgclient->flags & FLAGS_ALL_NICK_VALID) 
         gflags |= FLAGS_ALL_NICK_VALID;
        if (msgclient->flags & FLAGS_OPER) gflags |= FLAGS_OPER;
        time(&clock);*gnew = 1;
        new(msgclient->passwd,msgclient->fromnick, msgclient->fromname,
            msgclient->fromhost,nick,sptr->user->username,
            sptr->user->host,gflags,msgclient->timeout,clock,"");
        break;
   }
  return message;
}

static void check_lastclient(sptr, mode, clock)
aClient *sptr;
char mode;
long clock;
{
  void check_messages();
  static aClient *client_list[LAST_CLIENTS + 1];
  static int time_list[LAST_CLIENTS + 1], reset = 0;
  register int t = 0, longest_time = 0, new = -1;

 if (!reset) { 
    for (t = 0; t < LAST_CLIENTS; t++) client_list[t] = 0;
    reset = 1;
  }
 if (mode == 'a')
     for (t = 0; t < LAST_CLIENTS; t++) {
          if (!client_list[t]) { new = t; break; }
          if (time_list[t] > longest_time) {
              longest_time = time_list[t];
              new = t;
	   }
       }
 for (t = 0; t < LAST_CLIENTS; t++)
     if (client_list[t] && 
         (clock - time_list[t] > GET_CHANNEL_TIME 
         || client_list[t] == sptr || t == new)) {
         check_messages(sptr, client_list[t], client_list[t]->name, 'v');
         client_list[t] = 0;
     }
 if (new != -1) {
    client_list[new] = sptr; 
    time_list[new] = clock;
  }
}

void check_messages(cptr, sptr, destination, mode)
aClient *cptr, *sptr;
char *destination, mode;
{
 aMsgClient *msgclient, **index_p;
 char dibuf[40], *newnick, *message, *sender, *tonick = destination;
 int last, first_tnl = 0, last_tnl = 0, first_tnil, last_tnil, nick_list = 1,
     number_matched = 0, send, t, repeat, gnew;
 long clock,flags;
 aClient *from_ptr = sptr;

 if (!sptr->user || !*tonick) return;
 if (mode != 'v' && mode != 'e' && mode != 'g' 
     && SecretChannel(sptr->user->channel)
     && StrEq(tonick,"IRCnote")) {
	 sender = sptr->user->server[0] ? me.name : tonick;
         sendto_one(sptr,":%s NOTICE %s : <%s> %s (Q/%d) %s",
                    sender, tonick, me.name, VERSION, number_fromname(),
                    StrEq(ptr,"IRCnote") ? "" : (!*ptr ? "-" : ptr));
      }
 if (!fromname_index) return;
 time(&clock);
 if (clock > old_clock+note_msf) {
    save_messages();old_clock = clock;
  }
 if (mode == 'v') {
     nick_list = 0; /* Rest done with A flag */
  }
 if (mode != 'v' && mode != 'g') check_lastclient(sptr, mode, clock);
 if (mode == 'n') {
     newnick = tonick;tonick = sptr->name;
   }
 if (mode != 'g') {
     first_tnl = first_tnl_indexnode(sptr->user->username);
     last_tnl = last_tnl_indexnode(sptr->user->username);
     first_tnil = first_tnl_indexnode(tonick);
     last_tnil = last_tnl_indexnode(tonick);
  }
 if (mode == 's' || mode == 'g') {
     t = first_fnl_indexnode(sptr->user->username);
     last = last_fnl_indexnode(sptr->user->username);
     index_p = FromNameList;
     sptr = client; /* Notice new sptr */
     while (sptr && (!sptr->user || !*sptr->name)) sptr = sptr->next;
     if (!sptr) return;
     tonick = sptr->name;
  } else {
           if (nick_list) {
             t = first_tnil; last = last_tnil;
	    } else {
                     t = first_tnl; last = last_tnl;
		}
           index_p = ToNameList;
      }
 while(1) {
    while (last && t <= last) {
           msgclient = index_p[t];
           if (msgclient->timeout < clock) {
              t++; continue;
	    }
           gnew = 0; repeat=1;
           if (!(msgclient->flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)
               && !(msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
               && (index_p != ToNameList 
                   || (!nick_list && msgclient->flags & FLAGS_NAME)
                   || (nick_list && !(msgclient->flags & FLAGS_NAME)))
               && !matches(msgclient->tonick, tonick)
               && !matches(msgclient->toname, sptr->user->username)
               && !matches(msgclient->tohost, sptr->user->host)
               && (mode != 'v' || wildcards(msgclient->tonick)) 
               && (mode != 's' && mode != 'g'
                   || (msgclient->flags & FLAGS_ALL_NICK_VALID
                       || !mycmp(msgclient->fromnick, destination))
                       && host_check(msgclient->fromhost, 
                                     from_ptr->user->host, 'l'))) {
               message = check_flags(cptr, sptr, from_ptr, tonick, newnick,
                                     destination, msgclient, first_tnl, 
                                     last_tnl, &send, &repeat, &gnew, mode);
               if (send) {
                   flags = msgclient->flags;
                   flags &= ~FLAGS_SEND_ONLY_IF_NOT_EXCEPTION;
                   display_flags(flags,dibuf);
                   sender = sptr->user->server[0] ? me.name : tonick;
                   if (flags & FLAGS_SERVER_GENERATED_NOTICE)
                       sendto_one(sptr,":%s NOTICE %s :[%s] %s",
                                  sender, tonick, myctime(msgclient->time),
                                  msgclient->message);
                    else sendto_one(sptr,":%s NOTICE %s :*%s %s %s@%s %d* %s",
                                    sender,tonick,dibuf,msgclient->fromnick,
                                    msgclient->fromname,msgclient->fromhost,
                                    msgclient->time,message);
       	        }
               if (!(msgclient->flags & FLAGS_NAME)) number_matched++;
	     }
           if (!repeat) msgclient->timeout = 0;
           if (gnew) {
               if (mode != 'g') {
                  first_tnl = first_tnl_indexnode(sptr->user->username);
                  last_tnl = last_tnl_indexnode(sptr->user->username);
                  first_tnil = first_tnl_indexnode(tonick);
                  last_tnil = last_tnl_indexnode(tonick);
		}
               if (mode == 's' || mode == 'g') {
                  t = msgclient->InFromName;
                  last = last_fnl_indexnode(from_ptr->user->username);
		} else {
                         if (index_p == ToNameList) {
                             t = msgclient->InToName;
                             if (nick_list) last = last_tnil; 
                              else last = last_tnl;
      		          }
		    }
            } 
           if (repeat && (mode == 's' || mode =='g')) {
               sptr = sptr->next;
               while (sptr && (!sptr->user || !*sptr->name)) sptr = sptr->next;
                if (!sptr || number_matched) {
                    number_matched = 0; sptr = client; 
                    while (sptr && (!sptr->user || !*sptr->name))
                           sptr = sptr->next;
                    if (!sptr) return;
                    tonick = sptr->name; t++; continue;
		 }
               tonick = sptr->name;
            } else t++;
	 }
     if (mode == 's' || mode == 'g') return;
     if (index_p == ToNameList) {
         if (nick_list) {
             if (mode == 'a') break; /* Do rest with V flag */
             nick_list = 0; t = first_tnl; last = last_tnl;
	  } else {
                  index_p = WildCardList;
                  t = 1; last = wildcard_index;
	     }
      } else {
               if (mode == 'n') {
                   mode = 'c'; tonick = newnick;
                   first_tnil = first_tnl_indexnode(tonick);
                   last_tnil = last_tnl_indexnode(tonick);
                   t = first_tnil; last = last_tnil;
                   nick_list = 1; index_p = ToNameList;
	        } else break;
	 }
  }
  if (mode == 'c' || mode == 'a') 
     check_messages(sptr, sptr, tonick, 'g');
}

static void msg_remove(sptr, passwd, flag_s, id_s, name, time_s)
aClient *sptr;
char *passwd, *flag_s, *id_s, *name, *time_s;
{
 aMsgClient *msgclient;
 int removed = 0, t, last, id;
 long clock, flags = 0, time_l;
 char dibuf[40], tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3];

 if (!set_flags(sptr,flag_s, &flags,'d',"")) return;
 if (!time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time (&clock);
 while (last && t <= last) {
       msgclient = FromNameList[t];flags = msgclient->flags;
        if (clock>msgclient->timeout) {
            remove_msg(msgclient);last--;
            continue;
         }
        set_flags(sptr, flag_s, &flags, 'd',"");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,id)) {
            display_flags(msgclient->flags,dibuf),
            sendto_one(sptr,"NOTICE %s :### Removed -> %s %s (%s@%s)",
                       sptr->name,dibuf,msgclient->tonick,
                       msgclient->toname,msgclient->tohost);
            remove_msg(msgclient);last--;removed++;

        } else t++;
   }
 if (!removed) 
  sendto_one(sptr,"NOTICE %s :#?# No such message(s) found", sptr->name);
}

static void msg_save(sptr)
aClient *sptr;
{
 if (!changes_to_save) {
     sendto_one(sptr,"NOTICE %s :### No changes to save",sptr->name);
     return;
 }
 if (IsOperHere(sptr)) {
    save_messages();
    sendto_one(sptr,"NOTICE %s :### Messages are now saved",sptr->name);
  } else
      sendto_one(sptr,"NOTICE %s :### Oops! All messages lost...",sptr->name);
}

static void setvar(sptr,msg,l,value)
aClient *sptr;
int *msg,l;
char *value;
{
 int max;
 static char *message[] = {
             "Max server messages:",
             "I don't think that this is a good idea...",
             "Max server messages are set to:",
             "Max server messages with wildcards:",
             "Too many wildcards makes life too hard...",
             "Max server messages with wildcards are set to:",
             "Max user messages:",
             "Too cheeky fingers on keyboard error...",
             "Max user messages are set to:",
             "Max user messages with wildcards:",
             "Give me $$$, and I may fix your problem...",
             "Max user messages with wildcards are set to:",
             "Max server hours:",
             "Can't remember that long time...",
             "Max server hours are set to:",
             "Note save frequency:",
             "Save frequency may not be like that...",
             "Note save frequency is set to:" 
          };

 if (value) {
    max = atoi(value);
    if (!IsOperHere(sptr))
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,message[l+1]);
     else { 
           if (!max && (msg == &note_mst || msg == &note_msf)) max = 1;
           *msg = max;if (msg == &note_msf) *msg *= 60;
           sendto_one(sptr,"NOTICE %s :### %s %d",sptr->name,message[l+2],max);
           changes_to_save = 1;
       }
 } else {
         max = (*msg);if (msg == &note_msf) max /= 60;
         sendto_one(sptr,"NOTICE %s :### %s %d",sptr->name,message[l],max);
     }
}

static void msg_stats(sptr, arg, value, fromnick, fromname)
aClient *sptr;
char *arg, *value, *fromnick, *fromname;
{
 long clock;
 char buf[BUF_LEN],*fromhost = NULL;
 int tonick_wildcards = 0,toname_wildcards = 0,tohost_wildcards = 0,any = 0,
     nicks = 0,names = 0,hosts = 0,t = 1,last = fromname_index,flag_notice = 0,
     flag_destination = 0;
 aMsgClient *msgclient;

 if (arg) {
     if (!mycmp(arg,"MSM")) setvar(sptr,&note_msm,0,value); else 
     if (!mycmp(arg,"MSW")) setvar(sptr,&note_msw,3,value); else
     if (!mycmp(arg,"MUM")) setvar(sptr,&note_mum,6,value); else
     if (!mycmp(arg,"MUW")) setvar(sptr,&note_muw,9,value); else
     if (!mycmp(arg,"MST")) setvar(sptr,&note_mst,12,value); else
     if (!mycmp(arg,"MSF")) setvar(sptr,&note_msf,15,value); else
     if (MyEq(arg,"USED")) {
         time(&clock);
         while (last && t <= last) {
                msgclient = FromNameList[t];
                if (clock > msgclient->timeout) {
                    remove_msg(msgclient);last--;
                    continue;
                 }
                any++;
                if (msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
                    flag_destination++; else
                if (msgclient->flags & FLAGS_SERVER_GENERATED_NOTICE)
                    flag_notice++; else
                if (IsOperHere(sptr) || Key(sptr))
                    if (!fromhost || 
                        !host_check(msgclient->fromhost,fromhost,'l')) {
                        nicks++;names++;hosts++;
                        fromhost = msgclient->fromhost;
                        fromname = msgclient->fromname;
                        fromnick = msgclient->fromnick;
                     } else if (Usermycmp(msgclient->fromname,fromname)) {
                                nicks++;names++;
                                fromname = msgclient->fromname;
                                fromnick = msgclient->fromnick;
            	             } else if (mycmp(msgclient->fromnick,fromnick)) {
                                        nicks++;
                                        fromnick = msgclient->fromnick;
	  	                     } 
                if (wildcards(msgclient->tonick))
                    tonick_wildcards++;
                if (wildcards(msgclient->toname))
                    toname_wildcards++;
                if (wildcards(msgclient->tohost))
                    tohost_wildcards++;
                t++;
	    }
	    if (!any) 
             sendto_one(sptr,"NOTICE %s :#?# No message(s) found",sptr->name);
                else {
                     if (IsOperHere(sptr) || Key(sptr)) {
                         sprintf(buf,"%s%d /%s%d /%s%d /%s (%s%d /%s%d /%s%d)",
                                 "Nicks:",nicks,"Names:",names,
                                 "Hosts:",hosts,"W.cards",
                                 "Tonicks:",tonick_wildcards,
                                 "Tonames:",toname_wildcards,
                                 "Tohosts:",tohost_wildcards);
                         sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
		      }
                      sprintf(buf,"%s %s%d / %s%d",
                              "Server generated",
                              "G-notice: ",flag_notice,
                              "H-header: ",flag_destination);
                      sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
		 }
     } else if (!mycmp(arg,"RESET")) {
                if (!IsOperHere(sptr)) 
                    sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                               "Wrong button - try another next time...");
                 else {
                       note_mst = NOTE_MAXSERVER_TIME,
                       note_mum = NOTE_MAXUSER_MESSAGES,
                       note_msm = NOTE_MAXSERVER_MESSAGES,
                       note_msw = NOTE_MAXSERVER_WILDCARDS,
                       note_muw = NOTE_MAXUSER_WILDCARDS;
                       note_msf = NOTE_SAVE_FREQUENCY*60;
                       sendto_one(sptr,"NOTICE %s :### %s",
                                  sptr->name,"Stats have been reset");
                       changes_to_save = 1;
		   }
	    }
  } else {
      t = number_fromname();
      sprintf(buf,"%s%d /%s%d /%s%d /%s%d /%s%d /%s%d /%s%d",
              "QUEUE:", t,
              "MSM(dynamic):", t < note_msm ? note_msm : t,
              "MSW:",note_msw,
              "MUM:",note_mum,
              "MUW:",note_muw,
              "MST:",note_mst,
              "MSF:",note_msf/60);
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
    }
}

static void msg_send(sptr, silent, passwd, flag_s, timeout_s, name, message)
aClient *sptr;
int silent;
char *passwd, *flag_s, *timeout_s, *name, *message;
{
 aMsgClient **index_p, *msgclient;
 int sent_wild = 0, wildcard = 0, sent = 0, nick_list = 1,
     first_tnl, last_tnl, t, first, last, savetime, join = 0;
 long clock, timeout, flags = 0;
 char buf1[BUF_LEN], buf2[BUF_LEN], dibuf[40], *c, *null_char = "",
      tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3];

 time (&clock);
 t = first_tnl_indexnode(sptr->name);
 last = last_tnl_indexnode(sptr->name);
 first_tnl = first_tnl_indexnode(sptr->user->username);
 last_tnl = last_tnl_indexnode(sptr->user->username);
 index_p = ToNameList;
 time (&clock);
 while (1) {
     while (last && t <= last) {
  	    msgclient = index_p[t];
	    if (clock > msgclient->timeout) {
  	        remove_msg(msgclient);last--;
 	        continue;
	     }
            if (msgclient->flags & FLAGS_NOT_QUEUE_REQUESTS
		&& !(msgclient->flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)
		&& !matches(msgclient->tonick,sptr->name)
		&& !matches(msgclient->toname,sptr->user->username)
		&& !matches(msgclient->tohost,sptr->user->host)) {
		sendto_one(sptr,"NOTICE %s :### %s (%s@%s) %s %s",sptr->name,
			   msgclient->fromnick,msgclient->fromname,
			   msgclient->fromhost,
                           "doesn't allow you to queue messages:",
                           msgclient->message);
		return;
	    }
            t++;
	}
     if (index_p == ToNameList) {
         if (nick_list) {
             nick_list = 0; t = first_tnl; last = last_tnl;
          } else {
                   index_p = WildCardList;
                   t = 1; last = wildcard_index;
              }

      } else break;
 }
 if (number_fromname() >= note_msm 
     && !IsOperHere(sptr) && !Key(sptr)) {
     if (!note_msm || !note_mum)
         sendto_one(sptr,
                    "NOTICE %s :#?# The notesystem is closed for no-operators",
                    sptr->name);
      else sendto_one(sptr,"NOTICE %s :#?# No more than %d message%s %s",
                      sptr->name, note_msm, note_msm < 2?"":"s",
                      "allowed in the server");
     return;
  }
 if (clock > old_clock+note_msf) {
    save_messages();old_clock = clock;
  }
 if (!set_flags(sptr,flag_s,&flags,'s',"")) return;
 split(name, tonick, toname, tohost);
 if (IsOper(sptr)) flags |= FLAGS_OPER;
 if (*timeout_s == '+') timeout = atoi(timeout_s + 1) * 24;
  else if (*timeout_s == '-') timeout = atoi(timeout_s + 1);
 if (timeout > note_mst && !(flags & FLAGS_OPER) && !Key(sptr)) {
    sendto_one(sptr,"NOTICE %s :#?# Max time allowed is %d hour%s",
               sptr->name,note_mst,note_mst>1?"":"s");
    return;
  }
 if (!message) {
    if (!send_flag(flags)) message = null_char; 
     else {
           sendto_one(sptr,"NOTICE %s :#?# No message specified",sptr->name);
           return;
       }
  }
 if (strlen(message) > MSG_LEN) {
    sendto_one(sptr,"NOTICE %s :#?# Message can't be longer than %d chars",
               sptr->name,MSG_LEN);
    return;
 }
 first = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 t = first;
 while (last && t <= last) {
        msgclient = FromNameList[t];
        if (clock > msgclient->timeout) {
            remove_msg(msgclient); last--;
            continue;
         }
        if (!mycmp(sptr->name, msgclient->fromnick)
            && !Usermycmp(sptr->user->username, msgclient->fromname)
            && host_check(sptr->user->host, msgclient->fromhost,'l')
            && StrEq(msgclient->tonick, tonick)
            && StrEq(msgclient->toname, toname)
            && StrEq(msgclient->tohost, tohost)
            && StrEq(msgclient->passwd, passwd)
            && StrEq(msgclient->message, message)
            && msgclient->flags == msgclient->flags | flags) {
            msgclient->timeout = timeout*3600+clock;
            join = 1; 
	  }
        t++;
    }
  if (wildcards(tonick) || wildcards(toname)) wildcard = 1;
  if (!join && !(flags & FLAGS_OPER) && !Key(sptr)) {
     time(&clock); t = first;
     while (last && t <= last) {
         if (!Usermycmp(sptr->user->username,FromNameList[t]->fromname)) {
             if (host_check(sptr->user->host,FromNameList[t]->fromhost,'l')) {
                 sent++;
                 if (wildcards(FromNameList[t]->tonick)
                     && wildcards(FromNameList[t]->toname))
                    sent_wild++;  
              }
                 
          }
         t++;
       }
     if (sent >= note_mum) {
        sendto_one(sptr,"NOTICE %s :#?# No more than %d message%s %s",
                   sptr->name,note_mum,note_mum < 2?"":"s",
                   "for each user allowed in the server");
        return;
      }
     while (wildcard) {
         if (!note_msw || !note_muw)
            sendto_one(sptr,"NOTICE %s :#?# No-operators are not allowed %s",
                       sptr->name,
                       "to send to nick and username with wildcards");
          else if (wildcard_index >= note_msw) 
                   sendto_one(sptr,"NOTICE %s :#?# No more than %d msg. %s",
                              sptr->name, note_msw,
                              "with nick and username w.cards allowed.");
          else if (sent_wild >= note_muw) 
                   sendto_one(sptr,"NOTICE %s :#?# No more than %d %s %s",
                              sptr->name, note_muw, 
                              note_muw < 2?"sessage":"messages with",
                              "nick and username w.cards allowed.");
          else break;
          return;
     }
   }
 if (!join) {
     time(&clock);
     new(passwd,sptr->name,sptr->user->username,sptr->user->host,
         tonick,toname,tohost,flags,timeout*3600+clock,clock,message);
  }
 if (join && silent) return;
 display_flags(flags,dibuf);
 sprintf(buf1,"%s %s (%s@%s) / %s%d / %s%s",
         join ? "Joined ->" : "Queued ->",tonick,toname,tohost,
         "Max.hours:",timeout,"Flags:",dibuf);
 *buf2 = 0; if (!(flags & FLAGS_NOT_SAVE)) {
              savetime = (note_msf-clock+old_clock)/60;
              sprintf(buf2,"%s%d","Save.min:",savetime);        
           }   
 sendto_one(sptr,"NOTICE %s :### %s %s",sptr->name,buf1,buf2);
 if (!join) check_messages(sptr, sptr, sptr->name, 's');
}

static void msg_list(sptr, arg, passwd, flag_s, id_s, name, time_s)
aClient *sptr;
char *arg, *passwd, *flag_s, *id_s, *name, *time_s;
{
 aMsgClient *msgclient;
 int number = 0, t, last, ls = 0, count = 0, log = 0, id;
 long clock, flags = 0, time_l;
 char tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3],
      *message, buf[BUF_LEN], dibuf[40];

 if (MyEq(arg,"ls")) ls = 1; else
 if (MyEq(arg,"count")) count = 1; else
 if (MyEq(arg,"log")) log = 1; else
 if (!MyEq(arg,"cat")) {
     sendto_one(sptr,"NOTICE %s :#?# No such option: %s",sptr->name,arg); 
     return;
  }
 if (!set_flags(sptr,flag_s, &flags,'d',"")) return;
 if (!time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time(&clock); 
 while (last && t <= last) {
        msgclient = FromNameList[t];
        flags = msgclient->flags;
        if (clock > msgclient->timeout) {
            remove_msg(msgclient);last--;
            continue;
          }
        set_flags(sptr,flag_s,&flags,'d',"");
        if ((!log || msgclient->flags & FLAGS_LOG) 
            && local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,id)) {
            if (!ls) message = msgclient->message;
             else message = NULL;
            display_flags(msgclient->flags,dibuf);
            if (log) sendto_one(sptr,"NOTICE %s :### %s: %s (%s@%s)",
                                sptr->name, myctime(msgclient->time),
                                msgclient->tonick, msgclient->toname,
                                msgclient->tohost);
             else 
              if (!count) {
                 sprintf(buf,"until %s", myctime(msgclient->timeout));
                 sendto_one(sptr,"NOTICE %s :#%d %s %s (%s@%s) %s: %s",
                            sptr->name,msgclient->id,dibuf,msgclient->tonick,
                            msgclient->toname,msgclient->tohost,buf,
                            message?message:(*msgclient->message ? "...":""));
             }
            number++;
	 };
        t++;
     }
 if (!number) sendto_one(sptr,"NOTICE %s :#?# No such %s", sptr->name,
                         log ? "log(s)" : "message(s) found");
  else if (count) sendto_one(sptr,"NOTICE %s :### %s %s (%s@%s): %d",
                             sptr->name,"Number of messages to",
                             tonick ? tonick : "*", toname ? toname:"*",
                             tohost ? tohost : "*", number);
}

static void msg_flag(sptr, passwd, flag_s, id_s, name, newflag_s)
aClient *sptr;
char *passwd, *flag_s, *id_s, *name, *newflag_s;
{
 aMsgClient *msgclient;
 int flagged = 0, t, last, id;
 long clock, flags = 0;
 char tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3],
      dibuf1[40], dibuf2[40];

 if (!newflag_s) {
     sendto_one(sptr,"NOTICE %s :#?# No flag changes specified",sptr->name);
     return;
  }
 if (!set_flags(sptr, flag_s, &flags,'d',"in match flag")) return;
 if (!set_flags(sptr, newflag_s, &flags,'c',"in flag changes")) return;
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time(&clock);
 while (last && t <= last) {
       msgclient = FromNameList[t];flags = msgclient->flags;
        if (clock > msgclient->timeout) {
            remove_msg(msgclient);last--;
            continue;
         }
        set_flags(sptr,flag_s,&flags,'d',"");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,0,id)) {
            flags = msgclient->flags;display_flags(flags,dibuf1);
            set_flags(sptr,newflag_s,&msgclient->flags,'s',"");
            display_flags(msgclient->flags,dibuf2);
            if (flags == msgclient->flags) 
                sendto_one(sptr,"NOTICE %s :### %s -> %s %s (%s@%s)",sptr->name,
                           "No flag change for",dibuf1,msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
             else                                        
                sendto_one(sptr,"NOTICE %s :### %s -> %s %s (%s@%s) to %s",
                          sptr->name,"Flag change",dibuf1,msgclient->tonick,
                          msgclient->toname,msgclient->tohost,dibuf2);
           flagged++;
        } 
       t++;
   }
 if (!flagged) 
  sendto_one(sptr,"NOTICE %s :#?# No such message(s) found",sptr->name);
}

static void msg_sent(sptr, arg, name, time_s, delete)
aClient *sptr;
char *arg, *name, *time_s, *delete;
{
 aMsgClient *msgclient,*next_msgclient;
 char fromnick[BUF_LEN+3], fromname[BUF_LEN+3], fromhost[BUF_LEN+3]; 
 int number = 0, t, t1, last, nick = 0, count = 0, users = 0;
 long clock,time_l;

 if (!arg) nick = 1; else
  if (MyEq(arg,"COUNT")) count = 1; else
   if (MyEq(arg,"USERS")) users = 1; else
   if (!MyEq(arg,"NAME")) {
       sendto_one(sptr,"NOTICE %s :#?# No such option: %s",sptr->name,arg); 
       return;
  }
 if (users) {
    if (!IsOperHere(sptr)) {
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                   "A dragon is guarding the names...");
        return;
     }
    if (!time_s) time_l = 0; 
     else { 
            time_l = set_date(sptr,time_s);
            if (time_l < 0) return;
       }
    split(name, fromnick, fromname, fromhost); 
    time(&clock);
    for (t = 1;t <= fromname_index;t++) {
         msgclient = FromNameList[t]; t1 = t;
         do next_msgclient = t1 < fromname_index ? FromNameList[++t1] : NULL;
          while (next_msgclient && next_msgclient->timeout <= clock);
         if (msgclient->timeout > clock
             && (!time_l || msgclient->time >= time_l 
                 && msgclient->time < time_l+24*3600)
             && (!matches(fromnick,msgclient->fromnick))
             && (!matches(fromname,msgclient->fromname))
             && (!matches(fromhost,msgclient->fromhost))
             && (!delete || !mycmp(delete,"RM")
                 || !mycmp(delete,"RMBF") &&
                    (msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION ||
                     msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE))) {
             if (delete || !next_msgclient
                 || mycmp(next_msgclient->fromnick,msgclient->fromnick)
                 || mycmp(next_msgclient->fromname,msgclient->fromname)     
                 || !(host_check(next_msgclient->fromhost,
                                 msgclient->fromhost,'l'))) {
                  sendto_one(sptr,"NOTICE %s :%s#%d# %s (%s@%s) @%s",
                             sptr->name,
                             delete ? "### Removing -> " : "", 
                             delete ? 1 : count+1,
                             msgclient->fromnick,msgclient->fromname,
                             local_host(msgclient->fromhost),
                             msgclient->fromhost);
                    if (delete) msgclient->timeout = 0;
                 count = 0;number = 1;
	     } else count++;
	 }
     }
    if (!number) 
     sendto_one(sptr, "NOTICE %s :#?# No message(s) from such user(s) found",
                sptr->name);
  return;
 }
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time (&clock);
 while (last && t <= last) {
        msgclient = FromNameList[t];
        if (clock>msgclient->timeout) {
             remove_msg(msgclient);last--;
             continue;
         }
        if (!Usermycmp(sptr->user->username,msgclient->fromname)
            && (!nick || !mycmp(sptr->name,msgclient->fromnick))) {
            if (!IsOper(sptr) && msgclient->flags & FLAGS_OPER) {
                t++; continue;
	     }
            if (host_check(sptr->user->host,msgclient->fromhost,'l')) { 
                if (!count) 
                    sendto_one(sptr,"NOTICE %s :-> Sent from host %s on %s",
                               sptr->name,msgclient->fromhost,
                               date(msgclient->time));
                number++;
             }
         }
       t++;
    }
 if (!number) 
  sendto_one(sptr,"NOTICE %s :#?# No such message(s) found",sptr->name);
  else if (count) sendto_one(sptr,"NOTICE %s :<> %s %d",sptr->name,
                             "Number of no-operator messages queued:",number);
}

static name_len_error(sptr, name)
aClient *sptr;
char *name;
{
 if (strlen(name) > BUF_LEN) {
    sendto_one(sptr,
               "NOTICE %s :#?# Nick!name@host can't be longer than %d chars",
               sptr->name,BUF_LEN);
    return 1;
  }
 return 0;
}

static flag_len_error(sptr, flag_s)
aClient *sptr;
char *flag_s;
{
 if (strlen(flag_s) > BUF_LEN) {
    sendto_one(sptr,"NOTICE %s :#?# Flag string can't be longer than %d chars",
               sptr->name,BUF_LEN);
    return 1;
  }
 return 0;
}

static void do_service(cptr, sptr, parc, parv, option)
aClient *cptr, *sptr;
int parc;
char *parv[], *option;
{
 char *c, nick[BUF_LEN], command[BUF_LEN + 3];
 
 if (!IsOperHere(sptr)) {
     sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                "Beyond your power poor soul...");
     return;
  }
 for (c = option; *c; c++);
 while (c != option && *c != '_') c--;
 strncpy(command, c+1, BUF_LEN);
 *c = 0; strcpy(nick, option + 8);
 parv[0] = nick;
 parv[1] = command;

 for (cptr = client; cptr; cptr = cptr->next)
      if (cptr->user && StrEq(nick, cptr->name)) break;
 if (cptr) m_note(cptr, cptr, parc, parv);
}

static alias_send(sptr, option, flags, msg, timeout)
aClient *sptr;
char *option, **flags, **msg, **timeout;
{
 static char flag_s[BUF_LEN+10], *deft = "+31",
        *waitfor_message = "<< WAITING FOR YOU ON IRC RIGHT NOW >>";
 
 if (MyEq(option,"SEND")) {
     sprintf(flag_s,"+D%s", *flags);
     if (!*timeout) *timeout = deft; 
  } else
 if (MyEq(option,"CHANNEL")) {
     sprintf(flag_s,"+M%s", *flags);
     if (!*timeout) *timeout = deft; 
  } else
 if (MyEq(option,"WAITFOR")) { 
     sprintf(flag_s,"+YD%s", *flags); 
     if (!*msg) *msg = waitfor_message; 
  } else 
 if (MyEq(option,"SPY")) sprintf(flag_s,"+RX%s", *flags); else
 if (MyEq(option,"FIND")) sprintf(flag_s,"+FR%s", *flags); else
 if (MyEq(option,"WALL")) sprintf(flag_s,"+BR%s", *flags); else
 if (MyEq(option,"WALLOPS")) sprintf(flag_s,"+BRW%s", *flags); else
 if (MyEq(option,"DENY")) sprintf(flag_s,"+RZ%s", *flags); else
  return 0;
 *flags = flag_s;
 return 1;
}

m_note(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
 char *option, *arg[10], *args, *timeout = NULL, *passwd = NULL,  
      *id = NULL, *flags = NULL, *name = NULL, *msg = NULL,
      *wildcard = "*", *c, *default_timeout = "+1";
 int t, t2, slen = 10, silent = 0;

 option = parc > 1 ? parv[1] : NULL;
 for (t = 0; t < 10; t++) arg[t] = NULL;
 for (t = 2; t <= 6; t++)
      if (parc > t) {
         arg[t-1] = parv[t]; slen += strlen(parv[t]);
       } else arg[t-1] = NULL; 

 if (!IsRegistered(sptr)) { 
	sendto_one(sptr, ":%s %d * :You have not registered as an user", 
		   me.name, ERR_NOTREGISTERED); 
	return -1;
   }
 if (!option) {
    sendto_one(sptr,"NOTICE %s :#?# No option specified.", sptr->name); 
    return -1;
  }

 msg = args = MyMalloc(slen+1);
 sprintf(args,"%s %s %s %s %s", 
         arg[1] ? arg[1] : "",
         arg[2] ? arg[2] : "",
         arg[3] ? arg[3] : "",
         arg[4] ? arg[4] : "",
         arg[5] ? arg[5] : "");

 for (t = 1; t <= 5 && arg[t]; t++) {
      switch (*arg[t]) {
              case '$' : passwd = arg[t]+1; break; 
              case '%' : passwd = arg[t]+1; silent = 1; break; 
              case '#' : id = arg[t]+1; break; 
              case '+' : if (numeric(arg[t]+1)) timeout = arg[t]; 
                          else flags = arg[t]; break; 
              case '-' : if (numeric(arg[t]+1)) timeout = arg[t]; 
                          else flags = arg[t]; break;  
              default : goto break2c;
        }
    } 
 break2c:;
 for (t2 = 1; t2 <= t; t2++) {
      while (*msg && *msg++ != ' ');
   }
 c = msg; while (*c) c++;
 while (c != msg && *--c == ' '); 
 if (c != msg) c++; *c = 0;
 if (!*msg) msg = NULL;
 if (!passwd || !*passwd) passwd = wildcard;
 if (!flags) flags = (char *) "";
 if (flags && flag_len_error(sptr, flags)) { free(args); return; }

 if (!strncmp(option,"SERVICE_",8)) 
     do_service(cptr, sptr, parc, parv, option); else
 if (MyEq(option,"STATS")) msg_stats(sptr, arg[t], arg[t+1]); else
 if (alias_send(sptr, option, &flags, &msg, &timeout) || MyEq(option,"USER")) {
     if (!arg[1]) {
        sendto_one(sptr,
                   "NOTICE %s :#?# Please specify at least one argument", 
                   sptr->name);
        free(args); return;
      }
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }
     if (!timeout) timeout = default_timeout;  
     msg_send(sptr, silent, passwd, flags, timeout, name, msg);
  } else
 if (MyEq(option,"LS") || MyEq(option,"CAT") 
     || MyEq(option,"COUNT") || MyEq(option,"LOG")) {
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }
     msg_list(sptr, option, passwd, flags, id, name, arg[t+1]);
   } else
 if (MyEq(option,"SAVE")) msg_save(sptr); else
 if (MyEq(option,"FLAG")) {
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }
     msg_flag(sptr, passwd, flags, id, name, arg[t+1]); 
  } else
 if (MyEq(option,"SENT")) {
    name = arg[t+1];if (!name) name = wildcard;
    if (name_len_error(sptr, name)) { free(args); return; }
    msg_sent(sptr, arg[t], name, arg[t+2], arg[t+3]); 
  } else
 if (MyEq(option,"RM")) {
     if (!arg[1]) {
        sendto_one(sptr,
                   "NOTICE %s :#?# Please specify at least one argument", 
                   sptr->name);
        free(args); return;
      }
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }  
     msg_remove(sptr, passwd, flags, id, name, arg[t+1]); 
  } else sendto_one(cptr,"NOTICE %s :#?# No such option: %s", 
                    sptr->name, option);
 free(args);
} 
#endif

