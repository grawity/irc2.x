#!/bin/make -f
#************************************************************************
#*   IRC - Internet Relay Chat, Makefile
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
# $Header: Makefile,v 2.3 90/04/06 00:29:48 casie Exp $
# Top-level makefile for the irc source tree
#
# $Log:	Makefile,v $
# 
# Revision 2.3  90/04/06  00:29:48  casie
#  alpha release
# 

# SYS = sysv   SysV NOT working... under construction...
# Use bsd for HPUX, SUNs, ULTRIX, ...

SYS=bsd

#if compiling on a mips, uncomment the next line
#SYSOPT=-systype bsd43

CFLAGS = ${SYSOPT} -g 
CC = cc 

SHELL=/bin/sh
MAKE=make
MK=${MAKE} 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'SYS=${SYS}'

RM=rm -f
GET=co
CLEAN=rcsclean 

SRC_PREFIX=
 
SRC=Makefile README

#SUB_DIRS=include lib ircd irc ircII-1.75 services
SUB_DIRS=include lib ircd

# Standard targets

all:	build_irc
 
build_irc:
	@for i in ${SUB_DIRS}; do \
		echo "Building targets under ${SRC_PREFIX}$$i ..."; \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/); \
		echo; \
	 done	# $(MAKE)
 
sources: ${SRC}
	@for i in ${SUB_DIRS}; do \
		echo "Get sources under ${SRC_PREFIX}$$i ..."; \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/ sources); \
	 done	# $(MAKE)
 
${SRC}:
	${GET} $@
 
names:
	@for i in ${SRC}; do echo ${SRC_PREFIX}$$i; done
	@for i in ${SUB_DIRS}; do \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/ names); \
	 done	# $(MAKE)
 
clean:
	@$(RM) $(RMFILES)
	@for i in ${SUB_DIRS}; do \
		echo "Cleaning under ${SRC_PREFIX}$$i ..."; \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/ clean); \
	 done	# $(clean)
 
clobber:
	@for i in ${SUB_DIRS}; do \
		echo "Clobbering under ${SRC_PREFIX}$$i ..."; \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/ clobber); \
	 done	# $(clobber)
 
nuke:
	-${CLEAN} ${SRC}
	-${GET} Makefile
	@for i in ${SUB_DIRS}; do \
		echo "Nuking under ${SRC_PREFIX}$$i ..."; \
		(cd $$i; ${MK} SRC_PREFIX=${SRC_PREFIX}$$i/ nuke); \
	 done	# $(nuke)
