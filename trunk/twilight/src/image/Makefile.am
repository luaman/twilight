# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES = -I$(top_srcdir)/src/include

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_image.a

noinst_HEADERS= \
	image.h pcx.h qlmp.h tga.h

libtwilight_image_a_SOURCES= \
	image.c pcx.c qlmp.c tga.c

EXTRA_DIST = \
	twilight_image.dsp twilight_image.vcproj

