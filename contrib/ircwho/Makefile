# BINDIR is the directory where executable binaries go
BINDIR=/usr/local/bin
# SHBINDIR is the directory where executable shell programs should go
SHBINDIR=/usr/local/bin
# MANDIR is the directory where you want your manual pages installed.
MANDIR=/usr/local/man/man1

CFLAGS= -O

all: ircw ircwho
	@echo bah humbug

ircw:	ircwatch.c config.h strings.h
	cc $(CFLAGS) ircwatch.c -o ircw

ircwho:	Ircwho ircwho.sh
	@cp ircwho.sh ircwho
	@echo edit ircwho to set IRCWHOBIN correctly...

Ircwho: ircwho.c
	cc $(CFLAGS) ircwho.c -o Ircwho

clean:
	rm -f ircwho Ircwho ircwho.o
	rm -f ircw ircwatch.o

debug:
	cc $(CFLAGS) -DEBUG -gx ircwatch.c -o diw

install:	all
	install -s ircw $(BINDIR)
	install -s Ircwho $(BINDIR)
	@echo Now editing final version of ircwho prior to install-ing
	@sleep 5
	$${VISUAL-$${EDITOR-/usr/ucb/vi}} +28 ircwho
	install ircwho $(SHBINDIR)
	install -c irctools.1 $(MANDIR)
