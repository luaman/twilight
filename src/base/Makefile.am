# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

INCLUDES = -I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/fs \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/client

lib_LIBRARIES=libtwilight_base.a

libtwilight_base_a_SOURCES= \
	buildnum.c \
	cpu.c cpu.h \
	cmd.c cmd.h \
	collision.c collision.h \
	crc.c crc.h \
	cvar.c cvar.h \
	lh_parser.c lh_parser.h \
	mathlib.c mathlib.h \
	matrixlib.c matrixlib.h \
	menu.c menu.h \
	mdfour.c mdfour.h \
	model.c model.h \
	mod_brush.c mod_brush.h mod_brush_disk.h \
	parm.c \
	strlib.c strlib.h \
	locs.c locs.h \
	sdlstub.c sdlstub.h \
	hash.c hash.h \
	loadso.c loadso.h \
	zone.c zone.h

EXTRA_DIST = \
	twilight_base.dsp twilight_base.vcproj
