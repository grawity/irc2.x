# Makefile for IRC 2.0.1v6
# 


# Object files shared with the server and client
LIBOBJS= parse.o send.o debug.o sys.o packet.o version.o

SYS	 =bsd
INC	 =.

CFLAGS   = -g -I$(INC)
CC 	 = cc 

all:  $(LIBOBJS)


clean:
	-@rm *.o *~ TAGS


### Dependencies

parse.o : parse.c msg.h struct.h sys.h numeric.h  config.h

send.o : send.c struct.h  config.h

debug.o : debug.c struct.h  config.h

sys.o : sys.c struct.h  config.h

version.o: version.c struct.h

sys.c : $(SYS).c
	cp $(SYS).c sys.c

sys.h : $(SYS).h
	cp $(SYS).h sys.h
