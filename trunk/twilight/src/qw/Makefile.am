# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES= \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/sound


LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_qw_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@

if CLIENTS
clients=twilight-qw
twilight_qw_LDADD= \
	$(top_builddir)/src/base/libtwilight_base.a \
	$(top_builddir)/src/renderer/libtwilight_renderer.a \
	$(top_builddir)/src/image/libtwilight_image.a \
	$(top_builddir)/src/sound/libtwilight_sound.a
endif

if SERVERS
servers=twilight-qwsv
twilight_qwsv_LDADD= \
	$(top_builddir)/src/base/libtwilight_base.a
endif

bin_PROGRAMS=$(clients) $(servers)

if SYS_WIN32
twilight_qw_sys_libs=-lwinmm -lwsock32
else
twilight_qw_sys_libs=
endif

twilight_qw_SOURCES= \
	cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c \
	cl_model.c cl_parse.c cl_pred.c cl_tent.c client.h \
	cmd.h common.c console.c \
	gl_draw.c gl_ngraph.c gl_rlight.c \
	gl_rmain.c gl_rsurf.c gl_screen.c \
	info.c keys.c \
	menu.c net.h net_chan.c net_udp.c pmove.c pmovetst.c protocol.h \
	render.h sbar.c screen.h skin.c \
	snd_dma.c sv_null.c teamplay.c sys.c vid_sdl.c view.c

twilight_qwsv_SOURCES= \
	cl_null.c common.c info.c net_chan.c net_udp.c pmove.c \
	pmovetst.c pr_cmds.c pr_edict.c pr_exec.c progdefs.h progs.h \
	server.h sv_ccmds.c sv_ents.c sv_init.c sv_main.c sv_model.c \
	sv_move.c sv_nchan.c sv_phys.c sv_send.c sv_sys.c sv_user.c \
	world.h world.c
	

EXTRA_DIST= \
	qw.dsp
