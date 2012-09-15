#************************************************************************
#*   IRC - Internet Relay Chat, Makefile
#*   Copyright (C) 1990, Jarkko Oikarinen
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

CC=cc 
RM=/bin/rm
INCLUDEDIR=../include

# use the following on MIPS:
# CFLAGS=-systype bsd43 -I$(INCLUDEDIR)
# on NEXT use:
# CFLAGS=-bsd -I$(INCLUDEDIR)
#otherwise this:
CFLAGS=-I$(INCLUDEDIR)

#use the following on SUN OS without nameserver libraries inside libc
# IRCDLIBS=-lresolv
#
#on NeXT other than 2.0:
# IRCDLIBS=-lsys_s
#
# HPUX:
# IRCDLIBS=-lBSD
#
#and otherwise:
IRCDLIBS=

# IRCDMODE is the mode you want the binary to be.
# the 4 at the front is important (allows for setuidness)
IRCDMODE = 4711

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}'
SHELL=/bin/sh
SUBDIRS=common ircd irc
# use this if you don't want the default client compiled
# SUBDIRS=common ircd

all:	build

build:
	@for i in $(SUBDIRS); do \
		echo "Building $$i";\
		cd $$i;\
		${MAKE} build; cd ..;\
	done

clean:
	${RM} -f *~ #* core
	@for i in $(SUBDIRS); do \
		echo "Cleaning $$i";\
		cd $$i;\
		${MAKE} clean; cd ..;\
	done

depend:
	@for i in $(SUBDIRS); do \
		echo "Making dependencies in $$i";\
		cd $$i;\
		${MAKE} depend; cd ..;\
	done

install:
	echo "Installing...";

rcs:
	cii -H -R Makefile common include irc ircd
