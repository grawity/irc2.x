# Makefile for IRC 2.0.1v6 
# 
# Remember to modify the struct.h file as appropriate 
#
# Author list and credits in AUTHORS
#
# SYS = sysv   SysV NOT working... under construction...
# Use bsd for HPUX, SUNs, ULTRIX, ...
# 
# After defining sys,  'make config' to install the correct system files

SYS=bsd

INC =../lib
CFLAGS = -O -I$(INC)
CC = cc
MAKEOPTS = 'INC="$(INC)" CC="$(CC)" CFLAGS="$(CFLAGS)" SYS="$(SYS)"'

SUBDIRS = lib server client services


all:
	@for i in $(SUBDIRS); \
	do \
	echo "making $$i"; \
	(cd $$i; $(MAKE) MAKEOPTS='$(MAKEOPTS)' ); \
	done

config:
	cp lib/$(SYS).h lib/sys.h
	cp lib/$(SYS).c lib/sys.c
	cp client/c_$(SYS).c client/c_sys.c
	cp server/s_$(SYS).c server/s_sys.c


clean:
	-@rm *.o *~ 
	@for i in $(SUBDIRS); \
	do \
	echo "cleaning $$i"; \
	(cd $$i; $(MAKE) clean); \
	done

realclean: clean
	-rm lib/sys.h
	-rm lib/sys.c
	-rm client/c_sys.c
	-rm server/s_sys.c


