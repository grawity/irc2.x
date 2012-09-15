# -*- text -*-
# Makefile to install the m4 macro package
# $Id: Makefile,v 2.1 1993/11/15 15:22:18 pioch Exp $
# This is the version number of this release.
PACKAGE_VERSION=2.1

# ------------------------------------------------------------------------
# Set this to the directory where ircd.conf is located
# (DPATH in ircd include/config.h)

IRCDDIR=/usr/local/lib/ircd

# ------------------------------------------------------------------------
# You shouldn't have to edit anything below.

SHELL=/bin/sh
INSTALL=install
INSTALL_DATA=$(INSTALL) -m 644
RM=rm -f
COMPRESS=gzip -9
TAR=tar

INSTALL_TARGETS=ircd.m4ext example.conf.m4ext
SRCS=ChangeLog INSTALL Makefile NEWS README \
	example.conf.m4ext ircd.m4ext m4ext.doc


install:
	-@if [ ! -d $(IRCDDIR) ]; then \
		echo "Looks like $(IRCDDIR) does not exist."; \
		echo "I will try to create this directory for you."; \
	mkdir $(IRCDDIR); fi
	@for i in $(INSTALL_TARGETS); do \
		$(INSTALL_DATA) $$i $(IRCDDIR)/$$i; \
	done
	-@if [ ! -f $(IRCDDIR)/ircd.m4 ]; then \
		echo "There is no ircd.m4 file in $(IRCDDIR)"; \
		echo "Probably you haven't done a 'make install'"; \
		echo "for the ircd package yet."; \
		echo "I will create a dummy ircd.m4 file in the meantime."; \
		touch $(IRCDDIR)/ircd.m4; \
	fi
	@echo "Installation successful."

clean:
	$(RM) *~ \#*\#

ci:
	ci -l $(SRCS)

dist:
	mkdir m4ext-$(PACKAGE_VERSION)
	cp $(SRCS) m4ext-$(PACKAGE_VERSION)
	$(TAR) -cvf m4ext-$(PACKAGE_VERSION).tar m4ext-$(PACKAGE_VERSION)
	$(COMPRESS) m4ext-$(PACKAGE_VERSION).tar
	$(RM) -r m4ext-$(PACKAGE_VERSION)
	ls -l m4ext-$(PACKAGE_VERSION).tar.gz

