# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

INCLUDES = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/client

lib_LIBRARIES=libtwilight_base.a

libtwilight_base_a_SOURCES= \
	buildnum.c cpu.c cmd.c collision.c crc.c cvar.c lh_parser.c mathlib.c \
	matrixlib.c menu.c mdfour.c model.c mod_brush.c parm.c strlib.c wad.c \
	zone.c locs.c sdlstub.c sdlstub.h

EXTRA_DIST = \
	twilight_base.dsp twilight_base.vcproj
