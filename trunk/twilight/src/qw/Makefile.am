# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= -I$(top_srcdir)/include

bin_PROGRAMS=twilight-qw

if SYS_WIN32
twilight_nq_sys=snd_win.c
twilight_nq_sys_libs=-lwinmm -lwsock32
else
twilight_nq_sys=snd_linux.c
twilight_nq_sys_libs=
endif

twilight_qw_SOURCES= \
	cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c cl_parse.c \
	cl_pred.c cl_tent.c client.h \
	cmd.c cmd.h common.c common.h console.c console.h crc.c crc.h \
	cvar.c gl_funcs.c \
	gl_draw.c gl_mesh.c gl_model.c gl_model.h modelgen.h spritegn.h \
	gl_ngraph.c gl_refrag.c gl_rlight.c gl_rmain.c anorms.h anorm_dots.h \
	gl_rmisc.c gl_rsurf.c gl_screen.c screen.h gl_warp.c \
	glquake.h gl_proc.c hash.c hash.h \
	keys.c mathlib.c mdfour.c mdfour.h menu.c menu.h \
	net.h net_chan.c net_udp.c strlib.c \
	pmove.c pmove.h pmovetst.c r_part.c sbar.c sbar.h skin.c \
	snd_dma.c snd_mem.c \
	snd_mix.c sound.h cd_sdl.c cdaudio.h \
	pcx.c tga.c \
	sys.h vid_sdl.c vid.h \
	view.c view.h wad.c wad.h zone.c zone.h \
	buildnum.c \
	bothdefs.h bspfile.h draw.h input.h keys.h protocol.h \
	render.h sys.c $(twilight_nq_sys)

EXTRA_twilight_qw_SOURCES= \
	snd_win.c winquake.h \
	snd_linux.c

EXTRA_DIST= \
	qw.dsp

LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@

