/************************************************************************
 *   IRC - Internet Relay Chat, VM/config.h
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
 
#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7 register
#define Reg8 register
#define Reg9 register
#define Reg10 register
#define Reg11
#define Reg12
#define Reg13
#define Reg14
#define Reg15
#define Reg16

/*
** Of course the passwd should be crypted in any way ...
** Modules in VM are readable, so the passwd would be
** present in *plain* text
*/
 
#undef  DEBUG
#define VMSP
#define GETPASS
#define CLIENT
#define UPHOST   "131.159.0.76" /* DEFAULT SERVER IF NO SPECIFIED */
#undef IRCPASS    /* if defined: IRCPASS sent to UPHOST */
 
#define MAXUNKILLS  5
#define BUFFERLEN 8192
#define VERSION  "2.6.1VM"
 
#define PORTNUM  6667             /* assumed portnumber */
#define ESC_HI "\35\10"
#define ESC_NORM "\35\20"
 
#define isascii(x) (isalnum(x) || iscntrl(x) || isgraph(x))
 
#define BANNER   ESC_HI "IRC" ESC_NORM VERSION
#define WELCOME  \
"*** Internet Relay Chat *** Type /help to get help *** Client V" VERSION " ***"
