# IRC 2.1: Services directory
#
# Current list includes: 
# 
# gm (wumpus)
# dip (diplomacy)
# 
# card and deck are under construction 

D = ..

IRCOBJ= $D/parse.o $D/packet.o $D/send.o $D/debug.o $D/str.o \
        $D/sys.o $D/ignore.o $D/help.o $D/c_numeric.o $D/client.o

LOC = a_msg.o  a_irc.o a_sys.o

AOBJ = $(IRCOBJ) $(LOC)

CFLAGS = -g

#CC=gcc -Wall
#TARGET_ARCH=


CPPFLAGS = -DAUTOMATON -I$D
CCOM = $(CC) $(CFLAGS) $(CPPFLAGS)


all:  gm dip

clean: 
	-rm *.o gm dip

#  Now the services

gm: $(AOBJ) wumpus.o
	$(CCOM)  wumpus.o $(AOBJ) -o $@ 
	chmod 711 $@

dip: $(AOBJ) diplomacy.o
	$(CCOM)  diplomacy.o $(AOBJ) -o $@ 
	chmod 711 $@



# The object flags

a_sys.o: $D/c_sys.c 
	$(CCOM) -c -o $@ $? 


a_irc.o: $D/irc.c 
	$(CCOM) -c -o $@ $?


a_msg.o: $D/c_msg.c 
	$(CCOM) -c -o $@ $?


