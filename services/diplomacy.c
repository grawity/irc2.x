/* dip.c
 *
 * Copyright 1989 Mike Bolotski 
 *
 * Redistribution of this program is governed by the GNU General Public
 * License, as published by the Free Software Foundation. If you
 * don't have a copy, ask me to send you one.
 *
 * mikeb@ee.ubc.ca  (misha)
 *
 * compiling with IRC:
 * 
 * WOBJS= card.o parse.o packet.o send.o w_msg.o debug.o str.o r_sys.o
 * sys.o c_conf.o wumpus.o ignore.o
 *
 * where card.o is irc.c compiled with -DAUTOMATON, r_sys is the r_sys
 * file distributed for the card daemon, and w_msg is c_msg compiled
 * with -DAUTOMATON.
 *
 */

#include <stdio.h>
#include "struct.h"
#include <assert.h>

#define DIPPLAYERS 7
#define MINRESET 2 		/* require 2 reset request to reset */

typedef struct player {
  int frozen;
  char *moves[100];
  int nmoves;
  int hasreset;
  int avail;
  char name[NICKLEN+1];
} aPlayer ;

static aPlayer players[DIPPLAYERS];


static int nplayers = 0;
static int channel = 0;
static int suspended = 0;
static int nturns = 0;
static int nfrozen = 0;
static int nreset = 0;
static int gameon = 0;
extern struct Client *client;

static aPlayer *lookup();

/*---------------------------------------------------------*/
/* interface functions to irc */

a_init()
{ 
  int i;
  printf("Initialization\n");
  for (i = 0; i < DIPPLAYERS; i++)
   players[i].avail = 1; 
}

a_nick(sender, nickname)
char *sender, *nickname;
{
  aPlayer *p = lookup(sender);
  if (p) 
    strcpy(p->name, nickname);
}

a_leave(sender)
char *sender;
{
}

a_join(sender)
char *sender;
{
}

a_quit(sender)
char *sender;
{
}

a_msg(sender, buf)
char *sender, *buf;
{
}

#include <ctype.h>
/* various function declarations */

extern char *mycncmp();

static void signup(), move(), freeze(), help(), list(), quit(), 
     death(), query(), reset(), stats(), gamestart(), clear();

a_privmsg(sender, buf2)
char *sender, *buf2;
{
  char *ptr, buf[255];
  aPlayer *p; 
  /* pre-start commands */
 
  if (ptr = mycncmp(buf2,"JOIN", 1)) { 	signup(sender); return; }
  if (ptr = mycncmp(buf2,"HELP", 1)) { 	help(sender); return; }
  if (ptr = mycncmp(buf2,"START", 1)) { 	gamestart(); return; }
  if (ptr = mycncmp(buf2,"QUIT", 1)) { 	quit(sender); return; }

  if (!gameon) {
    a_sendu(sender,"Game is not on");
    return;
  }  

  /* post-game start commands */
  if (ptr = mycncmp(buf2,"STATUS", 1)) { 	stats(sender); return; }

  if (!(p = lookup(sender))) {
    a_sendu(sender, "You're not playing yet");
    return;
  } 
  /* player only commands */
  if (ptr = mycncmp(buf2,"DEATH", 1)) { 	death(sender);  return; }
  if (ptr = mycncmp(buf2,"FREEZE", 1)) { 	freeze(p);  return; }
  if (ptr = mycncmp(buf2,"LIST", 1)) { 		list(p, sender); return; }
  if (ptr = mycncmp(buf2,"QUERY", 1)) { 	query(sender, ptr); return; }
  if (ptr = mycncmp(buf2,"TURN", 1)) 	{ 	reset(p,sender); return; }

  if (p->frozen) {
    a_sendu(sender, "You've already frozen your moves");
    return;
  } 

  if (ptr = mycncmp(buf2,"MOVE", 1)) { 		move(p, sender, ptr);  return; }
  if (ptr = mycncmp(buf2,"CLEAR", 1)) { 	clear(p); return; }
 
  sprintf(buf, "Uknown command: %s\n", buf2);
  a_sendu(sender, buf); 
}

/* move: accept a line of orders */

a_invite(sender, chan)
char *sender, *chan;
{
  int new;

  sscanf(chan, "%d", &new);
  if (new == channel) {
    a_sendu(sender, "Hey, I'm already here!");
/*
  } else if (new < 0) {
    a_sendu(sender, "Sorry, I only invite to non-negative channels!");
*/
  } else if (nplayers != 0) {
    a_sendu(sender, "Sorry, I can only move when I have no players!");
  } else {
    sendto_one(client,"MSG :%s invited me away, Byebye...", sender);
    sendto_one(client,"CHANNEL %s", chan);
    channel = new;
    sendto_one(client,"MSG :Hello %s, you called me?", sender);
  }
}

a_nosuch(ptr)
char *ptr;
{
  a_senda("a_nosuch?");
  a_senda(ptr); 
}

char *a_myreal()
{
  return("The Diplomacy GameMaster");
}

char *a_myname()
{
  return("dip");
}

char *a_myuser()
{
  return("mikeb");
}

/*-------------------------------------------------------------*/
/* my utility functions */

a_sendu(name, msg)
char *name, *msg;
{
  char buf[100];
  sprintf(buf, "%s :%s", name, msg); 
  sendto_one(client,"PRIVMSG %s", buf);
}

a_senda(msg)
char *msg;
{
  sendto_one(client, "MSG :%s", msg);
}

/*-------------------------------------------------------------------*/
/* and now the game */

static void clear(p)
aPlayer *p;
{
  int i;
  for (i=0; i < p->nmoves; i++)
    free(p->moves[i]);
  p->nmoves = 0;
}

static void init_player(p)
aPlayer *p;
{ 
  p->frozen = 0;
  p->hasreset = 0;
  clear(p);
}

static aPlayer *findslot()
{
  int i;
  for (i=0; i < DIPPLAYERS; i++)
    if (players[i].avail)
       return &players[i];
  return 0;
}

static void signup(name)
char *name;
{
  aPlayer *p; 

  if (gameon) {
    a_sendu(name, "Sorry, can't join game in progress");
    return;
  }

  printf("Signing up %s\n", name);

  if (lookup(name)) {
    a_sendu(name, "You're already playing!");
    return;
  }

  if (!(p = findslot())) {
    a_sendu(name, "Sorry, the Diplomacy game is full.");
    return;
  }
  strcpy(p->name, name);
  p->avail = 0;
  init_player(p);
  nplayers++;
  {
   char buf[100];
   sprintf(buf,"Adding %s as player %d", name, nplayers);
   a_senda(buf);  
  }
}

static void help(name)
char *name;
{
  a_sendu(name, "Commands available are:");
  a_sendu(name, "JOIN CLEAR MOVE FREEZE LIST QUERY TURN STATUS START CLEAR");
}

static void stats(name)
char *name;
{
  char buf[100];

  sprintf(buf, "Current turn: %d.  Frozen moves: %d\n", nturns, nfrozen);    
  a_sendu(name, buf);
}

static aPlayer *lookup(name)
char *name;
{
  int i;

  for(i = 0 ; i < DIPPLAYERS ; i++)
    if (*players[i].name)
      if (!strcmp(players[i].name, name)) 
	return &players[i];
  return 0;
}

static void new_turn()
{
  int i;
  nreset = 0;
  nturns++; 
  nfrozen = 0;
  a_senda("New Turn");
 
  for (i=0; i < nplayers; i++)
    init_player(&players[i]); 
}

static void reset(p, name)
aPlayer *p;
char *name;
{
   if (nfrozen != nplayers) {
     a_sendu(name,"Can't reset, not all players have finished"); 
     return;
   }
  if (p->hasreset) {
     a_sendu(name,"You've already reset!");
     return;
  }
  p->hasreset = 1;

  nreset++;
  {
    char buf[100];
    sprintf(buf, "Reset by %s. %d left", name, MINRESET-nreset);
    a_senda(buf);
  }

  if (nreset < MINRESET)
    return;

  new_turn(); 
}

static void freeze(p)
aPlayer *p;
{
  if (p->frozen)
    return; 
  p->frozen = 1;
  nfrozen++;
  if (nfrozen == nplayers)
    a_senda("All players have finished their turn");
}

static void quit(name)
char *name;
{
  a_sendu(name,"Quit not implemented");
}

static void query(name, ptr)
char *name;
char *ptr;
{
   aPlayer *q;
   char buf[100];
   int i;
   if (nfrozen != nplayers) {
     a_sendu(name,"Not all players have finished"); 
     return;
   }
   if (!(q = lookup(ptr))) {
     sprintf(buf, "Unknown player: %s", ptr);
     a_sendu(name, buf);
     return;
   }
   for (i=0; i < q->nmoves; i++) 
     a_sendu(name, q->moves[i]); 
}

static void gamestart()
{
  gameon = 1;
  a_senda("Game starting...");
}

static void move(p, sender, line)
aPlayer *p;
char *sender, *line;
{
   p->moves[p->nmoves] = (char *) malloc(strlen(line)+1);
   strcpy(p->moves[p->nmoves], line);
   p->nmoves++; 
}

static void list(p, sender)
aPlayer *p;
char *sender;
{
  int i;
  for (i=0; i < p->nmoves; i++) 
     a_sendu(sender,p->moves[i]);
}

static void death(sender)
char *sender;
{
  char buf[100];

  static int ndeath = 0;
  if (++ndeath == 3) {
     sprintf(buf,"Death Notice by %s", sender);    
     a_senda(buf);
     exit(1);
  }
}
