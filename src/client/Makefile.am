# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES= \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/fs \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/sound


CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_client.a

libtwilight_client_a_SOURCES= \
	cclient.c cclient.h \
	console.c console.h \
	keys.c keys.h \
	hud.c hud.h \
	teamplay.c teamplay.h

EXTRA_DIST = \
	twilight_client.dsp twilight_client.vcproj

