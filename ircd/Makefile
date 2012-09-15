#************************************************************************
#*   IRC - Internet Relay Chat, ircd/Makefile
#*   Copyright (C) 1990 Jarkko Oikarinen
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

# IRCDMODE given in top level Makefile, but added here to make sure
# compilation works if started in ircd subdirectory

CC=cc
RM=/bin/rm
SHELL=/bin/sh

IRCDMODE=4711
INCLUDE=../include

# For MIPS, use the following:
# CFLAGS=-g -systype bsd43 -I${INCLUDE}
CFLAGS=-g -I${INCLUDE}

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

OBJS=channel.o date.o ircd.o list.o s_bsd.o s_conf.o s_msg.o s_numeric.o\
     whowas.o s_debug.o note.o class.o hash.o

SRC=channel.c date.c ircd.c list.c s_bsd.c s_conf.c s_msg.c s_numeric.c\
     whowas.c s_debug.c note.c class.c hash.c

COMMONOBJS=../common/bsd.o ../common/dbuf.o \
           ../common/packet.o ../common/send.o\
           ../common/match.o ../common/parse.o ../common/support.o

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}'

all: build

build: ircd

ircd: $(OBJS) $(COMMONOBJS) ../include/patchlevel.h
	$(SHELL) version.c.SH
	$(CC) $(CFLAGS) -c version.c
	$(CC) $(OBJS) $(COMMONOBJS) version.o -o ircd $(IRCDLIBS)
	chmod $(IRCDMODE) ircd

../common/parse.o:
	(cd ../common; ${MAKE} build);

../common/bsd.o:
	(cd ../common; ${MAKE} build);

../common/dbuf.o:
	(cd ../common; ${MAKE} build);

../common/packet.o:
	(cd ../common; ${MAKE} build);

../common/send.o:
	(cd ../common; ${MAKE} build);

../common/match.o:
	(cd ../common; ${MAKE} build);

../common/support.o:
	(cd ../common; ${MAKE} build);

clean:
	${RM} -f *.o *~ core ircd version.c #* *.bak

depend:
	makedepend -I${INCLUDE} ${SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.

note.o: ../include/struct.h ../include/numeric.h
channel.o: ../include/struct.h ../include/config.h ../include/dbuf.h
channel.o: ../include/numeric.h channel.h
date.o: ../include/struct.h ../include/config.h ../include/dbuf.h
ircd.o: ../include/struct.h ../include/config.h
ircd.o: ../include/dbuf.h ../include/numeric.h
list.o: ../include/struct.h ../include/config.h ../include/dbuf.h
list.o: ../include/sys.h
s_bsd.o: ../include/struct.h ../include/config.h ../include/dbuf.h
s_bsd.o: ../include/sys.h
s_conf.o: ../include/struct.h ../include/config.h ../include/numeric.h
s_conf.o: ../include/dbuf.h ../include/sys.h
s_msg.o: ../include/struct.h ../include/config.h
s_msg.o: ../include/dbuf.h ../include/sys.h channel.h
s_msg.o: ../include/msg.h ../include/numeric.h ../include/whowas.h
s_numeric.o: ../include/config.h ../include/sys.h
s_numeric.o: ../include/struct.h
s_numeric.o: ../include/dbuf.h ../include/numeric.h
whowas.o: ../include/struct.h ../include/config.h
whowas.o: ../include/dbuf.h ../include/sys.h
whowas.o: ../include/numeric.h
whowas.o: ../include/whowas.h
s_debug.o: ../include/struct.h
class.o: ../include/struct.h ../include/class.h
