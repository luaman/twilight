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
	acconfig.h bootstrap \
	twilight.dsw \
	twilight.spec

ChangeLog:
	$(top_srcdir)/tools/cvs2cl.pl
	-rm -f ChangeLog.bak

.PHONY: ChangeLog

