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

BUILT_SOURCES=embedded_data.h

noinst_HEADERS= \
	embedded_data.h bsp.h dir.h embedded.h fs.h fs_hash.h fs_new.h pak.h rw_ops.h wad.h

libtwilight_fs_a_SOURCES= \
	bsp.c dir_posix.c embedded.c fs.c fs_hash.c fs_new.c pak.c rw_ops.c wad.c

EXTRA_DIST = \
	twilight_image.dsp twilight_image.vcproj

ed = $(top_srcdir)/embedded_data

embedded_data.h: $(ed)/menu.mnu $(ed)/twilight.rc $(ed)/gfx/palette.lmp $(ed)/gfx/conchars.tga $(ed)/progs/torch.mdl $(ed)/sound/misc/menu1.wav $(ed)/sound/misc/menu2.wav $(ed)/sound/misc/menu3.wav
	$(top_srcdir)/src/tools/lhbin2c -array -base=$(ed) menu.mnu twilight.rc gfx/palette.lmp gfx/conchars.tga progs/torch.mdl sound/misc/menu1.wav sound/misc/menu2.wav sound/misc/menu3.wav > $@
