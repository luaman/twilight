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

#include "cclient.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "gl_arrays.h"
#include "gl_info.h"
#include "host.h"
#include "hud.h"
#include "keys.h"
#include "mathlib.h"
#include "palette.h"
#include "quakedef.h"
#include "strlib.h"
#include "sys.h"
#include "video.h"

extern cvar_t *cl_verstring;

static cvar_t *width_2d;
static cvar_t *height_2d;
static cvar_t *text_scale;
static cvar_t *gl_driver;

static cvar_t *vid_width;
static cvar_t *vid_height;
static cvar_t *vid_windowed;
static cvar_t *vid_bpp;

viddef_t	vid;

qboolean VID_Inited;

int sdl_flags = SDL_OPENGL;


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

	Com_Printf ("Video mode %dx%d initialized: %s.\n", vid.width, vid.height,
			SDL_VideoDriverName(sdl_driver, sizeof(sdl_driver)));
}

void
VID_Shutdown (void)
{
	DynGL_CloseLibrary ();
//	SDL_QuitSubSystem (SDL_INIT_VIDEO);
	SDL_SetVideoMode (vid.width, vid.height, vid.bpp, sdl_flags & ~(SDL_OPENGL | SDL_FULLSCREEN));
	VID_Inited = false;

	memset (&vid, 0, sizeof (viddef_t));

	Com_DPrintf ("VID_Shutdown: Successful.\n");
}
