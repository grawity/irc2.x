# Makefile for IRC 2.1.0 Beta
# 
# Remember to modify the struct.h file as appropriate 
#
# Author list and credits in AUTHORS
#
# SYS = sysv   SysV NOT working... under construction...
# Use bsd for HPUX, SUNs, ULTRIX, ...
# 
# After defining sys,  'make config' to create links

SYS = bsd

DOBJS= ircd.o debug.o s_msg.o parse.o packet.o send.o list.o s_sys.o sys.o\
       date.o channel.o s_numeric.o server.o

COBJS= irc.o parse.o packet.o send.o c_msg.o debug.o str.o edit.o screen.o\
       c_sys.o sys.o help.o ignore.o swear.o c_numeric.o client.o
       

CFLAGS = -g

#if using gcc on a Sun
#CC=gcc -Wall
#TARGET_ARCH=

# At least HPUX needs only curses library, not termcap...
#TLIBS = -lcurses

TLIBS = -lcurses -ltermcap

# Libraries needed for ircd

DLIBS =


DIST= Makefile COPYRIGHT *.c *.h README example.conf \
        doc services contrib

VAXDIST = $(DIST) c_compile.opt compile.com link.com

all: ircd irc

irc.tar.Z: irc.tar
	compress irc.tar

irc.tar: $(VAXDIST)
	tar -cfv $@  $(VAXDIST)

irc.shar: $(DIST)
	shar $(DIST) > $@


irc: $(COBJS)
	$(CC) $(COBJS) -o $@ $(TLIBS)
	chmod 711 $@


ircd: $(DOBJS)
	$(CC) $(DOBJS) -o $@ $(DLIBS)
	chmod 4711 $@

services:
	cd services; \
	$(MAKE) all 

clean: 
	-rm *.o irc ircd
	(cd  services; $(MAKE) clean)

realclean: 
	rm sys.h sys.c c_sys.c s_sys.c

config:
	-ln -s $(SYS).h sys.h
	-ln -s $(SYS).c sys.c
	-ln -s c_$(SYS).c c_sys.c
	-ln -s s_$(SYS).c s_sys.c


### Dependencies

swear.o: struct.h

ircd.o: ircd.c struct.h

ignore.o: ignore.c struct.h

sys.o: struct.h sock.h sys.h

c_sys.o: struct.h sock.h Makefile sys.h

s_sys.o: struct.h sock.h Makefile sys.h

c_numeric.o:  struct.h numeric.h

s_numeric.o:  struct.h numeric.h

date.o: date.c

channel.o: channel.c struct.h

send.o: send.c struct.h

debug.o: debug.c struct.h

help.o: help.c struct.h help.h

s_msg.o: s_msg.c msg.h struct.h sys.h

parse.o: parse.c msg.h struct.h sys.h

packet.o: packet.c struct.h msg.h

str.o: str.c sys.h

irc.o: irc.c struct.h msg.h sys.h irc.h

c_msg.o: c_msg.c struct.h msg.h

list.o: list.c struct.h

edit.o: edit.c

screen.o: screen.c


