# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= -I$(top_srcdir)/include
LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -DTWILIGHT_NQ

bin_PROGRAMS=twilight-nq

if SYS_WIN32
twilight_nq_sys_libs=-lwinmm -lwsock32
else
twilight_nq_sys_libs=
endif

twilight_nq_SOURCES= \
	chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c cl_tent.c \
	cmd.c cmd.h common.c console.c console.h crc.c \
	cvar.c gl_funcs.c \
	gl_draw.c gl_mesh.c gl_model.c \
	gl_refrag.c gl_rlight.c gl_rmain.c gl_rmisc.c \
	gl_rsurf.c gl_screen.c screen.h gl_warp.c glquake.h \
	r_part.c sbar.c \
	host.c host_cmd.c keys.c mathlib.c menu.c \
	net.h net_bsd.c net_dgrm.c net_dgrm.h net_loop.c net_loop.h \
	net_main.c net_udp.c net_udp.h strlib.c \
	pr_cmds.c pr_edict.c pr_exec.c progs.h progdefs.h \
	progdefs.q1 \
	sv_main.c sv_move.c sv_phys.c sv_user.c \
	sys.h vid_sdl.c \
	snd_dma.c snd_mem.c snd_mix.c snd_sdl.c \
	cd_sdl.c \
	pcx.c tga.c \
	view.c wad.c world.c world.h zone.c \
	buildnum.c \
	quakedef.h bspfile.h client.h keys.h protocol.h \
	render.h server.h sys.c

EXTRA_twilight_nq_SOURCES= \
	sys_win.c conproc.c conproc.h \
	sys_linux.c \
	sys_wind.c

EXTRA_DIST= \
	nq.dsp

