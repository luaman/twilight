#!/usr/bin/make -f

SHAREPATH=\"/usr/share/games/twilight\"
USERPATH=\"/home/knghtbrd/.twilight\"


OBJECTS=cd_linux.o cl_cam.o cl_demo.o cl_ents.o cl_input.o cl_main.o \
	cl_parse.o cl_pred.o cl_tent.o cmd.o common.o console.o crc.o \
	cvar.o gl_draw.o gl_mesh.o gl_model.o gl_ngraph.o gl_refrag.o \
	gl_rlight.o gl_rmain.o gl_rmisc.o gl_rsurf.o gl_screen.o gl_warp.o \
	keys.o mathlib.o md4.o menu.o net_chan.o net_udp.o nonintel.o \
	pmove.o pmovetst.o r_part.o sbar.o skin.o snd_dma.o snd_mem.o \
	snd_mix.o snd_linux.o sys_linux.o vid_sdl.o view.o wad.o zone.o
#ASM_OBJECTS=math.o snd_mixa.o sys_dosa.o

PATHS=-DSHAREPATH=$(SHAREPATH) -DUSERPATH=$(USERPATH)
CFLAGS=-ggdb -Wall -Werror $(PATHS) `sdl-config --cflags`
OPTIMIZATIONS= -O6 -ffast-math -funroll-loops -fomit-frame-pointer \
	-fexpensive-optimizations

LDFLAGS= `sdl-config --libs` -lGL

all: twilight-qw

twilight-qw: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

.SUFFIXES: .c .o

.c.o:
	gcc $(CFLAGS) $(OPTIMIZATIONS) -o $@ -c $<

clean:
	rm -f twilight-qw *.o

.PHONY: clean

