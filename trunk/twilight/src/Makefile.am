# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

if CLIENTS
clients = image renderer sound client
endif
if SERVERS
servers = server
endif
SUBDIRS=tools ../embedded_data include base fs $(clients) $(servers) nq qw


noinst_HEADERS= \
	config.h win32config.h
