# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/renderer \
	-I$(top_srcdir)/src/sound

LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -DTWILIGHT_NQ

bin_PROGRAMS=twilight-nq
twilight_nq_LDADD= \
		   $(top_srcdir)/src/base/libtwilight_base.a \
		   $(top_srcdir)/src/renderer/libtwilight_renderer.a \
		   $(top_srcdir)/src/sound/libtwilight_sound.a

if SYS_WIN32
twilight_nq_sys_libs=-lwinmm -lwsock32
twilight_nq_sys_sources=conproc.c
else
twilight_nq_sys_libs=
twilight_nq_sys_sources=
endif

twilight_nq_SOURCES= \
	chase.c \
	cl_demo.c cl_input.c cl_main.c cl_model.c cl_parse.c cl_tent.c \
	cmd.c cmd.h common.c console.c console.h \
	cvar.c \
        gl_draw.c \
	gl_refrag.c gl_rlight.c gl_rmain.c gl_rmisc.c \
	gl_rsurf.c gl_screen.c screen.h gl_warp.c \
	r_part.c sbar.c \
        host.c host_cmd.c keys.c menu.c \
	net.h net_bsd.c net_dgrm.c net_dgrm.h net_loop.c net_loop.h \
	net_main.c net_udp.c net_udp.h \
	pr_cmds.c pr_edict.c pr_exec.c progs.h progdefs.h \
	progdefs.q1 \
	sv_main.c sv_move.c sv_phys.c sv_user.c \
	vid_sdl.c \
	snd_dma.c \
	pcx.c tga.c \
        view.c world.c world.h zone.c \
        quakedef.h bspfile.h client.h keys.h protocol.h \
	render.h server.h sys.c \
        gl_poly.c gl_poly.h \
	$(twilight_nq_sys_sources)

EXTRA_twilight_nq_SOURCES= \
	conproc.c conproc.h

EXTRA_DIST= \
	nq.dsp

