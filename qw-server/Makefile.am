# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= -I$(top_srcdir)/include

bin_PROGRAMS=twilight-qwsv

if SYS_WIN32
twilight_nq_sys=sys_win.c
twilight_nq_sys_libs=-lwsock32 -lwinmm
else
twilight_nq_sys=sys_unix.c
twilight_nq_sys_libs=
endif

twilight_qwsv_SOURCES= \
	common.c common.h cmd.c cmd.h crc.c crc.h cvar.c mathlib.c \
	mathlib.h mdfour.c mdfour.h strlib.c strlib.h console.h \
	model.c model.h modelgen.h spritegn.h \
	net.h net_chan.c net_udp.c \
	pmove.c pmove.h pmovetst.c pr_cmds.c pr_edict.c pr_exec.c \
	progs.h progdefs.h pr_comp.h \
	sv_ccmds.c sv_ents.c sv_init.c sv_main.c sv_move.c sv_nchan.c \
	sv_phys.c sv_send.c sv_user.c \
	sys.h world.c world.h zone.c zone.h \
	bothdefs.h bspfile.h protocol.h server.h \
	$(twilight_nq_sys)

EXTRA_twilight_qwsv_SOURCES= \
	sys_win.c \
	sys_unix.c

EXTRA_DIST= \
	qwsv.dsp

LIBS=@LIBS@ -lm $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -Umain

