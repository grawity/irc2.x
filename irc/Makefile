# Makefile for IRC 2.0.1v6
#************************************************************************
#*   IRC - Internet Relay Chat, irc/Makefile
#*   Copyright (C) 1990
#*
#*   Author list and credits in doc/AUTHORS
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
# 

SYS=bsd

# The library and include file directory
L=../lib/ircd

# The include directory
INC =../include

# Object files shared with the server
LIBOBJS= $L/send.o $L/debug.o $L/sys.o $L/packet.o $L/dbuf.o $L/parse.o

# Client specific files
COBJS= irc.o c_msg.o str.o edit.o screen.o c_conf.o c_numeric.o \
       c_sys.o help.o ignore.o swear.o c_version.o
      
CFLAGS   = -g -I$(INC)
CC = cc

# At least HPUX needs only curses library, not termcap...
#TLIBS = -lcurses

TLIBS = -lcurses -ltermcap


irc: $(COBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(COBJS) -o $@ $(TLIBS) $(LIBOBJS)

$(LIBOBJS):
	(cd $L; make)

clean:
	-@rm *.o *~ irc


### Dependencies

c_sys.c		: c_$(SYS).c
		cp c_$(SYS).c c_sys.c

irc.o       : irc.c $(INC)/struct.h $(INC)/config.h $(INC)/msg.h \
              $(INC)/sys.h irc.h 
c_msg.o     : c_msg.c $(INC)/struct.h $(INC)/config.h $(INC)/msg.h 
str.o       : str.c $(INC)/struct.h $(INC)/config.h $(INC)/sys.h 
edit.o      : edit.c $(INC)/struct.h $(INC)/config.h 
screen.o    : screen.c $(INC)/struct.h $(INC)/config.h 
c_conf.o    : c_conf.c $(INC)/struct.h $(INC)/sys.h $(INC)/config.h 
c_numeric.o : c_numeric.c $(INC)/sys.h $(INC)/struct.h $(INC)/numeric.h \
              $(INC)/config.h 
c_sys.o     : c_sys.c $(INC)/struct.h $(INC)/sys.h $(INC)/config.h 
help.o      : help.c $(INC)/struct.h $(INC)/sys.h help.h $(INC)/config.h 
ignore.o    : ignore.c $(INC)/struct.h $(INC)/sys.h $(INC)/config.h 
swear.o     : swear.c $(INC)/struct.h $(INC)/config.h 
