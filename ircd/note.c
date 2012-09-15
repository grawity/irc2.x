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
 *        Author: Jarle Lyngaas (University of Oslo, Norway)
 *        E-mail: jarlek@ifi.uio.no
 *           IRC: jarlek@*.uio.no
 */

#include "struct.h"
#ifdef MSG_NOTE
#include "numeric.h"
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#define VERSION "v1.3pre8"

#define NOTE_SAVE_FREQUENCY 60 /* Frequency of save time in minutes */
#define NOTE_MAXSERVER_TIME 24*60 /* Max hours for a message in the server */
#define NOTE_MAXSERVER_MESSAGES 2000 /* Max number of messages in the server */
#define NOTE_MAXUSER_MESSAGES 200 /* Max number of messages for each user */
#define NOTE_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
#define NOTE_MAXUSER_WILDCARDS 40 /* Max number of user toname wildcards */
#define NOTE_MAX_WHOWAS_LOG 50000 /* Max number of whowas logs */

#define BUF_LEN 256
#define MSG_LEN 512
#define REALLOC_SIZE 2048
#define MAX_WHOWAS_LISTING 500

#define FLAGS_OPER (1<<0)
#define FLAGS_LOCALOPER (1<<1)
#define FLAGS_NAME (1<<2)
#define FLAGS_LOG (1<<3)
#define FLAGS_NOT_SAVE (1<<4)
#define FLAGS_REPEAT_ONCE (1<<5)
#define FLAGS_REPEAT_UNTIL_TIMEOUT (1<<6)
#define FLAGS_ALL_NICK_VALID (1<<7)
#define FLAGS_SERVER_GENERATED_NOTICE (1<<8)
#define FLAGS_SERVER_GENERATED_WHOWAS (1<<9)
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
#define FLAGS_WHOWAS (1<<23)
#define FLAGS_WHOWAS_LOG_EACH_DAY (1<<24)
#define FLAGS_WHOWAS_SECRET (1<<16)
#define FLAGS_WHOWAS_CONNECTED_HERE (1<<17)
#define FLAGS_WHOWAS_LIST_ONLY_USERNAME (1<<25)
#define FLAGS_WHOWAS_COUNT (1<<7)
#define FLAGS_WHOWAS_FLAGS ((1<<7) | (1<<25))
#define FLAGS_SEND_ONLY_IF_SIGNONOFF (1<<25)
#define FLAGS_REGISTER_NEWNICK (1<<26)
#define FLAGS_NOTICE_RECEIVED_MESSAGE (1<<27)
#define FLAGS_RETURN_CORRECT_DESTINATION (1<<28)
#define FLAGS_NOT_USE_SEND (1<<29)
#define FLAGS_NEWNICK_DISPLAYED (1<<30)
#define FLAGS_NEWNICK_SECRET_DISPLAYED (1<<31)

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
           max_tonick,
           max_toname,
           max_wildcards,
           max_fromname,
           old_clock = 0,
           m_id = 0,
           changes_to_save = 0;

static aMsgClient **ToNameList, **FromNameList, **WildCardList;
extern aClient *client;
extern aChannel *find_channel();
extern char *MyMalloc(), *MyRealloc(), *myctime();
static char *ptr = "IRCnote", *note_save_filename_tmp;

static char *find_chars(c1,c2,string)
char c1,c2,*string;
{
 register char *c;

 for (c = string; *c; c++) if (*c == c1 || *c == c2) return(c);
 return 0;
}

static numeric(string)
char *string;
{
 register char *c = string;

 while (*c) if (!isdigit(*c)) return 0; else c++;
 return 1;
}

static only_wildcards(string)
char *string;
{
 register char *c;

 for (c = string;*c;c++) 
      if (*c != '*' && *c != '?') return 0;
 return 1;
}

static number_whowas()
{
 register int t, nr = 0;
 long clock;

 time (&clock);
 for (t = 1;t <= fromname_index; t++) 
      if (FromNameList[t]->timeout > clock &&
          FromNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS) nr++;
 return nr;         
}

static number_fromname()
{
 register int t, nr = 0;
 long clock;

 time (&clock);
 for (t = 1;t <= fromname_index; t++) 
      if (FromNameList[t]->timeout > clock &&
          !(FromNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS)) nr++;
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


static first_tnl_indexnode(toname)
char *toname;
{
 int s,t = toname_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNameList[s]->toname,toname) < 0) b = s; else t = s;
 return t;
}

static last_tnl_indexnode(toname)
char *toname;
{
 int s,t = toname_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNameList[s]->toname,toname) > 0) t = s; else b = s;
 return b;
}

static first_fnl_indexnode(fromname)
char *fromname;
{
 int s,t = fromname_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(FromNameList[s]->fromname,fromname)<0) b = s; else t = s;
 return t;
}

static last_fnl_indexnode(fromname)
char *fromname;
{
 int s,t = fromname_index+1,b = 0;

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
            
 if (number_whowas() > NOTE_MAX_WHOWAS_LOG 
     && flags & FLAGS_SERVER_GENERATED_WHOWAS
     || number_fromname() > note_msm 
        && !(flags & FLAGS_SERVER_GENERATED_WHOWAS)) return;
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
 if (flags & FLAGS_NAME) {
    n = last_tnl_indexnode(toname)+1;
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
 if (!(msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)) m_id++;
 msgclient->id = m_id;
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

 max_fromname = max_tonick = max_toname = max_wildcards = REALLOC_SIZE;
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
     sendto_one(sptr,"ERROR :Unknown date");
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
      sendto_one(sptr,"ERROR :Unknown month");
      return -1; 
  }       
 year = atoi(arg[2]);
 if (*arg[2] && (year<71 || year>99)) {
     sendto_one(sptr,"ERROR :Unknown year");
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
  if (flags & FLAGS_WHOWAS
      || flags & FLAGS_RETURN_CORRECT_DESTINATION
      || flags & FLAGS_DISPLAY_IF_CORRECT_FOUND
      || flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL
      || flags & FLAGS_DISPLAY_IF_DEST_REGISTER
      || flags & FLAGS_SERVER_GENERATED_DESTINATION
      || flags & FLAGS_SERVER_GENERATED_WHOWAS
      || flags & FLAGS_NOT_USE_SEND
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
 if (flags & FLAGS_WHOWAS) c[t++] = 'A';
 if (flags & FLAGS_REGISTER_NEWNICK) c[t++] = 'T';
 if (flags & FLAGS_WHOWAS_LOG_EACH_DAY) c[t++] = 'J';
 if (flags & FLAGS_SEND_ONLY_IF_SIGNONOFF) c[t++] = 'U';
 if (flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER) c[t++] = 'W';
 if (flags & FLAGS_NOT_USE_SEND) c[t++] = 'Z';
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
 if (msgclient->flags & FLAGS_NAME) {
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
 int t,last;
 long clock;
 aMsgClient **index_p,*msgclient; 
 
 t = first_tnl_indexnode(sptr->user->username);
 last = last_tnl_indexnode(sptr->user->username);
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
         index_p = WildCardList;
         t = 1; last = wildcard_index;
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
              case 'A': if (KeyFlags(sptr,FLAGS_WHOWAS) 
                            || op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_WHOWAS;
                             else *flags &= ~FLAGS_WHOWAS;
                         } else buf[uf++] = cu;
                        break;
              case 'J': if (KeyFlags(sptr,FLAGS_WHOWAS_LOG_EACH_DAY) 
                            || op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_WHOWAS_LOG_EACH_DAY;
                             else *flags &= ~FLAGS_WHOWAS_LOG_EACH_DAY;
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
              case 'Z': if (KeyFlags(sptr,FLAGS_NOT_USE_SEND)
                            || op || mode == 'd' || !on) {
                             if (on) *flags |= FLAGS_NOT_USE_SEND;
			      else *flags &= ~FLAGS_NOT_USE_SEND;
                         } else buf[uf++] = cu;
                         break;  
              default:  buf[uf++] = cu;
        } 
  }
 buf[uf] = 0;
 if (uf) {
     sendto_one(sptr,"ERROR :Unknown flag%s: %s %s",
                uf> 1  ? "s" : "",buf,type);
     return 0;
  }
 if (mode == 's') {
    if (*flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)
        sendto_one(sptr,"NOTICE %s :*** %s", sptr->name,
                   "WARNING: Recipient got keys to unlock the secret portal;");
     else if (*flags & FLAGS_NOT_USE_SEND)
              sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                         "WARNING: Recipient can't use SEND;");
      else if (*flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE
               && *flags & FLAGS_REPEAT_UNTIL_TIMEOUT
               && send_flag(*flags)) 
               sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
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
 if (!t1) 
     for (t = 1;t <= fromname_index; t++) 
          if (!(FromNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS))
          FromNameList[t]->id = ++t1;          
 if (changes_to_save) changes_to_save = 0; else return;
 time(&clock);
 fp = fopen(note_save_filename_tmp,"w");
 if (!fp) return;
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

static char *check_flags(sptr,from_ptr,nick,newnick,msgclient,first_tnl,
                         last_tnl, send,repeat,gnew,mode,reserved)
aClient *sptr, *from_ptr;
aMsgClient *msgclient;
char *nick,*newnick;
int first_tnl, last_tnl, *send, *repeat, *gnew, mode, *reserved;
{
 char *prev_nick,*new_nick,*c,mbuf[MSG_LEN+3],buf[BUF_LEN+3],
      ebuf[BUF_LEN+3],abuf[20],*sender,*message,wmode[2];
 long prev_clock,new_clock,clock,time_l,gflags,timeout = 0;
 int t,t1,first,last, exception, secret = 0, ss_hidden = 0, 
     ss_secret = 0, hidden = 0, local_server = 0;
 aChannel *chptr;
 aMsgClient **index_p;
 Link *link;

 if (mode != 'g')
    for (from_ptr = client; from_ptr; from_ptr = from_ptr->next)
         if (from_ptr->user && 
             (!mycmp(from_ptr->name,msgclient->fromnick)
              || msgclient->flags & FLAGS_ALL_NICK_VALID)
             && !Usermycmp(from_ptr->user->username,msgclient->fromname)
             && host_check(from_ptr->user->host,msgclient->fromhost,'l')) 
             break; 
 if (SecretChannel(sptr->user->channel)) secret = 1;
 if (secret) hidden = 1; 
 if (!from_ptr || !IsOper(from_ptr) || !MyConnect(sptr)) {
     ss_secret = secret; ss_hidden = hidden;
  }
 chptr = find_channel("+NOTE", (char *) 0);
 wmode[1] = 0; *send = 1; *repeat = 0; *gnew = 0; 
 message = msgclient->message;
 if (host_check(sptr->user->server, sptr->user->host,'e')) local_server = 1;
 if (!local_server && msgclient->flags & FLAGS_SEND_ONLY_IF_LOCALSERVER
     || !MyConnect(sptr) && msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER
     || (msgclient->flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME 
         && mode == 'a' && StrEq(nick,sptr->user->username))
     || (!IsOper(sptr) &&
         msgclient->flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER)
     || msgclient->flags & FLAGS_SEND_ONLY_IF_NOTE_CHANNEL &&
        (!sptr->user->channel || !chptr || !IsMember(sptr,chptr))
     || msgclient->flags & FLAGS_SEND_ONLY_IF_SIGNONOFF && mode != 'a'
        && mode != 'e' && !(msgclient->flags & FLAGS_WHOWAS)) { 
     *send = 0;*repeat = 1;return message;
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
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER
     && from_ptr && from_ptr != sptr) {
     time(&clock);
     if (!ss_secret) {
         msgclient->time = clock;
         msgclient->flags |= FLAGS_LOG;
       }
     for (chptr = from_ptr->user->channel; chptr; chptr = chptr->nextch)
          for (link = chptr->members; link; link = link->next)
              if ((aClient *)link->value == sptr) goto break2b;
              sprintf(buf,"%s@%s",sptr->user->username,sptr->user->host);
              switch (mode) {
		case 'r' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                               && !(only_wildcards(msgclient->tonick)
                                    && only_wildcards(msgclient->toname))
                               && !ss_secret) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :*** %s (%s) %s: %s",
                                           from_ptr->name,
                                           ss_hidden ? msgclient->tonick : nick,
                                           buf,"signs on",
			        	   message);
			        msgclient->flags &= 
                                           ~FLAGS_NEWNICK_SECRET_DISPLAYED;
				msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
		              }
                           break;
		case 'a' : sendto_one(from_ptr,"NOTICE %s :*** %s (%s) %s: %s",
                                      from_ptr->name,
                                      ss_hidden ? msgclient->tonick : nick,
                                      buf, "signs on",message);
                           msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                           break;
		case 'c' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                               && !(only_wildcards(msgclient->tonick)
                                    && only_wildcards(msgclient->toname))
                               && !ss_secret) {
                                sendto_one(from_ptr,
                                           "NOTICE %s :*** %s (%s) %s: %s",
					   from_ptr->name,
					   ss_hidden ? msgclient->tonick : nick,
					   buf,"is here",message);
                                msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
		case 's' : if (!ss_secret &&
                               (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                                && !(only_wildcards(msgclient->tonick)
                                     && only_wildcards(msgclient->toname)))) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :*** %s (%s) %s: %s",
			         	   from_ptr->name,
			       	           ss_hidden ? msgclient->tonick : nick,
					   buf,"is on IRC now",message);
                               msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
		case 'g' : if (!ss_secret &&
                               (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                                && !(only_wildcards(msgclient->tonick)
                                     && only_wildcards(msgclient->toname)))) {
                               sendto_one(from_ptr,
                                          "NOTICE %s :*** %s (%s) %s: %s",
			         	   from_ptr->name,
			       	           ss_hidden ? msgclient->tonick : nick,
					   buf,"is on IRC now",message);
                               msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
                case 'e' : if (!ss_secret)
                                sendto_one(from_ptr,
					   "NOTICE %s :*** %s (%s) %s: %s",
					   from_ptr->name,
					   ss_hidden ? msgclient->tonick:nick,
					   buf,"signs off",message);
                           break;
	        case 'n' : 
                         msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED;
                         if (ss_secret) {
                          if (msgclient->flags & FLAGS_NEWNICK_SECRET_DISPLAYED
                              || only_wildcards(msgclient->tonick)
                                 && only_wildcards(msgclient->toname))
                              goto break2b;
			    msgclient->flags |= FLAGS_NEWNICK_SECRET_DISPLAYED;
		          } else {
                                if (!matches(msgclient->tonick,newnick))
                                    msgclient->flags |= 
                                               FLAGS_NEWNICK_DISPLAYED;
                                    msgclient->flags &= 
                                               ~FLAGS_NEWNICK_SECRET_DISPLAYED;
			       }
                        if (ss_secret)
                          sendto_one(from_ptr,"NOTICE %s :*** %s (%s) %s: %s",
                                     from_ptr->name, msgclient->tonick,
                                     buf, "signs off", message);
                         else
                           sendto_one(from_ptr,
                                      "NOTICE %s :*** %s (%s) %s <%s> %s",
                                      from_ptr->name, nick, buf, 
                                      "changed name to", newnick, message);
               }
           break2b:;
         }

 while (mode != 'g' && msgclient->flags & FLAGS_WHOWAS) {
        gflags = 0;time(&clock); new_clock = clock; prev_clock = 0;
        time_l = clock;start_day(&time_l,'l');
        if (*message)
            if (*message == '+') 
                timeout = atoi(message+1)*24*3600+clock;
              else if (*message == '-') timeout = atoi(message+1)*3600+clock;
        if (timeout <= 0) timeout = msgclient->timeout;
        new_nick = prev_nick = nick; t = first_tnl;
        while(last_tnl && t <= last_tnl) {
             if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS
                 && ToNameList[t]->timeout > clock
                 && (msgclient->flags & FLAGS_OPER
                     && ToNameList[t]->flags & FLAGS_OPER
                     || !(ToNameList[t]->flags & FLAGS_OPER))
                 && (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
                    == (ToNameList[t]->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
                 && (msgclient->flags & FLAGS_REGISTER_NEWNICK)
                    == (ToNameList[t]->flags & FLAGS_REGISTER_NEWNICK)
                 && !Usermycmp(ToNameList[t]->toname,sptr->user->username)
                 && host_check(ToNameList[t]->tohost,sptr->user->host,'l')
                 && (!(msgclient->flags & FLAGS_REGISTER_NEWNICK)
                     || !mycmp(ToNameList[t]->tonick,nick))
                 && (!(msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
                     || ToNameList[t]->time >= time_l
                     || *ToNameList[t]->passwd != '*')) {
                 *gnew = 2;
                 if (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
                     clock = ToNameList[t]->time;
	         timeout = ToNameList[t]->timeout;
                 if (clock < time_l && 
                     msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY) {
    		     *gnew = 3;clock = time_l;
		  }
	         if ((secret || mode == 'e')
                     && *ToNameList[t]->passwd == 'a' 
	             && new_clock - atol(ToNameList[t]->message) < 60) { 
                     ToNameList[t]->flags |= FLAGS_WHOWAS_SECRET;
                     gflags |= FLAGS_WHOWAS_SECRET;
	          }
                 if (ToNameList[t]->flags & FLAGS_WHOWAS_SECRET) {
                     prev_nick = ToNameList[t]->fromnick;
                     prev_clock = atol(ToNameList[t]->fromhost);
	          } else {
   	                   prev_nick = ToNameList[t]->tonick;
                           prev_clock = atol(ToNameList[t]->message);
	              }
                 if (ToNameList[t]->flags & FLAGS_WHOWAS_CONNECTED_HERE
                     && mode != 'a') gflags |= FLAGS_WHOWAS_CONNECTED_HERE;
                 ToNameList[t]->timeout = 0;
                 break;
	     } 
           t++;
	 }
        if (secret) gflags |= FLAGS_WHOWAS_SECRET;
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_WHOWAS;
        gflags |= FLAGS_NAME;
        if (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
         gflags |= FLAGS_WHOWAS_LOG_EACH_DAY;
        if (msgclient->flags & FLAGS_REGISTER_NEWNICK)
         gflags |= FLAGS_REGISTER_NEWNICK;
        if (IsOper(sptr)) gflags |= FLAGS_OPER;
        if (MyConnect(sptr)) gflags |= FLAGS_WHOWAS_CONNECTED_HERE;
        if (mode == 'a') *wmode = 'a';
         else 
           if (mode == 'e' || mode == 'n' && mycmp(nick,newnick)) *wmode = '*'; 
             else *wmode = '-';
        if (*gnew == 3) new("*","","?","",ToNameList[t]->tonick,
                            ToNameList[t]->toname,ToNameList[t]->tohost,
                            ToNameList[t]->flags,timeout,
                            ToNameList[t]->time,ltoa(time_l));
        strcpy(abuf,ltoa(new_clock));
        if (*gnew == 2 && StrEq(ToNameList[t]->toname,sptr->user->username)) {
            ToNameList[t]->timeout = timeout;
            ToNameList[t]->time = clock;
            ToNameList[t]->flags = gflags;
            DupNewString(ToNameList[t]->passwd,wmode);
            DupNewString(ToNameList[t]->fromnick,prev_nick);
            DupNewString(ToNameList[t]->fromhost,ltoa(prev_clock));
            DupNewString(ToNameList[t]->tonick,new_nick);
            DupNewString(ToNameList[t]->tohost,sptr->user->host);
            DupNewString(ToNameList[t]->message,abuf);
          } else new(wmode,prev_nick,"?",ltoa(prev_clock),new_nick,
                     sptr->user->username,sptr->user->host,gflags,
                     timeout,clock,abuf);
        *gnew = 1;break;
    }
 if (*send && secret && !StrEq(msgclient->tonick, sptr->name)
     || mode == 'e' || mode == 'n') { 
     *repeat = 1; *send = 0;
     return message;
  }
 while (mode != 'g' && (msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION
        || msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)) {
        if (*message && matches(message,sptr->info) || ss_secret) {
           *repeat = 1; break;
	 }
        sprintf(mbuf,"Match for %s!%s@%s (%s) is: %s!%s@%s (%s)",
                msgclient->tonick,msgclient->toname,msgclient->tohost,
                *msgclient->message ? message : "*", 
                ss_hidden ? msgclient->tonick : nick,
                sptr->user->username,sptr->user->host,sptr->info);
        if (msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND && from_ptr) {
            sendto_one(from_ptr,"NOTICE %s :*** %s",from_ptr->name,mbuf);
            break;
	 }
        t1 = 0; t = first_tnl_indexnode(msgclient->fromname);
        last = last_tnl_indexnode(msgclient->fromname);
        while (last && t <= last) {
              if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_NOTICE &&
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
        gflags |= FLAGS_NAME;
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
        sprintf(buf,"%s (%s@%s) has received note",
                ss_hidden ? msgclient->tonick :
                nick,sptr->user->username,sptr->user->host);
        if (msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED && from_ptr) {
           sendto_one(from_ptr,"NOTICE %s :*** %s dated %s",
                      from_ptr->name,buf,myctime(msgclient->time));
           break;
         }
       gflags = 0;
       gflags |= FLAGS_OPER;
       gflags |= FLAGS_NAME;
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
 if (mode != 'g' && msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE
     && msgclient->flags & FLAGS_REPEAT_UNTIL_TIMEOUT
     && msgclient->flags & FLAGS_REPEAT_ONCE) {
     if (!*reserved && !mycmp(nick,msgclient->fromnick)
     && (Usermycmp(sptr->user->username,msgclient->fromname)
         || !host_check(sptr->user->host,msgclient->fromhost,'l'))) {
         time(&clock);*reserved = 1;
	 t = strlen(nick) + 36;
	 t1 = strlen(msgclient->fromname) + strlen(msgclient->fromhost) + 8;
	 sprintf(ebuf,"%d seconds ago. Please choose another.",
		 clock-msgclient->time);
	 if (t1 < t) t1 = t; if(t1 < strlen(ebuf)) t1 = strlen(ebuf)+1;
	 buf[0] = '+';for (t = 1;t <= t1;t++) buf[t] = '-'; 
	 buf[t++] = '+';buf[t] = 0;
	 sender = sptr->user->server[0] ? me.name : nick;
	 sendto_one(sptr,":%s NOTICE %s :%s",sender,nick,buf);            
	 sendto_one(sptr,
                    ":%s NOTICE %s :! Nickname <%s> is already reserved for",
		    sender,nick,msgclient->fromnick);            
	 sendto_one(sptr,":%s NOTICE %s :! -- %s@%s --",sender,nick,
		    msgclient->fromname,msgclient->fromhost);      
	 sendto_one(sptr,":%s NOTICE %s :! %s",sender,nick,ebuf);
	 sendto_one(sptr,":%s NOTICE %s :%s",sender,nick,buf);            
      }
     *send = 0; return message; /* This is no bug! */
  }
 if (msgclient->flags & FLAGS_REPEAT_ONCE) {
     msgclient->flags &= ~FLAGS_REPEAT_ONCE;
     *repeat = 1;
   } 
 while (*send && msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) {
        if (mode == 'g') {
            *repeat = 1; *send = 0; break;
	  }
        if (*gnew) last_tnl = last_tnl_indexnode(sptr->user->username);
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
        gflags |= FLAGS_NAME;
        if (msgclient->flags & FLAGS_OPER) gflags |= FLAGS_OPER;
        time(&clock);*gnew = 1;
        new(msgclient->passwd,
            msgclient->flags & FLAGS_ALL_NICK_VALID ? "*":msgclient->fromnick,
            msgclient->fromname,msgclient->fromhost,nick,sptr->user->username,
            sptr->user->host,gflags,msgclient->timeout,clock,"");
        break;
   }
  return message;
}

void check_messages(cptr, sptr, destination, mode)
aClient *cptr, *sptr;
char *destination, mode;
{
 aMsgClient *msgclient, **index_p;
 char dibuf[40], *newnick, *message, *sender, *tonick = destination;
 int last, first_tnl = 0, last_tnl = 0, number_matched = 0, 
     send, t, repeat, gnew, reserved = 0;
 long clock,flags;
 aClient *from_ptr = sptr;

 if (!sptr->user || !*tonick) return;
 if (mode != 'e' && SecretChannel(sptr->user->channel)
     && StrEq(tonick,"IRCnote")) {
	 sender = sptr->user->server[0] ? me.name : tonick;
         sendto_one(sptr,":%s NOTICE %s : <%s> %s (Q/%d W/%d) %s",
                    sender, tonick, me.name, VERSION, number_fromname(),
                    number_whowas(),
                    StrEq(ptr,"IRCnote") ? "" : (!*ptr ? "-" : ptr));
      }
 if (!fromname_index) return;
 time(&clock);
 if (clock > old_clock+note_msf) {
    save_messages();old_clock = clock;
  }
 if (mode == 'n') {
     newnick = tonick;tonick = sptr->name;
  }
 if (mode != 'g') {
     first_tnl = first_tnl_indexnode(sptr->user->username);
     last_tnl = last_tnl_indexnode(sptr->user->username);
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
           t = first_tnl; last = last_tnl;
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
               && !(msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)
               && !(msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
               && !matches(msgclient->tonick, tonick)
               && !matches(msgclient->toname, sptr->user->username)
               && !matches(msgclient->tohost, sptr->user->host)
               && (mode != 's' && mode != 'g'
                   || (msgclient->flags & FLAGS_ALL_NICK_VALID
                       || !mycmp(msgclient->fromnick, destination))
                       && host_check(msgclient->fromhost, 
                                     from_ptr->user->host, 'l'))) {
               if (mode == 'a' || 
                   mode == 'g' && (!(msgclient->flags & FLAGS_ALL_NICK_VALID)
                                   || StrEq(destination, from_ptr->name))) {
                   msgclient->flags &= ~FLAGS_NEWNICK_SECRET_DISPLAYED; 
                   msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED; 
   	        }
               message = check_flags(sptr, from_ptr, tonick, newnick, 
                                     msgclient, first_tnl, last_tnl, 
                                     &send, &repeat, &gnew, mode, &reserved);
               if (send) {
                   flags = msgclient->flags;
                   flags &= ~FLAGS_SEND_ONLY_IF_NOT_EXCEPTION;
                   display_flags(flags,dibuf);
                   sender = sptr->user->server[0] ? me.name : tonick;
                   sendto_one(sptr,":%s NOTICE %s :*%s %s %s@%s %d* %s",
                              sender,tonick,dibuf,msgclient->fromnick,
                              msgclient->fromname,msgclient->fromhost,
                              msgclient->time,message);
       	        }
               number_matched++;
	     }
           if (!repeat) msgclient->timeout = 0;
           if (gnew) {
               if (mode != 'g') {
                   first_tnl = first_tnl_indexnode(sptr->user->username);
                   last_tnl = last_tnl_indexnode(sptr->user->username);
		}
               if (mode == 's' || mode == 'g') {
                  t = msgclient->InFromName;
                  last = last_fnl_indexnode(from_ptr->user->username);
		} else {
                         if (index_p == ToNameList) {
                             t = msgclient->InToName;
                             last = last_tnl;
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
         index_p = WildCardList;
         t = 1; last = wildcard_index;
      } else {
               if (mode == 'n') {
                   mode = 'c'; tonick = newnick;
                   t = first_tnl; last = last_tnl;
                   index_p = ToNameList;
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

 if (!set_flags(sptr,flag_s, &flags,'d',"in first arg")) return;
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
            sendto_one(sptr,"NOTICE %s :*** Removed -> %s %s (%s@%s)",
                       sptr->name,dibuf,msgclient->tonick,
                       msgclient->toname,msgclient->tohost);
            remove_msg(msgclient);last--;removed++;

        } else t++;
   }
 if (!removed) sendto_one(sptr,"ERROR :No such message(s) found");
}

static void msg_save(sptr)
aClient *sptr;
{
 if (!changes_to_save) {
     sendto_one(sptr,"NOTICE %s :*** No changes to save",sptr->name);
     return;
 }
 if (IsOperHere(sptr)) {
    save_messages();
    sendto_one(sptr,"NOTICE %s :*** Messages are now saved",sptr->name);
  } else
      sendto_one(sptr,"NOTICE %s :*** Oops! All messages lost...",sptr->name);
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
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,message[l+1]);
     else { 
           if (!max && (msg == &note_mst || msg == &note_msf)) max = 1;
           *msg = max;if (msg == &note_msf) *msg *= 60;
           sendto_one(sptr,"NOTICE %s :*** %s %d",sptr->name,message[l+2],max);
           changes_to_save = 1;
       }
 } else {
         max = (*msg);if (msg == &note_msf) max /= 60;
         sendto_one(sptr,"NOTICE %s :*** %s %d",sptr->name,message[l],max);
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
     flag_whowas = 0,flag_destination = 0;
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
                if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)
                    flag_whowas++; else 
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
                if (find_chars('*','?',msgclient->tonick))
                    tonick_wildcards++;
                if (!(msgclient->flags & FLAGS_NAME)) 
                    toname_wildcards++;
                if (find_chars('*','?',msgclient->tohost))
                    tohost_wildcards++;
                t++;
	    }
	    if (!any) sendto_one(sptr,"ERROR :No message(s) found");
                else {
                     if (IsOperHere(sptr) || Key(sptr)) {
                         sprintf(buf,"%s%d /%s%d /%s%d /%s (%s%d /%s%d /%s%d)",
                                 "Nicks:",nicks,"Names:",names,
                                 "Hosts:",hosts,"W.cards",
                                 "Tonicks:",tonick_wildcards,
                                 "Tonames:",toname_wildcards,
                                 "Tohosts:",tohost_wildcards);
                         sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
		      }
                      sprintf(buf,"%s %s%d / %s%d / %s%d",
                              "Server generated",
                              "?-whowas: ",flag_whowas,
                              "G-notice: ",flag_notice,
                              "H-header: ",flag_destination);
                      sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
		 }
     } else if (!mycmp(arg,"RESET")) {
                if (!IsOperHere(sptr)) 
                    sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                               "Wrong button - try another next time...");
                 else {
                       note_mst = NOTE_MAXSERVER_TIME,
                       note_mum = NOTE_MAXUSER_MESSAGES,
                       note_msm = NOTE_MAXSERVER_MESSAGES,
                       note_msw = NOTE_MAXSERVER_WILDCARDS,
                       note_muw = NOTE_MAXUSER_WILDCARDS;
                       note_msf = NOTE_SAVE_FREQUENCY*60;
                       sendto_one(sptr,"NOTICE %s :*** %s",
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
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
    }
}

static void msg_send(sptr, passwd, flag_s, timeout_s, name, message)
aClient *sptr;
char *passwd, *flag_s, *timeout_s, *name, *message;
{
 aMsgClient **index_p, *msgclient;
 int sent_wild = 0, sent = 0, t, last, savetime, nr_whowas;
 long clock, timeout, flags = 0;
 char buf1[BUF_LEN], buf2[BUF_LEN], dibuf[40], *c, *null_char = "",
      tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3];

 time (&clock);
 t = first_tnl_indexnode(sptr->user->username);
 last = last_tnl_indexnode(sptr->user->username);
 index_p = ToNameList;
 time (&clock);
 while (1) {
     while (last && t <= last) {
  	    msgclient = index_p[t];
	    if (clock > msgclient->timeout) {
  	        remove_msg(msgclient);last--;
 	        continue;
	     }
            if (msgclient->flags & FLAGS_NOT_USE_SEND
		&& !(msgclient->flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)
		&& !matches(msgclient->tonick,sptr->name)
		&& !matches(msgclient->toname,sptr->user->username)
		&& !matches(msgclient->tohost,sptr->user->host)) {
		sendto_one(sptr,"NOTICE %s :*** %s (%s@%s) %s %s",sptr->name,
			   msgclient->fromnick,msgclient->fromname,
			   msgclient->fromhost,
                           "doesn't allow you to use SEND:",
                           msgclient->message);
		return;
	    }
            t++;
	}
     if (index_p == ToNameList) {
         index_p = WildCardList;
         t = 1; last = wildcard_index;
      } else break;
 }
 if (number_fromname() >= note_msm 
     && !IsOperHere(sptr) && !Key(sptr)) {
     if (!note_msm || !note_mum)
         sendto_one(sptr,"ERROR :The notesystem is closed for no-operators");
      else sendto_one(sptr,"ERROR :No more than %d message%s %s",
                      note_msm,note_msm < 2?"":"s",
                      "allowed in the server");
     return;
  }
 if (clock > old_clock+note_msf) {
    save_messages();old_clock = clock;
  }
 if (!set_flags(sptr,flag_s,&flags,'s',"\n")) return;
 split(name, tonick, toname, tohost);
 if (!find_chars('*','?',toname)) flags |= FLAGS_NAME;            
 if (IsOper(sptr)) flags |= FLAGS_OPER;
 if (*timeout_s == '+') timeout = atoi(timeout_s + 1) * 24;
  else if (*timeout_s == '-') timeout = atoi(timeout_s + 1);
 if (timeout > note_mst && !(flags & FLAGS_OPER) && !Key(sptr)) {
    sendto_one(sptr,"ERROR :Max time allowed is %d hour%s",
               note_mst,note_mst>1?"":"s");
    return;
  }
 if (!message) {
    if (!send_flag(flags)) message = null_char; 
     else {
           sendto_one(sptr,"ERROR :No message specified");
           return;
       }
  }
 if (strlen(message) > MSG_LEN) {
    sendto_one(sptr,"ERROR :Message can't be longer than %d chars",
               MSG_LEN);
    return;
 }
  if (!(flags & FLAGS_OPER) && !Key(sptr)) {
     t = first_fnl_indexnode(sptr->user->username);
     last = last_fnl_indexnode(sptr->user->username);
     time(&clock);
     while (last && t <= last) {
         if (clock>FromNameList[t]->timeout) {
             remove_msg(FromNameList[t]);last--;
             continue;
          }
         if (!Usermycmp(sptr->user->username,FromNameList[t]->fromname)) {
             if (host_check(sptr->user->host,FromNameList[t]->fromhost,'l')) {
                 sent++;
                 if (!(FromNameList[t]->flags & FLAGS_NAME)) sent_wild++;  
              }
                 
          }
         t++;
       }
     if (sent >= note_mum) {
        sendto_one(sptr,"ERROR :No more than %d message%s %s",
                note_mum,note_mum < 2?"":"s",
                "for each user allowed in the server");
        return;
      }
     while (!(flags & FLAGS_NAME)) {
         if (!note_msw || !note_muw)
            sendto_one(sptr,"ERROR :No-operators are not allowed %s",
                       "to send to username with wildcards");
          else if (wildcard_index >= note_msw) 
                   sendto_one(sptr,"ERROR :No more than %d msg. %s %s",
                              note_msw,"with username w.cards",
                              "allowed in the server");
          else if (sent_wild >= note_muw) 
                   sendto_one(sptr,"ERROR :No more than %d %s %s",
                              note_muw,note_muw < 2?"sessage":"messages with",
                              "username w.cards allowed per user");
          else break;
          return;
     }
 }
 time(&clock);
 new(passwd,sptr->name,sptr->user->username,sptr->user->host,
     tonick,toname,tohost,flags,timeout*3600+clock,clock,message);
 display_flags(flags,dibuf);
 sprintf(buf1,"%s %s (%s@%s) / %s%d / %s%s",
         "Queued ->",tonick,toname,tohost,
         "Max.hours:",timeout,"Flags:",dibuf);
 *buf2 = 0; if (!(flags & FLAGS_NOT_SAVE)) {
              savetime = (note_msf-clock+old_clock)/60;
              sprintf(buf2,"%s%d","Save.min:",savetime);        
           }   
 sendto_one(sptr,"NOTICE %s :*** %s %s",sptr->name,buf1,buf2);
 check_messages(sptr, sptr, sptr->name, 's');
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
     sendto_one(sptr,"ERROR :No such option: %s",arg); 
     return;
  }
 if (!set_flags(sptr,flag_s, &flags,'d',"in first arg")) return;
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
        msgclient = FromNameList[t];flags = msgclient->flags;
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
            if (log) sendto_one(sptr,"NOTICE %s :*** %s: %s (%s@%s)",
                                sptr->name, myctime(msgclient->time),
                                msgclient->tonick, msgclient->toname,
                                msgclient->tohost);
             else 
              if (!count) {
                 display_flags(msgclient->flags,dibuf),
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
 if (!number) sendto_one(sptr,"ERROR :No such %s",
                         log ? "log(s)" : "message(s) found");
  else if (count) sendto_one(sptr,"NOTICE %s :*** %s %s (%s@%s): %d",
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
     sendto_one(sptr,"ERROR :No flag changes specified");
     return;
  }
 if (!set_flags(sptr, newflag_s, &flags,'c',"in changes")) return;
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
                sendto_one(sptr,"NOTICE %s :*** %s -> %s %s (%s@%s)",sptr->name,
                           "No flag change for",dibuf1,msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
             else                                        
                sendto_one(sptr,"NOTICE %s :*** %s -> %s %s (%s@%s) to %s",
                          sptr->name,"Flag change",dibuf1,msgclient->tonick,
                          msgclient->toname,msgclient->tohost,dibuf2);
           flagged++;
        } 
       t++;
   }
 if (!flagged) sendto_one(sptr,"ERROR :No such message(s) found");
}

static void msg_sent(sptr, arg, name, time_s, delete)
aClient *sptr;
char *arg, *name, *time_s, *delete;
{
 aMsgClient *msgclient,*next_msgclient;
 char fromnick[BUF_LEN+3], fromname[BUF_LEN+3], fromhost[BUF_LEN+3]; 
 int number = 0,t,last,nick = 0,count = 0,users = 0;
 long clock,time_l;

 if (!arg) nick = 1; else
  if (MyEq(arg,"COUNT")) count = 1; else
   if (MyEq(arg,"USERS")) users = 1; else
   if (!MyEq(arg,"NAME")) {
       sendto_one(sptr,"ERROR :No such option: %s",arg); 
       return;
  }
 if (users) {
    if (!IsOperHere(sptr)) {
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
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
    for (t=1;t <= fromname_index;t++) {
         msgclient = FromNameList[t];
         do next_msgclient = t < fromname_index ? FromNameList[t+1] : NULL;
          while (next_msgclient && next_msgclient->timeout <= clock);
         if (msgclient->timeout > clock
             && !(msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)
             && (!time_l || msgclient->time >= time_l 
                 && msgclient->time < time_l+24*3600)
             && (!matches(fromnick,msgclient->fromnick))
             && (!matches(fromname,msgclient->fromname))
             && (!matches(fromhost,msgclient->fromhost))
             && (!delete || !mycmp(delete,"RM")
                 || !mycmp(delete,"RMAB") &&
                    (msgclient->flags & FLAGS_WHOWAS ||
                     msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE))) {
             if (delete || !next_msgclient
                 || mycmp(next_msgclient->fromnick,msgclient->fromnick)
                 || mycmp(next_msgclient->fromname,msgclient->fromname)     
                 || !(host_check(next_msgclient->fromhost,
                                 msgclient->fromhost,'l'))) {
                  sendto_one(sptr,"NOTICE %s :%s#%d# %s (%s@%s) @%s",
                             sptr->name,
                             delete ? "*** Removing -> " : "", 
                             delete ? 1 : count+1,
                             msgclient->fromnick,msgclient->fromname,
                             local_host(msgclient->fromhost),
                             msgclient->fromhost);
                    if (delete) msgclient->timeout = 0;
                 count = 0;number = 1;
	     } else count++;
	 }
     }
  if (!number) sendto_one(sptr,"ERROR :No message(s) from such user(s) found");
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
            if (!IsOper(sptr) && msgclient->flags & FLAGS_OPER) continue;
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
 if (!number) sendto_one(sptr,"ERROR :No such message(s) found");
  else if (count) sendto_one(sptr,"NOTICE %s :<> %s %d",sptr->name,
                             "Number of no-operator messages queued:",number);
}

static nick_prilog(sptr, nr_msgclient, clock, time1_s, flag_s)
aClient *sptr;
aMsgClient *nr_msgclient;
long clock;
char *time1_s, *flag_s;
{
 register int t, m_wled, m_rn, i_wled, i_rn, nr, last;
 aMsgClient *msgclient;
 long nr_flags = 0;

 i_wled = nr_msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY;
 i_rn = nr_msgclient->flags & FLAGS_REGISTER_NEWNICK;
 nr = nr_msgclient->InToName;
 t = first_tnl_indexnode(nr_msgclient->toname);
 last = last_tnl_indexnode(nr_msgclient->toname);
 while (last && t <= last) {
        msgclient = ToNameList[t];
        nr_flags = msgclient->flags;
        set_flags(sptr, flag_s, &nr_flags, 'd', "");
        if (t != nr && clock <= msgclient->timeout
            && ((!mycmp(msgclient->tonick, nr_msgclient->tonick)
                && (msgclient->flags & FLAGS_OPER)
                    == (nr_msgclient->flags & FLAGS_OPER))
                || nr_flags & FLAGS_WHOWAS_LIST_ONLY_USERNAME)
            && !Usermycmp(msgclient->toname,nr_msgclient->toname)
            && host_check(msgclient->tohost,nr_msgclient->tohost,'l')
            && msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS
            && msgclient->flags == (nr_flags & ~FLAGS_WHOWAS_FLAGS)) {
            m_wled = msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY;
            m_rn = msgclient->flags & FLAGS_REGISTER_NEWNICK;
            if (nr_flags & FLAGS_WHOWAS_LIST_ONLY_USERNAME) {
                if (msgclient->time > nr_msgclient->time
                    || msgclient->time == nr_msgclient->time && t < nr) 
                 return 0;
	      } else 
                 if (!i_wled && (m_wled || m_rn && !i_rn)
                     || i_wled && !i_rn && m_wled && m_rn
                     || !i_wled && !m_wled && m_rn == i_rn && t < nr
                     || i_rn && m_rn && m_wled && i_wled && 
                        (t < nr && same_day(msgclient->time,nr_msgclient->time)
                         || !time1_s && msgclient->time > nr_msgclient->time))
                     return 0;
	}
       t++;
   }
  return 1;
}

static void msg_whowas(sptr, flag_s, max_s, name, time1_s, time2_s, delete)
aClient *sptr;
char *flag_s, *name, *max_s, *time1_s, *time2_s, *delete;
{
 aMsgClient *msgclient,**index_p;
 int number = 0, t, last,secret, displayed = 0;
 register int t2;
 long max, flags, clock, clock2, time1_l, time2_l;
 char *c, tonick[BUF_LEN+3], toname[BUF_LEN+3], tohost[BUF_LEN+3], 
      *search = NULL, buf1[BUF_LEN], buf2[BUF_LEN], buf3[BUF_LEN], 
      search_sl[BUF_LEN+3], search_sh[BUF_LEN+3], dibuf[40];

 if (!set_flags(sptr,flag_s, &flags,'d',"")) return;
 if (!name) {
    for (t2 = 1; t2 <= fromname_index; t2++)
         if (FromNameList[t2]->flags & FLAGS_WHOWAS &&
             !(FromNameList[t2]->flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL)) {
            display_flags(FromNameList[t2]->flags,dibuf);
            sendto_one(sptr,"NOTICE %s :*** Listed from: %s %s %s (%s@%s) %s",
                       sptr->name,myctime(FromNameList[t2]->time),
                       dibuf,FromNameList[t2]->tonick,
                       FromNameList[t2]->toname,
                       FromNameList[t2]->tohost,
                       FromNameList[t2]->message);
	         }
    return;
  }
 split(name, tonick, toname, tohost);
 if (!IsOperHere(sptr) && !Key(sptr)
     && only_wildcards(tonick)
     && only_wildcards(toname)
     && only_wildcards(tohost)) {
     sendto_one(sptr,"ERROR :%s %s","Both nick, name and host",
                "can not be specified with only wildcards");
     return;
 }
 search = toname; 
 strcpy(search_sl,search);
 strcpy(search_sh,search);
 c = find_chars('*','?',search_sl);if (c) *c = 0;
 c = find_chars('*','?',search_sh);if (c) *c = 0;
 if (!*search_sl) search = 0; 
  else for (c = search_sh;*c;c++); 
 (*--c)++;
 if (!search) { 
     t = first_fnl_indexnode("?");
     last = last_fnl_indexnode("?");
     index_p = FromNameList;
  } else {
           t = first_tnl_indexnode(search_sl);
           last = last_tnl_indexnode(search_sh);
           index_p = ToNameList;
      }
 if (!time1_s) time1_l = 0; 
  else { 
        time1_l = set_date(sptr,time1_s);
        if (time1_l < 0) return;
    }
 if (!time2_s) time2_l = time1_l+3600*24; 
  else { 
        time2_l = set_date(sptr,time2_s);
        if (time2_l < 0) return;
        time2_l += 3600*24;
    }
 if (delete && mycmp(delete,"RM") && mycmp(delete,"RMS")) {
     sendto_one(sptr,"ERROR :Unknown argument: %s",delete); 
     return;
 }
 if (delete && !IsOperHere(sptr) && !KeyFlags(sptr,FLAGS_WHOWAS)) {
     sendto_one(sptr,"NOTICE %s :*** Ask the operator about this...",
                sptr->name); 
     return;
 }
 if (max_s) max = atoi(max_s);
  else max = 0;
 time (&clock);
 while (last && t <= last) {
       msgclient = index_p[t];
        if (clock > msgclient->timeout) {
            remove_msg(msgclient);last--;
            continue;
         }
        flags = msgclient->flags;
        set_flags(sptr, flag_s, &flags, 'd', "");
        if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS
            && msgclient->flags == (flags & ~FLAGS_WHOWAS_FLAGS)
            && (!time1_l || msgclient->time >= time1_l 
                && msgclient->time < time2_l)
            && (!matches(tonick,msgclient->tonick))
            && (!matches(toname,msgclient->toname))
            && (!matches(tohost,msgclient->tohost))
            && (delete || 
                nick_prilog(sptr, msgclient, clock, time1_s, flag_s))) {
            if (msgclient->flags & FLAGS_WHOWAS_SECRET
                 && (!(msgclient->flags & FLAGS_WHOWAS_CONNECTED_HERE) 
                     || !KeyFlags(sptr,FLAGS_WHOWAS) && !IsOper(sptr)))
                 secret = 1; else secret = 0;
             clock2 = atol(secret ? msgclient->fromhost : msgclient->message);
             if (!clock2) {
                 t++; continue;
              }
             number++;
             strcpy(buf2,myctime(clock2)); 
             strcpy(buf1,myctime(msgclient->time));
             if (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY) {
		       buf1[19] = 0;strcpy(buf3,buf1);buf1[19] = ' ';
		       buf3[19] = ' ';buf3[20] = '+';buf2[19] = 0;
		       strcpy(buf3+21,buf2+10);buf2[19] = ' ';
		       strcpy(buf3+30,buf1+19);
	     } else strcpy(buf3,buf2);
            t2 = 0;
            if (msgclient->flags & FLAGS_WHOWAS_CONNECTED_HERE) 
                dibuf[t2++] = ';'; 
             else dibuf[t2++] = ':';
            if (msgclient->flags & FLAGS_OPER) dibuf[t2++] = '*';
             else dibuf[t2++] = ' ';
            dibuf[t2] = 0;
            if (!(flags & FLAGS_WHOWAS_COUNT)
                && (!delete || mycmp(delete,"RMS"))) {
                sendto_one(sptr,"NOTICE %s :*** %s%s%s %s (%s@%s)",
                           sptr->name,delete ? "Removed -> " : "",
                           buf3,dibuf,
                           secret ? msgclient->fromnick : msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
                if (!max && displayed++ > MAX_WHOWAS_LISTING) {
                    sendto_one(sptr,
                               "ERROR :--- Too many match. #max = %d. ---",
                               MAX_WHOWAS_LISTING); 
                    return;
                 }
	      }
            if (max && number >= max) {
                sendto_one(sptr,
                           "ERROR :--- ...skipping after #%d match. ---",max);
                return;
             }                
            if (delete) {
                remove_msg(msgclient);last--;
                continue;
	    }
        } 
        t++;
   }
  flags = 0; set_flags(sptr, flag_s, &flags, 'd', "");
  if (!number) sendto_one(sptr,"ERROR :No such user(s) found");
   else if (delete)
           sendto_one(sptr,"NOTICE %s :*** %d username%s removed",
                      sptr->name, number, number < 2 ? "" : "s");   
    else if (flags & FLAGS_WHOWAS_COUNT) {
             sendto_one(sptr,"NOTICE %s :*** %d user match.",
                        sptr->name, number);   
          }
}

static name_len_error(sptr, name)
aClient *sptr;
char *name;
{
 if (strlen(name) > BUF_LEN) {
    sendto_one(sptr,"ERROR :Nick!name@host can't be longer than %d chars",
               BUF_LEN);
    return 1;
  }
 return 0;
}

static flag_len_error(sptr, flag_s)
aClient *sptr;
char *flag_s;
{
 if (strlen(flag_s) > BUF_LEN) {
    sendto_one(sptr,"ERROR :Flag string can't be longer than %d chars",
               BUF_LEN);
    return 1;
  }
 return 0;
}

static alias_send(sptr, option, flags, msg, timeout)
aClient *sptr;
char *option, **flags, **msg, **timeout;
{
 static char flag_s[BUF_LEN+10], *deft = "+7",
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
 if (MyEq(option,"PLOG")) sprintf(flag_s,"+AR%s", *flags); else
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
 int t, t2, slen = 10;

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
    sendto_one(sptr,"ERROR :No option for NOTE specified."); 
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
 if (!passwd) passwd = wildcard;
 if (!flags) flags = (char *) "";
 if (flags && flag_len_error(sptr, flags)) { free(args); return; }

 if (MyEq(option,"STATS")) msg_stats(sptr, arg[t], arg[t+1]); else
 if (MyEq(option,"WHOWAS")) {
     name = arg[t];
     if (name && name_len_error(sptr, name)) { free(args); return; }
     msg_whowas(sptr, flags, id, name, arg[t+1], arg[t+2], arg[t+3]); 
  } else
 if (alias_send(sptr, option, &flags, &msg, &timeout) || MyEq(option,"USER")) {
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }
     if (!timeout) timeout = default_timeout;  
     msg_send(sptr, passwd, flags, timeout, name, msg);
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
        sendto_one(sptr,"ERROR :Please specify at least one argument");
        free(args); return;
      }
     name = arg[t];if (!name) name = wildcard;
     if (name_len_error(sptr, name)) { free(args); return; }  
     msg_remove(sptr, passwd, flags, id, name, arg[t+1]); 
  } else sendto_one(cptr,"ERROR :No such option: %s",option);
 free(args);
} 
#endif


