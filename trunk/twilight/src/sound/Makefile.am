# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES = -I$(top_srcdir)/src/include

CFLAGS=@CFLAGS@ @SDL_CFLAGS@

lib_LIBRARIES=libtwilight_sound.a

libtwilight_sound_a_SOURCES= \
	cdaudio.h sound.h \
	cdmusic.c mem.c mix.c target.c

