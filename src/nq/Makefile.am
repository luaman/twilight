# $Id$
#
# This file is intended to be processed by automake

INCLUDES= -I$(top_srcdir)/include

bin_PROGRAMS=twilight-nq

if SYS_WIN32
twilight_nq_sys=sys_win.c snd_win.c conproc.c
twilight_nq_sys_libs=-lwinmm -lwsock32
else
twilight_nq_sys=sys_linux.c snd_linux.c
twilight_nq_sys_libs=
endif

twilight_nq_SOURCES= \
	chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c cl_tent.c \
	cmd.c cmd.h common.c common.h console.c console.h crc.c crc.h \
	cvar.c cvar.h gl_funcs.c \
	gl_draw.c gl_mesh.c gl_model.c gl_model.h spritegn.h modelgen.h \
	gl_refrag.c gl_rlight.c gl_rmain.c anorms.h anorm_dots.h gl_rmisc.c \
	gl_rsurf.c gl_screen.c screen.h gl_warp.c gl_warp_sin.h glquake.h \
	r_part.c sbar.c sbar.h \
	host.c host_cmd.c keys.c mathlib.c mathlib.h menu.c menu.h \
	net.h net_bsd.c net_dgrm.c net_dgrm.h net_loop.c net_loop.h \
	net_main.c net_udp.c net_udp.h \
	pr_cmds.c pr_edict.c pr_exec.c progs.h progdefs.h pr_comp.h \
	sv_main.c sv_move.c sv_phys.c sv_user.c \
	sys.h vid_sdl.c vid.h \
	snd_dma.c snd_mem.c snd_mix.c sound.h \
	cd_sdl.c cdaudio.h \
	view.c view.h wad.c wad.h world.c world.h zone.c zone.h \
	quakedef.h bspfile.h client.h draw.h input.h keys.h protocol.h \
	render.h server.h $(twilight_nq_sys)

LIBS=@SDL_LIBS@ @LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@
