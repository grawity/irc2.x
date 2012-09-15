#************************************************************************
#*   IRC - Internet Relay Chat, irc/Makefile
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

# 17 Jun 1990 - Armin
# Now, common/debug.c has changed to irc/c_debug.c, for the benefit of
# displaying wrong server stuff

# 30 May 1990 - Jto
# swear.o isn't needed when compiling irc.
# Removed it (as suggested by Kim).

# Check the curses libraries if your systems wants them differently...

INCLUDE =../include

#For MIPS, use this:
#CFLAGS =-O -systype bsd43 -I${INCLUDE}
CFLAGS =-O -I${INCLUDE}

# For AIX, use the following:
#IRCLIBS=-lcurses -lcur
IRCLIBS=-lcurses -ltermcap

CC=cc
RM=/bin/rm
OBJS=c_bsd.o c_conf.o c_msg.o c_numeric.o c_version.o edit.o help.o ignore.o\
     irc.o screen.o str.o c_debug.o

SRC=c_bsd.c c_conf.c c_msg.c c_numeric.c c_version.c edit.c help.c ignore.c\
     irc.c screen.c str.c c_debug.c

COMMONOBJS=../common/bsd.o ../common/dbuf.o\
           ../common/packet.o ../common/send.o\
	   ../common/match.o ../common/parse.o ../common/support.o


MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}'

all: build

build: irc

irc: $(OBJS) $(COMMONOBJS)
	$(CC) $(OBJS) $(COMMONOBJS) -o irc $(IRCLIBS) $(LIBFLAGS)

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
	rm -f *.o *~ irc core #* *.bak

depend:
	makedepend -I${INCLUDE} ${SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.

c_bsd.o: ../include/struct.h ../include/config.h ../include/dbuf.h
c_conf.o: ../include/struct.h ../include/config.h
c_conf.o: ../include/dbuf.h ../include/sys.h
c_msg.o: ../include/struct.h ../include/config.h ../include/dbuf.h
c_msg.o: ../include/msg.h
c_numeric.o: ../include/config.h ../include/sys.h
c_numeric.o: ../include/struct.h
c_numeric.o: ../include/dbuf.h ../include/numeric.h
c_version.o: ../include/struct.h ../include/config.h ../include/dbuf.h
c_version.o: ../include/patchlevel.h
edit.o: ../include/struct.h
edit.o: ../include/config.h ../include/dbuf.h
help.o: ../include/struct.h ../include/config.h ../include/dbuf.h
help.o: ../include/sys.h
help.o: /usr/include/pwd.h help.h
ignore.o: ../include/struct.h ../include/config.h ../include/dbuf.h
ignore.o: ../include/sys.h
ignore.o: /usr/include/pwd.h
irc.o: ../include/struct.h ../include/config.h ../include/dbuf.h
irc.o: ../include/msg.h ../include/sys.h
irc.o: irc.h
screen.o: ../include/struct.h ../include/config.h ../include/dbuf.h
str.o: ../include/struct.h ../include/config.h ../include/dbuf.h
str.o: ../include/sys.h
swear.o: ../include/struct.h ../include/config.h
swear.o: ../include/dbuf.h ../include/sys.h /usr/include/pwd.h
c_debug.o: ../include/struct.h
