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

#include <stdarg.h>
#include <stdio.h>

#include "SDL.h"

#include "quakedef.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "host.h"
#include "keys.h"
#include "strlib.h"
#include "sys.h"


unsigned		d_8to32table[256];

cvar_t     *vid_mode;
cvar_t     *m_filter;
cvar_t     *_windowed_mouse;
cvar_t     *gl_ztrick;
cvar_t     *gl_driver;

cvar_t  *v_hwgamma;
cvar_t  *v_gamma;
cvar_t  *v_gammabias_r;
cvar_t  *v_gammabias_b;
cvar_t  *v_gammabias_g;
cvar_t  *v_tgamma;
cvar_t  *v_tgammabias_r;
cvar_t  *v_tgammabias_b;
cvar_t  *v_tgammabias_g;

static Uint16   hw_gamma_ramps[3][256];
static Uint8    tex_gamma_ramps[3][256];
qboolean		VID_Inited;

static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;

static int  old_windowed_mouse;
static qboolean  use_mouse = false;

static int  scr_width = 640, scr_height = 480, scr_bpp = 15;

/*-----------------------------------------------------------------------*/

//int       texture_mode = GL_NEAREST;
//int       texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int       texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int         texture_mode = GL_LINEAR;

//int       texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int       texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int         texture_extension_number = 1;

float       gldepthmin, gldepthmax;


const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean    isPermedia = false;
qboolean    gl_mtexable = false;

/*-----------------------------------------------------------------------*/

void
VID_Shutdown (void)
{
	SDL_Quit ();
}


#define GAMMA(c, g, b, n)	(Q_pow((double) c / n, (double) 1 / g) * BIT(b))
#define BUILD_GAMMA_RAMP(ramp, gamma, type, n) do {							\
		int _i, _bits;														\
		_bits = sizeof(type) * 8;											\
		for (_i = 0; _i < n; _i++) {										\
			ramp[_i] = (type)bound_bits(GAMMA(_i, gamma, _bits, n), _bits);	\
		}																	\
	} while (0)
	
static void
VID_InitTexGamma (void)
{
	int i;
	Uint8	*pal;
	Uint8	r, g, b;
	Uint32	*table;
	vec3_t	tex;

	tex[0] = v_tgamma->value + v_tgammabias_r->value;
	tex[1] = v_tgamma->value + v_tgammabias_g->value;
	tex[2] = v_tgamma->value + v_tgammabias_b->value;

	BUILD_GAMMA_RAMP(tex_gamma_ramps[0], tex[0], Uint8, 256);
	BUILD_GAMMA_RAMP(tex_gamma_ramps[1], tex[1], Uint8, 256);
	BUILD_GAMMA_RAMP(tex_gamma_ramps[2], tex[2], Uint8, 256);

	// 8 8 8 encoding
	pal = host_basepal;
	table = d_8to32table;
	for (i = 0; i < 256; i++) {
		r = tex_gamma_ramps[0][pal[0]];
		g = tex_gamma_ramps[1][pal[1]];
		b = tex_gamma_ramps[2][pal[2]];
		pal += 3;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		*table++ = (r << 24) + (g << 16) + (b << 8) + (255 << 0);
#else
		*table++ = (r << 0) + (g << 8) + (b << 16) + (255 << 24);
#endif
	}
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	d_8to32table[255] &= 0xffffff00;		// 255 is transparent
#else
	d_8to32table[255] &= 0x00ffffff;		// 255 is transparent
#endif
}

static void
GammaChanged (cvar_t *cvar)
{
	vec3_t	hw;

	if (!VID_Inited)
		return;

	// Might be init, we don't want to segfault.
	if (!(v_hwgamma && v_gamma && v_gammabias_r && v_gammabias_g &&
				v_gammabias_b))
		return;

	// Do we have and want to use hardware gamma?
	if (v_hwgamma->value != 0) {
		hw[0] = v_gamma->value + v_gammabias_r->value;
		hw[1] = v_gamma->value + v_gammabias_g->value;
		hw[2] = v_gamma->value + v_gammabias_b->value;

		BUILD_GAMMA_RAMP(hw_gamma_ramps[0], hw[0], Uint16, 256);
		BUILD_GAMMA_RAMP(hw_gamma_ramps[1], hw[1], Uint16, 256);
		BUILD_GAMMA_RAMP(hw_gamma_ramps[2], hw[2], Uint16, 256);

		if (SDL_SetGammaRamp(hw_gamma_ramps[0], hw_gamma_ramps[1],
			hw_gamma_ramps[2]) < 0) {
			// No hardware gamma support, turn off and set ROM.
			Con_Printf("No hardware gamma support: Disabling. (%s)\n",
						SDL_GetError());
			Cvar_Set(v_hwgamma, "0");
			v_hwgamma->flags |= CVAR_ROM;
		}
	}
}

/*
	CheckMultiTextureExtensions

	Check for ARB or SGIS multitexture support
*/
GLenum      gl_mtex_enum = 0;

void
CheckMultiTextureExtensions (void)
{
	Con_Printf ("Checking for multitexture... ");
	if (COM_CheckParm ("-nomtex")) {
		Con_Printf ("disabled\n");
		return;
	}
	if (strstr (gl_extensions, "GL_ARB_multitexture")) {
		Con_Printf ("GL_ARB_multitexture\n");
		qglMTexCoord2f = SDL_GL_GetProcAddress ("glMultiTexCoord2fARB");
		qglSelectTexture = SDL_GL_GetProcAddress ("glActiveTextureARB");
		gl_mtex_enum = GL_TEXTURE0_ARB;
		gl_mtexable = true;
	} else if (strstr (gl_extensions, "GL_SGIS_multitexture")) {
		Con_Printf ("GL_SGIS_multitexture\n");
		qglMTexCoord2f = SDL_GL_GetProcAddress ("glMTexCoord2fSGIS");
		qglSelectTexture = SDL_GL_GetProcAddress ("glSelectTextureSGIS");
		gl_mtex_enum = TEXTURE0_SGIS;
		gl_mtexable = true;
	} else
		Con_Printf ("none found\n");
}


/*
===============
GL_Init
===============
*/
void
GL_Init (void)
{
	Con_Printf ("Forcing glFinish\n");
	qglFinish();

	gl_vendor = qglGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = qglGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = qglGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = qglGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	CheckMultiTextureExtensions ();

	qglClearColor (0.3f, 0.3f, 0.3f, 0.5f);
	qglCullFace (GL_FRONT);
	qglEnable (GL_TEXTURE_2D);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
=================
GL_BeginRendering

=================
*/
void
GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}


void
GL_EndRendering (void)
{
	SDL_GL_SwapBuffers ();
}



void
VID_Init_Cvars (void)
{
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_NONE, NULL);
	m_filter = Cvar_Get ("m_filter", "0", CVAR_NONE, NULL);
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "1", CVAR_ARCHIVE, NULL);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", CVAR_NONE, NULL);
	gl_driver = Cvar_Get ("gl_driver", GL_LIBRARY, CVAR_ROM, NULL);
	v_hwgamma = Cvar_Get ("v_hwgamma", "1", CVAR_NONE, NULL);
	v_gamma = Cvar_Get ("v_gamma", "1", CVAR_NONE, &GammaChanged);
	v_gammabias_r = Cvar_Get ("v_gammabias_r", "0", CVAR_NONE, &GammaChanged);
	v_gammabias_g = Cvar_Get ("v_gammabias_g", "0", CVAR_NONE, &GammaChanged);
	v_gammabias_b = Cvar_Get ("v_gammabias_b", "0", CVAR_NONE, &GammaChanged);
	v_tgamma = Cvar_Get ("v_tgamma", "1", CVAR_ROM, NULL);
	v_tgammabias_r = Cvar_Get ("v_tgammabias_r", "0", CVAR_ROM, NULL);
	v_tgammabias_g = Cvar_Get ("v_tgammabias_g", "0", CVAR_ROM, NULL);
	v_tgammabias_b = Cvar_Get ("v_tgammabias_b", "0", CVAR_ROM, NULL);
}

void
VID_Init (unsigned char *palette)
{
	int         i;
	char        gldir[MAX_OSPATH];
	int         flags = SDL_OPENGL;
	const		SDL_VideoInfo *info = NULL;

	vid.colormap = host_colormap;

	// interpret command-line params

	// set vid parameters
	if ((i = COM_CheckParm ("-nomouse")) == 0)
		use_mouse = true;

	if ((i = COM_CheckParm ("-window")) == 0)
		flags |= SDL_FULLSCREEN;

	if ((i = COM_CheckParm ("-width")) != 0)
		scr_width = Q_atoi (com_argv[i + 1]);
	if (scr_width < 320)
		scr_width = 320;

	if ((i = COM_CheckParm ("-height")) != 0)
		scr_height = Q_atoi (com_argv[i + 1]);
	if (scr_height < 200)
		scr_height = 200;

	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = Q_atoi (com_argv[i + 1]);
	else
		vid.conwidth = scr_width;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = Q_atoi (com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (vid.conheight > scr_height)
		vid.conheight = scr_height;
	if (vid.conwidth > scr_width)
		vid.conwidth = scr_width;

	if (SDL_Init (SDL_INIT_VIDEO) != 0) {
		Sys_Error ("Could not init SDL video: %s\n", SDL_GetError ());
	}

    info = SDL_GetVideoInfo();
	
	if (!info) {
		Sys_Error ("Could not get video information!\n");
    }

	if ((i = COM_CheckParm ("-bpp")) != 0)
		scr_bpp = Q_atoi (com_argv[i + 1]);
	else
		scr_bpp = info->vfmt->BitsPerPixel;

	if (!GLF_Init()) {
		Sys_Error ("Could not init the libGL!\n");
	}

	// We want at least 444 (16 bit RGB)
	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 4);
	if (scr_bpp == 32) SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 4);

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);
	
	if (SDL_SetVideoMode (scr_width, scr_height, scr_bpp, flags) == NULL) {
		Sys_Error ("Could not init video mode: %s", SDL_GetError ());
	}

	SDL_WM_SetCaption ("Twilight NetQuake", "twilight");

	vid.height = scr_height;
	vid.width = scr_width;
	vid.bpp = scr_bpp;
	vid.aspect = ((float) vid.height / (float) vid.width) * (4.0 / 3.0);

	VID_Inited = true;
	GammaChanged(v_gamma);

	GL_Init ();

	snprintf (gldir, sizeof (gldir), "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	VID_InitTexGamma ();

	Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	if (use_mouse)
		SDL_ShowCursor (0);

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
					case SDLK_DELETE:
						sym = K_DEL;
						break;
					case SDLK_BACKSPACE:
						sym = K_BACKSPACE;
						break;
					case SDLK_F1:
						sym = K_F1;
						break;
					case SDLK_F2:
						sym = K_F2;
						break;
					case SDLK_F3:
						sym = K_F3;
						break;
					case SDLK_F4:
						sym = K_F4;
						break;
					case SDLK_F5:
						sym = K_F5;
						break;
					case SDLK_F6:
						sym = K_F6;
						break;
					case SDLK_F7:
						sym = K_F7;
						break;
					case SDLK_F8:
						sym = K_F8;
						break;
					case SDLK_F9:
						sym = K_F9;
						break;
					case SDLK_F10:
						sym = K_F10;
						break;
					case SDLK_F11:
						sym = K_F11;
						break;
					case SDLK_F12:
						sym = K_F12;
						break;
					case SDLK_BREAK:
					case SDLK_PAUSE:
						sym = K_PAUSE;
						break;
					case SDLK_UP:
						sym = K_UPARROW;
						break;
					case SDLK_DOWN:
						sym = K_DOWNARROW;
						break;
					case SDLK_RIGHT:
						sym = K_RIGHTARROW;
						break;
					case SDLK_LEFT:
						sym = K_LEFTARROW;
						break;
					case SDLK_INSERT:
						sym = K_INS;
						break;
					case SDLK_HOME:
						sym = K_HOME;
						break;
					case SDLK_END:
						sym = K_END;
						break;
					case SDLK_PAGEUP:
						sym = K_PGUP;
						break;
					case SDLK_PAGEDOWN:
						sym = K_PGDN;
						break;
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
					case SDLK_KP0:
						if (modstate & KMOD_NUM)
							sym = K_INS;
						else
							sym = SDLK_0;
						break;
					case SDLK_KP1:
						if (modstate & KMOD_NUM)
							sym = K_END;
						else
							sym = SDLK_1;
						break;
					case SDLK_KP2:
						if (modstate & KMOD_NUM)
							sym = K_DOWNARROW;
						else
							sym = SDLK_2;
						break;
					case SDLK_KP3:
						if (modstate & KMOD_NUM)
							sym = K_PGDN;
						else
							sym = SDLK_3;
						break;
					case SDLK_KP4:
						if (modstate & KMOD_NUM)
							sym = K_LEFTARROW;
						else
							sym = SDLK_4;
						break;
					case SDLK_KP5:
						sym = SDLK_5;
						break;
					case SDLK_KP6:
						if (modstate & KMOD_NUM)
							sym = K_RIGHTARROW;
						else
							sym = SDLK_6;
						break;
					case SDLK_KP7:
						if (modstate & KMOD_NUM)
							sym = K_HOME;
						else
							sym = SDLK_7;
						break;
					case SDLK_KP8:
						if (modstate & KMOD_NUM)
							sym = K_UPARROW;
						else
							sym = SDLK_8;
						break;
					case SDLK_KP9:
						if (modstate & KMOD_NUM)
							sym = K_PGUP;
						else
							sym = SDLK_9;
						break;
					case SDLK_KP_PERIOD:
						if (modstate & KMOD_NUM)
							sym = K_DEL;
						else
							sym = SDLK_PERIOD;
						break;
					case SDLK_KP_DIVIDE:
						sym = SDLK_SLASH;
						break;
					case SDLK_KP_MULTIPLY:
						sym = SDLK_ASTERISK;
						break;
					case SDLK_KP_MINUS:
						sym = SDLK_MINUS;
						break;
					case SDLK_KP_PLUS:
						sym = SDLK_PLUS;
						break;
					case SDLK_KP_ENTER:
						sym = SDLK_RETURN;
						break;
					case SDLK_KP_EQUALS:
						sym = SDLK_EQUALS;
						break;
				}
				// If we're not directly handled and still above 255
				// just force it to 0
				if (sym > 255)
					sym = 0;
				Key_Event (sym, state);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (!use_mouse)
					break;

				but = event.button.button;
				if (but == 2)
					but = 3;
				else if (but == 3)
					but = 2;
				switch (but) {
					case 1:
					case 2:
					case 3:
						Key_Event (K_MOUSE1 + but - 1, event.type
								   == SDL_MOUSEBUTTONDOWN);
						break;
					case 4:
						Key_Event (K_MWHEELUP, true);
						Key_Event (K_MWHEELUP, false);
						break;
					case 5:
						Key_Event (K_MWHEELDOWN, true);
						Key_Event (K_MWHEELDOWN, false);
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				if (!use_mouse)
					break;
				
				if (_windowed_mouse->value) {
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;

			case SDL_QUIT:
				CL_Disconnect ();
				Host_ShutdownServer (false);
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
	mouse_x = mouse_y = 0.0;
	old_mouse_x = old_mouse_y = 0.0;
}

void
IN_Shutdown (void)
{
}

/*
===========
IN_Commands
===========
*/
void
IN_Commands (void)
{
	if (!use_mouse)
		return;

	// FIXME: Move this to a Cvar callback when they're implemented
	if (old_windowed_mouse != _windowed_mouse->value) {
		old_windowed_mouse = _windowed_mouse->value;
		if (!_windowed_mouse->value)
			SDL_WM_GrabInput (SDL_GRAB_OFF);
		else
			SDL_WM_GrabInput (SDL_GRAB_ON);
	}
}

/*
===========
IN_Move
===========
*/
void
IN_Move (usercmd_t *cmd)
{
	if (m_filter->value) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

	if ((in_strafe.state & 1) || (lookstrafe->value
				&& freelook))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if (freelook)
		V_StopPitchDrift ();

	if (freelook && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward->value * mouse_y;
		else
			cmd->forwardmove -= m_forward->value * mouse_y;
	}

	mouse_x = mouse_y = 0.0;
}

