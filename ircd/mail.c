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

#define MAIL_SAVE_FREQUENCY 60 /* Frequency of save time in minutes */
#define MAIL_MAXSERVER_TIME 24*28 /* Max hours for a message in the server */
#define MAIL_MAXSERVER_MESSAGES 1000 /* Max number of messages in the server */
#define MAIL_MAXUSER_MESSAGES 200 /* Max number of messages for each user */
#define MAIL_MAXSERVER_WILDCARDS 200 /* Max number of server toname wildcards */
#define MAIL_MAXUSER_WILDCARDS 20 /* Max number of user toname wildcards */

#define MSG_LEN 512
#define BUF_LEN 50
#define LINE_BUF 256
#define REALLOC_SIZE 1024

#define FLAGS_OPER (1<<0)
#define FLAGS_WILDCARD (1<<1)
#define FLAGS_NAME (1<<2)
#define FLAGS_NICK (1<<3)
#define FLAGS_NOT_SAVE (1<<4)
#define FLAGS_REPEAT_ONCE (1<<5)
#define FLAGS_REPEAT_UNTIL_TIMEOUT (1<<6)
#define FLAGS_SERVER_GENERATED_NOTICE (1<<7)
#define FLAGS_SERVER_GENERATED_CORRECT (1<<8)
#define FLAGS_SERVER_GENERATED_WHOWAS (1<<9)
#define FLAGS_SERVER_GENERATED_DESTINATION (1<<10)
#define FLAGS_DISPLAY_IF_RECEIVED (1<<11)
#define FLAGS_DISPLAY_IF_CORRECT_FOUND (1<<12)
#define FLAGS_DISPLAY_IF_DEST_REGISTER (1<<13)
#define FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER (1<<14)
#define FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER (1<<15)
#define FLAGS_NOT_DELETE (1<<16)
#define FLAGS_NOT_SEND (1<<17)
#define FLAGS_SEND_ONLY_IF_LOCALSERVER (1<<18)
#define FLAGS_SEND_ONLY_IF_THIS_SERVER (1<<19)
#define FLAGS_SEND_ONLY_IF_DESTINATION_OPER (1<<20)
#define FLAGS_SEND_ONLY_IF_NICK_NOT_NAME (1<<21)
#define FLAGS_FIND_CORRECT_DEST_SEND_ONCE (1<<22)
#define FLAGS_WHOWAS (1<<23)
#define FLAGS_NOTICE_RECEIVED_MESSAGE (1<<24)
#define FLAGS_RETURN_CORRECT_DESTINATION (1<<25)
#define FLAGS_NEWNICK_DISPLAYED (1<<26)
#define FLAGS_EXTERNAL_IGNORE (1<<27)
#define FLAGS_LOCAL_EXIT (1<<28)
#define FLAGS_IGNORE_EXIT_EXCEPTION (1<<29)


#define DupString(x,y) do { x = MyMalloc(strlen(y)); strcpy(x,y);} while (0)

typedef struct MsgClient {
            char *fromnick,*fromname,*fromhost,*tonick,*toname,*tohost,
                 *message,*passwd;
            long timeout,time,flags;
            int InToNick,InToName,InFromName;
          } aMsgClient;
  
static aMsgClient **ToNameList,**ToNickList,**FromNameList,**WildCardList;
static int mail_mst=MAIL_MAXSERVER_TIME,mail_mum=MAIL_MAXUSER_MESSAGES,
           mail_msm=MAIL_MAXSERVER_MESSAGES,mail_msw=MAIL_MAXSERVER_WILDCARDS,
           mail_muw=MAIL_MAXUSER_WILDCARDS,mail_msf=MAIL_SAVE_FREQUENCY*60,
           wildcard_index=0,toname_index=0,tonick_index=0,fromname_index=0,
           max_tonick,max_toname,max_wildcards,max_fromname,old_clock=0;
static char *ptr = "IRCmail";
extern aClient *client;

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
   "  MAIL [SEND] <passwd+-<FLAGS>> <tonick> <toname@host> <valid hours> <msg>",
   "  MAIL [LS|CAT|COUNT] <passwd+-FLAGS> (<tonick>) (<toname@host>) (<time>)",
   "  MAIL [RM] <password+-<FLAGS>> <tonick> <toname@host> (<time>)",
   "  MAIL [FLAG] <passwd+-<FLAGS>> <tonick> <toname@host> (<time>) +-<FLAGS>",
   "  MAIL [SENT] [NICK|NAME|COUNT]",
   "  MAIL [WHOWAS] <tonick> <toname@host> (<time1>) (<time2>)",
   "u MAIL [STATS] [MSM|MSW|MUM|MUW|MST|MSF]",
   "o MAIL [STATS] ([MSM|MSW|MUM|MUW|MST|MSF|USED|RESET] (<value>))",
   "o MAIL [SAVE]",
   "  MAIL [HELP] <option>",
   "SEND",
   "  MAIL [SEND] <passwd+-<FLAGS>> <tonick> <toname@host> <valid hours> <msg>",
   "  With SEND you can put a message into the server, and when the",
   "  destination user register her/him in this or another server, the",
   "  messages are sent. This works even if the sender is not on IRC.",
   "  Password can be up to ten characters long. If a wildcard is specified,",
   "  no password is set. See HELP FLAGS for info about flag settings.", 
   "  Toname can be specified wihout @host. Wildcards (* or ?) are relatively",
   "  time consuming for the server to search through if used in both tonick",
   "  and toname, so do not use wildcards there if you know the correct",
   "  toname. Valid time before the message(s) is automatically removed from",
   "  the queue is specified with hours. No msg. is needed if T flag is set.",
   "  Example: MAIL SEND secret+S Wizible jarle?@*.uio.no 10 Freedom on earth!",
   "  send a message which is not saved (thus it can be lost if the server",
   "  is restarted), to the specified user.",
   "LS",
   "  MAIL [LS] <passwd+-FLAGS> (<tonick>) (<toname@host>) (<time>)",
   "  Shows tonick/toname/tohost sent from your nick and username without",
   "  the content of the message itself. If no tonick is specified, tonick",
   "  is set to '*'. The same is done if no toname@host is specified.",
   "  Use FLAGS for matching all messages which have the specified flags set",
   "  on or off. See HELP FLAG for more info about flag settings. Time can",
   "  be specified on the form day.month.year or only day, or day/month, and",
   "  separated with one of the three '.,/' characters. You can also specify",
   "  -n for n days ago. Examples: 1.jan-90, 1/1.90, 3, 3/5, 3.may, -4.",
   "CAT",
   "  MAIL [CAT] <passwd+-FLAGS> (<tonick>) (<toname@host>) (<time>)",
   "  Shows tonick/toname/tohost sent from your nickname and username",
   "  including the content of the message itself. See HELP LS for more info.",
   "  Use FLAGS for matching all messages which have the specified flags set",
   "  on or off. See HELP FLAG for more info about flag settings.", 
   "COUNT",
   "  MAIL [COUNT] <passwd+-FLAGS> (<tonick>) (<toname@host>) (<time>)",
   "  Shows the number of messages sent from your nickname and username.",
   "  Use FLAGS for matching all messages which have the specified flag(s)",
   "  set on or off. See HELP FLAG for more info about flag settings.", 
   "RM",
   "  MAIL [RM] <password+-<FLAGS>> <tonick> <toname@host> (<time>)",
   "  Deletes any messages sent from your nick and username which matches",
   "  with tonick and toname@host. Use FLAGS for matching all messages which",
   "  have the specified flags set on or off. See HELP FLAG for more info",
   "  about flag settings, and HELP LS for info about time.", 
   "FLAG",
   "  MAIL [FLAG] <passwd+-<FLAGS>> <tonick> <toname@host> (<time>) +-<FLAGS>",
   "  You can add  as many flags as you wish with +/- FLAG.",
   "  + switch the flag on, and - switch it off. Example: -S+RL",
   "  Password+-flags or password or +-flags combinations are legal.", 
   "  Following flags with its default set specified first are available:",
   "  -S > Message is not saved. (Else with frequency specified with /MSF)",
   "  -P > Repeat the send of message(s) one and only one time.",
   "  -T > Message is not sent.",
   "  -Q > Send/show only if dest. register with nick diff. from username.",
   "  -V > Send/show only if dest. is on a server in same country.",
   "  -M > Send/show only if destination is on this server.",
   "  -W > Send/show only if destination is an operator.",
   "  -Y > Repeat and set V flag if dest. is not on a server in same country.",
   "  -U > Repeat and set M flag if dest. is not on this server.",
   "  -N > Let server send a notice-message if message is sent to dest.",
   "  -D > Same as N, but message is instead sent to sender if this is on IRC",
   "  -F > Let server send correct address for destination if this is found.",
   "  -L > Same as F, but message is instead sent to sender if this is on IRC",
   "  -X > Let server show if dest. enter, quit IRC or change nickname",
   "u -R > Repeat the message until timeout. Notice that P flag is set.", 
   "o -R > Repeat the message until timeout.",
   "o -J > Message will never be deleted.",
   "o -A > Generate a whowas list for all matching nick/name/host. Timeout",
   "o      for whowas messages is set identical to the flag A message timeout",
   "o      However, separate timeout will be set if specified as a message.",
   "o -B > Generate a header message to matching nick/name/host with T",
   "o      flag set if correct address is found.",
   "o -E > Exit an user from this server if the user is directly connected",
   "o      to this server. Works in the principle as the I flag.",
   "o -I > Ignore an user on this server. After the message is sent the user", 
   "o      is exited from this server, but still exist in others. In this", 
   "o      way, no messages from this user are recieved. Including E flag.",
   "o      Warning: Be careful using E or I flag...", 
   "o -Z > Exceptions for an user which the I or E flags is not valid for.",
   "u Notice: T flag is always set using F, L or X flag.", 
   "o Notice: T flag is always set using F, L, X, E, I or Q flag.", 
   "u         If using F or L flag, no message may be specified. However",
   "o         If using F,L,E or I flag, no message may be specified. However",
   "          if a message is specified the message will specify what", 
   "          to match with the realname of the destination user.",
   "  Other flags which are only displayed but can't be set:",
   "  -O > Message is sent from an operator.",
   "  -G > Message is generated using flag N.",
   "  -C > Message is generated using flag F.",
   "u -E > User can't connect to this server.",
   "u -I > Broadcasting message sent once to all who matches destination.",
   "u -I > User is ignored by this server. This include also the E flag.",
   "o -H > Message is generated using flag B.",
   "See HELP LS for info about time.",
   "SENT",
   "  MAIL [SENT] ([NAME|COUNT])",
   "  Shows host and time for messages which is queued without specifying",
   "  any password. If no option is specified SENT shows host/time for",
   "  messages sent from your nick and username.",
   "  NAME shows host/time for messages sent from your username",
   "  COUNT shows number of messages sent from your username",
   "WHOWAS",
   "  MAIL [WHOWAS] <tonick> <toname@host> (<time1>) (<time2>)",
   "  Shows information about who used the given nickname, username or",
   "  hostname first and last time each day. Only operators can start",
   "  specifying tonick and toname with wildcards. This is because the",
   "  mail system use binaer search if possible, thus start with wildcards",
   "  would result in that the server has to search through all usernames...",
   "  If one time is specified, only messages from that day will be displayed.",
   "  Else if another time is specified, all messages which is dated from",
   "  time one to time two is displayed.",  
   "STATS",
   "u MAIL [STATS] ([MSM|MSW|MUM|MUW|MST|MSF])",
   "o MAIL [STATS] ([MSM|MSW|MUM|MUW|MST|MSF|USED|RESET] (<value>))",
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
   "o You can change any of them by specifying new value after it.",
   "o If option USED is specified, the number of fromnicks, fromnames,",
   "o tonicks and tonames are displayed. RESET sets the stats to the values", 
   "o specified first in this file. Notice that stats is saved using SAVE",
   "o or when the messages are automatically saved.",
   "SAVE",
   "o MAIL [SAVE]",
   "o SAVE saves all messages with the save flag set. Notice that the ",
   "o messages is automatically saved (see HELP STATS)",
   "END_OF_HELPLIST"
 };
 arg = parc > 1 ? parv[1] : NULL;  
 do {
      c = help[l++];if (c[1] == ' ') continue; if (s) break;
      if (BadPtr(arg) || !matches(arg,c)) s = l;
    } while (!StrEq(c,"END_OF_HELPLIST"));   
 if (s || BadPtr(arg))
     for (t = s; t<l-1; t++) {
          c = help[t]; any = 1;
          if (*c == ' ' || *c == 'o' && 
              IsOper(sptr) || *c == 'u' && !IsOper(sptr))
              sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,c+2);
      }
 if (!any) sendto_one(sptr,"ERROR :No help available for %s",arg);
}

static char *MyMalloc(size)
int size;
{
  char *ret = (char *) malloc(size+1);
  if (!ret) {
     perror("malloc");
     debug(DEBUG_FATAL,"Out of memory: restarting server...");
     restart();
  }
  return ret;
}

static char *MyRealloc(ptr,size)
char *ptr;
int size;
{
  char *ret = (char *) realloc(ptr,(unsigned int)size+1);
  if (!ret) {
     perror("malloc");
     debug(DEBUG_FATAL,"Out of memory: restarting server...");
     restart();
  }
  return ret;
}

static char *find_chars(c1,c2,string)
char c1,c2,*string;
{
 register char *c;

 for (c = string; *c; c++) if (*c == c1 || *c == c2) return(c);
 return 0;
}

static strcasecmp(s1, s2)
char *s1, *s2;
{
 static unsigned char charmap[] = {
        '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
        '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
        '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
        '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
        '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
        '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
        '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
        '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
        '\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
        '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
        '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
        '\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
        '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
        '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
        '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
        '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
        '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
        '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
        '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
        '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
        '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
        '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
        '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
        '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
        '\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
        '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
        '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
        '\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
        '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
        '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
        '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
        '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
   };
 register unsigned char *cm = charmap,
                   *us1 = (unsigned char *)s1,
                   *us2 = (unsigned char *)s2;

 while (cm[*us1] == cm[*us2++])
        if (*us1++ == '\0') return(0);
 return(cm[*us1] - cm[*--us2]);
}


static first_tnl_indexnode(toname)
char *toname;
{
 int s,t = toname_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNameList[s]->toname,toname)<0) b = s; else t = s;
 return t;
}

static last_tnl_indexnode(toname)
char *toname;
{
 int s,t = toname_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNameList[s]->toname,toname)>0) t = s; else b = s;
 return b;
}

static first_tnil_indexnode(tonick)
char *tonick;
{
 int s,t = tonick_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNickList[s]->tonick,tonick)<0) b = s; else t = s;
 return t;
}

static last_tnil_indexnode(tonick)
char *tonick;
{
 int s,t = tonick_index+1,b = 0;

 if (!t) return 0;
 while ((s = b+t >> 1) != b)
       if (strcasecmp(ToNickList[s]->tonick,tonick)>0) t = s; else b = s;
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
            
 if (fromname_index == max_fromname-1) {
    max_fromname += REALLOC_SIZE;allocate = max_fromname*sizeof(FromNameList);
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (wildcard_index == max_wildcards-1) {
    max_wildcards += REALLOC_SIZE;allocate = max_wildcards*sizeof(WildCardList);
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
                     index_p[1] = (*index_p--);
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
                     index_p[1] = (*index_p--);
                 }
              msgclient->InToNick = n;
              ToNickList[n] = msgclient;
          }
     }
 first = first_fnl_indexnode(fromname);
 last = last_fnl_indexnode(fromname);
 if (!(n = first)) n = 1;
 index_p = FromNameList+n;
 while (n <= last && strcasecmp(msgclient->fromhost,(*index_p)->fromhost) > 0) {
        index_p++;n++;
   }
 while (n <= last && strcasecmp(msgclient->fromnick,(*index_p)->fromnick) > 0) {
        index_p++;n++;
   }
 index_p = FromNameList+fromname_index;
 fromname_index++;t = fromname_index-n; 
 while(t--) {
      (*index_p)->InFromName++; 
       index_p[1] = (*index_p--);
   }
 FromNameList[n] = msgclient;
 msgclient->InFromName = n;
}

static void rw_code(mode,string,fp)
register int mode;
register char *string;
register FILE *fp;
{
 register char *cp = ptr;

 if (!mode) {
     do {
          putc((char)(*string+*cp),fp);
          if (!*++cp) cp = ptr;
      } while (*string++);
     return;
  } 
 if (mode == 1)  
     do {
         *string = getc(fp)-*cp;
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
 ToNickList = (aMsgClient **) malloc(allocate);
 ToNameList = (aMsgClient **) malloc(allocate);
 FromNameList = (aMsgClient **) malloc(allocate);
 WildCardList = (aMsgClient **) malloc(allocate); 
 time(&clock);old_clock = clock;
 fp = fopen(MAIL_SAVE_FILENAME,"r");
 if (!fp) return;
 rw_code(1,buf,fp);mail_msm = atol(buf);
 rw_code(1,buf,fp);mail_mum = atol(buf);
 rw_code(1,buf,fp);mail_msw = atol(buf);
 rw_code(1,buf,fp);mail_muw = atol(buf);
 rw_code(1,buf,fp);mail_mst = atol(buf);
 rw_code(1,buf,fp);mail_msf = atol(buf);
 for (;;) {
        rw_code(1,passwd,fp);if (!*passwd) break;
        rw_code(1,fromnick,fp);rw_code(1,fromname,fp);
        rw_code(1,fromhost,fp);rw_code(1,tonick,fp);rw_code(1,toname,fp);
        rw_code(1,tohost,fp);rw_code(1,buf,fp),flags = atol(buf);
        rw_code(1,buf,fp);timeout = atol(buf);rw_code(1,buf,fp);
        atime = atol(buf);rw_code(1,message,fp);
        if (clock<timeout)  
            new(passwd,fromnick,fromname,fromhost,tonick,toname,
                tohost,flags,timeout,atime,message);
  }
 fclose(fp);
}

static host_check(host1,host2,mode)
char *host1,*host2,mode;
{
 int t;
 char *hosta = host1,*hostb = host2,hosta_type = 'n',hostb_type = 'n';
 
 if (!*hosta || !*hostb) return 0; 
 while (*hosta) {
        if (!isdigit(*hosta) && *hosta != '.') hosta_type = 's';
        hosta++;
   } 
 while (*hostb) {
       if (!isdigit(*hostb) && *hostb != '.') hostb_type = 's'; 
       hostb++;
   }
 if (hosta_type != hostb_type) return 0;
 if (hosta_type == 's')
     if (mode == 'l') {
         while(*host1 && *host1++ != '.');
         while(*host2 && *host2++ != '.');
         if (StrEq(host1,host2)) return 1;
         return 0;
      } else {    
               while(hosta != host1 && *hosta != '.') hosta--;
               while(hostb != host2 && *hostb != '.') hostb--;
               if (StrEq(hosta,hostb)) return 1; 
               return 0;
         }
 if (mode == 'l') {
     for (t=0;t<2;t++)
          while (*host1 && *host1 != '.' 
                 && *host2 && *host2 != '.')
                 if (*host1++ != (*host2++)) return 0;
     return 1;
   } else {
            while (*host1 && *host1 != '.' 
                   && *host2 && *host2 != '.')
                   if (*host1++ != (*host2++)) return 0;
            return 1;
       }
}

static long this_day(clock)
int clock;
{
 struct tm *tm;
 
 tm = localtime(&clock);
 tm->tm_sec = 0;
 tm->tm_min = 0;
 tm->tm_hour = 0;
 return (timelocal(tm));
}

static long set_date(sptr,time_s)
aClient *sptr;
char *time_s;
{
 static char buf[LINE_BUF];
 struct tm *tm;
 long clock;
 int t,t1,month,date,year;
 char *c = time_s,arg[3][BUF_LEN];
 static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
    }; 

 if (*time_s == '-') {
    time (&clock);
    return clock-3600*24*atoi(time_s+1);
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
     return 0;
  }
 month = atoi(arg[1]);
 if (month) month--;
  else for (t = 0;t<12;t++) {
            if (!myncmp(arg[1],months[t],strlen(arg[1]))) { 
                month = t;break;
             } 
            month = -1;
        }
 if (*arg[1] && (month<0 || month>11)) {
      sendto_one(sptr,"ERROR :Unknown month");
      return 0; 
  }       
 year = atoi(arg[2]);
 if (*arg[2] && (year<71 || year>99)) {
     sendto_one(sptr,"ERROR :Unknown year");
     return 0;
   }
 time (&clock);
 tm = localtime(&clock);
 tm->tm_sec = 0;
 tm->tm_min = 0;
 tm->tm_hour = 0;
 tm->tm_mday = date;
 if (*arg[1]) tm->tm_mon = month;
 if (*arg[2]) tm->tm_year = year;
 clock = timelocal(tm);
 return clock;
}

static local_check(sptr,msgclient,passwd,flags,tonick,
                   toname,tohost,time_l,clock)
aClient *sptr;
aMsgClient *msgclient;
char *passwd,*tonick,*toname,*tohost;
long flags,time_l,clock;
{
 if (msgclient->flags == flags
     && StrEq(sptr->user->username,msgclient->fromname)
     && !mycmp(sptr->name,msgclient->fromnick)
     && (!time_l || clock >= time_l && clock<time_l+24*3600)
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

static void display_flags(flags,c)
long flags;
char *c;
{
 char t = 0;
 
 if (flags & FLAGS_OPER) c[t++] = 'O';
 if (flags & FLAGS_SERVER_GENERATED_DESTINATION) c[t++] = 'H';
 if (flags & FLAGS_SERVER_GENERATED_NOTICE) c[t++] = 'G';
 if (flags & FLAGS_SERVER_GENERATED_CORRECT) c[t++] = 'C';
 if (flags & FLAGS_DISPLAY_IF_RECEIVED) c[t++] = 'D';
 if (flags & FLAGS_DISPLAY_IF_CORRECT_FOUND) c[t++] = 'L';
 if (flags & FLAGS_NOT_SAVE) c[t++] = 'S';
 if (flags & FLAGS_REPEAT_ONCE) c[t++] = 'P';
 if (flags & FLAGS_DISPLAY_IF_DEST_REGISTER) c[t++] = 'X';
 if (flags & FLAGS_REPEAT_UNTIL_TIMEOUT) c[t++] = 'R';
 if (flags & FLAGS_NOT_DELETE) c[t++] = 'J';
 if (flags & FLAGS_NOT_SEND) c[t++] = 'T';
 if (flags & FLAGS_SEND_ONLY_IF_LOCALSERVER) c[t++] = 'V';
 if (flags & FLAGS_SEND_ONLY_IF_THIS_SERVER) c[t++] = 'M';
 if (flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME) c[t++] = 'Q';
 if (flags & FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER) c[t++] = 'Y';
 if (flags & FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER) c[t++] = 'U';
 if (flags & FLAGS_NOTICE_RECEIVED_MESSAGE) c[t++] = 'N';
 if (flags & FLAGS_RETURN_CORRECT_DESTINATION) c[t++] = 'F';
 if (flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) c[t++] = 'B';
 if (flags & FLAGS_WHOWAS) c[t++] = 'A';
 if (flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER) c[t++] = 'W';
 if (flags & FLAGS_EXTERNAL_IGNORE) c[t++] = 'I';
 if (flags & FLAGS_LOCAL_EXIT) c[t++] = 'E';
 if (flags & FLAGS_IGNORE_EXIT_EXCEPTION) c[t++] = 'Z';
 if (!t) c[t++] = '-';
 c[t] = 0;
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
     if (!set_flags(sptr,c,&flags,'d',"in search")) return NULL;
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
 free(msgclient->message);
 free(msgclient);
 if (max_fromname-fromname_index>REALLOC_SIZE<<1) {
    max_fromname -= REALLOC_SIZE;allocate = max_fromname*sizeof(FromNameList);
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (max_wildcards-wildcard_index>REALLOC_SIZE<<1) {
    max_wildcards -= REALLOC_SIZE;allocate = max_wildcards*sizeof(WildCardList);
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

void save_messages()
{
 aMsgClient *msgclient;
 long clock;
 FILE *fp,*fopen();
 int t;

 time(&clock);
 fp = fopen(MAIL_SAVE_FILENAME,"w");
 if (!fp) return;
 rw_code(0,ltoa((long)mail_msm),fp);
 rw_code(0,ltoa((long)mail_mum),fp);
 rw_code(0,ltoa((long)mail_msw),fp);
 rw_code(0,ltoa((long)mail_muw),fp);
 rw_code(0,ltoa((long)mail_mst),fp);
 rw_code(0,ltoa((long)mail_msf),fp);
 for (t = 1;t <= fromname_index;t++) {
     msgclient = FromNameList[t];
     if (clock>msgclient->timeout && !(msgclient->flags & FLAGS_NOT_DELETE)) { 
         remove_msg(msgclient);continue;
      }  
     if (msgclient->flags & FLAGS_NOT_SAVE) continue;
     rw_code(0,msgclient->passwd,fp),rw_code(0,msgclient->fromnick,fp);
     rw_code(0,msgclient->fromname,fp);rw_code(0,msgclient->fromhost,fp);
     rw_code(0,msgclient->tonick,fp),rw_code(0,msgclient->toname,fp);
     rw_code(0,msgclient->tohost,fp),
     rw_code(0,ltoa(msgclient->flags),fp);
     rw_code(0,ltoa(msgclient->timeout),fp);
     rw_code(0,ltoa(msgclient->time),fp);
     rw_code(0,msgclient->message,fp);
  }
 rw_code(0,"\0",fp);
 fclose(fp);
}

static set_flags(sptr,string,flags,mode,type)
aClient *sptr;
char *string,mode,*type;
long *flags;
{
 char *c,on,buf[40],cu,send = -1;
 int op,uf = 0;

 op = IsOper(sptr)?1:0; 
 for (c = string;*c;c++) {
      if (*c == '+') { 
          on = 1;continue;
       } 
      if (*c == '-') { 
          on = 0;continue;
       } 
      cu = islower(*c)?toupper(*c):*c;
      switch (cu) {
              case 'T': if (on) send = 0;
                          else send = 1;
                        break;             
              case 'S': if (on) *flags |= FLAGS_NOT_SAVE;
                         else *flags &= ~FLAGS_NOT_SAVE;
                        break;             
              case 'R': if (on) *flags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
                         else *flags &= ~FLAGS_REPEAT_UNTIL_TIMEOUT;
                        break;        
              case 'P': if (on) *flags |= FLAGS_REPEAT_ONCE;
                         else *flags &= ~FLAGS_REPEAT_ONCE;
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
              case 'Y': if (on) *flags |= FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER;
                         else *flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER;
                        break;             
              case 'U': if (on) *flags |= FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER;
                         else *flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER;
                        break;             
              case 'N': if (on) *flags |= FLAGS_NOTICE_RECEIVED_MESSAGE;
                         else *flags &= ~FLAGS_NOTICE_RECEIVED_MESSAGE;
                        break;             
              case 'D': if (on) *flags |= FLAGS_DISPLAY_IF_RECEIVED;
                         else *flags &= ~FLAGS_DISPLAY_IF_RECEIVED;
                        break;             
              case 'F': if (on) *flags |= FLAGS_RETURN_CORRECT_DESTINATION;
                         else *flags &= ~FLAGS_RETURN_CORRECT_DESTINATION;
              case 'L': if (on) *flags |= FLAGS_DISPLAY_IF_CORRECT_FOUND;
                         else *flags &= ~FLAGS_DISPLAY_IF_CORRECT_FOUND;
                        break;
              case 'X':  if (on) *flags |= FLAGS_DISPLAY_IF_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_IF_DEST_REGISTER;
                        break;
              case 'J': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_NOT_DELETE;
                             else *flags &= ~FLAGS_NOT_DELETE;
                         } else buf[uf++] = cu;
                        break;
              case 'B': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                             else *flags &= ~FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                         } else buf[uf++] = cu;
                        break;
              case 'Z': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_IGNORE_EXIT_EXCEPTION;
                             else *flags &= ~FLAGS_IGNORE_EXIT_EXCEPTION;
                         } else buf[uf++] = cu;
                        break;
              case 'A': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_WHOWAS;
                             else *flags &= ~FLAGS_WHOWAS;
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
              case 'C': if (mode == 'd') {
                           if (on) *flags |= FLAGS_SERVER_GENERATED_CORRECT;
                             else *flags &= ~FLAGS_SERVER_GENERATED_CORRECT;
		         } else buf[uf++] = cu;
                        break;
              case 'H': if (mode == 'd') {
                           if (on) *flags |= FLAGS_SERVER_GENERATED_DESTINATION;
                            else *flags &= ~FLAGS_SERVER_GENERATED_DESTINATION;
                          } else buf[uf++] = cu;
                        break;
              case 'I': if (op || mode == 'd' || !on) {
                             if (on) *flags |= FLAGS_EXTERNAL_IGNORE;
			      else *flags &= ~FLAGS_EXTERNAL_IGNORE;
                         } else buf[uf++] = cu;
                         break;  
              case 'E': if (op || mode == 'd' || !on) {
                             if (on) *flags |= FLAGS_LOCAL_EXIT;
			      else *flags &= ~FLAGS_LOCAL_EXIT;
                         } else buf[uf++] = cu;
                         break;  
              default:  buf[uf++] = cu;
        } 
  }
 buf[uf] = 0;
 if (uf) {
     sendto_one(sptr,"NOTICE %s :Unknown flag%s: %s %s",sptr->name, 
                uf>1?"s":"\0",buf,type);
     return 0;
  }

 if (mode == 's') {
     if (send == -1 && 
         (*flags & FLAGS_NOT_SEND
         || *flags & FLAGS_WHOWAS
         || *flags & FLAGS_RETURN_CORRECT_DESTINATION
         || *flags & FLAGS_DISPLAY_IF_CORRECT_FOUND
         || *flags & FLAGS_DISPLAY_IF_DEST_REGISTER
         || *flags & FLAGS_IGNORE_EXIT_EXCEPTION
         || *flags & FLAGS_EXTERNAL_IGNORE
         || *flags & FLAGS_LOCAL_EXIT)) send = 0;
    if (send && *flags & FLAGS_REPEAT_UNTIL_TIMEOUT
        && !(*flags & FLAGS_REPEAT_ONCE)) { 
        if (op) sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                           "WARNING: Repeat of send-message activated;");
         else *flags |= FLAGS_REPEAT_ONCE;
     }
    if (*flags & FLAGS_NOT_DELETE)
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                   "WARNING: Message will never be deleted;");
    if (*flags & FLAGS_EXTERNAL_IGNORE)
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                   "WARNING: External ignore activated;");
    if (*flags & FLAGS_LOCAL_EXIT)
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                   "WARNING: Local exit activated;");
    if (*flags & FLAGS_REPEAT_ONCE) *flags &= ~FLAGS_REPEAT_UNTIL_TIMEOUT;
 }
 if (!send) *flags |= FLAGS_NOT_SEND;
  else if (send == 1) *flags &= ~FLAGS_NOT_SEND;
 return 1;
}

static void check_flags(sptr,nick,newnick,msgclient,local_server,
                        first_tnl,last_tnl,first_tnil,last_tnil,
                        send,repeat,exit,gnew,mode)
aClient *sptr;
aMsgClient *msgclient;
char *nick,*newnick;
int local_server,first_tnl,last_tnl,first_tnil,
    last_tnil,*send,*repeat,*exit,*gnew,mode;
{
 char buf[LINE_BUF];
 long clock,time_l,gflags,timeout = 0;
 int t,first,last;
 aClient *cptr;
 aMsgClient **index_p;

 *send = 1;*repeat = 0;*exit = 0;*gnew = 0;
 if (!local_server && msgclient->flags & FLAGS_SEND_ONLY_IF_LOCALSERVER
     || !MyConnect(sptr) && msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER
     || (msgclient->flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME  && 
        StrEq(nick,sptr->user->username))
     || ((!IsOper(sptr) || mode != 'o')  && 
         msgclient->flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER)) {
         *send = 0;*repeat = 1;return;
  }
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER)
     for (cptr = client;cptr;cptr = cptr->next)
          if (StrEq(cptr->name,msgclient->fromnick)
              && StrEq(cptr->user->username,msgclient->fromname)
              && StrEq(cptr->user->host,msgclient->fromhost)
              && MyConnect(cptr) && cptr != sptr) {
              date(&clock);
              sprintf(buf,"%s %s@%s",nick,sptr->user->username,
                      sptr->user->host);
              if (mode == 'a') {
                             if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)) 
                                 sendto_one(cptr,"NOTICE %s :*** %s %s %s: %s",
                                 cptr->name,
                                 buf,"entered IRC on",myctime(clock),
                                 *msgclient->message?msgclient->message:"\0");
                              else 
                               msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED;    
	       }
              else
                if (mode == 'n') {
                          msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                          sendto_one(cptr,"NOTICE %s :*** %s %s %s on %s: %s",
                                     cptr->name,
                                     buf,"changed name to",
                                     SecretChannel(sptr->user->channel)
                                     ? "something secret...":newnick,
                                     myctime(clock),
                                   *msgclient->message?msgclient->message:"\0");
				
                  } else
                     if (mode == 'e') {
                         sendto_one(cptr,"NOTICE %s :*** %s %s %s: %s",
                                    cptr->name,buf,"signed off on",
                                    myctime(clock),
                                   *msgclient->message?msgclient->message:"\0");
		      }
	    }
 while (msgclient->flags & FLAGS_WHOWAS) {
        time (&clock);time_l = this_day(clock);
        if (*msgclient->message) timeout = atoi(msgclient->message)*3600+clock;
        if (!timeout) timeout = msgclient->timeout;
        for (t = first_tnl;t <= last_tnl;t++)
             if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_WHOWAS
                 && StrEq(ToNameList[t]->toname,sptr->user->username)
                 && host_check(ToNameList[t]->tohost,sptr->user->host,'l')
                 /* && !mycmp(ToNameList[t]->tonick,nick) Future use maybe...*/
                 && (mode == 'e' || ToNameList[t]->time >= time_l
                      && ToNameList[t]->time < time_l+24*3600)) {
                 free(ToNameList[t]->tonick);
                 free(ToNameList[t]->tohost);
                 DupString(ToNameList[t]->tohost,sptr->user->host);
                 if (mode == 'n') DupString(ToNameList[t]->tonick,newnick);
                  else DupString(ToNameList[t]->tonick,nick);
                 ToNameList[t]->timeout = timeout;
                 ToNameList[t]->time = clock;
                 if (IsOper(sptr)) ToNameList[t]->flags |= FLAGS_OPER;
                  else ToNameList[t]->flags &= ~FLAGS_OPER;
                 break;
	     }
        if (t <= last_tnil || mode == 'e' || mode == 'n' || mode == 'o') break;
        gflags = 0;
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_WHOWAS;
        gflags |= FLAGS_NOT_SEND;
        gflags |= FLAGS_NICK;
        gflags |= FLAGS_NAME;
        if (IsOper(sptr)) gflags |= FLAGS_OPER;
        *gnew = 1;
        new("*","\0","WHOWAS","\0",nick,sptr->user->username,
            sptr->user->host,gflags,timeout,clock,ltoa(clock));
        break;
    }
 if (mode == 'e' || mode == 'n') { 
     *repeat = 1;*send = 0;
     return;
 }
 while ((msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION
        || msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)
        && (!*msgclient->message || !matches(msgclient->message,sptr->info))) {
        sprintf(buf,"Correct address for %s %s@%s is %s %s@%s",
                msgclient->tonick,msgclient->toname,msgclient->tohost,
                nick,sptr->user->username,sptr->user->host);
        if (msgclient->flags & FLAGS_DISPLAY_IF_CORRECT_FOUND)
            for (cptr = client;cptr;cptr = cptr->next)
                if (StrEq(cptr->name,msgclient->fromnick)
                    && StrEq(cptr->user->username,msgclient->fromname)
                    && host_check(cptr->user->host,msgclient->fromhost,'l')
                    && MyConnect(cptr)) {
                    sendto_one(cptr,"NOTICE %s :*** %s",cptr->name,buf);
                    break;
   	        }
        if (!(msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION)) break;
        gflags = 0;
        gflags |= FLAGS_OPER;
        gflags |= FLAGS_NAME;
        gflags |= FLAGS_SERVER_GENERATED_CORRECT;
        if (msgclient->flags & FLAGS_OPER) 
            gflags |= FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
        time(&clock);*gnew = 1;
        new(msgclient->passwd,"SERVER","*","*",
            msgclient->fromnick,msgclient->fromname,msgclient->fromhost,
            gflags,msgclient->timeout,clock,buf);
        break;
  }
 if (msgclient->flags & FLAGS_NOT_DELETE  || 
     msgclient->flags & FLAGS_REPEAT_UNTIL_TIMEOUT) *repeat = 1;
 if ((msgclient->flags & FLAGS_LOCAL_EXIT && MyConnect(sptr)
     || msgclient->flags & FLAGS_EXTERNAL_IGNORE)
     && (!*msgclient->message || !matches(msgclient->message,sptr->info))) {
     index_p = ToNameList;first = first_tnl;last = last_tnl;
     while(1) {
               for (t = first;t <= last;t++) 
                    if (index_p[t]->flags & FLAGS_IGNORE_EXIT_EXCEPTION
                        && !mycmp(index_p[t]->fromnick,msgclient->fromnick)
                        && !mycmp(index_p[t]->fromname,msgclient->fromname)
                        && !mycmp(index_p[t]->fromhost,msgclient->fromhost)
                        && !matches(index_p[t]->tonick,nick)
                        && !matches(index_p[t]->toname,sptr->user->username)
                        && !matches(index_p[t]->tohost,sptr->user->host)
                        && (!*index_p[t]->message)  || 
                            !matches(index_p[t]->message,sptr->info)) {
                        *exit = -1;break;
		    }
               if (*exit == -1 || index_p == WildCardList) break;
                else if (index_p == ToNameList) {
                         index_p = ToNickList;
                         first = first_tnil;last = last_tnil;
		     } else {
                             index_p = WildCardList;
                             first = 1;last = wildcard_index;
                        }
      }
    if (!*exit) *exit = 1; else *exit = 0;
 }
 if (msgclient->flags & FLAGS_NOT_SEND) *send = 0;
 if (!local_server && 
     msgclient->flags & FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER) {
     msgclient->flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_LOCALSERVER; 
     msgclient->flags |= FLAGS_SEND_ONLY_IF_LOCALSERVER;
     *repeat = 1;
  }
 if (!MyConnect(sptr) && 
     msgclient->flags & FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER) {
     msgclient->flags &= ~FLAGS_REPEAT_LOCAL_IF_NOT_THIS_SERVER;
     *repeat = 1;
  }    
 if (msgclient->flags & FLAGS_REPEAT_ONCE) {
     msgclient->flags &= ~FLAGS_REPEAT_ONCE;
     *repeat = 1;
   } 
 while (*send && msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) {
        for (t = first_tnil;t <= last_tnil;t++)
             if (mode != 'n' 
                 && ToNickList[t]->flags & FLAGS_SERVER_GENERATED_DESTINATION
                 && !mycmp(ToNickList[t]->fromnick,msgclient->fromnick)
                 && StrEq(ToNickList[t]->fromname,msgclient->fromname)
                 && StrEq(ToNickList[t]->fromhost,msgclient->fromhost)
                 && StrEq(ToNickList[t]->tonick,nick)
                 && StrEq(ToNickList[t]->toname,sptr->user->username)
                 && host_check(ToNickList[t]->tohost,sptr->user->host,'l')) {
                 *send = 0;break;
             }
        if (!*send) break;
        gflags = 0;
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_DESTINATION;
        gflags |= FLAGS_NAME;
        gflags |= FLAGS_NOT_SEND;
        if (msgclient->flags & FLAGS_OPER) gflags |= FLAGS_OPER;
        time(&clock);*gnew = 1;
        new(msgclient->passwd,msgclient->fromnick,msgclient->fromname,
            msgclient->fromhost,nick,sptr->user->username,
            sptr->user->host,gflags,msgclient->timeout,clock,"\0");
        break;
   }
 while (*send && (msgclient->flags & FLAGS_NOTICE_RECEIVED_MESSAGE
        || msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED)) {
        sprintf(buf,"%s %s@%s has received mail",
                nick,sptr->user->username,sptr->user->host);
        if (msgclient->flags & FLAGS_DISPLAY_IF_RECEIVED)
            for (cptr = client;cptr;cptr = cptr->next)
                 if (StrEq(cptr->name,msgclient->fromnick)
                     && StrEq(cptr->user->username,msgclient->fromname)
                     && StrEq(cptr->user->host,msgclient->fromhost)
                     && MyConnect(cptr)) {
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
       new(msgclient->passwd,"SERVER","*","*",
           msgclient->fromnick,msgclient->fromname,msgclient->fromhost,
           gflags,msgclient->timeout,clock,buf);
       break;
    }
}

void check_messages(cptr,sptr,tonick,mode)
aClient *cptr,*sptr;
char *tonick,mode;
{
 aMsgClient *msgclient,**index_p;
 char buf[LINE_BUF],dibuf[40],*name,*newnick;
 int last,first_tnl,last_tnl,first_tnil,last_tnil,
     send,repeat,local_server = 0,t,exit,gnew;
 long clock;

 if (!fromname_index) return;
 time(&clock);
 if (clock > old_clock+mail_msf) {
    save_messages();old_clock = clock;
  }
 if (!sptr->user) return;
 if (mode == 'n') {
     newnick = tonick;tonick = sptr->name;
  }
 if (host_check(sptr->user->server,sptr->user->host,'e')) local_server = 1;
 name=sptr->user->username;
 t = first_tnl = first_tnl_indexnode(name);
 last = last_tnl = last_tnl_indexnode(name);
 first_tnil = first_tnil_indexnode(tonick);
 last_tnil = last_tnil_indexnode(tonick);
 index_p = ToNameList;
 while(1) {
    while (last && t <= last) {
           msgclient = index_p[t];
           if (!matches(msgclient->tonick,tonick)
               && !matches(msgclient->toname,name)
               && !matches(msgclient->tohost,sptr->user->host)) {
               check_flags(sptr,tonick,newnick,msgclient,local_server,
                           first_tnl,last_tnl,first_tnil,last_tnil,
                           &send,&repeat,&exit,&gnew,mode);
               if (send) {
                   display_flags(msgclient->flags,dibuf);
                   sprintf(buf,"%s (h%.2f)",myctime(msgclient->time),
                           (float)(clock-msgclient->time)/3600);
                   sendto_one(sptr,":%s NOTICE %s :*/%s/ %s %s@%s on %s* %s",
                              me.name,tonick,dibuf,msgclient->fromnick,
                              msgclient->fromname,msgclient->fromhost,
                              buf,msgclient->message);
       	        }
               if (gnew) {
                   first_tnl = first_tnl_indexnode(name);
                   last_tnl = last_tnl_indexnode(name);
                   first_tnil = first_tnil_indexnode(tonick);
                   last_tnil = last_tnil_indexnode(tonick);
         	   if (index_p == ToNameList) { 
                       last=last_tnl; 
                       while (t <= last && msgclient != index_p[t]) t++;
 		    }
                   if (index_p == ToNickList) {
                       last=last_tnil; 
                       while (t <= last && msgclient != index_p[t]) t++;
		    }
		}
               if (!repeat) remove_msg(msgclient);
               if (exit) { ExitClient(cptr, sptr);return; }
               if (!repeat) { last--;continue; }
	   }
           if (clock>msgclient->timeout && 
               !(msgclient->flags & FLAGS_NOT_DELETE)) { 
               remove_msg(msgclient);last--; 
           } else t++;
       }
     if (index_p == WildCardList) {
         if (mode == 'n') {
             mode = 'a';tonick = newnick;
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

static void msg_save(sptr)
aClient *sptr;
{
 if (IsOper(sptr)) {
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
             "Max server messages is set to:",
             "Max server messages with wildcards:",
             "To many wildcards makes life too hard...",
             "Max server messages with wildcards is set to",
             "Max user messages:",
             "Too cheeky fingers on keyboard error...",
             "Max user messages is set to:",
             "Max user messages with wildcards:",
             "Give me $$$, and I may fix your problem...",
             "Max user messages with wildcards is set to",
             "Max server hours:",
             "Can't remember that long time...",
             "Max server hours is set to:",
             "Mail save frequency:",
             "Save frequency may not be like that...",
             "Mail save frequency is set to:" 
          };

 if (!BadPtr(value)) {
    max = atoi(value);
    if (!IsOper(sptr))
        sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,message[l+1]);
     else { 
           if (!max && (msg == &mail_mst || msg == &mail_msf)) max = 1;
           *msg = max;if (msg == &mail_msf) *msg *= 60;
           sendto_one(sptr,"NOTICE %s :*** %s %d",sptr->name,message[l+2],max);
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
 char buf[LINE_BUF],*arg,*value,*fromnick = NULL,
      *fromname = NULL,*fromhost = NULL;
 int tonick_wildcards = 0,toname_wildcards = 0,tohost_wildcards = 0,
     nicks = 0,names = 0,hosts = 0,t = 1,last = fromname_index,flag_notice = 0,
     flag_correct = 0,flag_whowas = 0,flag_destination = 0;
 aMsgClient *msgclient;

 arg = parc > 1 ? parv[1] : NULL;
 value = parc > 2 ? parv[2] : NULL;
 if (!BadPtr(arg)) {
     if (!mycmp(arg,"MSM")) setvar(sptr,&mail_msm,0,value); else 
     if (!mycmp(arg,"MSW")) setvar(sptr,&mail_msw,3,value); else
     if (!mycmp(arg,"MUM")) setvar(sptr,&mail_mum,6,value); else
     if (!mycmp(arg,"MUW")) setvar(sptr,&mail_muw,9,value); else
     if (!mycmp(arg,"MST")) setvar(sptr,&mail_mst,12,value); else
     if (!mycmp(arg,"MSF")) setvar(sptr,&mail_msf,15,value); else
     if (!matches(arg,"USED")) {
         if (!IsOper(sptr)) 
             sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,
                        "Ask the operator about this...");
          else {
               time(&clock);
               while (last && t <= last) {
                      msgclient = FromNameList[t];
                      if (clock>msgclient->timeout && 
                          !(msgclient->flags & FLAGS_NOT_DELETE)) {
                          remove_msg(msgclient);last--;
                          continue;
                       }
                      if (msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
                          flag_destination++; else
                      if (msgclient->flags & FLAGS_SERVER_GENERATED_NOTICE)
                          flag_notice++; else
                      if (msgclient->flags & FLAGS_SERVER_GENERATED_CORRECT)
                          flag_correct++; else
                      if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS)
                          flag_whowas++; else 
                       if (!fromhost || !StrEq(msgclient->fromhost,fromhost)) {
                            nicks++;names++;hosts++;
                            fromhost = msgclient->fromhost;
  		       } else {
                             if (!fromname || 
                                 !StrEq(msgclient->fromname,fromname)) {
                                 nicks++;names++;
                                 fromname = msgclient->fromname;
                   	      } else 
                                  if (!fromnick || mycmp(msgclient->fromnick,
                                      fromnick)) {
                                                  nicks++;
                                                  fromnick=msgclient->fromnick;
	  	               } 
	  	          }
                      if (find_chars('*','?',msgclient->tonick))
                          tonick_wildcards++;
                      if (msgclient->flags & FLAGS_WILDCARD) 
                          toname_wildcards++;
                      if (find_chars('*','?',msgclient->tohost))
                          tohost_wildcards++;
                     t++;
		  }
                  if (!names) sendto_one(sptr,"ERROR :No such user(s) found");
                    else {
                          sprintf(buf,"%s%d /%s%d /%s%d /%s (%s%d /%s%d /%s%d)",
                                  "Nicks:",nicks,"Names:",names,
                                  "Hosts:",hosts,"W.cards",
                                  "Tonicks:",tonick_wildcards,
                                  "Tonames:",toname_wildcards,
                                  "Tohosts:",tohost_wildcards);
                          sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
                          sprintf(buf,"%s /%s%d /%s%d /%s%d /%s%d",
                                  "Server generated",
                                  "H-header:",flag_destination,
                                  "G-received:",flag_notice,
                                  "C-correct:",flag_correct,
                                  "?-whowas:",flag_whowas);
                          sendto_one(sptr,"NOTICE %s :*** %s",sptr->name,buf);
		      }
	   }
     } else if (!mycmp(arg,"RESET")) {
                if (!IsOper(sptr)) 
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
		   }
	    }
  } else { 
      sprintf(buf,"%s%d /%s%d /%s%d /%s%d /%s%d /%s%d /%s%d",
              "QUEUE:",fromname_index,
              "MSM(dynamic):",fromname_index < mail_msm?mail_msm:fromname_index,
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
 int sent_wild = 0,sent = 0,t,last,savetime;
 long clock,timeout,flags = 0;
 char buf1[LINE_BUF],buf2[LINE_BUF],dibuf[40],*c,*passwd,*null_char = "\0",
      *tonick,*toname,*timeout_s,*message,tohost[BUF_LEN+3],*flag_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 timeout_s = parc > 4 ? parv[4] : NULL;
 message = parc > 5 ? parv[5] : NULL;
 if (fromname_index >= mail_msm && IsOper(sptr)) {
    if (!mail_msm || !mail_mum)
        sendto_one(sptr,"ERROR :The mailsystem is closed for no-operators");
     else sendto_one(sptr,"ERROR :No more than %d message%s %s",
                     mail_msm,mail_msm < 2?"\0":"s",
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
 timeout = atoi(timeout_s);
 if (timeout > mail_mst && !(flags & FLAGS_OPER)) {
    sendto_one(sptr,"ERROR :Max time allowed is %d hour%s",
               mail_mst,mail_mst>1?"\0":"s");
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
    if (flags & FLAGS_NOT_SEND) message = null_char; 
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
  if (!(flags & FLAGS_OPER)) {
    /* What if foobar@mars.foo.edu send messages to all his five hundred *
     * friends to wish them a good day...? We just won't accept that ;)  */
     t = first_fnl_indexnode(sptr->user->username);
     last = last_fnl_indexnode(sptr->user->username);
     time(&clock);
     while (last && t <= last) {
         if (clock>FromNameList[t]->timeout 
             && !(FromNameList[t]->flags & FLAGS_NOT_DELETE)) {
             remove_msg(FromNameList[t]);last--;
             continue;
          }
         if (StrEq(sptr->user->username,FromNameList[t]->fromname)) {
             if (host_check(sptr->user->host,FromNameList[t]->fromhost,'l')) {
                 sent++;
                 if (FromNameList[t]->flags & FLAGS_WILDCARD) sent_wild++;  
              }
                 
          }
         t++;
       }
     if (sent >= mail_mum) {
        sendto_one(sptr,"ERROR :No more than %d message%s %s",
                mail_mum,mail_mum < 2?"\0":"s",
                "for each user is allowed in the server");
        return;
      }
     while (flags & FLAGS_WILDCARD) {
         if (!mail_msw || !mail_muw)
            sendto_one(sptr,"ERROR :No-operators are not allowed %s",
                       "to send to nick or username with wildcards");
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
              sprintf(buf2,"/ %s%d","Save.min:",savetime);        
           }   
 sendto_one(sptr,"NOTICE %s :*** %s %s",sptr->name,buf1,buf2);
}

static void msg_list(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient;
 int number = 0,t,last,ls = 0,count = 0;
 long clock,flags,time_l;
 char *passwd,*message,tohost[BUF_LEN+3],*arg,*tonick,
      *toname,buf[LINE_BUF],dibuf[40],*flag_s,*time_s;

 arg = parc > 0 ? parv[0] : NULL;
 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 time_s = parc > 4 ? parv[4] : NULL;

 if (!mycmp(arg,"ls")) ls = 1; else
 if (!matches(arg,"count")) count = 1; else
 if (matches(arg,"cat")) {
     sendto_one(sptr,"ERROR :No such option: %s",arg); 
     return;
  }
 if (!(flag_s = passwd_check(sptr,passwd))) return;
 if (BadPtr(time_s)) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l <= 0) return;
    }
 split(toname,tohost);
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time(&clock); 
 while (last && t <= last) {
        msgclient = FromNameList[t];flags = msgclient->flags;
        if (clock>msgclient->timeout && 
            !(msgclient->flags & FLAGS_NOT_DELETE)) {
            remove_msg(msgclient);last--;
            continue;
          }
        set_flags(sptr,flag_s,&flags,'d',"\0");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,clock)) {
            if (!BadPtr(arg) && !ls) message = msgclient->message;
                   else message = NULL;
            if (!count) {
                display_flags(msgclient->flags,dibuf),
                sprintf(buf,">%.2f< %s",(float)(msgclient->timeout-clock)/3600,
                        myctime(msgclient->time));
                sendto_one(sptr,"NOTICE %s :-> /%s/ %s %s@%s %s: %s",
                           sptr->name,dibuf,msgclient->tonick,
                           msgclient->toname,msgclient->tohost,buf,
                           message?message:(*msgclient->message?"...":"\0"));
             }
            number++;
	 }
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
 int flagged = 0,t,last;
 long clock,flags,time_l;
 char *passwd,*tonick,*toname,*newflag_s,tohost[BUF_LEN+3],
      dibuf1[40],dibuf2[40],*flag_s,*time_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 if (parc == 5) {
     time_s = NULL;
     newflag_s = parv[4];
  } else if (parc>5) {
             time_s = parv[4];
             newflag_s = parv[5];
          } else { 
                   sendto_one(sptr,"ERROR :No flag changes specified");
                   return;
              }
 if (!(flag_s = passwd_check(sptr,passwd)) 
     || ttnn_error(sptr,tonick,toname)) return;
 if (BadPtr(time_s)) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l <= 0) return;
    }
 if (!set_flags(sptr,newflag_s,&flags,'c',"in changes")) return;
 split(toname,tohost);
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time(&clock);
 while (last && t <= last) {
       msgclient = FromNameList[t];flags = msgclient->flags;
        if (clock>msgclient->timeout && 
            !(msgclient->flags & FLAGS_NOT_DELETE)) {
            remove_msg(msgclient);last--;
            continue;
         }
        set_flags(sptr,flag_s,&flags,'d',"\0");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,clock)) {
            flags = msgclient->flags;display_flags(flags,dibuf1);
            set_flags(sptr,newflag_s,&msgclient->flags,'s',"\0");
            display_flags(msgclient->flags,dibuf2);
            if (flags == msgclient->flags) 
                sendto_one(sptr,"NOTICE %s :*** %s -> /%s/ %s@%s",sptr->name,
                           "No flag change for",dibuf1,msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
             else                                        
                sendto_one(sptr,"NOTICE %s :*** %s -> /%s/ %s %s@%s to /%s/",
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
 aMsgClient *msgclient;
 char *arg; 
 int number = 0,t,last,nick = 0,count = 0;
 long clock;

 arg = parc > 1 ? parv[1] : NULL;

 if (BadPtr(arg)) nick = 1; else
  if (!matches(arg,"COUNT")) count = 1; else
   if (matches(arg,"NAME")) {
       sendto_one(sptr,"ERROR :No such option: %s",arg); 
       return;
  }
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time (&clock);
 while (last && t <= last) {
        msgclient = FromNameList[t];
        if (clock>msgclient->timeout && 
            !(msgclient->flags & FLAGS_NOT_DELETE)) {
             remove_msg(msgclient);last--;
             continue;
         }
        if (StrEq(sptr->user->username,msgclient->fromname)
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
 if (!number) sendto_one(sptr,"ERROR :No message(s) found");
  else if (count) sendto_one(sptr,"NOTICE %s :<> %s %d",sptr->name,
                             "Number of no-operator messages queued:",number);
}

static void msg_remove(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient;
 int removed = 0,t,last;
 long clock,flags,time_l;
 char *passwd,*tonick,*toname,dibuf[40],
      tohost[BUF_LEN+3],*flag_s,*time_s;

 passwd = parc > 1 ? parv[1] : NULL;
 tonick = parc > 2 ? parv[2] : NULL;
 toname = parc > 3 ? parv[3] : NULL;
 time_s = parc > 4 ? parv[4] : NULL;
 if (!(flag_s = passwd_check(sptr,passwd)) 
     || ttnn_error(sptr,tonick,toname)) return;
 if (BadPtr(time_s)) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l <= 0) return;
    }
 split(toname,tohost);
 t = first_fnl_indexnode(sptr->user->username);
 last = last_fnl_indexnode(sptr->user->username);
 time (&clock);
 while (last && t <= last) {
       msgclient = FromNameList[t];flags = msgclient->flags;
        if (clock>msgclient->timeout && 
            !(msgclient->flags & FLAGS_NOT_DELETE)) {
            remove_msg(msgclient);last--;
            continue;
         }
        set_flags(sptr,flag_s,&flags,'d',"\0");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,clock)) {
            display_flags(msgclient->flags,dibuf),
            sendto_one(sptr,"NOTICE %s :*** Removed -> /%s/ %s %s@%s",
                       sptr->name,dibuf,msgclient->tonick,
                       msgclient->toname,msgclient->tohost);
            remove_msg(msgclient);last--;removed++;

        } else t++;
   }
 if (!removed) sendto_one(sptr,"ERROR :No such message(s) found");
}

static void msg_whowas(sptr,parc,parv)
aClient *sptr;
int parc;
char *parv[];
{
 aMsgClient *msgclient,**index_p;
 int any = 0,t,last;
 register int t2;
 long clock,flags,time1_l,time2_l;
 char *c1,*c2,*tonick,*toname,tohost[BUF_LEN+3],*time1_s,*time2_s,
      *search,*flag_s,buf[LINE_BUF],search_sl[BUF_LEN+3],
      search_sh[BUF_LEN+3],dibuf[40];

 tonick = parc > 1 ? parv[1] : NULL;
 toname = parc > 2 ? parv[2] : NULL;
 time1_s = parc > 3 ? parv[3] : NULL;
 time2_s = parc > 4 ? parv[4] : NULL;
 if (BadPtr(tonick)) tonick=0;
 if (BadPtr(toname)) toname=0;
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
 split(toname,tohost);
 if (!IsOper(sptr)
     && (!tonick || *tonick == '*' || *tonick == '?')
     && (!toname || *toname == '*' || *tonick == '?')) {
     sendto_one(sptr,"ERROR :%s %s","Both tonick and toname",
                "can not begin with wildcards");
     return;
 }
 if (!tonick && toname) search = toname; 
  else if (!toname && tonick) search = tonick;
   else if (tonick && toname) {
            search = 0;c1 = tonick;c2 = toname;
            while (*c1 && *c2) {
                   if (*c1 == '*' || *c1++ == '?') { search = toname; break; }
                   if (*c2 == '*' || *c2++ == '?') { search = tonick; break; }
              }
            if (!search) search = toname; 
         } else search=0;
 if (search) { 
     strcpy(search_sl,search);
     strcpy(search_sh,search);
     c1 = find_chars('*','?',search_sl);if (c1) *c1 = 0;
     c1 = find_chars('*','?',search_sh);if (c1) *c1 = 0;
     if (!*search_sl) search=0; 
      else for (c1 = search_sh;*c1;c1++);(*--c1)++;
  }
 if (!search) { 
     t = first_fnl_indexnode("WHOWAS");
     last = last_fnl_indexnode("WHOWAS");
     index_p = FromNameList;
  } else if (search == toname) {
             t = first_tnl_indexnode(search_sl);
             last = last_tnl_indexnode(search_sh);
             index_p = ToNameList;
          } else if (search == tonick) {
                     t = first_tnil_indexnode(search_sl);
                     last = last_tnil_indexnode(search_sh);
                     index_p = ToNickList;
		  }
 if (BadPtr(time1_s)) time1_l = 0; 
  else { 
        time1_l = set_date(sptr,time1_s);
        if (time1_l <= 0) return;
    }
 if (BadPtr(time2_s)) time2_l = time1_l+3600*24; 
  else { 
        time2_l = set_date(sptr,time2_s);
        if (time2_l <= 0) return;
        time2_l+=3600*24;
    }
 time (&clock);
 while (last && t <= last) {
       msgclient = index_p[t];flags = msgclient->flags;
        if (clock>msgclient->timeout && 
            !(msgclient->flags & FLAGS_NOT_DELETE)) {
            remove_msg(msgclient);last--;
            continue;
         }
        if (msgclient->flags & FLAGS_SERVER_GENERATED_WHOWAS
            && (!time1_l || clock >= time1_l && clock < time2_l)
            && (!tonick || !matches(tonick,msgclient->tonick))
            && (!toname || !matches(toname,msgclient->toname))
            && (!*tohost || !matches(tohost,msgclient->tohost))) {
            if (!any)
               for (t2 = 1;t2 <= toname_index;t2++)
                    if (FromNameList[t2]->flags & FLAGS_WHOWAS) {
                        display_flags(FromNameList[t2]->flags,dibuf);
                        sendto_one(sptr,"NOTICE %s :*** %s: %s /%s/ %s %s@%s",
                                   sptr->name,"Listed from",
                                   myctime(FromNameList[t2]->time),dibuf,
                                   FromNameList[t2]->tonick,
                                   FromNameList[t2]->toname,
                                   FromNameList[t2]->tohost);
		    }
            strcpy(buf,myctime(atol(msgclient->message)));
            sendto_one(sptr,"NOTICE %s :*** %s >> %s : %s %s%s@%s",
                       sptr->name,buf,myctime(msgclient->time),
                       msgclient->tonick,
                       msgclient->flags & FLAGS_OPER ? "*":"\0",
                       msgclient->toname,msgclient->tohost);
           any = 1;
        } 
        t++;
   }
 if (!any) sendto_one(sptr,"ERROR :No such user(s) found");
}

m_mail(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
 char *arg;
 arg = parc > 1 ? parv[1] : NULL;
 if (!IsRegisteredUser(sptr)) { 
	sendto_one(sptr, ":%s %d * :You have not registered as an user", 
		   me.name, ERR_NOTREGISTERED); 
	return -1;
   }
 if (BadPtr(arg)) {
    sendto_one(sptr,"ERROR :No option specified. Try MAIL HELP"); 
    return -1;
  } else
 if (!myncmp(arg,"HELP",strlen(arg))) msg_mailhelp(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"STATS",strlen(arg))) msg_stats(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"SEND",strlen(arg))) msg_send(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"LS",strlen(arg)) || !myncmp(arg,"CAT") ||
     !myncmp(arg,"COUNT")) msg_list(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"SAVE",strlen(arg))) msg_save(sptr); else
 if (!myncmp(arg,"FLAG",strlen(arg))) msg_flag(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"SENT",strlen(arg))) msg_sent(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"WHOWAS",strlen(arg))) msg_whowas(sptr,parc-1,parv+1); else
 if (!myncmp(arg,"RM",strlen(arg))) msg_remove(sptr,parc-1,parv+1); else
  sendto_one(cptr,"ERROR :No such option: %s",arg);
} 
#endif
