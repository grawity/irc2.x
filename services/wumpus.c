/* wumpus.c
 *
 * Copyright 1989 Greg Lindahl
 *
 * Redistribution of this program is governed by the GNU General Public
 * License, as published by the Free Software Foundation. If you
 * don't have a copy, ask me to send you one.
 *
 * gl8f@virginia.{edu,bitnet}  POBox 3818 Charlottesville VA 22903
 *
 * don't bastardize Wumpus. stick to the ORIGINAL, please. no superbats,
 * no swords, only 1 wumpus, and the wumpus cannot ever be killed.
 *
 */

#include <stdio.h>
#include "struct.h"
#include <assert.h>

#define WUMPUS 1
#define PIT 2
#define BATS 3
#define MAXPLAYER 100

struct room {
  int doors;
  int door[3];
  int contents;
  int players;
};

static struct room *maze;
static unsigned short msize;
static int loc[ MAXPLAYER ];
static int ammo[ MAXPLAYER ];
static char names[ MAXPLAYER ][ NICKLEN+1 ];
static int nogame = 1;

static int w_kill, p_kill, h_kill;
static int num_players = 0;
static int channel = 0;

extern aClient *client;

/*---------------------------------------------------------*/
/* interface functions to irc */

a_init()
{
}

a_nick( sender, nickname )
char *sender, *nickname;
{
  change( sender, nickname );
}

a_leave( sender )
char *sender;
{
  resign( sender );
}

a_quit( sender )
char *sender;
{
  resign( sender );
}

a_msg( buf )
char *buf;
{
}

#include <ctype.h>
extern char *mycncmp();

a_privmsg( sender, buf2 )
char *sender, *buf2;
{
  char *ptr, buf[255];
  
  if (ptr = mycncmp(buf2,"JOIN", 1)) {
    signup(sender);
  } else if (ptr = mycncmp(buf2,"MOVE", 1)) {
    if( !isdigit(*ptr) )
      a_sendu( sender, "error: move needs a room number after it!" );
    else
      cmove(sender, atoi(ptr));
  } else if (ptr = mycncmp(buf2,"SHOOT", 1)) {
    if( !isdigit(*ptr) )
      a_sendu( sender, "error: shoot needs a room number after it!" );
    else
      shoot(sender, atoi(ptr));
  } else if (ptr = mycncmp(buf2,"HELP", 1)) {
    help( sender );
  } else if (ptr = mycncmp(buf2,"RULES", 1)) {
    rules( sender );
  } else if (ptr = mycncmp(buf2,"QUIT", 1)) {
    cresign(sender);
  } else if (ptr = mycncmp(buf2,"LOOK", 1)) {
    look( sender );
  } else if (ptr = mycncmp(buf2,"WHO", 1)) {
    who( sender );
  } else if (ptr = mycncmp(buf2,"STATS", 1)) {
    stats( sender );
  } else if (ptr = mycncmp(buf2,"WUMI", 1)) {
    init(sender, atoi(ptr));
  } else if (ptr = mycncmp(buf2,"WUMC", 1)) {
    sendto_one( client, "MSG :%s ordered me away, Byebye...", sender);
    sendto_one( client, "CHANNEL %s", ptr);
    sscanf( ptr, "%d", &channel );
    sendto_one( client, "MSG :Hillou, all!" );
  } else if (ptr = mycncmp(buf2,"WUMD", 1)) {
    a_sendu( "Bartender", "serve" );
  } else if (ptr = mycncmp(buf2,"WUMS", 1)) {
    a_senda( ptr );
  } else if (ptr = mycncmp(buf2,"WUMA", 1)) {
    underway();
  }
}

a_invite( sender, chan )
char *sender, *chan;
{
  int new;

  sscanf( chan, "%d", &new );
  if ( new == channel ) {
    a_sendu( sender, "Hey, I'm already here!" );
  } else if (new < 0 ) {
    a_sendu( sender, "Sorry, I only invite to non-negative channels!" );
  } else if (num_players != 0) {
    a_sendu( sender, "Sorry, I can only move when I have no players!" );
  } else {
    sendto_one(client,"MSG :%s invited me away, Byebye...", sender);
    sendto_one(client,"CHANNEL %s", chan);
    channel = new;
    sendto_one(client,"MSG :Hello %s, you called me?", sender);
  }
}

a_nosuch( ptr )
char *ptr;
{
  int length = strlen(ptr);

  ptr[length-1] = '\0'; /* chop off trailing ) */
  resign( ptr );
}

char *a_myreal()
{
  return( "The Wumpus GameMaster" );
}

char *a_myname()
{
  return( "tgm" );
}

char *a_myuser()
{
  return( "jto" );
}

/*-------------------------------------------------------------*/
/* my utility functions */

a_sendu( name, msg )
char *name, *msg;
{
  sendto_one(client,"PRIVMSG %s :%s", name, msg );
}

a_senda( msg )
char *msg;
{
  sendto_one(client, "MSG :%s", msg );
}

/*-------------------------------------------------------------------*/
/* and now the game */

signup( name )
char *name;
{
  int i, player = -1;

  if( nogame ) {
    init( "AUTOMATIC", 20 );
  }

  if( lookup(name) != -1 ) {
    a_sendu( name, "You're already playing!" );
    return;
  }

  for( i = 0 ; i < MAXPLAYER ; i++ ) {
    if( *names[i] == '\0' ) {
      strcpy( names[i], name );
      loc[i] = -1;
      ammo[i] = 5;
      a_sendu( name, "Welcome to Wumpus. You have 5 arrows." );
      move( name, get_empty() );
      player = i;
      num_players++;
      break;
    }
  }
  if( player == -1 )
    a_sendu( name, "Sorry, the Wumpus game is full." );
}

change( name, new )
char *name, *new;
{
  int player = lookup( name );

  if( player != -1 ) {
    strcpy( names[player], new );
  }
}

who( name )
char *name;
{
  int i, num;
  char buf[256];

  if( nogame ) {
    sorry( name );
    return;
  }
    
  strcpy( buf, "Current players: " );

  num = 0;
  for( i = 0 ; i < MAXPLAYER ; i++ )
    if( *names[i] ) {
      strcat( buf, names[i] );
      strcat( buf, " " );
      num++;
    }
  if( !num )
    strcat( buf, "(nobody)" );
  a_sendu( name, buf );
}  

cresign( name )
char *name;
{
  int player = lookup( name );

  if( nogame ) {
    sorry( name );
    return;
  }

  if( player == -1 ) {
    a_sendu( name, "You aren't currently playing!" );
    return;
  }
  resign( name );
  a_sendu( name, "OK, you're out of the game." );
}

resign( name )
char *name;
{
  int player = lookup( name );

  if( player == -1 )
    return;

  maze[loc[player]].players--;
  loc[player] = 0;
  *names[player] = '\0';
  num_players--;
}

int lookup( name )
char *name;
{
  int i;

  for( i = 0 ; i < MAXPLAYER ; i++ )
    if( *names[i] ) {
      if( strcmp( names[i], name ) == 0 ) {
	return(i);
      }
    }
  return( -1 );
}

look( name )
char *name;
{
  int player = lookup(name);

/*  if( nogame ) {
    sorry( name );
    return;
  }
*/
  if( player == -1 ) {
    a_sendu( name, "I'm sorry, you must JOIN first. Send /m GM help for more info." );
    return;
  }

  move( name, -1 );
}

cmove( name, room )
char *name;
int room;
{
  int player = lookup( name );

/*  if( nogame ) {
    sorry( name );
    return;
  }*/

  if( player == -1 ) {
    a_sendu( name, "I'm sorry, you must JOIN first. Send /m GM help for more info." );
    return;
  }

  if( room >= msize || room < 0 ) {
    a_sendu( name, "Sorry, that room number is invalid." );
    return;
  }
  move( name, room );
}

move( name, room )
char *name;
int room;
{
  int player = lookup( name );
  char buf[80];
  int i, j, check;
  int wumpus, bats, pit, human;

  if( loc[player] == room ) {
    sprintf( buf, "You're already in room %d.", room );
    a_sendu( name, buf );
    room = loc[player];
    loc[player] = -1;
    maze[room].players--;
  } /* fall through to get description */

  if( loc[player] == -1 ) { /* special case, initial move */
    loc[player] = room;
  } else if( room == -1 ) { /* special case, look */
    sprintf( buf, "You have %d arrow%s left.", ammo[player], ammo[player] != 1 ? "s" : "" );
    a_sendu( name, buf );
    room = loc[player];
    maze[room].players--; /* and fall thru to see description */
  } else {
    maze[loc[player]].players--;

    for( i = 0 ; i < 3 ; i++ ) {
      if( maze[loc[player]].door[i] == room ) {
	loc[player] = room;
	break;
      }
    }
  }
  maze[loc[player]].players++;

  if( loc[player] != room ) {
    sprintf( buf, "You can't get to room %d from room %d.", room, loc[player] );
    a_sendu( name, buf );
    room = loc[player];
  }

  if( maze[room].contents == WUMPUS ) {
    a_sendu( name, "You have been eaten by the Wumpus. Sorry." );
    a_senda( "You hear a blood-curdling scream." );
    w_kill++;
    resign( name );
    move_wumpus( room );
    return;
  }

  if( maze[room].contents == PIT ) {
    a_sendu( name, "You have fallen into a bottomless pit. Sorry. " );
    a_senda( "You hear a faint wail, which gets fainter and fainter." );
    p_kill++;
    resign( name );
    return;
  }

  if( maze[room].contents == BATS ) {
    /* bug: bats don't move */
    room = irand( msize );
    sprintf( buf, "You are picked up by bats! They carry you to room %d.", room );
    a_senda( "You hear a surprised yell." );
    a_sendu( name, "You are picked up by bats!" );
    maze[loc[player]].players--;
    loc[player] = room;
    maze[loc[player]].players++;
    move( name, -1 ); /* look */
    return;
  }

  wumpus = 0;
  bats = 0;
  pit = 0;
  human = 0;

  for( i = 0 ; i < 3 ; i++ ) {
    check = maze[room].door[i];
    if( check != room ) {
      if( maze[check].contents == WUMPUS )
	wumpus = 1;
      for( j = 0 ; j < 3 ; j++ )
	if( maze[maze[check].door[j]].contents == WUMPUS )
	  wumpus = 1;
      if( maze[check].contents == BATS )
	bats = 1;
      if( maze[check].contents == PIT )
	pit = 1;
      if( maze[check].players > 0 )
	human = 1;
    }
  }

  *buf = '\0';

  if( wumpus )
    strcat( buf, "You smell a Wumpus! " );
  if( bats )
    strcat( buf, "You hear bats! " );
  if( pit )
    strcat( buf, "You feel a draft! " );
  if( human )
    strcat( buf, "You smell Humans!" );
  
  if( *buf != '\0' )
    a_sendu( name, buf );


  if( maze[room].players > 1) {
    human = 0;
    for( i = 0 ; i < MAXPLAYER ; i++ ) {
      if( *names[i] && i != player )
	if( loc[i] == room ) {
	  sprintf( buf, "You see %s in this room!", names[i] );
	  a_sendu( name, buf );
	  sprintf( buf, "%s walks into room %d with you.", name, room );
	  a_sendu( names[i], buf );
	}
    }
  }

  sprintf( buf, "You are in room %d. There are tunnels leading to rooms %d, %d, and %d.", room, maze[room].door[0], maze[room].door[1], maze[room].door[2] );
  a_sendu( name, buf );
}

move_wumpus( start )
int start;
{
  /* of course this function wants get_empty() and ignore players. */

  int room, i, eaten;
  char buf[80];

  a_senda( "You hear footsteps..." );

  room = irand( msize );
  while( maze[room].contents != 0 )
    room = irand( msize );

  maze[start].contents = 0;
  maze[room].contents = WUMPUS;
  
  if( maze[room].players > 0 ) {
    eaten = 0;
    for( i = 0 ; i < MAXPLAYER ; i++ ) {
      if( *names[i] )
	if( loc[i] == room ) {
	  a_sendu( names[i], "The Wumpus walks into your room and eats you." );
	  w_kill++;
	  resign( names[i] );
	  eaten++;
	}
    }
    sprintf( buf, "You hear the screams of %d surprised adventurer%s.", eaten, eaten > 1 ? "s" : "" );
    a_senda( buf );
  }
}
 
shoot( name, room )
char *name;
int room;
{
  int i,j;
  int player = lookup( name );
  char buf[80];

/*  if( nogame ) {
    sorry( name );
    return;
  }
*/
  if( player == -1 ) {
    a_sendu( name, "I'm sorry, you must JOIN first. Send /m GM help for more info." );
    return;
  }

  if( room < 0 || room >= msize ) {
    a_sendu( name, "Sorry, that isn't a valid room number." );
    return;
  }

  for( i = 0 ; i < 3 ; i++ ) {
    if( maze[loc[player]].door[i] == room ) {
      if( maze[room].contents == WUMPUS ) {
	sprintf( buf, "%s killed the Wumpus. The game is over.", name );
	a_senda( buf );
	sprintf( buf, "The Wumpus ate %d human%s, %d fell into pits, and %d were shot by other humans.", w_kill, w_kill != 1 ? "s" : "", p_kill, h_kill );
	a_senda( buf );
	nogame = 1;
	for( j = 0 ; j < MAXPLAYER ; j++ )
	  *names[j] = '\0';
	num_players = 0;
	return;
      } else if( maze[room].players > 0 ) {
	for( j = 0 ; j < MAXPLAYER ; j++ ) {
	  if( *names[j] )
	    if( loc[j] == room ) {
	      a_sendu( names[j], "You are shot in the back by an arrow! You're dead!" );
	      h_kill++;
	      sprintf( buf, "%s shoots %s in the back. Ouch!", name, names[j] );
	      resign( names[j] );
	      a_senda( buf );
	      ammo[player] += 2;
	      break;
	    }
	}
      }
      break;
    }
  }
  for( i = 0 ; i < msize ; i++ ) {
    if( maze[i].contents == WUMPUS )
      break;
  }
  move_wumpus( i ); /* but this might kill the current player */

  (ammo[player])--;
  if( ammo[player] == 0 ) {
    a_sendu( name, "You are out of ammo! You die of heartbreak!" );
    a_senda( "You hear heartbroken wailing..." );
    resign( name );
  }
  if( *names[player] )
    move( name, -1 ); /* this will report ammo... */
}

init( name, size )
char *name;
int size;
{
  int i, j, k, ok, next, pit, bats, wumpus;
  char buf[80];
  int count;

  msize = size;

  if( strcmp( name, "Wumpus" ) ) {
    sprintf( buf, "%s has done an INIT of size %d.", name, size );
    a_sendu( "Wumpus", buf );
  }

  if( !nogame )
    a_sendu( name, "How rude, there was a game in progress" );

  if( (size % 2) != 0 ) {
    a_sendu( name, "Size has to be even you klutz!" );
    return;
  }

  if( size < 10 ) {
    a_sendu( name, "Need at least 10 rooms, minimum." );
    return;
  }

  srandom( time( (long *)NULL ) );
  
  maze = (struct room *)calloc( (unsigned) size, (unsigned) sizeof(struct room) );
  if( maze == NULL ) {
    a_sendu( name, "Malloc() failed!" );
    return;
  }
/*
 * the following algorithm is highly inefficient, but authentic :-)
 *
 * depends on the calloced memory (namely .doors) being zeroed.
 */

  for( i = 0 ; i < size ; i++ )
    maze[i].door[0] = maze[i].door[1] = maze[i].door[2] = -1;

  for( j = 0 ; j < 3 ; j++ )
    for( i = 0 ; i < size ; i++ ) {
      if( maze[i].door[j] == -1 ) {
	assert( maze[i].doors == j );
	ok = 0;
	/*  next isn't ok if there are too many connections to
         *  that room, or if we already connect to that room
         */
	count = 0;
	while( !ok ) {

	  count++;
	  if( count > 1000 ) {
	    printf( "i is %d\n",i );
	    printf( "j is %d\n",j );
	    for( j = 0 ; j < 3 ; j++ )
	      for( i = 0 ; i < size ; i++ ) {
		printf( "maze[i].doors = %d\n", maze[i].doors );
		printf( "maze[i].door[j] = %d\n", maze[i].door[j] );
	      }
	    exit(0);
	  }

	  next = irand(msize);
	  ok = 1;
	  if( next == i )
	    ok = 0;
	  if( maze[next].doors > 2 )
	    ok = 0;
	  if( count < 100 )
	    for( k = 0 ; k < maze[next].doors ; k++ )
	      if( maze[next].door[k] == i )
		ok = 0;
	}
	maze[next].door[maze[next].doors] = i;
	maze[i].door[maze[i].doors] = next;
	maze[next].doors++;
	maze[i].doors++;
      }
    }

/*
 * unfortunately the following isn't nearly so inefficient :-)
 */
  for( i = 0 ; i < 3 ; i++ ) {
    pit = get_empty();
    maze[pit].contents = PIT;
  }
  bats = get_empty();
  maze[bats].contents = BATS;
  wumpus = get_empty();
  maze[wumpus].contents = WUMPUS;

  for( i = 0 ; i < MAXPLAYER ; i++ )
    *names[i] = '\0';
  nogame = 0;
  w_kill = p_kill = h_kill = 0;
  num_players = 0;

  a_senda( "A new game of Wumpus has started. Use /m GM help for help," );
  a_senda( "or /ignore GM to not see any game messages." );

}

int get_empty()
{
  long spot;
  int count=0;

  spot = irand( msize );
  while( maze[spot].contents != 0 && maze[spot].players == 0 ) {
    spot = irand( msize );
    count++;
    assert( count < 1000 );
  }
  return( (int) spot );	
}

help( name )
char *name;
{
  a_sendu( name, "Commands available are:" );
  a_sendu( name, "JOIN MOVE SHOOT QUIT LOOK WHO RULES STATS" );
}

rules( name )
char *name;
{
  a_sendu( name, "Welcome to Hunt the Wumpus. The object of this game is to kill the Wumpus, before you get killed yourself. The Wumpus is a fearsome creature, and you have a bow and 5 arrows with which to kill it." );
  a_sendu( name, "Use MOVE # to move to other rooms, SHOOT # to shoot into an adjacent room. LOOK if you forget what's there, WHO to see who is playing. You can smell the Wumpus 2 rooms away -- don't walk into a room with him." );
  a_sendu( name, "Avoid bottomless pits and bats. These you can also detect when you are adjacent to them. The maze will stay the same until the Wumpus is killed, so make a map." );
}

int irand( size )
unsigned short size;
{
  return( (int) random() % (int) size );
}

sorry( name )
char *name;
{
  a_sendu( name, "I'm sorry, there is no game in progress at the moment." );
}

underway()
{
  a_senda( "A game of Wumpus is underway. Use /m GM help for help," );
  a_senda( "or /ignore GM to not see any game messages." );
  stats();
}

static int lasttime = 0;

stats(name)
char *name;
{
  char buf[256];

  if( nogame ) {
    sorry( name );
    return;
  }
    
  sprintf( buf, "So far, the Wumpus has eaten %d human%s, pits have claimed %d li%s, and other humans have shot %d.", w_kill, w_kill != 1 ? "s" : "", p_kill, p_kill != 1 ? "ves" : "fe", h_kill );

  if( time(0) > lasttime + 60 ) {
    a_senda( buf );
    lasttime = time(0);
  } else
    a_sendu( name, buf );
}

