# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

SUBDIRS=include src doc tools nq qw

EXTRA_DIST= \
	COPYING.BSD \
	acconfig.h bootstrap \
	twilight.dsw \
	twilight.spec

clean-generic:
	-rm $(top_builddir)/twilight-nq@EXEEXT@
	-rm $(top_builddir)/twilight-qw@EXEEXT@
	-rm $(top_builddir)/twilight-qwsv@EXEEXT@

all: all-recursive
if CLIENTS
	${INSTALL} $(top_builddir)/nq/twilight-nq@EXEEXT@ $(top_builddir)
	${INSTALL} $(top_builddir)/qw/twilight-qw@EXEEXT@ $(top_builddir)
endif
if SERVERS
	${INSTALL} $(top_builddir)/qw/twilight-qwsv@EXEEXT@ $(top_builddir)
endif

ChangeLog:
	$(top_srcdir)/tools/cvs2cl.pl
	-rm -f ChangeLog.bak

.PHONY: ChangeLog

