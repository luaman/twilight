# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/sound


CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_renderer.a

libtwilight_renderer_a_SOURCES= \
	dyngl.c dyngl.h dglfuncs.h \
	gl_alias.c gl_alias.h \
	gl_arrays.c gl_arrays.h \
	gl_draw.c gl_draw.h \
	gl_info.c gl_info.h \
	gl_textures.c gl_textures.h \
	liquid.c liquid.h \
	menu.c \
	mod_alias.c mod_alias.h \
	mod_brush.c mod_brush.h \
	mod_sprite.c mod_sprite.h \
	noise.c gen_textures.c \
	palette.c \
	r_explosion.c \
	r_part.c r_part.h \
	sky.c sky.h surface.c surface.h \
	light.h vis.c vis.h pointers.c pointers.h

