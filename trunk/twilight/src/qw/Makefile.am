# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/sound

LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_qw_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -DTWILIGHT_QW

bin_PROGRAMS=twilight-qw
twilight_qw_LDADD= \
	$(top_srcdir)/src/base/libtwilight_base.a \
	$(top_srcdir)/src/renderer/libtwilight_renderer.a \
	$(top_srcdir)/src/sound/libtwilight_sound.a

if SYS_WIN32
twilight_qw_sys_libs=-lwinmm -lwsock32
else
twilight_qw_sys_libs=
endif

noinst_HEADERS= client.h gl_poly.h cmd.h console.h keys.h protocol.h render.h

twilight_qw_SOURCES= \
		     cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c	\
		     cl_model.c cl_parse.c cl_pred.c cl_tent.c		\
		     cmd.c common.c console.c cvar.c			\
		     gl_draw.c gl_ngraph.c gl_poly.c gl_rlight.c	\
		     gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c	\
		     gl_warp.c						\
		     keys.c mdfour.c menu.c net_chan.c net_udp.c	\
		     pmove.c pmovetst.c					\
		     r_part.c r_explosion.c				\
		     sbar.c sys.c skin.c snd_dma.c vid_sdl.c view.c	\
		     zone.c

EXTRA_DIST= \
	qw.dsp

