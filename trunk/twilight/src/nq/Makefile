#!/usr/bin/make -f

OBJECTS=cd_linux.o chase.o cl_demo.o cl_input.o cl_main.o cl_parse.o \
	cl_tent.o cmd.o common.o console.o crc.o cvar.o gl_draw.o gl_mesh.o \
	gl_model.o gl_refrag.o gl_rlight.o gl_rmain.o gl_rmisc.o gl_rsurf.o \
	gl_screen.o gl_warp.o host.o host_cmd.o keys.o \
	mathlib.o menu.o net_bsd.o net_dgrm.o net_loop.o net_main.o \
	net_udp.o net_vcr.o pr_cmds.o pr_edict.o pr_exec.o r_part.o sbar.o \
	snd_dma.o snd_mem.o snd_mix.o snd_linux.o sv_main.o sv_move.o \
	sv_phys.o sv_user.o sys_linux.o vid_sdl.o view.o wad.o world.o zone.o
#ASM_OBJECTS=math.o snd_mixa.o sys_dosa.o worlda.o

CFLAGS= -DGLQUAKE -ggdb -Wall `sdl-config --cflags`
OPTIMIZATIONS= -O6 -ffast-math -funroll-loops -fomit-frame-pointer \
	-fexpensive-optimizations

LDFLAGS= `sdl-config --libs` -lGL

all: frost

frost: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

.SUFFIXES: .c .o

.c.o:
	gcc $(CFLAGS) $(OPTIMIZATIONS) -o $@ -c $<

clean:
	rm -f frost *.o

.PHONY: clean

