/************************************************************************
 *   IRC - Internet Relay Chat, ircd/hash.c
 *   Copyright (C) 1991 Darren Reed
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
char hash_id[] = "hash.c v1.2 (c) 1991 Darren Reed";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "hash.h"

extern  char *MyMalloc();
extern	aClient	*client;
extern	aChannel	*channel;

aClient	*hash_find_client();

static	aHashEntry	clientTable[HASHSIZE];
static	aHashEntry	channelTable[CHANNELHASHSIZE];

static	int	hash_mult[] = { 103, 107, 109, 113, 127, 131,
				137, 139, 149, 151, 163, 167,
				173, 179, 181, 191, 193, 197,
				199, 211, 223, 227, 229, 233,
				239, 241, 251, 257, 263, 269,
				271, 277, 281, 293, 307, 311};
/*
 * this function must be *quick*.  Thus there should be no multiplication
 * or division or modulus in the inner loop.  subtraction and other bit
 * operations allowed.
 */

int	hash_nick_name(name)
Reg1	u_char	*name;
{
	Reg2	u_char	ch;
	Reg3	int	i = 0;
	Reg4	int	hash = 7, *tab;

	for (tab = hash_mult; (ch = *name) && i < 10; name++, i++, tab++) {
#ifdef USE_OUR_CTYPE
		hash += tolower(ch) + *tab + hash + i + i;
#else
		hash += (islower(ch) ? ch : tolower(ch)) + *tab + hash + i + i;
#endif
	}
	if (hash < 0)
		hash = -hash;
	hash %= HASHSIZE;
	return (hash);
}

int	hash_channel_name(name)
Reg1	u_char	*name;
{
	Reg2	u_char	ch;
	Reg3	int	i = 0;
	Reg4	int	hash = 5, *tab;

	for (tab = hash_mult; (ch = *name) && i < 30; name++, i++, tab++) {
#ifdef USE_OUR_CTYPE
		hash += tolower(ch) + *tab + hash + i + i;
#else
		hash += (islower(ch) ? ch : tolower(ch)) + *tab + hash + i + i;
#endif
	}
	if (hash < 0)
		hash = -hash;
	hash %= CHANNELHASHSIZE;
	return (hash);
}

void	clear_client_hash_table()
{
	register	int	i;

	for (i = 0; i < HASHSIZE; i++) {
		bzero(&clientTable[i], sizeof(aHashEntry));
		clientTable[i].list = (void *)NULL;
	}
}

void	clear_channel_hash_table()
{
	register	int	i;

	for (i = 0; i < CHANNELHASHSIZE; i++) {
		bzero(&channelTable[i], sizeof(aHashEntry));
	}
}

int	add_to_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	int	hashv;

	hashv = hash_nick_name(name);
	cptr->hnext = (aClient *)clientTable[hashv].list;
	clientTable[hashv].list = (void *)cptr;
	clientTable[hashv].links++;
	clientTable[hashv].hits++;
	return 0;
}

int	add_to_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	Reg1	int	hashv;

	hashv = hash_channel_name(name);
	chptr->hnextch = (aChannel *)channelTable[hashv].list;
	channelTable[hashv].list = (void *)chptr;
	channelTable[hashv].links++;
	channelTable[hashv].hits++;
	return 0;
}

int	del_from_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	aClient	*tmp, *prev = (aClient *)NULL;
	Reg2	int	hashv;

	hashv = hash_nick_name(name);
	for (tmp = (aClient *)clientTable[hashv].list; tmp; tmp = tmp->hnext)
	    {
		if (tmp == cptr) {
			if (prev)
				prev->hnext = tmp->hnext;
			else
				clientTable[hashv].list = (void *)tmp->hnext;
			tmp->hnext = (aClient *)NULL;
			if (clientTable[hashv].links > 0) {
				clientTable[hashv].links--;
				return 1;
			} else
				return -1;
		}
		prev = tmp;
	}
	return 0;
}

int	del_from_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	Reg1	aChannel	*tmp, *prev = (aChannel *)NULL;
	Reg2	int	hashv;

	hashv = hash_channel_name(name);
	for (tmp = (aChannel *)channelTable[hashv].list; tmp;
	     tmp = tmp->hnextch) {
		if (tmp == chptr) {
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv].list=(void *)tmp->hnextch;
			tmp->hnextch = (aChannel *)NULL;
			if (channelTable[hashv].links > 0) {
				channelTable[hashv].links--;
				return 1;
			} else
				return -1;
		}
		prev = tmp;
	}
	return 0;
}


/*
 * hash_find_client
 */
aClient	*hash_find_client(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	aClient	*tmp;
	Reg2	aClient	*prv = (aClient *)NULL;
	Reg3	aHashEntry	*tmp3;
	int	hashv;

	hashv = hash_nick_name(name);
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	    {
		if (mycmp(name, tmp->name) == 0)
			if (!cptr)
				goto c_move_to_top;
			else
				if (cptr == tmp)
					goto c_move_to_top;
	    }
	return (cptr);

c_move_to_top:
	if (prv) {
		aClient *tmp2;

		tmp2 = (aClient *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	}
	return (tmp);
}

aClient	*hash_find_server(server, cptr)
char	*server;
aClient	*cptr;
{
	Reg1	aClient	*tmp, *prv = (aClient *)NULL;
	Reg2	char	*t;
	Reg3	char	ch;
	aHashEntry	*tmp3;

	int hashv;

	hashv = hash_nick_name(server);
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	    {
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		if (mycmp(server, tmp->name) == 0)
			if (!cptr)
				goto s_move_to_top;
			else
				if (cptr == tmp)
					goto s_move_to_top;
	    }
	t = ((char *)server + strlen(server));
	/*
	 * dont need to check IsServer() here since nicknames cant have
	 * *'s in them anyway.
	 */
	for (;;) {
		t--;
		for (; t > server; t--)
			if (*(t+1) == '.')
				break;
		if (*t == '*' || t == server)
			break;
		ch = *t;
		*t = '*';
		if ((tmp = hash_find_client(t, (aClient *)NULL))!=NULL) {
			*t = ch;
			return (tmp);
		}
		*t = ch;
	}
	return (cptr);

s_move_to_top:
	if (prv)
	    {
		aClient *tmp2;

		tmp2 = (aClient *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	    }
	return (tmp);
}

aChannel	*hash_find_channel(name, chptr)
char	*name;
aChannel	*chptr;
{
	int hashv;
	register	aChannel	*tmp;
	aChannel	*prv = (aChannel *)NULL;
	aHashEntry	*tmp3;

	hashv = hash_channel_name(name);
	tmp3 = &channelTable[hashv];

	for (tmp = (aChannel *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnextch)
	    {
		if (mycmp(name, tmp->chname) == 0)
			if (!chptr)
				goto c_move_to_top;
			else
				if (chptr == tmp)
					goto c_move_to_top;
	    }
	return (aChannel *)NULL;

c_move_to_top:
	if (prv)
	    {
		register aChannel *tmp2;

		tmp2 = (aChannel *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnextch = tmp->hnextch;
		tmp->hnextch = tmp2;
	    }
	return (tmp);
}

#ifdef DEBUGMODE

/*
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 */

int m_hash(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	register	int	l, i;
	register	aHashEntry	*tab;
	int	deepest = 0, deeplink = 0, showlist = 0, tothits = 0;
	int	mosthit = 0, mosthits = 0, used = 0, used_now = 0, totlink = 0;
	int	link_pop[10], size = HASHSIZE;
	char	ch;
	aHashEntry	*table;

	if (!IsRegisteredUser(sptr) || !MyConnect(sptr))
		return 0;
	if (parc > 1) {
		ch = *parv[1];
		if (islower(ch))
			table = clientTable;
		else {
			table = channelTable;
			size = CHANNELHASHSIZE;
		}
		if (ch == 'L' || ch == 'l')
			showlist = 1;
	}
	else {
		ch = '\0';
		table = clientTable;
	}

	for (i = 0; i < 10; i++)
		link_pop[i] = 0;
	for (i = 0; i < size; i++) {
		tab = &table[i];
		l = tab->links;
		if (showlist)
		    sendto_one(sptr,
			   "NOTICE %s :Hash Entry:%6d Hits:%7d Links:%6d",
			   parv[0], i, tab->hits, l);
		if (l > 0) {
			if (l < 10)
				link_pop[l]++;
			else
				link_pop[9]++;
			used_now++;
			totlink += l;
			if (l > deepest) {
				deepest = l;
				deeplink = i;
			}
		}
		else
			link_pop[0]++;
		l = tab->hits;
		if (l) {
			used++;
			tothits += l;
			if (l > mosthits) {
				mosthits = l;
				mosthit = i;
			}
		}
	}
	switch((int)ch)
	{
	case 'V' : case 'v' :
	    {
		register	aClient	*acptr;
		int	bad = 0, listlength = 0;

		for (acptr = client; acptr; acptr = acptr->next) {
			if (hash_find_client(acptr->name,acptr) != acptr) {
				if (ch == 'V')
				sendto_one(sptr, "NOTICE %s :Bad hash for %s",
					   parv[0], acptr->name);
				bad++;
			}
			listlength++;
		}
		sendto_one(sptr,"NOTICE %s :List Length: %d Bad Hashes: %d",
			   parv[0], listlength, bad);
	    }
	case 'P' : case 'p' :
		for (i = 0; i < 10; i++)
			sendto_one(sptr,"NOTICE %s :Entires with %d links : %d",
			parv[0], i, link_pop[i]);
		return (0);
	case 'r' :
	    {
		Reg1	aClient	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
		clear_client_hash_table();
		for (acptr = client; acptr; acptr = acptr->next)
			add_to_client_hash_table(acptr->name, acptr);
		break;
	    }
	case 'R' :
	    {
		Reg1	aChannel	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
		clear_channel_hash_table();
		for (acptr = channel; acptr; acptr = acptr->nextch)
			add_to_channel_hash_table(acptr->chname, acptr);
		break;
	    }
	case 'H' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   hash_channel_name(parv[2]));
		return (0);
	case 'h' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   hash_nick_name(parv[2]));
		return (0);
	case 'n' :
	    {
		aClient	*tmp;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % HASHSIZE;
		for (i = 0, tmp = (aClient *)clientTable[l].list; tmp;
		     i++, tmp = tmp->hnext)
			sendto_one(sptr,"NOTICE %s :Node: %d Link #%d is %s",
				   parv[0], l, i, tmp->name);
		return (0);
	    }
	case 'N' :
	    {
		aChannel *tmp;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % CHANNELHASHSIZE;
		for (i = 0, tmp = (aChannel *)channelTable[l].list; tmp;
		     i++, tmp = tmp->hnextch)
			sendto_one(sptr,"NOTICE %s :Node: %d Link #%d is %s",
				   parv[0], l, i, tmp->chname);
		return (0);
	    }
	case 'S' :
	    sendto_one(sptr, "NOTICE %s :s_bsd.c SBSDC ircd.c IRCDC",
			parv[0]);
	    sendto_one(sptr, "NOTICE %s :channel.c CHANC s_misc.c SMISC",
			parv[0]);
	    sendto_one(sptr, "NOTICE %s :hash.c HASHC version.c.SH VERSH",
			parv[0]);
	    return 0;
	default :
		break;
	}
	sendto_one(sptr,"NOTICE %s :Entries Hashed: %d NonEmpty: %d of %d",
		   parv[0], totlink, used_now, size);
	if (used_now == 0)
		used_now = 1;
	sendto_one(sptr,"NOTICE %s :Hash Ratio (av. depth): %f %Full: %f",
		  parv[0], (float)((1.0 * totlink) / (1.0 * used_now)),
		  (float)((1.0 * used_now) / (1.0 * size)));
	sendto_one(sptr,"NOTICE %s :Deepest Link: %d Links: %d",
		   parv[0], deeplink, deepest);
	if (used == 0)
		used = 1;
	sendto_one(sptr,"NOTICE %s :Total Hits: %d Unhit: %d Av Hits: %f",
		   parv[0], tothits, size-used,
		   (float)((1.0 * tothits) / (1.0 * used)));
	sendto_one(sptr,"NOTICE %s :Entry Most Hit: %d Hits: %d",
		   parv[0], mosthit, mosthits);
	return 0;
}
#endif
