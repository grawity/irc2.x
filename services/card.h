/*************************************************************************
 ** msg.h  Beta  v2.0    (22 Aug 1988)
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

#define CMD_RESET   "RESET"
#define CMD_SHUFFLE "SHUFFLE"
#define CMD_GAME    "GAME"
#define CMD_DEAL    "DEAL"
#define CMD_BET     "BET"
#define CMD_DROP    "DROP"
#define CMD_TAKE    "TAKE"
#define CMD_PLAYER  "PLAYER"
#define CMD_SWITCH  "SWITCH"
#define CMD_PUT     "PUT"

struct Game {
  char *name;
  int decks;           /* Decks in use in the game */
  int jokers;          /* No. of jokers in game */
  int initial_cards;   /* Initial cards delt to players */
  int newcardtimes;    /* How many times a player can take new cards */
  int newcardflag;     /* flag = 0, player can take new cards with
                                    no restriction.
                          flag = 1, player can take one new card at a time
                          flag = 2, player can switch some of his cards to
                                    new ones
                          flag = 4, player can always take new cards so that
                                    he always has them number of initial_cards
		        */
  int (*cmp)();        /* Compare function used to detect who won the game */
  int betflag;         /* flag = 0, players can make bets always after
                                    new cards have been dealt
                          flag = 1, players can make bet at the start of game
                          flag = 2, players can make bet after first deal */
  int basicpot;        /* Total amount of basic pot */
};
                          
extern int r_reset(), r_shuffle(), r_game(), r_deal(), r_bet(), r_drop();
extern int r_take(), r_player(), r_switch();

struct Message cmdtab[] = {
  { CMD_RESET,   r_reset 0, 3 },
  { CMD_SHUFFLE, r_shuffle 0, 3 },
  { CMD_GAME,    r_game 0, 3 },
  { CMD_DEAL,    r_deal 0, 3 },
  { CMD_BET,     r_bet 0, 3 },
  { CMD_DROP,    r_drop 0, 3 },
  { CMD_TAKE,    r_take 0, 3 },
  { CMD_PLAYER,  r_player 0, 3 },
  { CMD_SWITCH,  r_switch 0, 3 },
  { CMD_PUT,     r_put 0, 3 },
  { NULL,        (int (*)()) 0, 0, 3 }
};
