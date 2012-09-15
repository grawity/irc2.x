/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas.c
 *   Copyright (C) 1990 Markku Savela
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
 * --- avalon --- 6th April 1992
 * rewritten to scrap linked lists and use a table of structures which
 * is referenced like a circular loop. Should be faster and more efficient.
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"

typedef struct aname {
	anUser	*ww_user;
	aClient	*ww_online;
	long	ww_logout;
	char	ww_nick[NICKLEN+1];
	char	ww_info[REALLEN+1];
} aName;


static	aName	was[NICKNAMEHISTORYLENGTH];
static	int	ww_index = 0;

int add_history(cptr)
aClient	*cptr;
{
	aName	ntmp;

	strncpyzt(ntmp.ww_nick, cptr->name, NICKLEN);
	strncpyzt(ntmp.ww_info, cptr->info, REALLEN);
	ntmp.ww_user = cptr->user;
	ntmp.ww_logout = time(NULL);
	ntmp.ww_online = cptr->from ? cptr : NULL;
	ntmp.ww_user->refcnt++;

	if (was[ww_index].ww_user)
		free_user(was[ww_index].ww_user);

	bcopy(&ntmp, &was[ww_index], sizeof(aName));

	ww_index++;
	if (ww_index >= NICKNAMEHISTORYLENGTH)
		ww_index = 0;
	return 0;
}

/*
** GetHistory
**      Return the current client that was using the given
**      nickname within the timelimit. Returns NULL, if no
**      one found...
*/
aClient	*get_history(nick, timelimit)
char	*nick;
u_long	timelimit;
{
	Reg1	int	i;
	aName	*wptr = NULL;

	i = ww_index;
	timelimit = time(NULL)-timelimit;

	do {
		if (!mycmp(nick, was[i].ww_nick) &&
		    was[i].ww_logout >= timelimit)
		    {
			wptr = &was[i];
			break;
		    }
		i++;
		if (i >= NICKNAMEHISTORYLENGTH)
			i = 0;
	} while (i != ww_index);

	if (wptr)
		return (wptr->ww_online);
	return (NULL);
}

int off_history(cptr)
aClient	*cptr;
{
	Reg1	int	i;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		if (was[i].ww_online == cptr)
			was[i].ww_online = (aClient *)NULL;
	return 0;
}

int	init_whowas()
{
	Reg1	int	i;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		bzero(&was[i], sizeof(aName));
	return 0;
}


/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int m_whowas(cptr, sptr, parc, parv)
aClient	*cptr;
aClient	*sptr;
int	parc;
char	*parv[];
    {
	Reg1	aName	*nptr = (aName *)NULL;
	Reg2	int	i;

 	if (parc < 2)
	    {
		sendto_one(sptr, ":%s %d %s :No nickname specified",
			   me.name, ERR_NONICKNAMEGIVEN, sptr->name);
		return 0;
	    }

	i = ww_index;

	do {
		if (mycmp(parv[1], was[i].ww_nick) == 0)
		    {
			nptr = &was[i];

			sendto_one(sptr,":%s %d %s %s %s %s * :%s",
				   me.name, RPL_WHOWASUSER,
				   sptr->name, nptr->ww_nick,
				   nptr->ww_user->username,
				   nptr->ww_user->host,
				   nptr->ww_info);
			sendto_one(sptr,":%s %d %s %s %s :Signoff: %s",
				   me.name, RPL_WHOISSERVER,
				   sptr->name, nptr->ww_nick,
				   nptr->ww_user->server,
				   myctime(nptr->ww_logout));
			if (nptr->ww_user->away)
				sendto_one(sptr,":%s %d %s %s :%s",
					   me.name, RPL_AWAY,
					   sptr->name, nptr->ww_nick,
					   nptr->ww_user->away);
		    }
		i++;
		if (i >= NICKNAMEHISTORYLENGTH)
			i = 0;
	} while (i != ww_index);

	if (nptr == (aName *)NULL)
		sendto_one(sptr, ":%s %d %s %s :There was no such nickname",
			   me.name, ERR_WASNOSUCHNICK, sptr->name, parv[1]);
	return 0;
    }


#ifdef DEBUGMODE
int count_whowas_memory(wwu, wwa, wwam)
int	*wwu, *wwa;
u_long	*wwam;
{
	int	u = 0, a = 0, i = 0;
	u_long	am = 0;
	anUser	*tmp;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		if (tmp = was[i].ww_user)
			if (!was[i].ww_online)
			    {
				u++;
				if (tmp->away)
				    {
					a++;
					am += (strlen(tmp->away)+1);
				    }
			    }
	*wwu = u;
	*wwa = a;
	*wwam = am;

	return 0;
}
#endif
