#!/usr/bin/make -f

OBJECTS=common.o cmd.o crc.o cvar.o mathlib.o md4.o model.o net_chan.o \
	net_udp.o pmove.o pmovetst.o pr_cmds.o pr_edict.o pr_exec.o \
	sv_ccmds.o sv_ents.o sv_init.o sv_main.o sv_move.o sv_nchan.o \
	sv_phys.o sv_send.o sv_user.o sys_unix.o world.o zone.o
#ASM_OBJECTS=math.o worlda.o

CFLAGS= -ggdb -Wall -Werror
OPTIMIZATIONS= -O6 -ffast-math -funroll-loops -fomit-frame-pointer \
	-fexpensive-optimizations

LDFLAGS=

all: twilight-qwsv

frost-qwsv: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

.SUFFIXES: .c .o

.c.o:
	gcc $(CFLAGS) $(OPTIMIZATIONS) -o $@ -c $<

clean:
	rm -f twilight-qwsv *.o

.PHONY: clean

