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

OBJS=channel.o date.o ircd.o list.o s_bsd.o s_conf.o s_msg.o s_numeric.o\
     whowas.o s_debug.o mail.o

SRC=channel.c date.c ircd.c list.c s_bsd.c s_conf.c s_msg.c s_numeric.c\
     whowas.c s_debug.c mail.c

COMMONOBJS=../common/bsd.o ../common/dbuf.o \
           ../common/packet.o ../common/parse.o ../common/send.o\
           ../common/match.o

INCLUDE = ../include

CFLAGS = -g -I${INCLUDE}

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}'

all: build

build: ircd

ircd: $(OBJS) $(COMMONOBJS) ../include/patchlevel.h
	sh mkversion.sh
	$(CC) $(CFLAGS) -c version.c
	$(CC) $(OBJS) $(COMMONOBJS) version.o -o ircd
	chmod 4711 ircd

../common/bsd.o:
	(cd ../common; ${MAKE} build);

../common/dbuf.o:
	(cd ../common; ${MAKE} build);

../common/packet.o:
	(cd ../common; ${MAKE} build);

../common/parse.o:
	(cd ../common; ${MAKE} build);

../common/send.o:
	(cd ../common; ${MAKE} build);

../common/match.o:
	(cd ../common; ${MAKE} build);

clean:
	rm -f *.o *~ core ircd version.c #* *.bak

depend:
	makedepend -I${INCLUDE} ${SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.

mail.o: ../include/struct.h ../include/numeric.h
channel.o: ../include/struct.h ../include/config.h ../include/dbuf.h
channel.o: ../include/numeric.h
date.o: ../include/struct.h ../include/config.h ../include/dbuf.h
ircd.o: ../include/struct.h ../include/config.h
ircd.o: ../include/dbuf.h ../include/numeric.h
list.o: ../include/struct.h ../include/config.h ../include/dbuf.h
list.o: ../include/sys.h
s_bsd.o: ../include/struct.h ../include/config.h ../include/dbuf.h
s_conf.o: ../include/struct.h ../include/config.h ../include/numeric.h
s_conf.o: ../include/dbuf.h ../include/sys.h
s_msg.o: ../include/struct.h ../include/config.h
s_msg.o: ../include/dbuf.h ../include/sys.h
s_msg.o: ../include/msg.h ../include/numeric.h ../include/whowas.h
s_numeric.o: ../include/config.h ../include/sys.h
s_numeric.o: ../include/struct.h
s_numeric.o: ../include/dbuf.h ../include/numeric.h
whowas.o: ../include/struct.h ../include/config.h
whowas.o: ../include/dbuf.h ../include/sys.h
whowas.o: ../include/numeric.h
whowas.o: ../include/whowas.h
s_debug.o: ../include/struct.h
