# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=foreign dist-zip

INCLUDES= \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/image \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/sound \
	-I$(top_srcdir)/src/server \
	-I$(top_srcdir)/src/client


LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@

if CLIENTS
bin_PROGRAMS=twilight-nq
twilight_nq_LDFLAGS=--warn-common
twilight_nq_LDADD= \
	$(top_builddir)/src/base/libtwilight_base.a \
	$(top_builddir)/src/renderer/libtwilight_renderer.a \
	$(top_builddir)/src/image/libtwilight_image.a \
	$(top_builddir)/src/client/libtwilight_client.a \
	$(top_builddir)/src/sound/libtwilight_sound.a
endif

if SYS_WIN32
twilight_nq_sys_libs=-lwinmm -lwsock32
twilight_nq_sys_sources=
else
twilight_nq_sys_libs=
twilight_nq_sys_sources=
endif

twilight_nq_SOURCES= \
	chase.c \
	cl_demo.c cl_input.c cl_main.c cl_model.c cl_parse.c cl_ents.c \
	cl_tent.c \
	client.h common.c cl_console.h \
	gl_draw.c gl_rlight.c gl_rmain.c gl_rsurf.c gl_screen.c \
	host.c host_cmd.c menu.c \
	net.h net_bsd.c net_dgrm.c net_dgrm.h net_loop.c net_loop.h \
	net_main.c net_udp.c net_udp.h \
	pr_cmds.c pr_edict.c pr_exec.c progs.h progdefs.h \
	progdefs.q1 protocol.h pr_execprogram.h \
	render.h screen.h server.h \
	snd_dma.c \
	sv_main.c sv_move.c sv_phys.c sv_user.c \
	sys.c teamplay.c vid_sdl.c \
        view.c world.c world.h \
	$(twilight_nq_sys_sources)

EXTRA_DIST= \
	nq.dsp nq.vcproj

