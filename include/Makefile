#!/bin/make -f
#************************************************************************
#*   IRC - Internet Relay Chat, include/Makefile
#*   Copyright (C) 1990
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
# $Header: Makefile,v 2.3 90/04/06 00:23:00 casie Exp $
# Makefile for the include-level of the source tree
#
# $Log:	Makefile,v $
# Revision 2.3  90/04/06  00:23:00  casie
# alpha release
# 

SYS=bsd

GET=co
CLEAN=rcsclean

MAKE_SRC=Makefile
HEAD_SRC=config.h msg.h numeric.h patchlevel.h sock.h struct.h

SYSTEMS_DEP=sys.h
SYSTEMS_SRC=bsd.h sysv.h

SRC=${HEAD_SRC} ${SYSTEMS_SRC} ${MAKE_SRC}


# Standard targets

all:	${SRC} ${SYSTEMS_DEP}
 
sources: ${SRC}
 
${SRC}:
	${GET} $@
 
names:
	@for i in ${SRC}; do echo ${SRC_PREFIX}$$i; done

clean:
	@echo "Nothing to clean here."
 
clobber:
	rm -f ${SYSTEMS_DEP}
 
nuke:
	rm -f ${SYSTEMS_DEP}
	-${CLEAN}
	${GET} Makefile
 
${SYSTEMS_DEP}:
	@echo "Generating system dependency sys.h from ${SYS}.h"
	@ln ${SYS}.h sys.h
