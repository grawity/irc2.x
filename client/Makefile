# Makefile for IRC 2.0.1v6
# 
# Author list and credits in AUTHORS
#

SYS=bsd

# The library and include file directory
L=../lib

# The include directory
INC =../lib

# Object files shared with the server
LIBOBJS= $L/parse.o $L/send.o $L/debug.o $L/sys.o $L/packet.o $L/version.o

# Client specific files
COBJS= irc.o c_msg.o str.o edit.o screen.o c_conf.o c_numeric.o \
       c_sys.o help.o ignore.o swear.o 
      
CFLAGS   = -g -I$(INC)
CC = gcc 

# At least HPUX needs only curses library, not termcap...
#TLIBS = -lcurses

TLIBS = -lcurses -ltermcap


irc: $(COBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(COBJS) -o $@ $(TLIBS) $(LIBOBJS)
	chmod 4711 $@

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

