/************************************************************************
 *   IRC - Internet Relay Chat, VM/sys.h
 *   Copyright (C) 1990 Armin Gruner
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
#define SOCK_STREAM              0
#define SOCK_OPEN                1
#define SOCK_CLOSE               2
#define SOCK_READ                4
#define SOCK_EXTINTR             8
#define SOCK_FSIN_READ          16
#define SOCK_FSIN_ATTENTION     32
#define SOCK_ERROR              -1
#define SOCK_ERRORSTATE         -2
 
#define MINSOCKET 0
#define MAXSOCKET 32
 
#define AF_INET 1
#define AF_UNIX 4
#define AF_IUCV 4
#define AF_VMCF 8
#define AF_FS   16
 
#define NEWLINE                "\15\45"
#include <ctype.h>
#ifndef index 
#define index	strchr
#define rindex	strrchr
#endif
extern char *strchr(), *strrchr();
