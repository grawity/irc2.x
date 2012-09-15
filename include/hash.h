/************************************************************************
 *   IRC - Internet Relay Chat, include/hash.h
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

typedef	struct	hashentry {
	int	hits;
	int	links;
	void	*list;
	} aHashEntry;

#define	HASHSIZE	809	/* prime number */
/*
 * choose hashsize from these:
 *
 * 293, 313, 337, 359, 379, 401, 421, 443, 463, 487, 509, 541,
 * 563, 587, 607, 631, 653, 673, 701, 721, 739, 761, 787, 809
 */

#define	CHANNELHASHSIZE	293	/* prime number */
