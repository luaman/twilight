# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

if CLIENTS
clients = image renderer sound
endif
SUBDIRS=tools include base $(clients) nq qw


noinst_HEADERS= \
	config.h win32config.h
