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

ChangeLog:
	$(top_srcdir)/tools/cvs2cl.pl
	-rm -f ChangeLog.bak

.PHONY: ChangeLog

