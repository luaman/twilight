# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

SUBDIRS=src doc

EXTRA_DIST= \
	COPYING.BSD \
	acconfig.h bootstrap \
	twilight.dsw \
	twilight.spec

tbs = $(top_builddir)/src

clean-generic:
	-rm $(top_builddir)/twilight-nq@EXEEXT@
	-rm $(top_builddir)/twilight-qw@EXEEXT@
	-rm $(top_builddir)/twilight-qwsv@EXEEXT@

all: all-recursive
if CLIENTS
	${INSTALL} $(tbs)/nq/twilight-nq@EXEEXT@ $(top_builddir)
	${INSTALL} $(tbs)/qw/twilight-qw@EXEEXT@ $(top_builddir)
endif
if SERVERS
	${INSTALL} $(tbs)/qw/twilight-qwsv@EXEEXT@ $(top_builddir)
endif

ChangeLog:
	$(top_srcdir)/tools/cvs2cl.pl
	-rm -f ChangeLog.bak

.PHONY: ChangeLog

