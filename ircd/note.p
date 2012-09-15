From jarle@stud.cs.uit.no Wed Apr 28 05:44:27 1993
Received: from hpserv0.cs.UiT.No by coombs.anu.edu.au (5.61/1.0)
	id AA28051; Wed, 28 Apr 93 05:44:20 +1000
Received: from lglab19.cs.UiT.No by hpserv0.cs.uit.no (1.37.109.4/Task/HJ-5)
	id AA00224; Tue, 27 Apr 93 21:43:44 +0200
Received: by lgserv1.cs.uit.no (1.37.109.4/Task/HJ-5)
	id AA05591; Tue, 27 Apr 93 21:43:43 +0200
Date: Tue, 27 Apr 93 21:43:43 +0200
From: jarle@stud.cs.uit.no (Jarle Lyngaas)
Message-Id: <9304271943.AA05591@lgserv1.cs.uit.no>
Disclaimer: This message was sent from, not by, the University of Tromsoe
Apparently-To: avalon@coombs.anu.edu.au
Status: OR

*** ../../irc2.8.5/ircd/note.c	Tue Apr 20 15:00:00 1993
--- note.c	Tue Apr 27 21:12:03 1993
***************
*** 32,38 ****
  #include "common.h"
  #include "h.h"
  
! #define VERSION "v1.8.1"
  
  #define NOTE_SAVE_FREQUENCY 30 /* Frequency of save time in minutes */
  #define NOTE_MAXSERVER_TIME 120 /* Max days for a request in the server */
--- 32,38 ----
  #include "common.h"
  #include "h.h"
  
! #define VERSION "v1.8.2"
  
  #define NOTE_SAVE_FREQUENCY 30 /* Frequency of save time in minutes */
  #define NOTE_MAXSERVER_TIME 120 /* Max days for a request in the server */
***************
*** 41,48 ****
  #define NOTE_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
  #define NOTE_MAXUSER_WILDCARDS 5 /* Max number of user toname wildcards */
  
! #define GET_CHANNEL_TIME 5
! #define LAST_CLIENTS 1500 /* Max join or sign on within GET_CHANNEL_TIME */    
  #define BUF_LEN 256
  #define MSG_LEN 512
  #define REALLOC_SIZE 1024
--- 41,48 ----
  #define NOTE_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
  #define NOTE_MAXUSER_WILDCARDS 5 /* Max number of user toname wildcards */
  
! #define GET_CHANNEL_TIME 10
! #define LAST_CLIENTS 3000 /* Max join or sign on within GET_CHANNEL_TIME */    
  #define BUF_LEN 256
  #define MSG_LEN 512
  #define REALLOC_SIZE 1024
***************
*** 489,501 ****
  register char *string;
  register FILE *fp;
  {
   register char c, *cp = ptr;
  
   do {
!       if ((c = getc(fp)) == EOF) {
           debug(DEBUG_FATAL,"Corrupt file format: %s", NPATH);
           exit(-1);       
         }
        *string = c-*cp;
        if (!*++cp) cp = ptr;
    } while (*string++);
--- 489,503 ----
  register char *string;
  register FILE *fp;
  {
+  register int v;
   register char c, *cp = ptr;
  
   do {
!       if ((v = getc(fp)) == EOF) {
           debug(DEBUG_FATAL,"Corrupt file format: %s", NPATH);
           exit(-1);       
         }
+       c = v;
        *string = c-*cp;
        if (!*++cp) cp = ptr;
    } while (*string++);
***************
*** 1821,1827 ****
  void *info;
  char *command, *par1, *par2, *par3;
  {
!  char *arg, *c, mode, nick[50];
   int from_secret = 0, on = -1;
   long *delay, clock, last_call = 0, source;
   Link *link;
--- 1823,1831 ----
  void *info;
  char *command, *par1, *par2, *par3;
  {
!  char *arg, *c, mode, nick[BUF_LEN]; 
!  static char last = 0, last_command[BUF_LEN], last_nick[BUF_LEN], 
!         last_chn[BUF_LEN], last_mode = 0, last_modes[BUF_LEN];
   int from_secret = 0, on = -1;
   long *delay, clock, last_call = 0, source;
   Link *link;
***************
*** 1829,1834 ****
--- 1833,1842 ----
   aClient *sptr;
  
   time (&clock);
+  if (!last) {
+      last_command[0] = 0; last_nick[0] = 0; 
+      last_modes[0] = 0, last_chn[0] = 0; last = 1;
+   }
   if (!command) {
          delay = (long *) info;
          if (*delay > GET_CHANNEL_TIME) *delay = GET_CHANNEL_TIME;
***************
*** 1840,1854 ****
   source = (long) info;
   arg = split_string(command, 2, 1);
   if (StrEq(arg, "USER")) mode = 'a'; else
-  if (source != 1) return; else 
   if (StrEq(arg, "NICK")) mode = 'n'; else
   if (StrEq(arg, "JOIN")) mode = 'j'; else
   if (StrEq(arg, "MODE")) mode = 'm'; else
!  if (StrEq(arg, ":Closing") || StrEq(arg, "QUIT")) mode = 'e'; else
!  if (StrEq(arg, "PART") || StrEq(arg, "KICK")) mode = 'l'; else return;
   strcpy(nick, par1);
   if ((c = (char *)index(nick, '!')) != NULL) *c = '\0';
   if ((c = (char *)index(nick, '[')) != NULL) *c = '\0';
   if (mode == 'm' && *par3 == '+' && par3[1] == 'o') mode = 'o';
   sptr = find_person(nick, (aClient *)NULL);
   if (!sptr) for (sptr = client; sptr; sptr = sptr->next)
--- 1848,1881 ----
   source = (long) info;
   arg = split_string(command, 2, 1);
   if (StrEq(arg, "USER")) mode = 'a'; else
   if (StrEq(arg, "NICK")) mode = 'n'; else
   if (StrEq(arg, "JOIN")) mode = 'j'; else
   if (StrEq(arg, "MODE")) mode = 'm'; else
!  if (StrEq(arg, "QUIT")) mode = 'e'; else
!  if (StrEq(arg, "PART") || StrEq(arg, "KICK")) mode = 'l'; else 
!  if (StrEq(arg, "KILL")) {
!      par1 = par2; /* Who kills before who's killed here... */
!      mode = 'e';
!  } else return;
   strcpy(nick, par1);
   if ((c = (char *)index(nick, '!')) != NULL) *c = '\0';
   if ((c = (char *)index(nick, '[')) != NULL) *c = '\0';
+  if (mode == 'm' && (c = (char *)index(command, '+')) != NULL) par3 = c;
+  if (mode == 'm' && (c = (char *)index(command, '-')) != NULL) par3 = c;
+ 
+  if (last_mode == mode && StrEq(last_nick, nick)) {
+      if (mode == 'm') {
+          if (StrEq(last_modes, par3)) return;
+       }  else if (mode == 'j' || mode == 'l') {
+                   if (StrEq(last_chn, par2)) return;
+                } else return;
+   }
+  if (mode == 'm') strncpyzt(last_modes, par3, BUF_LEN-1);
+  if (mode == 'j' || mode == 'l') strncpyzt(last_chn, par2, BUF_LEN-1);
+  strncpyzt(last_command, command, BUF_LEN-1); 
+  strncpyzt(last_nick, nick, BUF_LEN-1); 
+  last_mode = mode;
+ 
   if (mode == 'm' && *par3 == '+' && par3[1] == 'o') mode = 'o';
   sptr = find_person(nick, (aClient *)NULL);
   if (!sptr) for (sptr = client; sptr; sptr = sptr->next)


