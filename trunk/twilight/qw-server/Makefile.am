# $Id$
#
# This file is intended to be processed by automake

AUTOMAKE_OPTIONS=gnu dist-zip

INCLUDES= \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/qw

LIBS=@LIBS@ -lm @SDL_LIBS@ $(twilight_qwsv_sys_libs)
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -Umain -DTWILIGHT_QWSV

bin_PROGRAMS=twilight-qwsv
twilight_qwsv_LDADD=$(top_srcdir)/src/base/libtwilight_base.a

if SYS_WIN32
twilight_qwsv_sys_libs=-lwsock32 -lwinmm
else
twilight_qwsv_sys_libs=
endif

twilight_qwsv_SOURCES= \
	cmd.h common.c console.h \
	mdfour.c net.h net_chan.c net_udp.c \
	pmove.c pmovetst.c \
	pr_cmds.c pr_edict.c pr_exec.c progdefs.h progs.h \
	protocol.h server.h sv_ccmds.c \
	sv_ents.c sv_init.c sv_main.c sv_model.c sv_move.c sv_nchan.c sv_phys.c \
        sv_send.c sv_sys.c sv_user.c world.c world.h

EXTRA_DIST= \
	qwsv.dsp
