#!/bin/make -f
#************************************************************************
#*   IRC - Internet Relay Chat, lib/ircd/Makefile
#*   Copyright (C) 1990  Casie Ashley Dyerman
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
# $Header: Makefile,v 2.3 90/04/06 00:27:40 casie Exp $
# Makefile for the lib-level of the source tree
#
#	Send bugs and suggestions to:
#
#		casie@earth.CChem.Berkeley.EDU
#
# $Log:	Makefile,v $
# 
# Revision 2.3  90/04/06  00:27:40  casie
#  alpha release
# 
SYS=bsd
MAKE=make
TEST=test -f
CC=cc
CFLAGS=-g

INCL=../../include

AR=ar
ARFLAGS=rv
RANLIB=ranlib

GET=co
CLEAN=rcsclean

LINT=lint
LINTFLAGS=-I${INCL}

UBIN=/usr/bin
 
MSRC=Makefile
HSRC=${INCL}/config.h ${INCL}/msg.h ${INCL}/numeric.h ${INCL}/patchlevel.h \
	 ${INCL}/sock.h ${INCL}/struct.h ${INCL}/sys.h
LSRC=dbuf.c debug.c packet.c parse.c send.c

LOBJ=dbuf.o debug.o packet.o parse.o send.o sys.o

SDEP=sys.c
SSRC=bsd.c sysv.c

TSRC=${MSRC} ${LSRC} ${SSRC}

OBJS=${LOBJ}

# Standard targets

all:	${HSRC} ${SDEP} libIrcd.a
 
libIrcd.a: ${LOBJ}
	${AR} ${ARFLAGS} libIrcd.a ${LOBJ}
	${TEST} ${UBIN}/${RANLIB} && ${RANLIB} libIrcd.a


#	Dependencies and rules for compiling objects
 
lint:
	${LINT} ${LINTFLAGS} ${LDEF} ${LSRC} 
 
sources: ${TSRC}
 
names:
	@for i in ${TSRC}; do echo ${SRC_PREFIX}$$i; done

clean:
	rm -f ${OBJS}
 
clobber:
	rm -f ${OBJS}
	rm -f ${SDEP}
	rm -f libIrcd.a
 
nuke:
	rm -f ${OBJS}
	rm -f ${SDEP}
	rm -f libIrcd.a
	-${CLEAN} ${TSRC}
	${GET} Makefile

${TSRC}:
	${GET} $@
 
${SDEP}:
	@echo "Generating system dependancy sys.c from ${SYS}.c"
	@ln ${SYS}.c sys.c

${LOBJ}:
	@echo -n "     "
	${CC} ${CFLAGS} -I${INCL} ${LDEF} -c $<

${HSRC}:
	@echo "Build targets under ${SRC_PREFIX}${INCL} ..."
		(cd ${INCL}; ${MAKE} SRC_PREFIX=${SRC_PREFIX}${INCL}/ $${SDEP})


#	Dependencies for object files

dbuf.o		: ${INCL}/dbuf.h  dbuf.c
parse.o		: ${INCL}/struct.h  ${INCL}/config.h  ${INCL}/sys.h  ${INCL}/msg.h
parse.o		: ${INCL}/numeric.h  ${INCL}/dbuf.h  parse.c
send.o		: ${INCL}/struct.h  ${INCL}/config.h  ${INCL}/dbuf.h  send.c 
debug.o		: ${INCL}/struct.h  ${INCL}/config.h  ${INCL}/dbuf.h  debug.c 
sys.o		: ${INCL}/struct.h  ${INCL}/config.h  ${INCL}/dbuf.h  sys.c 
