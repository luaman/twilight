# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES = \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_fs.a

noinst_HEADERS= \
	fs.h fs_hash.h fs_new.h dir.h pak.h rw_ops.h

libtwilight_fs_a_SOURCES= \
	fs.c fs_hash.c fs_new.c dir_posix.c pak.c rw_ops.c

EXTRA_DIST = \
	twilight_image.dsp twilight_image.vcproj

