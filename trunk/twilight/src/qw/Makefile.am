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

twilight_qw_SOURCES= \
	quakedef.h \
        cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c cl_model.c \
        cl_parse.c cl_pred.c cl_tent.c client.h \
	cmd.c cmd.h common.c console.c console.h \
	cvar.c \
        gl_draw.c \
	gl_ngraph.c gl_refrag.c gl_rlight.c gl_rmain.c \
	gl_rmisc.c gl_rsurf.c gl_screen.c screen.h gl_warp.c \
	keys.c mdfour.c menu.c \
	net.h net_chan.c net_udp.c \
	pmove.c pmovetst.c r_part.c sbar.c skin.c \
	snd_dma.c \
	pcx.c tga.c \
	vid_sdl.c \
	view.c wad.c zone.c \
	bspfile.h keys.h protocol.h \
	render.h sys.c \
	transform.c transform.h gl_poly.c gl_poly.h


EXTRA_DIST= \
	qw.dsp

