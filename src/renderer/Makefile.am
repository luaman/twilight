# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_renderer.a

noinst_HEADERS= \
	dynogl.h opengl.h opengl_ext.h oglfuncs.h \
	glquake.h transform.h \
	image.h tga.h pcx.h qlmp.h

libtwilight_renderer_a_SOURCES= \
	dynogl.c \
	noise.c noise_textures.c \
	transform.c \
	wad.c \
	image.c tga.c pcx.c qlmp.c

