#************************************************************************
#*   IRC - Internet Relay Chat, common/Makefile
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

CC=cc
RM=/bin/rm
INCLUDE=../include

# For MIPS, use:
# CFLAGS=-O -systype bsd43 -I${INCLUDE}
# and otherwise:
CFLAGS=-O -I${INCLUDE}


OBJS=bsd.o dbuf.o packet.o send.o match.o parse.o

SRC=bsd.c dbuf.c packet.c parse.c send.c match.c

all: build

build: $(OBJS)

clean:
	${RM} -f *.o *~ core #* *.bak

depend:
	makedepend -I${INCLUDE} ${SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.

match.o: ../include/struct.h ../include/sys.h
bsd.o: ../include/struct.h ../include/config.h
bsd.o: ../include/dbuf.h
dbuf.o: ../include/struct.h ../include/config.h ../include/dbuf.h
dbuf.o: ../include/sys.h
packet.o: ../include/struct.h ../include/config.h ../include/dbuf.h
packet.o: ../include/msg.h
parse.o: ../include/msg.h ../include/struct.h
parse.o: ../include/config.h ../include/dbuf.h ../include/sys.h
parse.o: ../include/numeric.h
send.o: ../include/struct.h ../include/config.h ../include/dbuf.h
