/*
	$RCSfile$

	Copyright (C) 1999  Sam Lantinga
	Copyright (C) 2001  Joseph Carter

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "SDL.h"

#include "opengl_ext.h"
#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "keys.h"
#include "glquake.h"
#include "host.h"
#include "mathlib.h"
#include "sys.h"
#include "console.h"


Uint32	d_8to32table[256];
float	d_8tofloattable[256][4];

cvar_t	*width_2d;
cvar_t	*height_2d;
cvar_t	*text_scale;
cvar_t	*i_keypadmode;
cvar_t	*vid_mode;
cvar_t	*m_filter;
cvar_t	*_windowed_mouse;
cvar_t	*gl_ztrick;
cvar_t	*gl_driver;

cvar_t	*v_hwgamma;
cvar_t	*v_gamma;
cvar_t	*v_gammabias_r;
cvar_t	*v_gammabias_b;
cvar_t	*v_gammabias_g;
cvar_t	*v_tgamma;
cvar_t	*v_tgammabias_r;
cvar_t	*v_tgammabias_b;
cvar_t	*v_tgammabias_g;

static Uint16	hw_gamma_ramps[3][256];
static Uint8	tex_gamma_ramps[3][256];
qboolean		VID_Inited;
qboolean		keypadmode = false;

static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;

static qboolean use_mouse = false;

static int  sdl_flags = SDL_OPENGL;

/*-----------------------------------------------------------------------*/

int         texture_extension_number = 1;

float       gldepthmin, gldepthmax;


const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean	gl_cva = false;
qboolean	gl_mtex = false;
qboolean	gl_mtexcombine = false;
int			gl_tmus = 1;

void		I_KeypadMode (cvar_t *cvar);
void		IN_WindowedMouse (cvar_t *cvar);

/*-----------------------------------------------------------------------*/
void
VID_Shutdown (void)
{
	DGL_CloseLibrary ();
	//SDL_Quit (); // LordHavoc: don't quit yet, we might be called by Sys_Error
}

static void
VID_Build_Gamma_Ramp8 (Uint8 *ramp, int n, double gam, double con, double bri)
{
	int	i;
	double i_d, i_s, invgam;

	i_s = 1.0 / n;
	invgam = 1.0 / gam;

	for (i_d = i = 0; i < n; i++, i_d++) {
		ramp[i] = bound_bits((Q_pow(i_d * i_s, invgam) * con + bri) * BIT(8),8);
	}
}

static void
VID_Build_Gamma_Ramp16 (Uint16 *ramp, int n, double gam, double con, double bri)
{
	int	i;
	double i_d, i_s, invgam;

	i_s = 1.0 / n;
	invgam = 1.0 / gam;

	for (i_d = i = 0; i < n; i++, i_d++) {
		ramp[i]=bound_bits((Q_pow(i_d * i_s, invgam) * con + bri) * BIT(16),16);
	}
}

static void
VID_InitTexGamma (void)
{
	int i;
	Uint8	*pal;
	Uint8	r, g, b;
	vec3_t	tex;

	tex[0] = v_tgamma->fvalue + v_tgammabias_r->fvalue;
	tex[1] = v_tgamma->fvalue + v_tgammabias_g->fvalue;
	tex[2] = v_tgamma->fvalue + v_tgammabias_b->fvalue;

	VID_Build_Gamma_Ramp8(tex_gamma_ramps[0], 256, tex[0], 1, 0);
	VID_Build_Gamma_Ramp8(tex_gamma_ramps[1], 256, tex[1], 1, 0);
	VID_Build_Gamma_Ramp8(tex_gamma_ramps[2], 256, tex[2], 1, 0);

	/* 8 8 8 encoding */
	pal = host_basepal;
	for (i = 0; i < 256; i++) {
		r = tex_gamma_ramps[0][pal[0]];
		g = tex_gamma_ramps[1][pal[1]];
		b = tex_gamma_ramps[2][pal[2]];
		pal += 3;

		d_8tofloattable[i][0] = (float) r / 255;
		d_8tofloattable[i][1] = (float) g / 255;
		d_8tofloattable[i][2] = (float) b / 255;
		d_8tofloattable[i][3] = 1;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		d_8to32table[i] = (r << 24) + (g << 16) + (b << 8) + (255 << 0);
#else
		d_8to32table[i] = (r << 0) + (g << 8) + (b << 16) + (255 << 24);
#endif
	}
	d_8to32table[255] = 0x00000000;		/* 255 is transparent */
	VectorSet4 (d_8tofloattable[255], 0, 0, 0, 0);
}

static void
GammaChanged (cvar_t *cvar)
{
	vec3_t	hw;

	if (!VID_Inited)
		return;

	/* Might be init, we don't want to segfault. */
	if (!(v_hwgamma && v_gamma && v_gammabias_r && v_gammabias_g &&
				v_gammabias_b))
		return;

	/* Do we have and want to use hardware gamma? */
	hw[0] = v_gamma->fvalue + v_gammabias_r->fvalue;
	hw[1] = v_gamma->fvalue + v_gammabias_g->fvalue;
	hw[2] = v_gamma->fvalue + v_gammabias_b->fvalue;

	if (v_hwgamma->ivalue == 1) {
		VID_Build_Gamma_Ramp16(hw_gamma_ramps[0], 256, hw[0], 1, 0);
		VID_Build_Gamma_Ramp16(hw_gamma_ramps[1], 256, hw[1], 1, 0);
		VID_Build_Gamma_Ramp16(hw_gamma_ramps[2], 256, hw[2], 1, 0);

		if (SDL_SetGammaRamp(hw_gamma_ramps[0], hw_gamma_ramps[1],
			hw_gamma_ramps[2]) < 0) {
			/* No hardware gamma support, turn off and set ROM. */
			Com_Printf("No hardware gamma support: Disabling. (%s)\n",
						SDL_GetError());
			Cvar_Set(v_hwgamma, "0");
			v_hwgamma->flags |= CVAR_ROM;
		}
	} else if (v_hwgamma->ivalue == 2) {
		SDL_SetGamma(hw[0], hw[1], hw[2]);
	}
}


/*
	CheckExtensions

	Check for the OpenGL extensions we use
*/

void
CheckExtensions (void)
{
	qboolean	gl_mtexcombine_arb = 0, gl_mtexcombine_ext = 0;

	if (!COM_CheckParm ("-nomtex")) {
		gl_mtex = DGL_HasExtension ("GL_ARB_multitexture");
	}
	Com_Printf ("Checking for multitexture: ");
	if (gl_mtex) {
		qglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_tmus);
		Com_Printf ("GL_ARB_multitexture. (%d TMUs)\n", gl_tmus);
	} else
		Com_Printf ("no.\n");

	if (gl_mtex && (!qglActiveTextureARB || !qglMultiTexCoord2fARB)) {
		Sys_Error ("Missing GL_ARB_multitexture functions. (%p %p)\n",
				qglActiveTextureARB, qglMultiTexCoord2fARB);
	}

	if (gl_mtex && !COM_CheckParm ("-nomtexcombine")) {
		gl_mtexcombine_arb = DGL_HasExtension ("GL_ARB_texture_env_combine");
		gl_mtexcombine_ext = DGL_HasExtension ("GL_EXT_texture_env_combine");
	}
	Com_Printf ("Checking for texenv combine: ");
	if (gl_mtex && gl_mtexcombine_arb) {
		Com_Printf ("GL_ARB_texture_env_combine.\n");
		gl_mtexcombine = true;
	} else if (gl_mtex && gl_mtexcombine_ext) {
		Com_Printf ("GL_EXT_texture_env_combine.\n");
		gl_mtexcombine = true;
	} else {
		Com_Printf ("no.\n");
	}

	if (!COM_CheckParm ("-nocva"))
		gl_cva = DGL_HasExtension ("GL_EXT_compiled_vertex_array");

	Com_Printf ("Checking for compiled vertex arrays... %s\n",
			gl_cva ? "GL_EXT_compiled_vertex_array." : "no.");
}

void
GL_Info_f (void)
{
	Com_Printf ("GL_VENDOR: %s\n", gl_vendor);
	Com_Printf ("GL_RENDERER: %s\n", gl_renderer);
	Com_Printf ("GL_VERSION: %s\n", gl_version);
	Com_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);
}
		

/*
===============
GL_Init
===============
*/
void
GL_Init (void)
{
	qglFinish();

	gl_vendor = qglGetString (GL_VENDOR);
	gl_renderer = qglGetString (GL_RENDERER);

	gl_version = qglGetString (GL_VERSION);
	gl_extensions = qglGetString (GL_EXTENSIONS);

	CheckExtensions ();

	qglClearColor (0.3f, 0.3f, 0.3f, 0.5f);
	qglCullFace (GL_FRONT);
	qglEnable (GL_TEXTURE_2D);

	qglAlphaFunc (GL_GREATER, 0.666);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void
GL_EndRendering (void)
{
	SDL_GL_SwapBuffers ();
}

void
Size_Changed2D (cvar_t *cvar)
{
	int		width, height;
	float	txt_scale;

	if (con) {
		txt_scale = bound (0.5, text_scale->fvalue, 3);
		con->tsize = 8 * txt_scale;
	}

	if (!VID_Inited) {
		vid.width_2d = 320;
		vid.height_2d = 240;
		Con_CheckResize();
		return;
	}

	width = width_2d->ivalue;
	height = height_2d->ivalue;

	if (width == -1)
		width = vid.width;

	width = bound (320, width, (int) vid.width);

	width &= 0xfff8;				/* make it a multiple of eight */

	/* pick a conheight that matches with correct aspect */
	if (height == -1)
		height = width * (vid.width / vid.height);

	height = bound (240, height, (int) vid.height);

	vid.width_2d = width;
	vid.height_2d = height;

	Con_CheckResize();
}

void
VID_Init_Cvars (void)
{
	width_2d = Cvar_Get ("width_2d", "-1", CVAR_ARCHIVE, &Size_Changed2D);
	height_2d = Cvar_Get ("height_2d", "-1", CVAR_ARCHIVE, &Size_Changed2D);
	text_scale = Cvar_Get ("text_scale", "1", CVAR_ARCHIVE, &Size_Changed2D);
	i_keypadmode = Cvar_Get ("i_keypadmode", "0", CVAR_NONE, &I_KeypadMode);
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_NONE, NULL);
	m_filter = Cvar_Get ("m_filter", "0", CVAR_NONE, NULL);
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "1", CVAR_ARCHIVE,
			&IN_WindowedMouse);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", CVAR_NONE, NULL);
	gl_driver = Cvar_Get ("gl_driver", GL_LIBRARY, CVAR_ROM, NULL);
	v_hwgamma = Cvar_Get ("v_hwgamma", "1", CVAR_NONE, &GammaChanged);
	v_gamma = Cvar_Get ("v_gamma", "1", CVAR_ARCHIVE, &GammaChanged);
	v_gammabias_r = Cvar_Get ("v_gammabias_r", "0", CVAR_ARCHIVE, &GammaChanged);
	v_gammabias_g = Cvar_Get ("v_gammabias_g", "0", CVAR_ARCHIVE, &GammaChanged);
	v_gammabias_b = Cvar_Get ("v_gammabias_b", "0", CVAR_ARCHIVE, &GammaChanged);
	v_tgamma = Cvar_Get ("v_tgamma", "1", CVAR_ROM, NULL);
	v_tgammabias_r = Cvar_Get ("v_tgammabias_r", "0", CVAR_ROM, NULL);
	v_tgammabias_g = Cvar_Get ("v_tgammabias_g", "0", CVAR_ROM, NULL);
	v_tgammabias_b = Cvar_Get ("v_tgammabias_b", "0", CVAR_ROM, NULL);
}

void
VID_Init (unsigned char *palette)
{
	int         i;
	const		SDL_VideoInfo *info = NULL;
	char		sdl_driver[256];

	/* interpret command-line params */

	/* set vid parameters */
	if ((i = COM_CheckParm ("-nomouse")) == 0)
		use_mouse = true;

	if ((i = COM_CheckParm ("-window")) == 0)
		sdl_flags |= SDL_FULLSCREEN;

	vid.width = 640;
	vid.height = 480;

	i = COM_CheckParm ("-width");
	if (i && i < com_argc - 1)
		vid.width = Q_atoi (com_argv[i + 1]);

	i = COM_CheckParm ("-height");
	if (i && i < com_argc - 1)
		vid.height = Q_atoi (com_argv[i + 1]);

	i = COM_CheckParm ("-winsize");
	if (i && i < com_argc - 2)
	{
		vid.width = Q_atoi (com_argv[i + 1]);
		vid.height = Q_atoi (com_argv[i + 2]);
	}

	vid.width = max (320, vid.width);
	vid.height = max (240, vid.height);

	i = COM_CheckParm ("-conwidth");
	if (i && i < (com_argc - 1))
		Cvar_Set(width_2d, com_argv[i + 1]);

	i = COM_CheckParm ("-conheight");
	if (i && i < (com_argc - 1))
		Cvar_Set(height_2d, com_argv[i + 1]);

	if (SDL_InitSubSystem (SDL_INIT_VIDEO) != 0) {
		Sys_Error ("Could not init SDL video: %s\n", SDL_GetError ());
	}

	info = SDL_GetVideoInfo();

	if (!info) {
		Sys_Error ("Could not get video information!\n");
    }

	Sys_Printf ("Using OpenGL driver '%s'\n", gl_driver->svalue);
	if (!DGL_LoadLibrary(gl_driver->svalue))
		Sys_Error("%s\n", DGL_GetError());
	Com_DPrintf ("VID_Init: DGL_LoadLibrary successful.\n");

	i = COM_CheckParm ("-bpp");
	if (i && i < com_argc - 1)
		vid.bpp = Q_atoi (com_argv[i + 1]);
	else
		vid.bpp = info->vfmt->BitsPerPixel;

	/* We want at least 444 (16 bit RGB) */
	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 4);
	if (vid.bpp == 32) SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 4);

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);

	if (SDL_SetVideoMode (vid.width, vid.height, vid.bpp, sdl_flags) == NULL) {
		Sys_Error ("Could not init video mode: %s", SDL_GetError ());
	}
	Com_DPrintf ("VID_Init: SDL_SetVideoMode successful.\n");

	if (!DGL_GetFuncs())
		Sys_Error("%s\n", DGL_GetError());
	Com_DPrintf ("VID_Init: DGL_GetFuncs successful.\n");

	SDL_WM_SetCaption ("Twilight QWCL", "twilight");
	Com_DPrintf ("VID_Init: Window caption set.\n");

	VID_Inited = true;
	GammaChanged(v_gamma);
	Size_Changed2D(NULL);

	GL_Init ();
	Com_DPrintf ("VID_Init: GL_Init successful.\n");

	VID_InitTexGamma ();

	Com_Printf ("Video mode %dx%d initialized: %s.\n", vid.width, vid.height,
			SDL_VideoDriverName(sdl_driver, sizeof(sdl_driver)));

	vid.recalc_refdef = true;	/* force a surface cache flush */

	if (use_mouse) {
		SDL_ShowCursor (0);
		SDL_WM_GrabInput (SDL_GRAB_ON);
	}

	Cmd_AddCommand ("gl_info", &GL_Info_f);
}

void
Sys_SendKeyEvents (void)
{
	SDL_Event   event;
	int         sym, state, but;
	SDLMod      modstate;

	while (SDL_PollEvent (&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				modstate = SDL_GetModState ();
				switch (sym) {
					case SDLK_DELETE: sym = K_DEL; break;
					case SDLK_BACKSPACE: sym = K_BACKSPACE; break;
					case SDLK_F1: sym = K_F1; break;
					case SDLK_F2: sym = K_F2; break;
					case SDLK_F3: sym = K_F3; break;
					case SDLK_F4: sym = K_F4; break;
					case SDLK_F5: sym = K_F5; break;
					case SDLK_F6: sym = K_F6; break;
					case SDLK_F7: sym = K_F7; break;
					case SDLK_F8: sym = K_F8; break;
					case SDLK_F9: sym = K_F9; break;
					case SDLK_F10: sym = K_F10; break;
					case SDLK_F11: sym = K_F11; break;
					case SDLK_F12: sym = K_F12; break;
					case SDLK_BREAK:
					case SDLK_PAUSE:
						sym = K_PAUSE;
						break;
					case SDLK_UP: sym = K_UPARROW; break;
					case SDLK_DOWN: sym = K_DOWNARROW; break;
					case SDLK_RIGHT: sym = K_RIGHTARROW; break;
					case SDLK_LEFT: sym = K_LEFTARROW; break;
					case SDLK_INSERT: sym = K_INS; break;
					case SDLK_HOME: sym = K_HOME; break;
					case SDLK_END: sym = K_END; break;
					case SDLK_PAGEUP: sym = K_PGUP; break;
					case SDLK_PAGEDOWN: sym = K_PGDN; break;
					case SDLK_RSHIFT:
					case SDLK_LSHIFT:
						sym = K_SHIFT;
						break;
					case SDLK_RCTRL:
					case SDLK_LCTRL:
						sym = K_CTRL;
						break;
					case SDLK_RALT:
					case SDLK_LALT:
						sym = K_ALT;
						break;
					case SDLK_NUMLOCK: sym = K_NUMLOCK; break;
					case SDLK_CAPSLOCK: sym = K_CAPSLOCK; break;
					case SDLK_SCROLLOCK: sym = K_SCROLLOCK; break;
					case SDLK_KP0: case SDLK_KP1: case SDLK_KP2: case SDLK_KP3:
					case SDLK_KP4: case SDLK_KP5: case SDLK_KP6: case SDLK_KP7:
					case SDLK_KP8: case SDLK_KP9: case SDLK_KP_PERIOD:
					case SDLK_KP_DIVIDE: case SDLK_KP_MULTIPLY:
					case SDLK_KP_MINUS: case SDLK_KP_PLUS:
					case SDLK_KP_ENTER: case SDLK_KP_EQUALS:
						if (keypadmode) {
							switch (sym) {
								case SDLK_KP0: sym = K_KP_0; break;
								case SDLK_KP1: sym = K_KP_1; break;
								case SDLK_KP2: sym = K_KP_2; break;
								case SDLK_KP3: sym = K_KP_3; break;
								case SDLK_KP4: sym = K_KP_4; break;
								case SDLK_KP5: sym = K_KP_5; break;
								case SDLK_KP6: sym = K_KP_6; break;
								case SDLK_KP7: sym = K_KP_7; break;
								case SDLK_KP8: sym = K_KP_8; break;
								case SDLK_KP9: sym = K_KP_9; break;
								case SDLK_KP_PERIOD: sym = K_KP_PERIOD; break;
								case SDLK_KP_DIVIDE: sym = K_KP_DIVIDE; break;
								case SDLK_KP_MULTIPLY:sym = K_KP_MULTIPLY;break;
								case SDLK_KP_MINUS: sym = K_KP_MINUS; break;
								case SDLK_KP_PLUS: sym = K_KP_PLUS; break;
								case SDLK_KP_ENTER: sym = K_KP_ENTER; break;
								case SDLK_KP_EQUALS: sym = K_KP_EQUALS; break;
							}
						} else if (modstate & KMOD_NUM) {
							switch (sym) {
								case SDLK_KP0: sym = K_INS; break;
								case SDLK_KP1: sym = K_END; break;
								case SDLK_KP2: sym = K_DOWNARROW; break;
								case SDLK_KP3: sym = K_PGDN; break;
								case SDLK_KP4: sym = K_LEFTARROW; break;
								case SDLK_KP5: sym = SDLK_5; break;
								case SDLK_KP6: sym = K_RIGHTARROW; break;
								case SDLK_KP7: sym = K_HOME; break;
								case SDLK_KP8: sym = K_UPARROW; break;
								case SDLK_KP9: sym = K_PGUP; break;
								case SDLK_KP_PERIOD: sym = K_DEL; break;
								case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
								case SDLK_KP_MULTIPLY:sym = SDLK_ASTERISK;break;
								case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
								case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
								case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
								case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
							}
						} else {
							switch (sym) {
								case SDLK_KP0: sym = SDLK_0; break;
								case SDLK_KP1: sym = SDLK_1; break;
								case SDLK_KP2: sym = SDLK_2; break;
								case SDLK_KP3: sym = SDLK_3; break;
								case SDLK_KP4: sym = SDLK_4; break;
								case SDLK_KP5: sym = SDLK_5; break;
								case SDLK_KP6: sym = SDLK_6; break;
								case SDLK_KP7: sym = SDLK_7; break;
								case SDLK_KP8: sym = SDLK_8; break;
								case SDLK_KP9: sym = SDLK_9; break;
								case SDLK_KP_PERIOD: sym = SDLK_PERIOD; break;
								case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
								case SDLK_KP_MULTIPLY:sym = SDLK_ASTERISK;break;
								case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
								case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
								case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
								case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
							}
						}
						break;
				}
				/* If we're not directly handled and still above 255 just force it to 0 */
				if (sym > 255)
					sym = 0;
				Key_Event (sym, state);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (!use_mouse)
					break;

				but = event.button.button;
				if ((but < 1) || (but > 16))
					break;

				switch (but) {
					case 2: but = 3; break;
					case 3: but = 2; break;
				}

				Key_Event (K_MOUSE1 + (but - 1),
						event.type == SDL_MOUSEBUTTONDOWN);
				break;

			case SDL_MOUSEMOTION:
				if (!use_mouse)
					break;

				if (_windowed_mouse->ivalue && (cls.state >= ca_connected)) {
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;

			case SDL_QUIT:
				CL_Disconnect ();
				/* Host_ShutdownServer (false); */
				Sys_Quit ();
				break;

			default:
				break;
		}
	}
}


void
Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void
IN_Init_Cvars (void)
{
}

void
IN_Init (void)
{
	mouse_x = 0.0f;
	mouse_y = 0.0f;
	old_mouse_x = old_mouse_y = 0.0f;
}

void
IN_Shutdown (void)
{
}

void
IN_WindowedMouse (cvar_t *cvar)
{
	if (!use_mouse)
		return;

	if (sdl_flags & SDL_FULLSCREEN)
	{
		_windowed_mouse->flags |= CVAR_ROM;
		Cvar_Set (_windowed_mouse, "1");
		return;
	}

	if (!_windowed_mouse->ivalue)
		SDL_WM_GrabInput (SDL_GRAB_OFF);
	else
		SDL_WM_GrabInput (SDL_GRAB_ON);
}

void
I_KeypadMode (cvar_t *cvar)
{
	keypadmode = !!cvar->ivalue;
}

/*
===========
IN_Commands
===========
*/
void
IN_Commands (void)
{
}

/*
===========
IN_Move
===========
*/
void
IN_Move (usercmd_t *cmd)
{
	if (m_filter->fvalue &&
		((mouse_x != old_mouse_x) ||
		(mouse_y != old_mouse_y))) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	mouse_x *= sensitivity->fvalue;
	mouse_y *= sensitivity->fvalue;

	if ((in_strafe.state & 1) || (lookstrafe->ivalue && freelook))
		cmd->sidemove += m_side->fvalue * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->fvalue * mouse_x;

	if (freelook)
		V_StopPitchDrift ();

	if (freelook && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch->fvalue * mouse_y;
		cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
	} else {
		if (in_strafe.state & 1)
			cmd->upmove -= m_forward->fvalue * mouse_y;
		else
			cmd->forwardmove -= m_forward->fvalue * mouse_y;
	}

	mouse_x = mouse_y = 0.0;
}

