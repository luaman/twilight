# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/sound


CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_renderer.a

noinst_HEADERS= \
	dyngl.h dglfuncs.h \
	glquake.h transform.h gl_textures.h \
	light.h \
	pointers.h

libtwilight_renderer_a_SOURCES= \
	dyngl.c \
	gl_textures.c \
	noise.c noise_textures.c \
	transform.c \
	pointers.c

