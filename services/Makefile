# IRC Services directory
#
# Current list includes: 
# 
# gm (wumpus)
# dip (diplomacy)
# 
# card and deck are under construction 

L=../lib

# The include directory
INC =../lib


# The client source directory
C  =../client


# Shared server/client  specific files
LOBJS= $L/parse.o $L/packet.o $L/send.o $L/debug.o $L/sys.o $L/version.o

# Shared client/automaton files
COBJS= $C/str.o $C/c_conf.o $C/c_numeric.o $C/ignore.o $C/help.o

# Automaton files
AOBJS= a_msg.o  a_irc.o a_sys.o       

# All the objects
TOBJS= $(AOBJS) $(COBJS) $(LOBJS)


CFLAGS   = -g -I$(INC) -I$C 
CC = cc 
CCOM = $(CC) $(CFLAGS) -DAUTOMATON 

all:  gm dip

gm: $(TOBJS) wumpus.o
	$(CCOM)  wumpus.o $(TOBJS) -o $@ 
	chmod 4711 $@

dip: $(TOBJS) diplomacy.o
	$(CCOM) diplomacy.o $(TOBJS) -o $@ 
	chmod 4711 $@

clean: 
	-@rm *.o *~ TAGS  gm dip


# Dependencies 

a_sys.o: $C/c_sys.c $(INC)/struct.h $(INC)/sys.h $(INC)/config.h
	$(CCOM)  -c -o $@ $C/c_sys.c 


a_irc.o: $C/irc.c $(INC)/struct.h  $(INC)/config.h
	$(CCOM)  -c -o $@ $C/irc.c 

a_msg.o: $C/c_msg.c $(INC)/struct.h $(INC)/msg.h $(INC)/config.h
	$(CCOM) -c -o $@ $C/c_msg.c 


$(LOBJS):
	(cd $L; make)

$(COBJS):
	(cd $C; make)

