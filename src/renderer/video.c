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

#include "twiconfig.h"

#include <string.h>

#include "SDL.h"

#include "quakedef.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "sys.h"
#include "gl_info.h"
#include "gl_arrays.h"
#include "palette.h"
#include "video.h"
#include "cclient.h"
#include "hud.h"
#include "strlib.h"

extern cvar_t *cl_verstring;

static cvar_t *width_2d;
static cvar_t *height_2d;
static cvar_t *text_scale;
static cvar_t *i_keypadmode;
static cvar_t *_windowed_mouse;
static cvar_t *gl_driver;

static cvar_t *vid_width;
static cvar_t *vid_height;
static cvar_t *vid_windowed;
static cvar_t *vid_bpp;

viddef_t	vid;

qboolean VID_Inited;
static qboolean keypadmode = false;

float mouse_x, mouse_y;

static enum { M_NONE, M_FREE, M_GRAB, M_FULL } mouse = M_NONE;

static int sdl_flags = SDL_OPENGL;

/*-----------------------------------------------------------------------*/

static void I_KeypadMode (cvar_t *cvar);
static void IN_WindowedMouse (cvar_t *cvar);

/*-----------------------------------------------------------------------*/
void
GL_EndRendering (void)
{
	SDL_GL_SwapBuffers ();
}

void
Size_Changed2D (cvar_t *cvar)
{
	int			width, height;
	float		txt_scale;

	cvar = cvar;

	if (con)
	{
		txt_scale = bound (0.5, text_scale->fvalue, 3);
		con->tsize = 8 * txt_scale;
	}

	if (!VID_Inited)
	{
		vid.width_2d = 320;
		vid.height_2d = 240;
		Con_CheckResize ();
		HUD_Changed (height_2d);
		return;
	}

	width = width_2d->ivalue;
	height = height_2d->ivalue;

	if (width == -1)
		width = vid.width;

	width = bound (320, width, (int) vid.width);

	width &= 0xfff8;					/* make it a multiple of eight */

	/* pick a conheight that matches with correct aspect */
	if (height == -1)
		height = width * (vid.width / vid.height);

	height = bound (240, height, (int) vid.height);

	vid.width_2d = width;
	vid.height_2d = height;

	Con_CheckResize ();
	HUD_Changed (height_2d);
}

void
VID_Init_Cvars (void)
{
	Uint	i;

	width_2d = Cvar_Get ("width_2d", "-1", CVAR_ARCHIVE, &Size_Changed2D);
	height_2d = Cvar_Get ("height_2d", "-1", CVAR_ARCHIVE, &Size_Changed2D);
	text_scale = Cvar_Get ("text_scale", "1", CVAR_ARCHIVE, &Size_Changed2D);
	i_keypadmode = Cvar_Get ("i_keypadmode", "0", CVAR_ARCHIVE, &I_KeypadMode);
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "1", CVAR_ARCHIVE,
			&IN_WindowedMouse);
	gl_driver = Cvar_Get ("gl_driver", GL_LIBRARY, CVAR_ARCHIVE, NULL);


	vid_bpp = Cvar_Get ("vid_bpp", "0", CVAR_ARCHIVE, NULL);
	vid_width = Cvar_Get ("vid_width", "640", CVAR_ARCHIVE, NULL);
	vid_height = Cvar_Get ("vid_height", "0", CVAR_ARCHIVE, NULL);
	vid_windowed = Cvar_Get ("vid_windowed", "0", CVAR_ARCHIVE, NULL);

	if ((i = COM_CheckParm ("-window")))
		Cvar_Set (vid_windowed, "1");

	i = COM_CheckParm ("-width");
	if (i && i < com_argc - 1) {
		Cvar_Set (vid_width, com_argv[i + 1]);
		Cvar_Set (vid_height, va("%d", vid_width->ivalue * 3 / 4));
	}

	i = COM_CheckParm ("-height");
	if (i && i < com_argc - 1)
		Cvar_Set (vid_height, com_argv[i + 1]);

	i = COM_CheckParm ("-winsize");
	if (i && i < com_argc - 2)
	{
		Cvar_Set (vid_width, com_argv[i + 1]);
		Cvar_Set (vid_height, com_argv[i + 2]);
	}

	i = COM_CheckParm ("-conwidth");
	if (i && i < (com_argc - 1))
		Cvar_Set (width_2d, com_argv[i + 1]);

	i = COM_CheckParm ("-conheight");
	if (i && i < (com_argc - 1))
		Cvar_Set (height_2d, com_argv[i + 1]);

	i = COM_CheckParm ("-bpp");
	if (i && i < com_argc - 1)
		Cvar_Set (vid_bpp, com_argv[i + 1]);
}

void
VID_Init (void)
{
	const		SDL_VideoInfo *info = NULL;
	char		sdl_driver[256];

	/* interpret command-line params */

	sdl_flags = SDL_OPENGL;

	if (vid_windowed->ivalue)
		sdl_flags &= ~SDL_FULLSCREEN;
	else
		sdl_flags |= SDL_FULLSCREEN;

	vid.width = vid_width->ivalue;
	vid.height = vid_height->ivalue;
	if (!vid.height)
		vid.height = vid.width * 3 / 4;

	vid.width = max (512, vid.width);
	vid.height = max (384, vid.height);

	vid.bpp = vid_bpp->ivalue;

	if (SDL_InitSubSystem (SDL_INIT_VIDEO) != 0)
		Sys_Error ("Could not init SDL video: %s\n", SDL_GetError ());

	info = SDL_GetVideoInfo();

	if (!info)
		Sys_Error ("Could not get video information!\n");

	Sys_Printf ("Using OpenGL driver '%s'\n", gl_driver->svalue);
	if (!DynGL_LoadLibrary (gl_driver->svalue))
		Sys_Error ("%s\n", SDL_GetError ());
	Com_DPrintf ("VID_Init: DynGL_LoadLibrary successful.\n");

	if (vid.bpp >= 24) {
		/* Insist on at least 8 bits per channel */
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
	} else if (vid.bpp >= 16) {
		/* Insist on at least 5 bits per channel */
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5);
	} else {
		/* Take whatever OpenGL gives us */
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 1);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 1);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 1);
	}

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);

	if (SDL_SetVideoMode (vid.width, vid.height, vid.bpp, sdl_flags) == NULL)
		Sys_Error ("Could not init video mode: %s", SDL_GetError ());

	Com_DPrintf ("VID_Init: SDL_SetVideoMode successful.\n");

	if (!DynGL_GetFunctions (Com_Printf))
		Sys_Error ("%s\n", SDL_GetError ());
	Com_DPrintf ("VID_Init: DynGL_GetFuncs successful.\n");

	SDL_WM_SetCaption (cl_verstring->svalue, "twilight");
	Com_DPrintf ("VID_Init: Window caption set.\n");

	VID_Inited = true;
	PAL_Init ();
	Size_Changed2D (NULL);
	IN_WindowedMouse (_windowed_mouse);

	Com_Printf ("Video mode %dx%d initialized: %s.\n", vid.width, vid.height,
			SDL_VideoDriverName(sdl_driver, sizeof(sdl_driver)));
}

void
VID_Shutdown (void)
{
	if (mouse > M_NONE)
	{
		SDL_ShowCursor (1);
		SDL_WM_GrabInput (SDL_GRAB_OFF);
	}

	DynGL_CloseLibrary ();
//	SDL_QuitSubSystem (SDL_INIT_VIDEO);
	SDL_SetVideoMode (vid.width, vid.height, vid.bpp, sdl_flags & ~SDL_OPENGL);
	VID_Inited = false;

	memset (&vid, 0, sizeof (viddef_t));

	Com_DPrintf ("VID_Shutdown: Successful.\n");
}

void
Sys_SendKeyEvents (void)
{
	SDL_Event	event;
	int			sym, state, but;
	SDLMod		modstate;

	while (SDL_PollEvent (&event) > 0)
	{
		switch (event.type)
		{
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				modstate = SDL_GetModState ();
				switch (sym)
				{
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
						if (keypadmode)
						{
							switch (sym)
							{
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
						}
						else if (modstate & KMOD_NUM)
						{
							switch (sym)
							{
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
						}
						else
						{
							switch (sym)
							{
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

				/* Anything else above 255 we just don't handle */
				if (sym > 255)
					sym = 0;
				Key_Event (sym, state);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (mouse <= M_FREE)
					break;

				but = event.button.button;
				if ((but < 1) || (but > 16))
					break;

				switch (but)
				{
					case 2: but = 3; break;
					case 3: but = 2; break;
				}

				Key_Event (K_MOUSE1 + (but - 1),
						event.type == SDL_MOUSEBUTTONDOWN);
				break;

			case SDL_MOUSEMOTION:
				if ((mouse <= M_FREE) || !(ccls.state >= ca_connected))
					break;

				mouse_x += event.motion.xrel;
				mouse_y += event.motion.yrel;
				break;

			case SDL_QUIT:
//				CL_Disconnect ();
//				Host_ShutdownServer (false);
				Sys_Quit (0);
				break;

			default:
				break;
		}
	}
}


void
IN_Init (void)
{
	mouse_x = 0.0f;
	mouse_y = 0.0f;
//	old_mouse_x = old_mouse_y = 0.0f;
}

void
IN_Shutdown (void)
{
}

static void
IN_WindowedMouse (cvar_t *cvar)
{
	cvar = cvar;

	if (!VID_Inited)
		return;

	if (COM_CheckParm ("-nomouse"))
		mouse = M_NONE;
	else if ((sdl_flags & SDL_FULLSCREEN))
		mouse = M_FULL;
	else if (_windowed_mouse->ivalue)
		mouse = M_GRAB;
	else
		mouse = M_FREE;

	if (mouse >= M_GRAB)
	{
		SDL_ShowCursor (0);
		SDL_WM_GrabInput (SDL_GRAB_ON);
	} else {
		SDL_ShowCursor (1);
		SDL_WM_GrabInput (SDL_GRAB_OFF);
	}
}

static void
I_KeypadMode (cvar_t *cvar)
{
	keypadmode = !!cvar->ivalue;
}
