# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

all-local: embedded_data.fs

clean-local:
	rm embedded_data.fs

embedded_data.fs: menu.mnu twilight.rc gfx/colormap.lmp gfx/palette.lmp gfx/conchars.png
	$(top_srcdir)/src/tools/lhbin2c -strings $^ > $@
