/*************************************************************************
 ** deck.c    Beta v2.0    (22 Aug 1988)
 **
 ** This file is part of Internet Relay Chat v2.0
 **
 ** Author:           Jarkko Oikarinen
 **         Internet: jto@tolsun.oulu.fi
 **             UUCP: ...!mcvax!tut!oulu!jto
 **           BITNET: toljto at finou
 **
 ** Copyright (c) 1988 University of Oulu, Computing Center
 **
 ** All rights reserved.
 **
 ** See file COPYRIGHT in this package for full copyright.
 **
 *************************************************************************/
 
char deck_id[] = "deck.c v2.0 (c) 1988 University of Oulu, Computing Center";

#include "struct.h"
#include "deck.h"

#define STATE_NONE   1
#define STATE_START  2
#define STATE_BET    3
#define STATE_TAKE   4
#define STATE_END    5

#define ST_INGAME    1
#define ST_GIVEUP    2
#define ST_NOMORE    3
#define ST_NEWUSER   4

struct Card {
  char suite;
  char card;
  struct Card *next;
};

struct Player {
  char name[NICKLEN+1];
  struct Card *cards;
  int status;
  int saldo;
  int pot;
  struct Player *next;
};

static int state = STATE_NONE;
static int pot = 0;

extern aClient *client;

static char *suites[] = {
  "Spade", "Heart", "Diamonds", "Club"
  };

static char *cards[] = {
  "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
  "jack", "queen", "king", "ace", "joker"
  };

static struct Player *player = (struct Player *) 0;
statuc struct Card *trash = (struct Card *) 0;
static struct Card *deck = (struct Card *) 0;
static struct Player *turn = (struct Player *) 0, *first = (struct Player *) 0;
static int cardsondeck = 0;
static int cardsperdeal = 0;
static int noofplayers = 0;

r_reset(user)
char *user;
{
  struct Card *ctmp = deck, *ctmp2;
  struct Player *ptmp = player, *ptmp2;
  sendto_one(client, "MSG :%s requested game reset...", user);
  while (ctmp) {
    ctmp2 = ctmp->next;
    free (ctmp);
    ctmp = ctmp2;
  }
  while (player) {
    ctmp = player->cards;
    while (ctmp) {
      ctmp2 = ctmp->next;
      free (ctmp);
      ctmp = ctmp2;
    }
    ptmp2 = ptmp->next;
    free(ptmp);
    ptmp = ptmp2;
  }
  ctmp = trash;
  while (ctmp) {
    ctmp2 = ctmp->next;
    free(ctmp);
    ctmp = ctmp2;
  }
  cardsondeck = cardsperdeal = noofplayers = 0;
  deck = trash = (struct Card *) 0;
  player = turn = first = (struct Player *) 0;
  return(1);
}

struct Player *nextplayer()
{
  if (noofplayers == 0)
    return (struct Player *) 0;
  if (turn == (struct Player *) 0)
    turn = first;
  do {
    turn++;
    if (turn > &(players[MAXPLAYERS-1]))
      turn = &(players[0]);
  } while (turn->name[0] == '\0');
  return (turn);
}

shuffle(name)
char *name;
{
  int i,j;
  if (state != START && state != END && state != NO_INIT) {
    sendto_one(client, "MSG :Shuffling in the middle of game don't work...");
    return(0);
  }
  if (state == NO_INIT) {
    state = START;
    init_game(5);
    return (0);
  }
  sendto_one(client, "MSG :%s is shuffling the deck ...", name);
  cardsondeck = 54;
  for (i = 0; i < 13; i++)
    for (j = 0; j < 4; j++) {
      deck[i + j * 13].suite = j;
      deck[i + j * 13].card = i+2;
      deck[i + j * 13].next = &(deck[i + j * 13 + 1]);
    }
  deck[52].next = &(deck[53]);
  deck[53].next = (struct Card *) 0;
  deck[53].suite = deck[52].suite = 4;
  deck[53].card = deck[52].card = 15;
  decktop = &deck[0];
}

init_game(x)
int x;
{
  int i;
  shuffle();
  cardsperdeal = x;
  for (i=0; i < MAXPLAYERS; i++) {
    players[i].name[0] = '\0';
    players[i].cards = (struct Card *) 0;
    players[i].pot = 0;
    players[i].saldo = 0;
  }
  srandom(time(0));
  state = START;
}

player(user)
char *user;
{
  int i;
  if (state != START) {
    sendto_one(client,"PRIVMSG %s :You cannot enter in the middle of game",
	       user);
    return(-1);
  }
  for (i=0; players[i].name[0] && i < MAXPLAYERS; i++);
  if (i == MAXPLAYERS) {
    sendto_one(client,"PRIVMSG %s :Sorry, no room for more players...",
	       user);
    return(-1);
  }
  noofplayers++;
  strncpy(players[i].name, user, NICKLEN);
  players[i].name[NICKLEN] = '\0';
  players[i].pot = 0;
  players[i].saldo = 0;
  players[i].status = ST_INGAME;
  sendto_one(client,"MSG :Player %s entered the game...", user);
  return(0);
}

names()
{
  int i;
  sendto_one(client,"MSG :Players on game:");
  for (i=0; i<MAXPLAYERS; i++) {
    if (players[i].name[0])
      sendto_one(client,"MSG :%s", players[i].name);
  }
}

struct Player *getplayer(name)
char *name;
{
  int i;
  for (i=0; i<MAXPLAYERS; i++)
    if (strncmp(name, players[i].name) == 0)
      break;
  if (i < MAXPLAYERS)
    return (&players[i]);
  else
    return (struct Player *) 0;
}

struct Card *pickcard()
{
  int i;
  struct Card *cp1 = (struct Card *) 0, *cp2 = decktop;
  if (cardsondeck < 1)
    return (struct Card *) 0;
  i = random() % cardsondeck;
  cardsondeck--;
  while (i-- > 0) {
    cp1 = cp2;
    cp2 = cp2->next;
  }
  if (cp1)
    cp1->next = cp2->next;
  cp2->next = (struct Card *) 0;
  return cp2;
}

deal()
{
  int i,j;
  struct Card *tmp;
  if (state != START) {
    sendto_one(client, "MSG :Cannot deal now...");
    return(-1);
  }
  state = DEAL;
  sendto_one(client, "MSG :Dealing cards ...");
  for (i=0; i<MAXPLAYERS; i++)
    if (players[i].name[0]) {
      sendto_one(client, "MSG :Player %s... %d cards",
		 players[i].name, cardsperdeal);
      for (j=0; j<cardsperdeal; j++) {
	tmp = pickcard();
	if (tmp == (struct Card *) 0) {
	  sendto_one(client, "MSG :No more cards... player %s got only %d..",
		     players[i].name, j);
	  break;
	}
	tmp->next = players[i].cards;
	players[i].cards = tmp;
      }
    }
}

hand(sender)
char *sender;
{
  struct Card *tmp;
  struct Player *playerptr = getplayer(sender);
  if (playerptr == (struct Player *) 0) {
    sendto_one(client,"PRIVMSG %s :You are not playing !");
    return(-1);
  }
  if (state == START) {
    sendto_one(client,"PRIVMSG %s :Game has not started yet !");
    return(-1);
  }
  tmp = playerptr->cards;
  sendto_one(client,"PRIVMSG %s :Your cards:", sender);
  sendto_one(client,"MSG :%s is having a look at his cards...", sender);
  while (tmp) {
    sendto_one(client, "PRIVMSG %s :%s of %s", sender,
		   cards[tmp->card - 2], suites[tmp->suite]);
    tmp = tmp->next;
  }
}

  
