# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

SUBDIRS=olddoc

EXTRA_DIST= \
	COPYING.txt \
	COPYING-BSD.txt \
	Credits.txt \
	cvar.txt \
	TODO.txt

ChangeLog:
	$(top_srcdir)/tools/cvs2cl.pl
	-rm -f ChangeLog.bak

.PHONY: ChangeLog
