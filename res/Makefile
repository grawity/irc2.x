#************************************************************************
#*   IRC - Internet Relay Chat, res/Makefile
#*   Copyright (C) 1992 Darren Reed
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

RESOBJS= res_init.o res_query.o res_send.o res_mkquery.o res_comp.o \
	res_debug.o gethostnamadr.o sethostent.o

all: build

build: $(RESOBJS)

clean:
	/bin/rm -f *.o
res_init.o: res_init.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_init.c
res_send.o: res_send.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_send.c
res_comp.o: res_comp.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_comp.c
res_query.o: res_query.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_query.c
res_mkquery.o: res_mkquery.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_mkquery.c
res_debug.o: res_debug.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c res_debug.c
gethostnamadr.o: gethostnamadr.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c gethostnamadr.c
sethostent.o: sethostent.c ../include/resolv.h ../include/nameser.h
	$(CC) $(CFLAGS) -c sethostent.c
