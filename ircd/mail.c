/************************************************************************
 *   IRC - Internet Relay Chat, ircd/mail.c
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
 *                jarle@shibuya.cc.columbia.edu
 *           IRC: jarlek@*.uio.no
 */

#include "struct.h"
#ifdef MSG_MAIL
#include "numeric.h"
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#define VERSION "v1.2+"

#define MAIL_SAVE_FREQUENCY 60 /* Frequency of save time in minutes */
#define MAIL_MAXSERVER_TIME 24*60 /* Max hours for a message in the server */
#define MAIL_MAXSERVER_MESSAGES 2000 /* Max number of messages in the server */
#define MAIL_MAXUSER_MESSAGES 200 /* Max number of messages for each user */
#define MAIL_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
#define MAIL_MAXUSER_WILDCARDS 40 /* Max number of user toname wildcards */

#define BUF_LEN 256
#define MSG_LEN 512
#define REALLOC_SIZE 1024

#define FLAGS_OPER (1<<0)
#define FLAGS_WILDCARD (1<<1)
#define FLAGS_NAME (1<<2)
#define FLAGS_NICK (1<<3)
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
#define FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER (1<<14)
#define FLAGS_SEND_ONLY_IF_MAIL_CHANNEL (1<<15)
#define FLAGS_SEND_ONLY_IF_LOCALSERVER (1<<16)
#define FLAGS_SEND_ONLY_IF_THIS_SERVER (1<<17)
#define FLAGS_SEND_ONLY_IF_DESTINATION_OPER (1<<18)
#define FLAGS_SEND_ONLY_IF_NICK_NOT_NAME (1<<19)
#define FLAGS_SEND_ONLY_IF_NOT_EXCEPTION (1<<20)
#define FLAGS_KEY_TO_OPEN_SECRET_PORTAL (1<<21)
#define FLAGS_FIND_CORRECT_DEST_SEND_ONCE (1<<22)
#define FLAGS_WHOWAS (1<<23)
#define FLAGS_WHOWAS_LOG_EACH_DAY (1<<24)
#define FLAGS_SEND_ONLY_IF_SIGNONOFF (1<<25)
#define FLAGS_REGISTER_NEWNICK (1<<26)
#define FLAGS_NOTICE_RECEIVED_MESSAGE (1<<27)
#define FLAGS_RETURN_CORRECT_DESTINATION (1<<28)
#define FLAGS_NOT_USE_SEND (1<<29)
#define FLAGS_NEWNICK_DISPLAYED (1<<30)
#define FLAGS_NEWNICK_SECRET_DISPLAYED (1<<31)

#define DupString(x,y) do { x = MyMalloc(strlen(y)); strcpy(x,y); } while (0)
#define DupNewString(x,y) if (!StrEq(x,y)) { free(x); DupString(x,y); }  
#define MyEq(x,y) (!myncmp(x,y,strlen(x)))
#define Usermycmp(x,y) mycmp(x,y)
#define Key(sptr) KeyFlags(sptr,-1)
#define IsOperHere(sptr) (IsOper(sptr) && MyConnect(sptr))

typedef struct MsgClient {
            char *fromnick,*fromname,*fromhost,*tonick,*toname,*tohost,
                 *message,*passwd;
            long timeout,time,flags;
            int id,InToNick,InToName,InFromName;
          } aMsgClient;
  
static aMsgClient **ToNameList,**ToNickList,**FromNameList,**WildCardList;
extern aClient *client;
extern aChannel *find_channel();

static int mail_mst = MAIL_MAXSERVER_TIME,
           mail_mum = MAIL_MAXUSER_MESSAGES,
           mail_msm = MAIL_MAXSERVER_MESSAGES,
           mail_msw = MAIL_MAXSERVER_WILDCARDS,
           mail_muw = MAIL_MAXUSER_WILDCARDS,
           mail_msf = MAIL_SAVE_FREQUENCY*60,
           wildcard_index = 0,
           toname_index = 0,
           tonick_index = 0,
           fromname_index = 0,
           max_tonick,
           max_toname,
           max_wildcards,
           max_fromname,
           old_clock = 0,
           m_id = 0,
           changes_to_save = 0;

static char *ptr = "IRCmail", *mail_save_filename_tmp;
extern char *MyMalloc(), *MyRealloc(), *myctime();

static char *find_chars(c1,c2,string)
char c1,c2,*string;
{
 register char *c;

 for (c = string;*c;c++) if (*c == c1 || *c == c2) return(c);
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

static number_whowas()
{
 register int t, nr = 0;
 long clock;

 time (&clock);
 for (t = 1;t <= fromname_index; t++) 
      if (FromNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS) nr++;
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

static first_tnil_indexnode(tonick)
char *tonick;
{
 int s,t = tonick_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNickList[s]->tonick,tonick) < 0) b = s; else t = s;
 return t;
}

static last_tnil_indexnode(tonick)
char *tonick;
{
 int s,t = tonick_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNickList[s]->tonick,tonick) > 0) t = s; else b = s;
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
            
 if (fromname_index > mail_msm && flags & FLAGS_SERVER_GENERATED_WHOWAS
     || fromname_index - number_whowas() > mail_msm) return;
 if (fromname_index == max_fromname-1) {
    max_fromname += REALLOC_SIZE;allocate = max_fromname*sizeof(FromNameList);
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (wildcard_index == max_wildcards-1) {
    max_wildcards += REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList);
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (toname_index == max_toname-1) {
    max_toname += REALLOC_SIZE;allocate = max_toname*sizeof(ToNameList);
    ToNameList = (aMsgClient **) MyRealloc((char *)ToNameList,allocate);
  }
 if (tonick_index == max_tonick-1) {
    max_tonick += REALLOC_SIZE;allocate = max_tonick*sizeof(ToNickList);
    ToNickList = (aMsgClient **) MyRealloc((char *)ToNickList,allocate);
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
 if (flags & FLAGS_WILDCARD) {
     wildcard_index++;
     WildCardList[wildcard_index] = msgclient;
     msgclient->InToName = wildcard_index;
  } else { 
          if (flags & FLAGS_NAME) {
             n = last_tnl_indexnode(toname)+1;
             index_p = ToNameList+toname_index;
             toname_index++;t = toname_index-n;
               while(t--) {
                     (*index_p)->InToName++; 
                     index_p[1] = (*index_p);index_p--;
                 }
             msgclient->InToName = n;
             ToNameList[n] = msgclient;
	  }
          if (flags & FLAGS_NICK) {
              n = last_tnil_indexnode(tonick)+1;
              index_p = ToNickList+tonick_index;
              tonick_index++;t = tonick_index-n;
               while(t--) {
                     (*index_p)->InToNick++; 
                     index_p[1] = (*index_p);index_p--;
                 }
              msgclient->InToNick = n;
              ToNickList[n] = msgclient;
          }
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
 allocate = REALLOC_SIZE*sizeof(FromNameList);
 ToNickList = (aMsgClient **) MyMalloc(allocate);
 ToNameList = (aMsgClient **) MyMalloc(allocate);
 FromNameList = (aMsgClient **) MyMalloc(allocate);
 WildCardList = (aMsgClient **) MyMalloc(allocate); 
 mail_save_filename_tmp = MyMalloc(strlen(MAIL_SAVE_FILENAME)+5);
 sprintf(mail_save_filename_tmp,"%s.tmp",MAIL_SAVE_FILENAME);
 time(&clock);old_clock = clock;
 fp = fopen(MAIL_SAVE_FILENAME,"r");
 if (!fp) return;
 r_code(buf,fp);mail_msm = atol(buf);
 r_code(buf,fp);mail_mum = atol(buf);
 r_code(buf,fp);mail_msw = atol(buf);
 r_code(buf,fp);mail_muw = atol(buf);
 r_code(buf,fp);mail_mst = atol(buf);
 r_code(buf,fp);mail_msf = atol(buf);
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

static char *local_host(host)
char *host;
{
 static char buf[BUF_LEN+3];
 char *buf_p = buf, *host_p;
 int t;

 if (!numeric_host(host)) {
     host_p = host;
     while(*host_p && *host_p != '.') {
           host_p++;buf_p++;
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
           if (host_p != buf) host_p+=2;
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

static local_check(sptr,msgclient,passwd,flags,tonick,
                   toname,tohost,time_l,id)
aClient *sptr;
aMsgClient *msgclient;
char *passwd,*tonick,*toname,*tohost;
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
     && (!tonick || !matches(tonick,msgclient->tonick))
     && (!toname || !matches(toname,msgclient->toname))
     && (!*tohost || !matches(tohost,msgclient->tohost))
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
 if (flags & FLAGS_SEND_ONLY_IF_MAIL_CHANNEL) c[t++] = 'I';
 if (flags & FLAGS_SEND_ONLY_IF_LOCALSERVER) c[t++] = 'V';
 if (flags & FLAGS_SEND_ONLY_IF_THIS_SERVER) c[t++] = 'M';
 if (flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME) c[t++] = 'Q';
 if (flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) c[t++] = 'E';
 if (flags & FLAGS_KEY_TO_OPEN_SECRET_PORTAL) c[t++] = 'K';
 if (flags & FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER) c[t++] = 'Y';
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

 if (msgclient->flags & FLAGS_WILDCARD) {
     n = msgclient->InToName;
     index_p = WildCardList+n;t = wildcard_index-n;
     while(t--) {
           *index_p = index_p[1];
          (*index_p++)->InToName--;
       }
     WildCardList[wildcard_index] = 0;
     wildcard_index--;
  } else { 
          n = msgclient->InToName;
          if (msgclient->flags & FLAGS_NAME) {
                 index_p = ToNameList+n;t = toname_index-n;   
                 while(t--) {
                       *index_p = index_p[1];
                       (*index_p++)->InToName--;
                   }
                 ToNameList[toname_index] = 0;
                 toname_index--;
	    }
          if (msgclient->flags & FLAGS_NICK) {
              n = msgclient->InToNick;
              index_p = ToNickList+n;t = tonick_index-n;   
              while(t--) {
                    *index_p = index_p[1];
                    (*index_p++)->InToNick--;
                }
              ToNickList[tonick_index] = 0;
              tonick_index--;
	  }
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
 free(msgclient);
 if (max_fromname-fromname_index>REALLOC_SIZE<<1) {
    max_fromname -= REALLOC_SIZE;allocate = max_fromname*sizeof(FromNameList);
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (max_wildcards-wildcard_index>REALLOC_SIZE<<1) {
    max_wildcards -= REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList);
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (max_tonick-tonick_index>REALLOC_SIZE<<1) {
    max_tonick -= REALLOC_SIZE;allocate = max_tonick*sizeof(ToNickList);
    ToNickList = (aMsgClient **) MyRealloc((char *)ToNickList,allocate);
  }
 if (max_toname-toname_index>REALLOC_SIZE<<1) {
    max_toname -= REALLOC_SIZE;allocate = max_toname*sizeof(ToNameList);
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
     if (index_p == WildCardList) return 0;
     if (index_p == ToNameList) {
         index_p = ToNickList;
         t = first_tnil_indexnode(sptr->name);
         last = last_tnil_indexnode(sptr->name);
      } else {
              index_p = WildCardList;
              t = 1;last = wildcard_index;
         }
 }
}

static set_flags(sptr,string,flags,mode,type)
aClient *sptr;
char *string,mode,*type;
long *flags;
{
 char *c,on,buf[40],cu;
 int op,uf = 0;

 op = IsOperHere(sptr) ? 1:0; 
 for (c = string;*c;c++) {
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
              case 'I': if (on) *flags |= FLAGS_SEND_ONLY_IF_MAIL_CHANNEL;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_MAIL_CHANNEL;
                        break;             
              case 'V': if (on) *flags |= FLAGS_SEND_ONLY_IF_LOCALSERVER;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_LOCALSERVER;
                        break;             
              case 'M': if (on) *flags |= FLAGS_SEND_ONLY_IF_THIS_SERVER; 
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
              case 'Y': if (on) 
                           *flags |= FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER;
                         else *flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER;
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

static char *passwd_check(sptr,passwd)
aClient *sptr;
char *passwd;
{
 static char flag_s[BUF_LEN+3];
 char *c;
 long flags = 0;

 if (BadPtr(passwd)) {
     sendto_one(sptr,"ERROR :No password or flags specified");
     return NULL;
 }
 if (c = find_chars('+','-',passwd)) { 
     if (!set_flags(sptr,c,&flags,'d',"in first arg")) return NULL;
     if (strlen(c) > BUF_LEN) {
         sendto_one(sptr,"ERROR :Flag-string can't be longer than %d chars",
                    BUF_LEN);
        return NULL;
     }
    strcpy(flag_s,c);
    *c = 0;
 } else *flag_s = 0;
 if (!*passwd) { 
     *passwd = '*';passwd[1] = 0;
  }
 if (strlen(passwd)>10) {
     sendto_one(sptr,"ERROR :Password can't be longer than ten chars");
     return NULL;
 }
 return flag_s;
}

static ttnn_error(sptr,nick,name)
aClient *sptr;
char *nick,*name;
{
 if (BadPtr(nick)) {
    sendto_one(sptr,"ERROR :No nick specified");
    return 1;
 }
 if (strlen(nick)>BUF_LEN) {
    sendto_one(sptr,"ERROR :Nick name can't be longer than %d chars",
               BUF_LEN);
    return 1;
 }
 if (BadPtr(name)) {
    sendto_one(sptr,"ERROR :No name or host specified");
    return 1;
 }
 if (strlen(name)>BUF_LEN) {
    sendto_one(sptr,"ERROR :Name and host can't be longer than %d chars",
               BUF_LEN);
    return 1;
  }
 return 0;
}

static void split(name,host)
char *name,*host;
{
 char *np = name;

 if (BadPtr(name)) {
    *host = 0;
    return;
 }
 while (*np && *np++ != '@');
 strcpy(host,np);
 if (*np) *(np-1) = 0;
 if (!*name) {
    *name = '*';name[1] = 0;
  }
 if (!*host) {
    *host = '*';host[1] = 0;
  }
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
 fp = fopen(mail_save_filename_tmp,"w");
 if (!fp) return;
 w_code(ltoa((long)mail_msm),fp);
 w_code(ltoa((long)mail_mum),fp);
 w_code(ltoa((long)mail_msw),fp);
 w_code(ltoa((long)mail_muw),fp);
 w_code(ltoa((long)mail_mst),fp);
 w_code(ltoa((long)mail_msf),fp);
 for (t = 1;t <= fromname_index;t++) {
     msgclient = FromNameList[t];
     if (clock > msgclient->timeout) { 
         remove_msg(msgclient);continue;
      }  
     if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS
         && clock - atol(msgclient->message) < 60
         || msgclient->flags & FLAGS_NOT_SAVE) continue;
     w_code(msgclient->passwd,fp),w_code(msgclient->fromnick,fp);
     w_code(msgclient->fromname,fp);w_code(msgclient->fromhost,fp);
     w_code(msgclient->tonick,fp),w_code(msgclient->toname,fp);
     w_code(msgclient->tohost,fp),
     w_code(ltoa(msgclient->flags),fp);
     w_code(ltoa(msgclient->timeout),fp);
     w_code(ltoa(msgclient->time),fp);
     w_code(msgclient->message,fp);
  }
 w_code("",fp);
 fclose(fp);unlink(MAIL_SAVE_FILENAME);
 link(mail_save_filename_tmp,MAIL_SAVE_FILENAME);
 unlink(mail_save_filename_tmp);
}

static char *check_flags(sptr,nick,newnick,msgclient,local_server,
                         first_tnl,last_tnl,first_tnil,last_tnil,
                         send,repeat,gnew,mode,reserved)
aClient *sptr;
aMsgClient *msgclient;
char *nick,*newnick;
int local_server,first_tnl,last_tnl,first_tnil,
    last_tnil,*send,*repeat,*gnew,mode,*reserved;
{
 char *prev_nick,*new_nick,*c,mbuf[MSG_LEN+3],buf[BUF_LEN+3],
      ebuf[BUF_LEN+3],abuf[20],*sender,*message,wmode[2];
 long prev_clock,new_clock,clock,time_l,gflags,timeout = 0;
 int t,t1,first,last,exception,secret = 0, 
     hidden = 0, remove_last = 0;
 aClient *cptr;
 aChannel *chptr;
 aMsgClient **index_p;
 struct tm *tm;

 if (SecretChannel(sptr->user->channel)) secret = 1;
 if (secret || !sptr->user->channel) hidden = 1; 
 chptr = find_channel("+MAIL", (char *) 0);
 wmode[1] = 0; *send = 1; *repeat = 0; *gnew = 0; 
 message = msgclient->message;
 if (!local_server && msgclient->flags & FLAGS_SEND_ONLY_IF_LOCALSERVER
     || !MyConnect(sptr) && msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER
     || (msgclient->flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME 
         && mode == 'a' && StrEq(nick,sptr->user->username))
     || (!IsOper(sptr) &&
         msgclient->flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER)
     || msgclient->flags & FLAGS_SEND_ONLY_IF_MAIL_CHANNEL &&
        (!sptr->user->channel || !chptr || !IsMember(sptr,chptr))
     || msgclient->flags & FLAGS_SEND_ONLY_IF_SIGNONOFF && mode != 'a'
        && mode != 'e' && !(msgclient->flags & FLAGS_WHOWAS)) { 
     *send = 0;*repeat = 1;return message;
  }
 if (!send_flag(msgclient->flags)) *send = 0;
 if (msgclient->flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) {
     c = message;
     for(;;) {
         exception = 0;t = t1 = 0 ;ebuf[0] = 0;
         while (*c != ' ') {
                if (!*c || t > BUF_LEN) goto break2;
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
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER)
     for (cptr = client;cptr;cptr = cptr->next)
          if (cptr->user && 
              (!mycmp(cptr->name,msgclient->fromnick)
               || msgclient->flags & FLAGS_ALL_NICK_VALID)
              && !Usermycmp(cptr->user->username,msgclient->fromname)
              && host_check(cptr->user->host,msgclient->fromhost,'l')
              && cptr != sptr) {
              sprintf(buf,"%s@%s",sptr->user->username,sptr->user->host);
              switch (mode) {
		case 'r' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                               && !only_wildcards(msgclient->tonick)
                               && !secret) {
                                sendto_one(cptr,
                                           "NOTICE %s :*** %s (%s) %s: %s",
                                            cptr->name,
                                            hidden ? msgclient->tonick : nick,
                                            buf,"appears in the distance",
					    message);
				msgclient->flags &= 
                                           ~FLAGS_NEWNICK_SECRET_DISPLAYED;
				msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
		              }
                           break;
		case 'a' : sendto_one(cptr,"NOTICE %s :*** %s (%s) %s: %s",
                                      cptr->name,
                                      hidden ? msgclient->tonick : nick,buf,
                                      "signs on",message);
                           msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                           break;
		case 'c' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                               && !only_wildcards(msgclient->tonick)
                               && !secret) {
                                sendto_one(cptr,
                                           "NOTICE %s :*** %s (%s) %s: %s",
					   cptr->name,
					   hidden ? msgclient->tonick : nick,
					   buf,"is here",message);
                                msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                            }
                           break;
                case 'e' : if (!secret)
                                sendto_one(cptr,
					   "NOTICE %s :*** %s (%s) %s: %s",
					   cptr->name,
					   hidden ? msgclient->tonick:nick,
					   buf,"left",message);
                           break;
	        case 'n' : 
                         msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED;
                         if (secret) {
                          if (msgclient->flags & FLAGS_NEWNICK_SECRET_DISPLAYED
                              || only_wildcards(msgclient->tonick)) 
                              goto break2b;
			    msgclient->flags |= FLAGS_NEWNICK_SECRET_DISPLAYED;
		          } else {
                                if (!matches(msgclient->tonick,newnick))
                                    msgclient->flags |= 
                                               FLAGS_NEWNICK_DISPLAYED;
                                    msgclient->flags &= 
                                               ~FLAGS_NEWNICK_SECRET_DISPLAYED;
			       }
                        sendto_one(cptr,"NOTICE %s :*** %s (%s) %s <%s> %s",
                                   cptr->name,secret ? msgclient->tonick:nick,
                                   buf,"changed name to",
                                   secret ? "something secret" : newnick,
                                   message);
               }
           break2b:;
         }
 while (msgclient->flags & FLAGS_WHOWAS) {
        gflags = 0;time(&clock); new_clock = prev_clock = clock;
        time_l = clock;start_day(&time_l,'l');
        if (*message)
            if (*message == '+') 
                timeout = atoi(message+1)*24*3600+clock;
              else timeout = atoi(message)*3600+clock;
        if (timeout <= 0) timeout = msgclient->timeout;
        new_nick = prev_nick = nick;
        for (t = first_tnl;t <= last_tnl;t++) 
             if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS
                 && ToNameList[t]->timeout > clock
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
                     *gnew = 2;clock = ToNameList[t]->time;
		     prev_nick = ToNameList[t]->tonick;
                     prev_clock = atol(ToNameList[t]->message);
		     timeout = ToNameList[t]->timeout;
                     if (clock < time_l && 
                         msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY) {
			 *gnew = 3;clock = time_l;
		      }
		     if ((secret || mode == 'e')
                         && *ToNameList[t]->passwd == 'a' 
		         && new_clock-prev_clock < 60) {
                        if (prev_clock == clock) {
                            ToNameList[t]->timeout = 0;break;
		  	 }
                        new_clock = prev_clock = atol(ToNameList[t]->fromhost);
                        new_nick = prev_nick = ToNameList[t]->fromnick;
		     } else if (secret && *ToNameList[t]->passwd == '*') break;
                 ToNameList[t]->timeout = 0;
                 remove_last = 1;break;
	     } 
        if (!remove_last && (secret || mode == 'e')) break; 
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_WHOWAS;
        gflags |= FLAGS_NAME;
        if (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY)
         gflags |= FLAGS_WHOWAS_LOG_EACH_DAY;
        if (msgclient->flags & FLAGS_REGISTER_NEWNICK)
         gflags |= FLAGS_REGISTER_NEWNICK;
        if (IsOper(sptr)) gflags |= FLAGS_OPER;
        if (MyConnect(sptr)) gflags |= FLAGS_SEND_ONLY_IF_THIS_SERVER;
        if (mode == 'a') *wmode = 'a';
         else if (mode == 'e' || secret
                  || mode == 'n' && mycmp(nick,newnick)) *wmode = '*'; 
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
 if (mode == 'e' || mode == 'n') { 
     *repeat = 1;*send = 0;
     return message;
  }
 while ((msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION
        || msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)) {
        if (*message && matches(message,sptr->info) || secret) {
           *repeat = 1; break;
	 }
        sprintf(mbuf,"Match for %s %s@%s (%s) is: %s %s@%s (%s)",
                msgclient->tonick,msgclient->toname,msgclient->tohost,
                *msgclient->message ? message : "*", 
                hidden ? msgclient->tonick : nick,
                sptr->user->username,sptr->user->host,sptr->info);
        if (msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)
            for (cptr = client; cptr; cptr = cptr->next)
                if (cptr->user &&
                    (!mycmp(cptr->name,msgclient->fromnick)
                     || msgclient->flags & FLAGS_ALL_NICK_VALID)
                    && !Usermycmp(cptr->user->username,msgclient->fromname)
                    && host_check(cptr->user->host,msgclient->fromhost,'l')) {
                    sendto_one(cptr,"NOTICE %s :*** %s",cptr->name,mbuf);
                    break;
   	        }
        if (!(msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION)) break;
        t1 = 0; t = first_tnl_indexnode(msgclient->fromname);
        last = last_tnl_indexnode(msgclient->fromname);
        while (last && t <= last) {
              if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_NOTICE &&
                  !mycmp(ToNameList[t]->tohost,
                         local_host(msgclient->fromhost))) {
                  t1++;
                  if (!mycmp(ToNameList[t]->message,mbuf)) {
                     t1 = mail_mum; break;
		   }
	       }	  
              t++;
          }  
        if (t1 >= mail_mum) break;
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
            gflags,mail_mst*3600+clock,clock,mbuf);
        break;
  }
 if (msgclient->flags & FLAGS_REPEAT_UNTIL_TIMEOUT) *repeat = 1;
 if (!local_server && 
     msgclient->flags & FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER) {
     msgclient->flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER; 
     msgclient->flags |= FLAGS_SEND_ONLY_IF_LOCALSERVER;
     *repeat = 1;
  }
 while (*send && (msgclient->flags & FLAGS_NOTICE_RECEIVED_MESSAGE
        || msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED)) {
        sprintf(buf,"%s %s@%s has received mail",
                hidden ? msgclient->tonick :
                nick,sptr->user->username,sptr->user->host);
        if (msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED)
            for (cptr = client; cptr; cptr = cptr->next)
                 if (cptr->user &&
                    (!mycmp(cptr->name,msgclient->fromnick)
                     || msgclient->flags & FLAGS_ALL_NICK_VALID)
                     && !Usermycmp(cptr->user->username,msgclient->fromname)
                     && host_check(cptr->user->host,msgclient->fromhost,'l')) {
                     sendto_one(cptr,"NOTICE %s :*** %s dated %s",
                                cptr->name,buf,myctime(msgclient->time));
                     break;
		 }
       if (!(msgclient->flags & FLAGS_NOTICE_RECEIVED_MESSAGE)) break;
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
           gflags,mail_mst*3600+clock,clock,buf);
       break;
    }
 if (msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE
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
        if (*gnew) last_tnl = last_tnl_indexnode(sptr->user->username);
        for (t = first_tnl;t <= last_tnl;t++)
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

void check_messages(cptr,sptr,tonick,mode)
aClient *cptr,*sptr;
char *tonick,mode;
{
 aMsgClient *msgclient,**index_p;
 char dibuf[40],*name,*newnick,*message,*sender;
 int last,first_tnl,last_tnl,first_tnil,last_tnil,
     send,local_server = 0,t,repeat,gnew,reserved = 0;
 long clock,flags;

 if (!sptr->user) return;
 if (mode != 'e' && SecretChannel(sptr->user->channel)
     && StrEq(tonick,"IRCmail")) {
	 sender = sptr->user->server[0] ? me.name : tonick;
         t = number_whowas();
         sendto_one(sptr,":%s NOTICE %s : <%s> %s (Q/%d W/%d) %s",
                    sender,tonick,me.name,VERSION,fromname_index - t,t,
                    StrEq(ptr,"IRCmail") ? "" : (!*ptr ? "-" : ptr));
      }
 if (!fromname_index) return;
 time(&clock);
 if (clock > old_clock+mail_msf) {
    save_messages();old_clock = clock;
  }
 if (mode == 'n') {
     newnick = tonick;tonick = sptr->name;
  }
 if (host_check(sptr->user->server,sptr->user->host,'e')) local_server = 1;
 name = sptr->user->username;
 t = first_tnl = first_tnl_indexnode(name);
 last = last_tnl = last_tnl_indexnode(name);
 first_tnil = first_tnil_indexnode(tonick);
 last_tnil = last_tnil_indexnode(tonick);
 index_p = ToNameList;
 while(1) {
    while (last && t <= last) {
           msgclient = index_p[t];gnew = 0;repeat=1;
           if (mode == 'a') {
               msgclient->flags &= ~FLAGS_NEWNICK_SECRET_DISPLAYED; 
               msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED; 
	   }
          if (!matches(msgclient->tonick,tonick)
               && !matches(msgclient->toname,name)
               && !matches(msgclient->tohost,sptr->user->host)) {
               message = check_flags(sptr,tonick,newnick,msgclient,
                                     local_server,first_tnl,last_tnl,
                                     first_tnil,last_tnil,
                                     &send,&repeat,&gnew,mode,&reserved);
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
	    }
           if (gnew)
               if (index_p == ToNameList) t = msgclient->InToName;
                   else if (index_p == ToNickList) t = msgclient->InToNick;
           if (clock > msgclient->timeout) repeat = 0;
           if (!repeat) remove_msg(msgclient);
           if (gnew || !repeat) {
               first_tnl = first_tnl_indexnode(name);
               last_tnl = last_tnl_indexnode(name);
               first_tnil = first_tnil_indexnode(tonick);
               last_tnil = last_tnil_indexnode(tonick);
               if (index_p == ToNameList) last=last_tnl;
 		else if (index_p == ToNickList) last=last_tnil;
                 else last = wildcard_index;      
           }           
           if (repeat) t++;
       }
     if (index_p == WildCardList) {
         if (mode == 'n') {
             mode = 'c';tonick = newnick;
             first_tnil = first_tnil_indexnode(tonick);
             last_tnil = last_tnil_indexnode(tonick);
             t = first_tnl;last = last_tnl;
             index_p = ToNameList;
	  } else break;
      } else if (index_p == ToNameList) {
                 index_p = ToNickList;
                 t = first_tnil;last = last_tnil;
              } else {   
                      index_p = WildCardList;
                      t = 1;last = wildcard_index;
                  }
  }
}

static void msg_remove(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient;
 int removed = 0,t,last,id;
 long clock,flags,time_l;
 char *passwd,*tonick,*toname,dibuf[40],*id_s,
      tohost[BUF_LEN+3],*flag_s,*time_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 time_s = parc > 4 ? parv[4] : NULL;
 id_s = parc > 5 ? parv[5] : NULL;
 if (!(flag_s = passwd_check(sptr,passwd)) 
     || ttnn_error(sptr,tonick,toname)) return;
 if (!time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(toname,tohost);
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
        set_flags(sptr,flag_s,&flags,'d',"");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,id)) {
            display_flags(msgclient->flags,dibuf),
            sendto_one(sptr,"NOTICE %s :*** Removed -> %s %s %s@%s",
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
             "Mail save frequency:",
             "Save frequency may not be like that...",
             "Mail save frequency is set to:" 
          };

 if (value) {
    max = atoi(value);
    if (!IsOperHere(sptr))
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,message[l+1]);
     else { 
           if (!max && (msg == &mail_mst || msg == &mail_msf)) max = 1;
           *msg = max;if (msg == &mail_msf) *msg *= 60;
           sendto_one(sptr,"NOTICE %s :*** %s %d",sptr->name,message[l+2],max);
           changes_to_save = 1;
       }
 } else {
         max = (*msg);if (msg == &mail_msf) max /= 60;
         sendto_one(sptr,"NOTICE %s :*** %s %d",sptr->name,message[l],max);
     }
}

static void msg_stats(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 long clock;
 char buf[BUF_LEN],*arg,*value,*fromnick,*fromname,*fromhost = NULL;
 int tonick_wildcards = 0,toname_wildcards = 0,tohost_wildcards = 0,any = 0,
     nicks = 0,names = 0,hosts = 0,t = 1,last = fromname_index,flag_notice = 0,
     flag_whowas = 0,flag_destination = 0;
 aMsgClient *msgclient;

 arg = parc > 1 ? parv[1] : NULL;
 value = parc > 2 ? parv[2] : NULL;
 if (arg) {
     if (!mycmp(arg,"MSM")) setvar(sptr,&mail_msm,0,value); else 
     if (!mycmp(arg,"MSW")) setvar(sptr,&mail_msw,3,value); else
     if (!mycmp(arg,"MUM")) setvar(sptr,&mail_mum,6,value); else
     if (!mycmp(arg,"MUW")) setvar(sptr,&mail_muw,9,value); else
     if (!mycmp(arg,"MST")) setvar(sptr,&mail_mst,12,value); else
     if (!mycmp(arg,"MSF")) setvar(sptr,&mail_msf,15,value); else
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
                if (msgclient->flags & FLAGS_WILDCARD) 
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
                       mail_mst = MAIL_MAXSERVER_TIME,
                       mail_mum = MAIL_MAXUSER_MESSAGES,
                       mail_msm = MAIL_MAXSERVER_MESSAGES,
                       mail_msw = MAIL_MAXSERVER_WILDCARDS,
                       mail_muw = MAIL_MAXUSER_WILDCARDS;
                       mail_msf = MAIL_SAVE_FREQUENCY*60;
                       sendto_one(sptr,"NOTICE %s :*** %s",
                                  sptr->name,"Stats have been reset");
                       changes_to_save = 1;
		   }
	    }
  } else {
      t = number_whowas(); 
      sprintf(buf,"%s%d /%s%d /%s%d /%s%d /%s%d /%s%d /%s%d",
              "QUEUE:",fromname_index - t,
              "MSM(dynamic):",fromname_index - t < mail_msm ? mail_msm : 
                              fromname_index - t,
              "MSW:",mail_msw,
              "MUM:",mail_mum,
              "MUW:",mail_muw,
              "MST:",mail_mst,
              "MSF:",mail_msf/60);
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
    }
}

static void msg_send(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient **index_p,*msgclient;
 int sent_wild = 0,sent = 0,t,last,savetime,nr_whowas;
 long clock,timeout,flags = 0;
 char buf1[BUF_LEN],buf2[BUF_LEN],dibuf[40],*c,*passwd,*null_char = "",
      *tonick,*toname,*timeout_s,*message,tohost[BUF_LEN+3],*flag_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 timeout_s = parc > 4 ? parv[4] : NULL;
 message = parc > 5 ? parv[5] : NULL;
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
		sendto_one(sptr,"NOTICE %s :*** %s %s@%s %s %s",sptr->name,
			   msgclient->fromnick,msgclient->fromname,
			   msgclient->fromhost,
                           "doesn't allow you to use SEND:",
                           msgclient->message);
		return;
	    }
            t++;
	}
     if (index_p == WildCardList) break;
     if (index_p == ToNameList) {
         index_p = ToNickList;
         t = first_tnil_indexnode(sptr->name);
         last = last_tnil_indexnode(sptr->name);
      } else {
              index_p = WildCardList;
              t = 1;last = wildcard_index;
         }
 }
 if (fromname_index - number_whowas() >= mail_msm 
     && !IsOperHere(sptr) && !Key(sptr)) {
     if (!mail_msm || !mail_mum)
         sendto_one(sptr,"ERROR :The mailsystem is closed for no-operators");
      else sendto_one(sptr,"ERROR :No more than %d message%s %s",
                      mail_msm,mail_msm < 2?"":"s",
                      "allowed in the server");
     return;
  }
 if (clock > old_clock+mail_msf) {
    save_messages();old_clock = clock;
  }
 if (!(flag_s = passwd_check(sptr,passwd)) 
     || ttnn_error(sptr,tonick,toname)) return;
 if (!set_flags(sptr,flag_s,&flags,'s',"\n")) return;
 split(toname,tohost);
 if (find_chars('*','?',toname)) {
     if (find_chars('*','?',tonick)) flags |= FLAGS_WILDCARD;
      else flags |= FLAGS_NICK;
  } else flags |= FLAGS_NAME;            
 if (IsOper(sptr)) flags |= FLAGS_OPER;
 if (BadPtr(timeout_s)) {
     sendto_one(sptr,"ERROR :No hours specified");
     return;
  }
 if (*timeout_s == '+') timeout = atoi(timeout_s+1) * 24;
  else timeout = atoi(timeout_s);
 if (timeout > mail_mst && !(flags & FLAGS_OPER) && !Key(sptr)) {
    sendto_one(sptr,"ERROR :Max time allowed is %d hour%s",
               mail_mst,mail_mst>1?"":"s");
    return;
  }
 if (timeout < 1) {
    for (c = timeout_s;*c;c++)
         if (!isdigit(*c)) {
             sendto_one(sptr,"ERROR :Missing argument");
             return;
          }
     timeout = 1;
  }
 if (BadPtr(message)) {
    if (!send_flag(flags)) message = null_char; 
     else {
           sendto_one(sptr,"ERROR :No message specified");
           return;
       }
  }
 if (strlen(message)>MSG_LEN) {
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
                 if (FromNameList[t]->flags & FLAGS_WILDCARD) sent_wild++;  
              }
                 
          }
         t++;
       }
     if (sent >= mail_mum) {
        sendto_one(sptr,"ERROR :No more than %d message%s %s",
                mail_mum,mail_mum < 2?"":"s",
                "for each user allowed in the server");
        return;
      }
     while (flags & FLAGS_WILDCARD) {
         if (!mail_msw || !mail_muw)
            sendto_one(sptr,"ERROR :No-operators are not allowed %s",
                       "to send to nick and username with wildcards");
          else if (wildcard_index >= mail_msw) 
                   sendto_one(sptr,"ERROR :No more than %d msg. %s %s",
                              mail_msw,"with nick/username w.cards",
                              "allowed in the server");
          else if (sent_wild >= mail_muw) 
                   sendto_one(sptr,"ERROR :No more than %d %s %s",
                              mail_muw,mail_muw < 2?"sessage":"messages with",
                              "nick/username w.cards allowed per user");
          else break;
          return;
     }
 }
 time(&clock);
 new(passwd,sptr->name,sptr->user->username,sptr->user->host,
     tonick,toname,tohost,flags,timeout*3600+clock,clock,message);
 display_flags(flags,dibuf);
 sprintf(buf1,"%s %s %s@%s / %s%d / %s%s",
         "Queued ->",tonick,toname,tohost,
         "Max.hours:",timeout,"Flags:",dibuf);
 *buf2 = 0; if (!(flags & FLAGS_NOT_SAVE)) {
              savetime = (mail_msf-clock+old_clock)/60;
              sprintf(buf2,"%s%d","Save.min:",savetime);        
           }   
 sendto_one(sptr,"NOTICE %s :*** %s %s",sptr->name,buf1,buf2);
}

static void msg_list(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient;
 int number = 0,t,last,ls = 0,count = 0,id;
 long clock,flags,time_l;
 char *passwd,*message,tohost[BUF_LEN+3],*arg,*tonick,*id_s,
      *toname,buf[BUF_LEN],dibuf[40],*flag_s,*time_s,*pw="*";

 arg = parc > 0 ? parv[0] : NULL;
 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 time_s = parc > 4 ? parv[4] : NULL;
 id_s = parc > 5 ? parv[5] : NULL;

 if (MyEq(arg,"ls")) ls = 1; else
 if (MyEq(arg,"count")) count = 1; else
 if (matches(arg,"cat")) {
     sendto_one(sptr,"ERROR :No such option: %s",arg); 
     return;
  }
 if (!passwd) passwd = pw;
 if (!(flag_s = passwd_check(sptr,passwd))) return;
 if (!time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(toname,tohost);
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
                        tonick,toname,tohost,time_l,id)) {
            if (arg && !ls) message = msgclient->message;
             else message = NULL;
            if (!count) {
                display_flags(msgclient->flags,dibuf),
                sprintf(buf,">%.2f< %s",(float)(msgclient->timeout-clock)/3600,
                        myctime(msgclient->time));
                sendto_one(sptr,"NOTICE %s :-> %d %s %s %s@%s %s: %s",
                           sptr->name,msgclient->id,dibuf,msgclient->tonick,
                           msgclient->toname,msgclient->tohost,buf,
                           message ? message:(*msgclient->message ? "...":""));
             }
            number++;
	 };
        t++;
     }
 if (!number) sendto_one(sptr,"ERROR :No such message(s) found");
  else if (count) sendto_one(sptr,"NOTICE %s :*** %s %s %s@%s: %d",
                             sptr->name,"Number of messages to",
                             tonick?tonick:"*",toname?toname:"*",
                             *tohost?tohost:"*",number);
}

static void msg_flag(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient;
 int flagged = 0,t,last,id;
 long clock,flags;
 char *passwd,*tonick,*toname,*newflag_s,tohost[BUF_LEN+3],
      dibuf1[40],dibuf2[40],*flag_s,*id_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 newflag_s = parc > 4 ? parv[4] : NULL;
 id_s = parc > 5 ? parv[5] : NULL;
 if (!newflag_s) {
     sendto_one(sptr,"ERROR :No flag changes specified");
     return;
  }
 if (!(flag_s = passwd_check(sptr,passwd)) 
     || ttnn_error(sptr,tonick,toname)) return;
 if (!set_flags(sptr,newflag_s,&flags,'c',"in changes")) return;
 split(toname,tohost);
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
                sendto_one(sptr,"NOTICE %s :*** %s -> %s %s %s@%s",sptr->name,
                           "No flag change for",dibuf1,msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
             else                                        
                sendto_one(sptr,"NOTICE %s :*** %s -> %s %s %s@%s to %s",
                          sptr->name,"Flag change",dibuf1,msgclient->tonick,
                          msgclient->toname,msgclient->tohost,dibuf2);
           flagged++;
        } 
       t++;
   }
 if (!flagged) sendto_one(sptr,"ERROR :No such message(s) found");
}

static void msg_sent(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient,*next_msgclient;
 char *arg,*fromnick,*fromname,*time_s,fromhost[BUF_LEN+3],*delete; 
 int number = 0,t,last,nick = 0,count = 0,users = 0;
 long clock,time_l;

 arg = parc > 1 ? parv[1] : NULL;
 fromnick = parc > 2 ? parv[2] : NULL;
 fromname = parc > 3 ? parv[3] : NULL;
 time_s = parc > 4 ? parv[4] : NULL;
 delete = parc > 5 ? parv[5] : NULL;

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
    split(fromname,fromhost);time(&clock);
    for (t=1;t <= fromname_index;t++) {
         msgclient = FromNameList[t];
         next_msgclient = t < fromname_index ? FromNameList[t+1] : NULL;
         if (msgclient->timeout > clock
             && !(msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)
             && (!time_l || msgclient->time >= time_l 
                 && msgclient->time < time_l+24*3600)
             && (!fromnick || !matches(fromnick,msgclient->fromnick))
             && (!fromname || !matches(fromname,msgclient->fromname))
             && (!*fromhost || !matches(fromhost,msgclient->fromhost))
             && (!delete || !mycmp(delete,"RM")
                 || !mycmp(delete,"RMAB") &&
                    (msgclient->flags & FLAGS_WHOWAS ||
                     msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE))) {
             if (delete || !next_msgclient
                 || mycmp(next_msgclient->fromnick,msgclient->fromnick)
                 || mycmp(next_msgclient->fromname,msgclient->fromname)     
                 || !(host_check(next_msgclient->fromhost,
                                 msgclient->fromhost,'l'))) {
                  sendto_one(sptr,"NOTICE %s :%s#%d# %s %s@%s (%s)",sptr->name,
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

static nick_prilog(nr_msgclient,clock)
aMsgClient *nr_msgclient;
long clock;
{
 register int t, m_wled, m_rn, i_wled, i_rn, nr, last;
 aMsgClient *msgclient;

 i_wled = nr_msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY;
 i_rn = nr_msgclient->flags & FLAGS_REGISTER_NEWNICK;
 nr = nr_msgclient->InToName;
 t = first_tnl_indexnode(nr_msgclient->toname);
 last = last_tnl_indexnode(nr_msgclient->toname);
 while (t && t <= last) {
        msgclient = ToNameList[t];
        if (t != nr && clock <= msgclient->timeout
            && !mycmp(msgclient->tonick,nr_msgclient->tonick)
            && !Usermycmp(msgclient->toname,nr_msgclient->toname)
            && host_check(msgclient->tohost,nr_msgclient->tohost,'l')
            && msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS) {
            m_wled = msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY;
            m_rn = msgclient->flags & FLAGS_REGISTER_NEWNICK;
            if (!i_wled && (m_wled || m_rn && !i_rn)
                || i_wled && !i_rn && m_wled && m_rn
                || !i_wled && !m_wled && m_rn == i_rn && t < nr
                || i_rn && m_rn && m_wled && i_wled && t < nr 
                   && same_day(msgclient->time,nr_msgclient->time)) return 0;
	}
       t++;
   }
  return 1;
}

static void msg_whowas(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient,**index_p;
 int any = 0,t,last;
 register int t2;
 long clock,clock2,flags,time1_l,time2_l;
 char *c,*tonick,*toname,tohost[BUF_LEN+3],*time1_s,*time2_s,
      *search = NULL,*flag_s,buf1[BUF_LEN],buf2[BUF_LEN],buf3[BUF_LEN],
      search_sl[BUF_LEN+3],search_sh[BUF_LEN+3],dibuf[40],*delete;

 tonick = parc > 1 ? parv[1] : NULL;
 toname = parc > 2 ? parv[2] : NULL;
 time1_s = parc > 3 ? parv[3] : NULL;
 time2_s = parc > 4 ? parv[4] : NULL;
 delete = parc > 5 ? parv[5] : NULL;
 if (tonick && strlen(tonick) > BUF_LEN) {
    sendto_one(sptr,"ERROR :Nick name can't be longer than %d chars",
               BUF_LEN);
    return;
 }
 if (toname && strlen(tonick) > BUF_LEN) {
    sendto_one(sptr,"ERROR :Name and host can't be longer than %d chars",
               BUF_LEN);
    return;
 }
 if (!tonick) {
    for (t2 = 1;t2 <= fromname_index;t2++)
         if (FromNameList[t2]->flags & FLAGS_WHOWAS) {
            display_flags(FromNameList[t2]->flags,dibuf);
            sendto_one(sptr,"NOTICE %s :*** %s: %s %s %s %s@%s",
                       sptr->name,"Listed from",
                       myctime(FromNameList[t2]->time),dibuf,
                       FromNameList[t2]->tonick,
                       FromNameList[t2]->toname,
                       FromNameList[t2]->tohost);
	         }
    return;
  }
 split(toname,tohost);
 if (!IsOperHere(sptr) && !Key(sptr)
     && only_wildcards(tonick)
     && (!toname || only_wildcards(toname))
     && (!*tohost || only_wildcards(tohost))) {
     sendto_one(sptr,"ERROR :%s %s","Both nick, name and host",
                "can not be specified with only wildcards");
     return;
 }
 if (toname) {
     search = toname; 
     strcpy(search_sl,search);
     strcpy(search_sh,search);
     c = find_chars('*','?',search_sl);if (c) *c = 0;
     c = find_chars('*','?',search_sh);if (c) *c = 0;
     if (!*search_sl) search = 0; 
      else for (c = search_sh;*c;c++); 
     (*--c)++;
  }
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
 time (&clock);
 while (last && t <= last) {
       msgclient = index_p[t];flags = msgclient->flags;
        if (clock > msgclient->timeout) {
            remove_msg(msgclient);last--;
            continue;
         }
        if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS
            && (!time1_l || msgclient->time >= time1_l 
                && msgclient->time < time2_l)
            && (!tonick || !matches(tonick,msgclient->tonick))
            && (!toname || !matches(toname,msgclient->toname))
            && (!*tohost || !matches(tohost,msgclient->tohost))
            && (delete || nick_prilog(msgclient,clock))) {
             any++;clock2 = atol(msgclient->message);
             strcpy(buf2,myctime(clock2));
             strcpy(buf1,myctime(msgclient->time));
             if (msgclient->flags & FLAGS_WHOWAS_LOG_EACH_DAY) {
		       buf1[19] = 0;strcpy(buf3,buf1);buf1[19] = ' ';
		       buf3[19] = ' ';buf3[20] = '-';buf2[19] = 0;
		       strcpy(buf3+21,buf2+10);buf2[19] = ' ';
		       strcpy(buf3+30,buf1+19);
	     } else strcpy(buf3,buf2);
            t2 = 0;
            if (msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER) 
                dibuf[t2++] = ';'; 
             else dibuf[t2++] = ':';
            if (msgclient->flags & FLAGS_OPER) dibuf[t2++] = '*';
             else dibuf[t2++] = ' ';
            dibuf[t2] = 0;
            if (!delete || mycmp(delete,"RMS"))
                sendto_one(sptr,"NOTICE %s :*** %s%s%s %s %s@%s",
                           sptr->name,delete ? "Removed -> " : "",
                           buf3,dibuf,msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
            if (delete) {
                remove_msg(msgclient);last--;
                continue;
	    }
        } 
        t++;
   }
 if (!any) sendto_one(sptr,"ERROR :No such user(s) found");
  else if (delete)
           sendto_one(sptr,"NOTICE %s :*** %d username%s removed",
                      sptr->name,any,any < 2 ? "" : "s");   
}

static void msg_mailhelp(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{ 
  int l = 0,t = 0,s = 0,any = 0;
  char *c,*arg;
  static char *help[] = {
   "START_OF_HELPLIST",
   "  ---- Send suggestions to jarlek@ifi.uio.no or Wizible jarlek@*.uio.no",
   "  MAIL [SEND] <passwd+-<FLAGS>> <tonick> <toname@host> <valid time> <msg>",
   "  MAIL [LS|CAT|COUNT] <passwd+-FLAGS> <tonick>) <toname@host> <time> <id>",
   "  MAIL [RM] <password+-<FLAGS>> <tonick> <toname@host> <time> <id>",
   "  MAIL [FLAG] <pwd+-<FLG>> <tonick> <toname@host> +-<FLAGS> <id>",
   "u MAIL [SENT] [NAME|COUNT]",
   "o MAIL [SENT] [NAME|COUNT|USERS] <f.nick> <f.name@host> <time> [RM|RMAB]",
   "u MAIL [WHOWAS] <nick> <name@host> <time1> <time2>",
   "o MAIL [WHOWAS] <nick> <name@host> <time1> <time2> [RM|RMS]",
   "u MAIL [STATS] [MSM|MSW|MUM|MUW|MST|MSF|USED]",
   "o MAIL [STATS] [MSM|MSW|MUM|MUW|MST|MSF|USED|RESET] <value>",
   "o MAIL [SAVE]",
   "  MAIL [HELP] <option>",
   "SEND",
   "  MAIL [SEND] <passwd+-<FLAGS>> <tonick> <toname@host> <valid time> <msg>",
   "  With SEND you can queue a message in the server, and when the recipient",
   "  signs on/off IRC, change nick or join any channel, mail checks for",
   "  valid messages. This works even if the sender is not on IRC. See HELP",
   "  FLAGS for more info. Password can be up to ten characters long. If a",
   "  wildcard is specified, no password is set. See HELP FLAGS for info",
   "  about flag settings. Toname can be specified without @host. Do not",
   "  use wildcards in both tonick and toname if there is no need for it.",
   "  Valid time before the server automatically remove the message from the",
   "  queue, is specified with hours (or days if a '+' character is specified",
   "  before this timeout.", 
   "  MAIL SEND secret+WN Wizible jarlek@ifi.uio.no +10 Freedom for everybody!",
   "  - is an example of a message sent only to the specified recipient if",
   "  this person is an operator, and after receiving the message, the server",
   "  sends a mail message back to sender to inform about the delivery.",
   "  MAIL SEND secret+XR Anybody * +30 <ctrl-G>",
   "  - is an example which makes the server to tell when Anybody signs",
   "  on/off irc, change nick etc. This process repeats for 30 days.",
   "  MAIL SEND secret+FL * * +9 *accout*",
   "  - is an example which makes the server send a message back if any real-",
   "  name of any user matches *account*. Message is sent back as mail from",
   "  server, or directly as a notice if sender is on IRC at this time.",
   "LS",
   "  MAIL [LS] <passwd+-FLAGS> <tonick> <toname@host> <time> <id>",
   "  Displays tonick/toname/tohost sent from your nick and username without",
   "  the content of the message itself. If no tonick is specified, tonick",
   "  is set to '*'. The same is done if no toname@host is specified.",
   "  Use FLAGS for matching all messages which have the specified flags set",
   "  on or off. See HELP FLAG for more info about flag settings. Time can",
   "  be specified on the form day.month.year or only day, or day/month, and",
   "  separated with one of the three '.,/' characters. You can also specify",
   "  -n for n days ago. Examples: 1.jan-90, 1/1.90, 3, 3/5, 3.may, -4.",
   "  If only '-' and no number is specified valid time is set to all days.",
   "  The time specified is always the local time on your system.",
   "CAT",
   "  MAIL [CAT] <passwd+-FLAGS> <tonick> <toname@host> <time> <id>",
   "  Displays tonick/toname/tohost sent from your nickname and username",
   "  including the content of the message. See HELP LS for more info.",
   "  See HELP FLAG for more info about flag settings.", 
   "COUNT",
   "  MAIL [COUNT] <passwd+-FLAGS> <tonick> <toname@host> <time> <id>",
   "  Displays the number of messages sent from your nick and username. See",
   "  HELP LS for more info. See HELP FLAG for more info about flag setting.", 
   "RM",
   "  MAIL [RM] <password+-<FLAGS>> <tonick> <toname@host> <time> <id>",
   "  Deletes any messages sent from your nick and username which matches",
   "  with tonick and toname@host. Use FLAGS for matching all messages which",
   "  have the specified flags set on or off. See HELP FLAG for more info",
   "  about flag settings, and HELP LS for info about time.", 
   "FLAG",
   "  MAIL [FLAG] <pwd+-<FLG>> <tonick> <toname@host> +-<FLAGS> <id>",
   "  You can add or delete as many flags as you wish with +/-<FLAG>.",
   "  + switch the flag on, and - switch it off. Example: -S+RL",
   "  <pword>[+-]<FLAGS> or <pword> or [+-]<FLAGS> combinations are valid.", 
   "  Following flags with its default set specified first are available:",
   "  -S > Message is never saved. (Else with frequency specified with /MSF)",
   "  -P > Repeat the send of message(s) once.",
   "  -I > Ignore message if recipient is not on a channel named MAIL.",
   "  -Q > Ignore message if recipient's first nick is equal to username.",
   "  -V > Ignore message if recipient is not on a server in same country.",
   "  -M > Ignore message if recipient is not on this server.",
   "  -W > Ignore message if recipient is not an operator.",
   "  -U > Ignore message if recipient is not signing on or off IRC.",
   "  -Y > Repeat and set V flag if reci. is not on a server in same country.",
   "  -N > Let server send a mail message if message is sent to recipient.",
   "  -D > Same as N, but msg. is sent to sender if this is on IRC.",
   "  -R > Repeat the message until timeout.",
   "  -F > Let server mail info for matching recipient(s). Any message",
   "       specify what to match with the realname of the recipient.", 
   "  -L > Same as F, but message is sent to sender if this is on IRC.",
   "  -C > Make sender's nicks be valid in all cases username is valid.",
   "  -X > Let server display if recipient signs on/off IRC or change",
   "       nickname. Any message specified is returned to sender.",
   "  -E > Ignore message if nick, name and host matches the message text",
   "       starting with any number of this format: '<nick> <name>@<host> '",
   "o -A > Generate a whowas list for all matching nick/name/host. Timeout",
   "o      for whowas messages is set identical to this flag message timeout.",
   "o      However, separate timeout is set if specified as a message.",
   "o -J > Whowas list log new time for each user every day.",
   "o -B > Let server generate a header message to matching nick/name/host",
   "o      Message is only sent to each matching recipient once.",
   "o -T > A or B flag list includes nicks for all matching names and hosts.",
   "o -K > Give keys to unlock privileged flags by setting that flags on.",
   "o      The recipient does also get privileges to queue unlimited msg.,",
   "o      list privileged flags and see all stats.",
   "o -Z > Make it impossible for recipient to use SEND option.",
   "  Other flags which are only displayed but can't be set by user:",
   "  -O > Message is sent from an operator.",
   "  -G > Notice message is generated by server.",
   "u -T > Whowas list has included nicks for all matching names and hosts.",
   "u -J > Whowas list log new time for each user every day.",
   "u -B > Broadcasting message sent once to all who matches recipient name.",
   "o -H > Header message generated using flag B.",
   "u Notice: Message is not sent to recipient using F, L, R or X flag.",
   "u Using this flags, no message needs to be specified.", 
   "o Notice: Message is not sent to recip. using F, L, R, X, A, K, Z or H", 
   "o flag (except if B flag is set for R). For this flags, no msg. needed.", 
   "SENT",
   "u MAIL [SENT] [NAME|COUNT]",
   "o MAIL [SENT] [NAME|COUNT|USERS] <f.nick> <f.name@host> <time> [RM|RMAB]",
   "  Displays host and time for messages which are queued without specifying",
   "  any password. If no option is specified SENT displays host/time for",
   "  messages sent from your nick and username.",
   "  NAME displays host/time for messages sent from your username",
   "  COUNT displays number of messages sent from your username",
   "o USERS Displays the number of messages in [], and names for all users",
   "o who have queued any message which matches with spec. nick/name/host.",
   "o RM means that the server removes the messages from the specified user.",
   "o RMAB means same as RM, but only messages with A or B flag is removed.",
   "WHOWAS",
   "u MAIL [WHOWAS] <nick> <name@host> <time1> <time2>",
   "o MAIL [WHOWAS] <nick> <name@host> <time1> <time2> [RM|RMS]",
   "  Displays information about who used the given nickname, username or",
   "  hostname the first and last time (every day or any time) this person",
   "  was on IRC. If a time is specified, only usernames from that day are",
   "  displayed. If another time is specified, all users which are dated from",
   "  time1 to time2 are displayed. See HELP LS for more info about time.",
   "o RM or RMS means that the specified username is deleted. RM displays",
   "o the deleted usernames, but RMS does not display that.",  
   "  If no names are specified, information is displayed about what group",
   "  of users is registered in the whowas system.", 
   "STATS",
   "u MAIL [STATS] [MSM|MSW|MUM|MUW|MST|MSF|USED]",
   "o MAIL [STATS] [MSM|MSW|MUM|MUW|MST|MSF|USED|RESET] <value>",
   "  STATS with no option displays the values of the following variables:",
   "  MSM: Max number of server messages.",
   "  MSW: Max number of server messages with toname-wildcards.",
   "  MUM: Max number of user messages.",
   "  MUW: Max number of user messages with toname-wildcards.",
   "  MST: Max server time.",
   "  MSF: Mail save frequency (checks for save only when an user register)",
   "  Notice that 'dynamic' mark after MSM means that if there are more",
   "  messages in the server than MSM, the current number of messages are",
   "  displayed instead of MSM.",
   "  Only one of this variables are displayed if specified.",
   "o You can change any of the stats by specifying new value after it.",
   "o RESET sets the stats to the same values which is set when starting the",
   "o server daemon if no mail file exist. Notice that this stats are saved in",
   "o same file as the other messages.",
   "SAVE",
   "o MAIL [SAVE]",
   "o SAVE saves all messages with the save flag set. Notice that the ",
   "o messages are automatically saved (see HELP STATS). Each time server is",
   "o restarted, the save file is read and messages are restored. If no users",
   "o are connected to this server when saving, the ID number for each",
   "o message is renumbered.",
   "END_OF_HELPLIST"
 };
 arg = parc > 1 ? parv[1] : NULL;  
 do {
      c = help[l++];if (c[1] == ' ') continue; if (s) break;
      if (!arg || MyEq(arg,c)) s = l;
    } while (!StrEq(c,"END_OF_HELPLIST"));   
 if (s || !arg)
     for (t = s; t<l-1; t++) {
          c = help[t]; any = 1;
          if (*c == ' ' || *c == 'o' && (IsOperHere(sptr) || Key(sptr)) 
              || *c == 'u' && !Key(sptr) && !IsOperHere(sptr))
              sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,c+2);
      }
 if (!any) sendto_one(sptr,"ERROR :No help available for %s",arg);
}

m_mail(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
 char *arg;
 arg = parc > 1 ? parv[1] : NULL;
 if (!IsRegistered(sptr)) { 
	sendto_one(sptr, ":%s %d * :You have not registered as an user", 
		   me.name, ERR_NOTREGISTERED); 
	return -1;
   }
 if (!arg) {
    sendto_one(sptr,"ERROR :No option specified. Try MAIL HELP"); 
    return -1;
  } else
 if (MyEq(arg,"HELP")) msg_mailhelp(sptr,parc-1,parv+1); else
 if (MyEq(arg,"STATS")) msg_stats(sptr,parc-1,parv+1); else
 if (MyEq(arg,"SEND")) msg_send(sptr,parc-1,parv+1); else
 if (MyEq(arg,"LS") || MyEq(arg,"CAT") ||
     MyEq(arg,"COUNT")) msg_list(sptr,parc-1,parv+1); else
 if (MyEq(arg,"SAVE")) msg_save(sptr); else
 if (MyEq(arg,"FLAG")) msg_flag(sptr,parc-1,parv+1); else
 if (MyEq(arg,"SENT")) msg_sent(sptr,parc-1,parv+1); else
 if (MyEq(arg,"WHOWAS")) msg_whowas(sptr,parc-1,parv+1); else
 if (MyEq(arg,"RM")) msg_remove(sptr,parc-1,parv+1); else
  sendto_one(cptr,"ERROR :No such option: %s",arg);
} 
#endif
