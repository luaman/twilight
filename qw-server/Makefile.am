# $Id$
#
# This file is intended to be processed by automake

bin_PROGRAMS=twilight-qwsv

twilight_qwsv_SOURCES= \
	common.c common.h cmd.c cmd.h crc.c crc.h cvar.c cval.h mathlib.c \
	mathlib.h md4.c \
	model.c model.h modelgen.h spritegn.h \
	net.h net_chan.c net_udp.c \
	pmove.c pmove.h pmovetst.c pr_cmds.c pr_edict.c pr_exec.c \
	progs.h progdefs.h pc_comp.h \
	sv_ccmds.c sv_ents.c sv_init.c sv_main.c sv_move.c sv_nchan.c \
	sv_phys.c sv_send.c sv_user.c \
	sys_unix.c sys.h world.c world.h zone.c zone.h \
	quakedef.h bothdefs.h bspfile.h protocol.h qwsvdef.h server.h 

LDFLAGS=-lm
CFLAGS=@CFLAGS@ -DSHAREPATH=\"$(SHAREPATH)\" -DUSERPATH=\"$(USERPATH)\"
