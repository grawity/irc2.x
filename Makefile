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

# Default flags:
CFLAGS= -I$(INCLUDEDIR) -g
IRCDLIBS=
IRCLIBS=-lcurses -ltermcap
#
# use the following on MIPS:
#CFLAGS= -systype bsd43 -DSYSTYPE_BSD43 -I$(INCLUDEDIR)
# For Irix 4.x (SGI), use the following:
#CFLAGS= -g -cckr -I${INCLUDE}
#
# on NEXT use:
#CFLAGS=-bsd -I$(INCLUDEDIR)
#on NeXT other than 2.0:
#IRCDLIBS=-lsys_s
#
# AIX 370 flags
#CLFAGS=-D_BSD -Hxa
#IRCDLIBS=-lbsd
#IRCLIBS=-lcurses -lcur
#
# Dynix/ptx V1.3.1
#IRCDLIBS= -lsocket -linet -lseq -lnsl
#
#use the following on SUN OS without nameserver libraries inside libc
#IRCDLIBS= -lresolv
#
# Solaris 2.0
#IRCDLIBS= -lsocket -lnsl
#IRCLIBS=-lcurses -ltermcap -lsocket -lnsl

# LDFLAGS - flags to send the loader (ld). SunOS users may want to add
# -Bstatic here.
#
#LDFLAGS=-Bstatic

# IRCDMODE is the mode you want the binary to be.
# The 4 at the front is important (allows for setuidness)
#
# WARNING: if you are making ircd SUID or SGID, check config.h to make sure
#          you are not defining CMDLINE_CONFIG 
IRCDMODE = 711

# IRCDDIR must be the same as DPATH in include/config.h
#
IRCDDIR=/usr/local/lib/ircd

SHELL=/bin/sh
SUBDIRS=common ircd irc
BINDIR=/usr/local/bin
MANDIR=/usr/local/man
INSTALL=/usr/bin/install

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
            'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}' 'BINDIR=${BINDIR}' \
            'INSTALL=${INSTALL}' 'IRCLIBS=${IRCLIBS}' 'IRCDDIR=${IRCDDIR}'

all:	build

server:
	@echo 'Making server'; cd ircd; ${MAKE} build; cd ..;

client:
	@echo 'Making client'; cd irc; ${MAKE} build; cd ..;

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

install: all
	@for i in ircd irc; do \
		echo "Installing $$i";\
		cd $$i;\
		${MAKE} install; cd ..;\
	done
	${INSTALL} -c doc/ircd.8 ${MANDIR}/man8
	${INSTALL} -c doc/irc.1 ${MANDIR}/man1


rcs:
	cii -H -R Makefile common include ircd

