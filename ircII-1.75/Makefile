# IRC ][  Version 1.63 
# Written by Michael Sandrof (ms5n+@andrew.cmu.edu)
# Copyright (c) 1990
# All Rights Reserved
# HP-UX modifications by Mark T. Dame (Mark.Dame@UC.EDU)

# Full path and name of installed irc program
INSTALL_NAME = /usr/local/bin/irc

# Full path of directory for irc help file
INSTALL_HELP = /usr/local/lib/irc

# For HPUX machines, comment the first LIBS line and uncomment the second. 
# For other machines, leave it along.
LIBS = -ltermcap
#LIBS = -lcurses -lBSD -lPW

##### You shouldn't have to change anything below here 

CFLAGS = -g
RM = rm -f

OBJECTS = irc.o ircaux.o parse.o numbers.o edit.o history.o vars.o help.o\
	lastlog.o alias.o output.o log.o ignore.o display.o term.o input.o\
	reg.o keys.o scandir.o

SOURCES = irc.c ircaux.c parse.c numbers.c edit.c history.c vars.c help.c\
	lastlog.c alias.c output.c log.c ignore.c display.c term.c input.c\
	reg.c keys.c scandir.c

HEADERS = irc.h term.h

all: irc

irc: ${OBJECTS}
	${CC} ${CFLAGS} -o irc ${OBJECTS} ${LIBS}

irc.h: config.h
	touch irc.h

irc.o: irc.c irc.h term.h
	${CC} ${CFLAGS} -c irc.c

numbers.o: numbers.c irc.h
	${CC} ${CFLAGS} -c numbers.c

ircaux.o: ircaux.c irc.h
	${CC} ${CFLAGS} -c ircaux.c

parse.o: parse.c irc.h
	${CC} ${CFLAGS} -c parse.c

edit.o: edit.c irc.h 
	${CC} ${CFLAGS} -c edit.c

history.o: history.c irc.h
	${CC} ${CFLAGS} -c history.c

vars.o: vars.c irc.h
	${CC} ${CFLAGS} -c vars.c

help.o: help.c irc.h
	${CC} -DIRC_HELP_PATH=\"${INSTALL_HELP}\" ${CFLAGS} -c help.c

lastlog.o: lastlog.c irc.h
	${CC} ${CFLAGS} -c lastlog.c

alias.o: alias.c irc.h
	${CC} ${CFLAGS} -c alias.c

output.o: output.c irc.h term.h
	${CC} ${CFLAGS} -c output.c

log.o: log.c irc.h
	${CC} ${CFLAGS} -c log.c

ignore.o: ignore.c irc.h
	${CC} ${CFLAGS} -c ignore.c

term.o: term.c irc.h term.h
	${CC} ${CFLAGS} -c term.c

input.o: input.c irc.h term.h
	${CC} ${CFLAGS} -c input.c

display.o: display.c irc.h term.h
	${CC} ${CFLAGS} -c display.c

reg.o: reg.c irc.h
	${CC} ${CFLAGS} -c reg.c

keys.o: keys.c irc.h
	${CC} ${CFLAGS} -c keys.c

scandir.o: scandir.c
	${CC} ${CFLAGS} -c scandir.c

install: irc
	install -s -m 775 irc ${INSTALL_NAME}
	cp help/* ${INSTALL_HELP}

clean:
	${RM} ${OBJECTS} irc
