#!/usr/bin/make -f
#************************************************************************
#*   IRC - Internet Relay Chat, ircd/Makefile
#*   Copyright (C) 1989,1990 Casie Ashley Dyerman
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
# $Header: Makefile,v 2.3 90/04/06 00:25:53 casie Exp $
# Makefile for the server-level of the IRC source tree
#
# Revision 2.4  90/05/10  01:39:00  Jto
#  Some fixes to dependencies
#
# Revision 2.3  90/04/06  00:25:53  casie
#  alpha release
# 
# Revision 1.1  90/03/11  20:03:33  casie
# Initial revision
# 
 
INCL=../include
LSRV=../lib/ircd
LMSC=../lib
UBIN=/usr/bin
 
GET=co
CLEAN=rcsclean

LINT=lint
LFLAGS=-I${I}

MK=make -${MAKEFLAGS}
CC=cc 

SYS=bsd
CFLAGS= -g
 
MSRC=Makefile ./mkversion.sh
FSRC=copyright.c
HSRC=${INCL}/config.h ${INCL}/msg.h ${INCL}/numeric.h ${INCL}/patchlevel.h \
	 ${INCL}/sock.h ${INCL}/struct.h ${INCL}/sys.h
ISRC=channel.c date.c ircd.c list.c s_conf.c s_msg.c s_numeric.c whowas.c
LSRC=version.c

FOBJ=copyright.o
LOBJ=version.o
IOBJ=channel.o date.o ircd.o list.o s_conf.o s_msg.o s_numeric.o s_sys.o\
	whowas.o

TOBJ=${FOBJ} ${IOBJ} ${LOBJ}

SDEP=s_sys.c
SSRC=s_bsd.c s_sysv.c

SRC=${MSRC} ${SSRC} ${ISRC} ${FSRC}
TSRC=${MSRC} ${SSRC} ${ISRC} ${FSRC} ${LSRC}

IRCLIBS=${LSRV}/libIrcd.a 

RMFILES= *~ #*# core 

# Standard targets
 
all:	${HSRC} ${SDEP} ircd

ircd:	${FOBJ} ${IOBJ} ${IRCLIBS} ${LOBJ} ./mkversion.sh
	@echo "Generating version.c";
	sh ./mkversion.sh; echo -n "     "
	${CC} ${CFLAGS} -I${INCL} -c version.c
	@echo -n "     "
	${CC} ${CFLAGS} -o ircd \
		${FOBJ} ${IOBJ} ${LOBJ} ${IRCLIBS} ${LIBS} 
	@echo -n "     "
	chmod 4711 ircd

lint:
	${LINT} ${LINTFLAGS} ${ISRC}
 
sources: ${SRC}
 
${SRC}:
	${GET} $@
 
names:
	@for i in ${TSRC}; do echo ${SRC_PREFIX}$$i; done

clean:
	rm -f ${TOBJ}
 
clobber:
	rm -f ${TOBJ}
	rm -f s_sys.c
	rm -f version.c
	rm -f ircd
 
nuke:
	rm -f ${TOBJ} ${RMFILES}
	rm -f s_sys.c
	rm -f version.c
	rm -f ircd
	-${CLEAN} ${SRC}
	-${GET} Makefile


#	Dependencies and rules for compiling objects

.c.o:
	@echo -n "     "
	${CC} ${CFLAGS} -I${INCL} -c $<

${SDEP}: s_${SYS}.c
	@echo "Generating dependency s_sys.c from s_${SYS}.c"
	@ln s_${SYS}.c s_sys.c

${LSRC}: mkversion.sh
	@echo "     Generating dependency version.c"
	@mkversion.sh 

${IRCLIBS}:
	@echo "Building server library under ${SRC_PREFIX}${LSRV} ..."
	@(cd ${LSRV}; ${MAKE} SRC_PREFIX=${SRC_PREFIX}${LSRV}/ $${IRCLIBS})
 
${HSRC}:
	@echo "Building missing headers under ${SRC_PREFIX}${INCL} ..."
	@(cd ${INCL}; ${MAKE} SRC_PREFIX=${SRC_PREFIX}${INCL}/ $${SDEP})
 

#	Dependencies for object files

channel.o:   ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h channel.c 
date.o:      ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h date.c 
ircd.o:      ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h ircd.c 
list.o:      ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h list.c 
s_conf.o:    ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h  
s_conf.o:    ${INCL}/sys.h s_conf.c 
s_msg.o	:    ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h
s_msg.o	:    ${INCL}/numeric.h ${INCL}/sys.h ${INCL}/msg.h
s_msg.o	:    ${INCL}/whowas.h s_msg.c 
s_numeric.o: ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h
s_numeric.o: ${INCL}/sys.h ${INCL}/numeric.h s_numeric.c 
s_rsummon.o: ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h rsummon.c
s_sys.o	:    ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h 
s_sys.o	:    ${INCL}/sys.h s_sys.c
whowas.o:    ${INCL}/struct.h ${INCL}/config.h ${INCL}/dbuf.h
whowas.o:    ${INCL}/numeric.h ${INCL}/whowas.h whowas.c
version.o:   ${INCL}/patchlevel.h version.c
