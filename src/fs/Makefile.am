# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES = \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/embedded_data

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_fs.a

noinst_HEADERS= \
	dir.h embedded.h fs.h fs_hash.h fs_new.h pak.h rw_ops.h wad.h

libtwilight_fs_a_SOURCES= \
	dir_posix.c embedded.c fs.c fs_hash.c fs_new.c pak.c rw_ops.c wad.c

EXTRA_DIST = \
	twilight_image.dsp twilight_image.vcproj

