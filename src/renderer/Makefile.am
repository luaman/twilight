# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES= \
	-I$(top_srcdir)/src//include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/client \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/sound


CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_renderer.a

libtwilight_renderer_a_SOURCES= \
	dyngl.c dyngl.h dglfuncs.h \
	entities.c entities.h \
	gl_alias.c gl_alias.h \
	gl_arrays.c gl_arrays.h \
	gl_brush.c gl_brush.h \
	gl_draw.c gl_draw.h \
	gl_sprite.c gl_sprite.h \
	gl_info.c gl_info.h \
	gl_textures.c gl_textures.h \
	liquid.c liquid.h \
	menu.c \
	mod_alias.c mod_alias.h \
	mod_brush.c \
	mod_sprite.c mod_sprite.h \
	noise.c gen_textures.c gen_textures.h \
	palette.c palette.h \
	r_explosion.c \
	r_part.c r_part.h \
	sky.c sky.h surface.c surface.h \
	light.h vis.c vis.h pointers.c pointers.h

EXTRA_DIST = \
	twilight_renderer.dsp twilight_renderer.vcproj

