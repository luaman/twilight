
bin_PROGRAMS=twilight-qw

twilight_qw_SOURCES= \
	cl_cam.c cl_demo.c cl_ents.c cl_input.c cl_main.c cl_parse.c \
	cl_pred.c cl_tent.c client.h \
	cmd.c cmd.h common.c common.h console.c console.h crc.c crc.h \
	cvar.c cvar.h \
	gl_draw.c gl_mesh.c gl_model.c gl_model.h modelgen.h spritegn.h \
	gl_ngraph.c gl_refrag.c gl_rlight.c gl_rmain.c anorms.h anorm_dots.h \
	gl_rmisc.c gl_rsurf.c gl_screen.c screen.h gl_warp.c gl_warp_sin.h \
	glquake.h \
	keys.c mathlib.c mathlib.h md4.c menu.c menu.h \
	net.h net_chan.c net_udp.c \
	nonintel.c pmove.c pmove.h pmovetst.c r_part.c sbar.c sbar.h skin.c \
	snd_dma.c snd_mem.c \
	snd_mix.c snd_linux.c sound.h cd_sdl.c cdaudio.h \
	sys_linux.c sys.h vid_sdl.c vid.h \
	view.c view.h wad.c wad.h zone.c zone.h \
	buildnum.c \
	quakedef.h bothdefs.h bspfile.h draw.h input.h keys.h protocol.h \
	render.h

EXTRA_twilight_qw_SOURCES=math.s snd_mixa.s sys_dosa.s quakeasm.h

LDFLAGS=@SDL_LIBS@ @GL_LIBS@ -lm
CFLAGS=@CFLAGS@ @SDL_CFLAGS@ -DSHAREPATH=\"$(SHAREPATH)\" -DUSERPATH=\"$(USERPATH)\"
