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

#include <stdio.h>
#include "config.h"
#include "struct.h"
#include "sys.h"
#include "hash.h"
#include <ctype.h>

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

int	HashNickName(name)
Reg1	char	*name;
{
	Reg2	char	ch;
	Reg3	int	i = 0;
	Reg4	int	hash = 7, *tab;

	for (tab = hash_mult, i = 0; (ch = *name) && i < 10;
	     name++, i++, tab++) {
		if (islower(ch))
			ch ^= 0x20;
		hash += ch + *tab + hash + i + i;
	}
	if (hash < 0)
		hash = -hash;
	hash %= HASHSIZE;
	return (hash);
}

int	HashChannelName(name)
Reg1	char	*name;
{
	Reg2	char	ch;
	Reg3	int	i = 0;
	Reg4	int	hash = 0, *tab;

	for (tab = hash_mult, i = 0; (ch = *name) && i < 30;
	     name++, i++, tab++) {
		if (islower(ch))
			ch ^= 0x20;
		hash += ch + *tab + hash + i + i;
	}
	if (hash < 0)
		hash = -hash;
	hash %= CHANNELHASHSIZE;
	return (hash);
}

void	clearClientHashTable()
{
	register	int	i;

	for (i = 0; i < HASHSIZE; i++)
		bzero(&clientTable[i], sizeof(aHashEntry));
}

void	clearChannelHashTable()
{
	register	int	i;

	for (i = 0; i < CHANNELHASHSIZE; i++)
		bzero(&channelTable[i], sizeof(aHashEntry));
}

int	addToClientHashTable(name, cptr)
char	*name;
aClient	*cptr;
{
	return (addHashTableLink(name, cptr, clientTable,
		HashNickName(name)));
}

int	addToChannelHashTable(name, chptr)
char	*name;
aChannel	*chptr;
{
	return (addHashTableLink(name, chptr, channelTable,
		HashChannelName(name)));
}

static	int	addHashTableLink(name, cptr, table, hashv)
char	*name;
void	*cptr;
aHashEntry	table[];
int	hashv;
{
	Reg1	aHashLink	*tmp;

	if (!(tmp = (aHashLink *)MyMalloc(sizeof(aHashLink))))
		return 0;
	table[hashv].links++;
	table[hashv].hits++;
	bzero(tmp, sizeof(aHashLink));
	tmp->next = table[hashv].list;
	table[hashv].list = tmp;
	tmp->ptr.client = (aClient *)cptr;
	tmp->ptr.channel = (aChannel *)cptr;
	return 1;
}

int	delFromClientHashTable(name, cptr)
char	*name;
aClient	*cptr;
{
	return (delHashTableLink(name, cptr, clientTable, HashNickName(name)));
}

int	delFromChannelHashTable(name, chptr)
char	*name;
aChannel	*chptr;
{
	return (delHashTableLink(name, chptr, channelTable,
		HashChannelName(name)));
}

static	int	delHashTableLink(name, cptr, table, hashv)
char	*name;
aClient	*cptr;
aHashEntry	table[];
int	hashv;
{
	aHashLink	*tmp, *prev = (aHashLink *)NULL;

	for (tmp = table[hashv].list; tmp; tmp = tmp->next) {
		if (tmp->ptr.client == cptr) {
			if (prev)
				prev->next = tmp->next;
			else
				table[hashv].list = tmp->next;
			free(tmp);
			if (table[hashv].links > 0) {
				table[hashv].links--;
				return 1;
			} else
				return -1;
		}
		prev = tmp;
	}
	return 0;
}

aClient	*hash_find_client(name, cptr)
char	*name;
aClient	*cptr;
{
	int hashv;
	register	aHashLink	*tmp, *tmp2;
	aHashLink	*prv;
	aHashEntry	*tmp3;

	hashv = HashNickName(name);
	tmp3 = &clientTable[hashv];

	for (prv = (aHashLink *)NULL, tmp = tmp3->list; tmp;
	     prv = tmp, tmp = tmp->next) {
		if (mycmp(name, tmp->ptr.client->name) == 0)
			if (!cptr)
				goto c_move_to_top;
			else
				if (cptr == tmp->ptr.client)
					goto c_move_to_top;
	}
	return (aClient *)NULL;

c_move_to_top:
	if (prv) {
		tmp2 = tmp3->list;
		tmp3->list = tmp;
		prv->next = tmp->next;
		tmp->next = tmp2;
	}
	return (tmp->ptr.client);
}

aClient	*hash_find_server(server, cptr)
char	*server;
aClient	*cptr;
{
	register	aHashLink	*tmp, *prv;
	Reg1	aClient	*acptr;
	Reg2	char	*s, *t;
	Reg3	char	ch;
	aHashLink	*tmp2;
	aHashEntry	*tmp3;

	int hashv;

	hashv = HashNickName(server);
	tmp3 = &clientTable[hashv];

	for (prv = (aHashLink *)NULL, tmp = tmp3->list; tmp;
	     prv = tmp, tmp = tmp->next) {
		if (!IsServer(tmp->ptr.client) && !IsMe(tmp->ptr.client))
			continue;
		if (mycmp(server, tmp->ptr.client->name) == 0)
			if (!cptr)
				goto s_move_to_top;
			else
				if (cptr == tmp->ptr.client)
					goto s_move_to_top;
	}
	t = server + strlen(server);
	/*
	 * dont need to check IsServer() here since nicknames cant have
	 * *'s in them anyway.
	 */
	for (;;) {
		t--;
		for (; t >= server; t--)
			if (*t == '.' || *t == '*')
				break;
		if (*t != '.')
			break;
		s = t;
		if (--s < server)
			break;
		ch = *s;
		*s = '*';
		if (acptr = hash_find_client(s, (aClient *)NULL)) {
			*s = ch;
			return (acptr);
		}
		*s = ch;
	}
	return (cptr);

s_move_to_top:
	if (prv) {
		tmp2 = tmp3->list;
		tmp3->list = tmp;
		prv->next = tmp->next;
		tmp->next = tmp2;
	}
	return (tmp->ptr.client);
}

aChannel	*hash_find_channel(name, chptr)
char	*name;
aChannel	*chptr;
{
	int hashv;
	register	aHashLink	*tmp, *tmp2;
	aHashLink	*prv;
	aHashEntry	*tmp3;

	hashv = HashChannelName(name);
	tmp3 = &channelTable[hashv];

	for (prv = (aHashLink *)NULL, tmp = tmp3->list; tmp;
	     prv = tmp, tmp = tmp->next) {
		if (mycmp(name, tmp->ptr.channel->chname) == 0)
			if (!chptr)
				goto c_move_to_top;
			else
				if (chptr == tmp->ptr.channel)
					goto c_move_to_top;
	}
	return (aChannel *)NULL;

c_move_to_top:
	if (prv) {
		tmp2 = tmp3->list;
		tmp3->list = tmp;
		prv->next = tmp->next;
		tmp->next = tmp2;
	}
	return (tmp->ptr.channel);
}

#ifdef DEBUGMODE

/*
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 */

m_hash(cptr, sptr, parc, parv)
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
		return;
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
			if (hash_find_client(acptr->name, acptr) != acptr) {
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
		clearClientHashTable();
		for (acptr = client; acptr; acptr = acptr->next)
			addToClientHashTable(acptr->name, acptr);
		break;
	    }
	case 'R' :
	    {
		Reg1	aChannel	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
		clearChannelHashTable();
		for (acptr = channel; acptr; acptr = acptr->nextch)
			addToChannelHashTable(acptr->chname, acptr);
		break;
	    }
	case 'H' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   HashChannelName(parv[2]));
		return (0);
	case 'h' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   HashNickName(parv[2]));
		return (0);
	case 'n' :
	    {
		aHashLink *tmp;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % HASHSIZE;
		for (i = 0, tmp = clientTable[l].list; tmp;
		     i++, tmp = tmp->next)
			sendto_one(sptr,"NOTICE %s :Node: %d Link #%d is %s",
				   parv[0], l, i, tmp->ptr.client->name);
		return (0);
	    }
	case 'N' :
	    {
		aHashLink *tmp;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % CHANNELHASHSIZE;
		for (i = 0, tmp = channelTable[l].list; tmp;
		     i++, tmp = tmp->next)
			sendto_one(sptr,"NOTICE %s :Node: %d Link #%d is %s",
				   parv[0], l, i, tmp->ptr.channel->chname);
		return (0);
	    }
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
