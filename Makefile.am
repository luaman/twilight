# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

SUBDIRS=include nq qw qw-server doc gamecode debian

EXTRA_DIST= \
	acconfig.h autogen.sh \
	twilight.dsw

ChangeLog:
if CVS2CL
	@CVS2CL@
else
	$(top_srcdir)/tools/cvs2cl.pl
endif
	rm -f ChangeLog.bak

.PHONY: ChangeLog

