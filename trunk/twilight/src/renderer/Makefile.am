# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES= \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/client


CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_renderer.a

libtwilight_renderer_a_SOURCES= \
	anorms.-h anorm_dots.-h \
	dyngl.c dyngl.h dglfuncs.h \
	entities.c entities.h \
	alias.c alias.h \
	gl_arrays.c gl_arrays.h \
	brush.c brush.h \
	draw.c draw.h \
	sprite.c sprite.h \
	gl_info.c gl_info.h \
	gl_light.c gl_light.h \
	gl_main.c gl_main.h \
	textures.c textures.h \
	liquid.c liquid.h \
	video.c video.h \
	menu.c \
	mod_alias.c mod_alias.h mod_alias_disk.h \
	mod_brush.c \
	mod_sprite.c mod_sprite.h mod_sprite_disk.h \
	noise.c gen_textures.c gen_textures.h \
	palette.c palette.h \
	explosion.c explosion.h \
	r_part.c r_part.h \
	sky.c sky.h \
	surface.c surface.h \
	vis.c vis.h \
	pointers.c pointers.h

EXTRA_DIST = \
	twilight_renderer.dsp twilight_renderer.vcproj

