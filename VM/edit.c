/************************************************************************
 *   IRC - Internet Relay Chat, VM/edit.c
 *   Copyright (C) 1990 Jarkko Oikarinen & Armin Gruner
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
#include <ctype.h>
#include <strings.h>
#include "struct.h"
#include "sys.h"
#include "msg.h"
#include "irc.h"
 
#define EMPTYMSG putline("*** Error: Empty message cannot be sent.")
 
static char cmdch = '/';
static char queryuser[NICKLEN + 1] = "";
static char buf[BUFFERLEN];
FILE * logfile;
 
extern char *last_to_me(), *last_from_me();
extern char *mycncmp();
extern aClient me;
extern anUser meUser;
 
do_cmdch(ptr)
char *ptr;
{
  if (BadPtr(ptr))
        putline("*** Error: Command character not changed.");
  else
        {
        putline(buf);
        cmdch = *ptr;
        }
}
 
do_quote(ptr)
char *ptr;
{
  if (BadPtr(ptr))
        putline("*** Error: Empty command");
  else
        sendto_one(&me, "%s", ptr);
}
 
do_query(ptr)
char *ptr;
{
  if (BadPtr(ptr))
        {
        sprintf(buf, "*** Ending a private chat with %s.", queryuser);
        queryuser[0] = '\0';
        }
  else  {
        strncpy(queryuser, ptr, NICKLEN);
        sprintf(buf, "*** Beginning a private chat with %s.", queryuser);
        }
  putline(buf);
}
 
do_mypriv(buf1)
char *buf1;
{
  char *tmp = index(buf1, ' ');
 
  if (tmp == NULL)
        EMPTYMSG;
else
  {
    if (buf1[0] == ',' && buf1[1] == ' ') {
      sendto_one(&me, "%s %s :%s", MSG_PRIVATE, 
                 last_to_me((char *) 0), &buf1[2]);
      last_from_me(last_to_me((char *) 0));
      *(tmp++) = '\0';
      sprintf(buf,"->%s!!*%s* %s%s", ESC_HI,
    last_to_me((char *) 0), tmp, ESC_NORM);
      putline(buf);
    } else if (buf1[0] == '.' && buf1[1] == ' ') {
      sendto_one(&me, "%s %s :%s", MSG_PRIVATE,
                 last_from_me((char *) 0), &buf1[2]);
      *(tmp++) = '\0';
      sprintf(buf,"->%s##*%s* %s%s",
    ESC_HI, last_from_me((char *) 0), tmp, ESC_NORM);
      putline(buf);
    } else {
      sendto_one(&me, "%s %s", MSG_PRIVATE, buf1);
      *(tmp++) = '\0';
      last_from_me(buf1);
      sprintf(buf,"->%s*%s* %s%s", ESC_HI, buf1, tmp, ESC_NORM);
      putline(buf);
    }
  }
}
 
do_myqpriv(buf1)
char *buf1;
{
  if (BadPtr(buf1))
        EMPTYMSG;
  else  {
        if (BadPtr(queryuser))
           putline("*** Error: No queryuser defined.");
        else if (sendto_one(&me, "%s %s :%s", MSG_PRIVATE, 
                            queryuser, buf1) == 0)
                {
                sprintf(buf,"->%s*%s* %s%s", ESC_HI, queryuser, buf1, ESC_NORM);
                putline(buf);
                }
        }
}

do_mytext(buf1)
char *buf1;
{
  if (!BadPtr(buf1))
     if (sendto_one(&me, "%s %s :%s", MSG_PRIVMSG, meUser.channel, buf1) == 0)
        {
        sprintf(buf,"%s>%s%s%s", meUser.channel, ESC_HI, buf1, ESC_NORM);
        putline(buf);
        }
}
 
sendit(line)
char *line;
{
  char *ptr;
  struct Command *cmd = commands;
 
  if (line[0] != cmdch)
        if (*queryuser)
                do_myqpriv(line);
        else
                do_mytext(line);
  else  {
        if (line[1] == ' ')
        do_mytext(&line[2]);
        else
        {
            if (line[1])
            {
            for ( ; cmd->name; cmd++)
                {
                if (ptr = mycncmp(&line[1], cmd->name))
                    {
                    switch (cmd->type)
                        {
                        case SERVER_CMD:
                            sendto_one(&me, "%s %s",
                                cmd->extra, ptr);
                            break;
                        case LOCAL_FUNC:
                            (*cmd->func)(ptr, cmd->extra);
                            break;
                        default:
                            putline("*** Error: Data error in c_irc.c.");
                            break;
                        }
                    break;
                    }
                }
                if (!cmd->name)
                    putline("*** Error: Unknown command.");
            }
        }
    }
}
 
 
do_log(ptr)
char *ptr;
{
  long tloc;
  char buf[150];
  char *ptr2;
 
  if (!logfile)
    {                         /* logging currently off */
    if (BadPtr(ptr))
        putline("*** You must specify a filename to start logging.");
    else
        {
        if (!(logfile = fopen(ptr, "a")))
            {
            sprintf(buf, "*** Error: Couldn't open log file %s.\n", ptr);
                putline(buf);
            }
        else
            {
            time(&tloc);
            sprintf(buf, "*** IRC session log started at %s", ctime(&tloc));
            ptr2 = rindex(buf, '\n');
            *ptr2 = '.';
            putline(buf);
            }
        }
    }
else
    {                            /* logging currently on */
    if (BadPtr(ptr))
        {
        time(&tloc);
        sprintf(buf, "*** IRC session log ended at %s", ctime(&tloc));
        ptr2 = rindex(buf, '\n');
        *ptr2 = '.';
        putline(buf);
        fclose(logfile);
        logfile = (FILE *)0;
        }
    else
        putline("*** Your session is already being logged.");
    }
}
 
/* remember the commas */
#define LASTKEEPLEN (20 * (NICKLEN+1))
 
char *
last_to_me(sender)
char *sender;
{
  static char name[LASTKEEPLEN] = ",";
 
  if (sender)
    strncpyzt(name, sender, sizeof(name));
 
  return (name);
}
 
char *
last_from_me(recipient)
char *recipient;
{
  static char name[LASTKEEPLEN] = ".";
 
  if (recipient)
    strncpyzt(name, recipient, sizeof(name));
 
  return (name);
}
 
#define COLS 80
 
putline(line)
char *line;
{
    char *ptr, c = 0;
    int len, pos;
    extern write();
    extern int fsd;
 
    ptr = line;
    while (1)
        {
        len = strlen(ptr);
        if (len > COLS)
            {
            pos = COLS;
            while (ptr[pos] != ' ')
                {
                if (--pos == 10)
                     {
                     pos = 0;
                     break;
                     }
                }
           if (pos)
               {
               ptr[pos] = '\0';
               c = ptr[++pos];
               }
           else {
               pos = COLS;
               c = ptr[pos];
               ptr[pos] = '\0';
                }
           }
        write(fsd, ptr, len);
        if (logfile)
                fprintf(logfile, "%s\n", ptr);
        if (c)
            {
            ptr[pos] = c;
            c = 0;
           ptr[pos - 1] = '+';
           ptr = &(ptr[pos - 1]);
           }
        else
           break;
        }
        }
 
do_clear()
{
   putline("*** Not implemented - don't need it (press PA2)");
}
 
do_bye(ptr, extra)
char *ptr, *extra;
{
   extern int hold;
   hold = 0;
   sendto_one(&me, extra);
}
 
do_unkill(ptr, extra)
char *ptr, *extra;
{
     extern int unkill;
 
     if (unkill)
        unkill = 0;
     else
        unkill = MAXUNKILLS;
     if (unkill)
        putline("*** Unkill feature set to on.");
     else
        putline("*** Unkill feature set to off.");
}
 
do_exec(ptr, extra)
char *ptr;
char *extra;
{
    extern int fsd;
 
    close(fsd);
    sprintf(buf, "EXEC IRCCMS %s", ptr);
    system(buf);
    fsd = openfs(BANNER);
}
 
do_oper(ptr, extra)
char *ptr, *extra;
{
   if (BadPtr(ptr))
      {
      putline("Enter Nick & Password:");
      sprintf(buf, "%c%s ", cmdch, extra);
      setline(buf, 0, 0);
      hideline();
      }
   else
      {
      sendto_one(&me, "%s %s", extra, ptr);
      seeline();
      }
}
 
do_channel(ptr, extra)
char *ptr, *extra;
{
   strncpy(meUser.channel, ptr, sizeof(meUser.channel));
   sendto_one(&me, "%s %s", extra, ptr);
}
 
char *mycncmp(str1, str2)
char *str1, *str2;
{
  int flag = 0;
  char *s1;
  for (s1 = str1; *s1 != ' ' && *s1 && *str2; s1++, str2++) {
    if (!isascii(*s1)) return 0;
    if (islower(*s1)) *s1 = toupper(*s1);
    if (*s1 != *str2) flag = 1;
  }
  if (*s1 && *s1 != ' ' && *str2 == '\0')
    return 0;
  if (flag) return 0;
  if (*s1) return s1 + 1;
  else return s1;
}
