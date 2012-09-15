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

#define	HASHSIZE	2003	/* prime number */
/*
 * choose hashsize from these:
 *
 * 293, 313, 337, 359, 379, 401, 421, 443, 463, 487, 509, 541,
 * 563, 587, 607, 631, 653, 673, 701, 721, 739, 761, 787, 809,
 * 907, 941, 983,1019,1051,1117,1163,1213,1249,1297,1319,1361,
 *1381,1427,1459,1493,1511,1567,1597,1607,1657,1669,1721,1759,
 *1801,1867,1889,1933,1987,2003,2011,2017,2027,2029,2039,2053,
 *2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,2131,2137,
 *2141,2143,2153,2161,2179,2203,2207,2213,2221,2237,2239,2243,
 *2251,2267,2269,2273,2281,2287,2293,2297,2309,2311,2333,2339,
 *2341,2347,2351,2357,2371,2377,2381,2383,2389,2393,2399,2411,
 *2417,2423,2437,2441,2447,2459,2467,2473,2477,2503,2521
 */

#define	CHANNELHASHSIZE	607	/* prime number */
