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

CC = cc
INCLUDE = ../include
# use the next line if using MIPS:
# CFLAGS = -O -systype bsd43 -I${INCLUDE}
# and on all other systems:
CFLAGS = -g -I${INCLUDE}

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}'
SUBDIRS=include common ircd irc
SHELL=/bin/sh

all: build

build:
	@for i in $(SUBDIRS); do \
		echo "Building $$i";\
		cd $$i;\
		${MAKE} build; cd ..;\
	done

clean:
	rm -f *~ #* core
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
