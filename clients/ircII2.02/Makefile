# IRC II
# Written by Michael Sandrof (ms5n+@andrew.cmu.edu)
# Copyright (c) 1990
# All Rights Reserved

# Full path and name of installed irc program
INSTALL_NAME = /afs/andrew/usr3/ms5n/bin/.irc

# Full path of directory for irc help files.  
# This must be a directory with nothing else in it, since any file 
# in this directory will be treated as an IRCII help file.  
INSTALL_HELP = /afs/andrew/usr3/ms5n/etc/irc

# Set the next line to the full path for installation of the ircserv program
# if you wish to use it. 
#
# If you do no with do use the ircserv program, you may comment out the
# next line and IRCII will handle all server connections directly (as
# it has done in the past.
#
# Pros:
# IRCII will run as two separate processes, with ircserv responding to server
# PINGs.  This means you can stop IRCII (with ^Z) and not get disconnected 
# from a server. It will buffer 4K of stuff before it blocks and PINGs
# will be missed.  If, for some reason, the ircserv program cannot be
# started, IRCII will connect directly to the server without it
# (transparently to the user)
#
# Cons:
# It blocks beyond 4K of stuff.
# While being smaller than IRCII, it still takes up disk space.
# Since it runs as a separate process, it uses system resources while
# running (although not much).
#
# Note: If you add #define NON_BLOCKING to ircserv.c, then ircserv will
# not block at all.  Rather, it will discard all info beyond the 4K.
# This means it will never miss a PING.
# It can, however, screw up certain things (such as NOTIFY) if some
# of the return information is discarded.  Use this NON_BLOCKING at
# your own discretion 
#
# Operation of IRCII is identical either way.  Remember to do a 
# "make installserv" to compile and install ircserv if you are using it.
INSTALL_IRCSERV = /afs/andrew/usr3/ms5n/etc/ircserv

# Choose the LIBS that are correct for your system. Use only ONE!
# Use the following for BSD and similar systems
LIBS = -ltermcap
# Use the following for HPUX machines.
#LIBS = -lcurses -lBSD -lPW
# Use the following for MUNIX systems
#LIBS = -lcurses -lresolv -lbsd -lc

# Add any additional #defines here
#DEFINES = -DMUNIX -D_AUX_SOURCE

##### You shouldn't have to change anything below here, unless you want to
CFLAGS = -g
RM = rm -f
CP = cp -r
INSTALL = install -s -m 755

all: source/Makefile
	cd source; make

ircserv: source/Makefile
	cd source; make ircserv

source/Makefile: Makefile
	sed -e 's;INSTALL_NAME_PROTO;${INSTALL_NAME};'\
	    -e 's;LIBS_PROTO;${LIBS};'\
	    -e 's;CFLAGS_PROTO;${CFLAGS};'\
	    -e 's;RM_PROTO;${RM};'\
	    -e 's;CP_PROTO;${CP};'\
	    -e 's;DEFINES_PROTO;${DEFINES};'\
	    -e 's;INSTALL_PROTO;${INSTALL};'\
	    -e 's;HELP_PROTO;${INSTALL_HELP};' \
	    -e 's;INSTALL_IRCSERV_PROTO;${INSTALL_IRCSERV};'\
	 	source/Makefile.proto > source/Makefile 

install: all
	${INSTALL} source/irc ${INSTALL_NAME}

installserv: ircserv
	${INSTALL} source/ircserv ${INSTALL_IRCSERV}

installhelp:
	-mkdir ${INSTALL_HELP}
	${CP} help/* ${INSTALL_HELP}
	chmod u+s ${INSTALL_HELP}/CONNECT
	chmod u+s ${INSTALL_HELP}/DIE
	chmod u+s ${INSTALL_HELP}/KILL
	chmod u+s ${INSTALL_HELP}/REHASH
	chmod u+s ${INSTALL_HELP}/SQUIT
	chmod u+s ${INSTALL_HELP}/TRACE
	chmod u+s ${INSTALL_HELP}/WALL
	chmod u+s ${INSTALL_HELP}/MAIL/SAVE
	chmod u+s ${INSTALL_HELP}/ON/WALLOP

clean:
	cd source; make clean

lint: source/Makefile
	cd source; make lint
