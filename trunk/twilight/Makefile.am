# $Id$
#
# This file is intended to be processed by automake

SUBDIRS=nq qw qw-server

ChangeLog:
if CVS2CL
	@CVS2CL@
else
	$(top_srcdir)/tools/cvs2cl.pl
endif

.PHONY: ChangeLog

