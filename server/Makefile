# Makefile for IRC 2.0.1v6
# 
# Author list and credits in AUTHORS
#

SYS=bsd

# The library and INC file directory
L=../lib

# The INC directory
INC =../lib

# Object files shared with the server
LIBOBJS= $L/parse.o $L/send.o $L/debug.o $L/sys.o $L/packet.o $L/version.o

# Server specific files
DOBJS= channel.o date.o ircd.o list.o s_conf.o s_msg.o s_numeric.o s_sys.o

CFLAGS   = -g -I$(INC)
CC = cc 


ircd: $(DOBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(DOBJS) -o $@ $(LIBOBJS)
	chmod 4711 $@

$(LIBOBJS):
	(cd $L; make)


clean:
	-@rm *.o *~ ircd TAGS


### Dependencies

s_sys.c	    : s_$(SYS).c
	cp s_$(SYS).c s_sys.c

channel.o   : channel.c $(INC)/struct.h $(INC)/config.h
date.o      : date.c $(INC)/struct.h  $(INC)/config.h
ircd.o      : ircd.c $(INC)/struct.h  $(INC)/config.h
list.o      : list.c $(INC)/struct.h  $(INC)/config.h
s_conf.o    : s_conf.c $(INC)/struct.h $(INC)/sys.h  $(INC)/config.h
s_msg.o     : s_msg.c $(INC)/struct.h $(INC)/sys.h $(INC)/msg.h \
              $(INC)/numeric.h  $(INC)/config.h
s_numeric.o : s_numeric.c $(INC)/sys.h $(INC)/struct.h \
              $(INC)/numeric.h  $(INC)/config.h
s_sys.o     : s_sys.c $(INC)/struct.h $(INC)/sys.h  $(INC)/config.h


