# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= -I$(top_srcdir)/include
LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_qw_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -DTWILIGHT_QW

bin_PROGRAMS=twilight-qw
twilight_qw_LDADD=$(top_srcdir)/src/base/libtwilight_base.a

if SYS_WIN32
twilight_qw_sys_libs=-lwinmm -lwsock32
else
twilight_qw_sys_libs=
endif

twilight_qw_SOURCES= \
	cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c cl_parse.c \
	cl_pred.c cl_tent.c client.h \
	cmd.c cmd.h common.c console.c console.h \
	cvar.c gl_funcs.c \
	gl_draw.c gl_mesh.c gl_model.c \
	gl_ngraph.c gl_refrag.c gl_rlight.c gl_rmain.c \
	gl_rmisc.c gl_rsurf.c gl_screen.c screen.h gl_warp.c \
	glquake.h hash.c hash.h \
	keys.c mdfour.c menu.c \
	net.h net_chan.c net_udp.c \
	pmove.c pmovetst.c r_part.c sbar.c skin.c \
	snd_dma.c snd_mem.c snd_sdl.c\
	snd_mix.c cd_sdl.c \
	pcx.c tga.c \
	sys.h vid_sdl.c \
	view.c wad.c zone.c \
	buildnum.c \
	bspfile.h keys.h protocol.h \
	render.h sys.c


EXTRA_DIST= \
	qw.dsp

