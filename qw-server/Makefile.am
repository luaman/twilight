# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= -I$(top_srcdir)/include
LIBS=@LIBS@ -lm @SDL_LIBS@ $(twilight_nq_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -Umain -DTWILIGHT_QWSV

bin_PROGRAMS=twilight-qwsv

if SYS_WIN32
twilight_nq_sys_libs=-lwsock32 -lwinmm
else
twilight_nq_sys_libs=
endif

twilight_qwsv_SOURCES= \
	common.c cmd.c cmd.h crc.c cvar.c mathlib.c \
	mdfour.c strlib.c console.h \
	model.c model.h \
	net.h net_chan.c net_udp.c \
	pmove.c pmovetst.c pr_cmds.c pr_edict.c pr_exec.c \
	progs.h progdefs.h \
	sv_ccmds.c sv_ents.c sv_init.c sv_main.c sv_move.c sv_nchan.c \
	sv_phys.c sv_send.c sv_user.c \
	sys.h world.c world.h zone.c \
	bspfile.h protocol.h server.h sys.c

EXTRA_DIST= \
	qwsv.dsp

