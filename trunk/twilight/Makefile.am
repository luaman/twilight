# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

if CLIENTS
clients = nq qw
endif
if SERVERS
servers = qw-server
endif

SUBDIRS=include src doc tools $(clients) $(servers)

EXTRA_DIST= \
	COPYING.BSD \
	acconfig.h autogen.sh \
	twilight.dsw \
	twilight.spec

ChangeLog:
if CVS2CL
	@CVS2CL@
else
	$(top_srcdir)/tools/cvs2cl.pl
endif
	-rm -f ChangeLog.bak

.PHONY: ChangeLog

