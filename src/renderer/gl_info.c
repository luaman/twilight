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
#include "SDL.h"

#include <string.h>

#include "common.h"
#include "qtypes.h"
#include "dyngl.h"
#include "cmd.h"

GLfloat whitev[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

int fb_size[4];
int accum_size[4];
int doublebuffer, buffer_size, depth_size, stencil_size;

/*-----------------------------------------------------------------------*/

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean gl_cva = false;
qboolean gl_mtex = false;
qboolean gl_mtexcombine = false;
qboolean gl_secondary_color = false;
qboolean gl_nv_register_combiners = false;
int gl_tmus = 1;

/*
	CheckDriverQuirks

	Check for buggy OpenGL drivers.
*/

void
GLInfo_CheckDriverQuirks (void)
{
	if (strstr(gl_vendor, "NVIDIA")) {		// nVidia drivers.
		DynGL_BadExtension("GL_EXT_compiled_vertex_array");
		Com_Printf("Disabiling GL_EXT_compiled_vertex_array due to buggy drivers.\n");
	}
}


/*
	CheckExtensions

	Check for the OpenGL extensions we use
*/

void
GLInfo_CheckExtensions (void)
{
	qboolean	gl_mtexcombine_arb = 0, gl_mtexcombine_ext = 0;

	if (!COM_CheckParm ("-nomtex"))
		gl_mtex = DynGL_HasExtension ("GL_ARB_multitexture");

	Com_Printf ("Checking for multitexture: ");
	if (gl_mtex)
	{
		qglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_tmus);
		Com_Printf ("GL_ARB_multitexture. (%d TMUs)\n", gl_tmus);
	}
	else
		Com_Printf ("no.\n");

	if (gl_mtex && !COM_CheckParm ("-nomtexcombine"))
	{
		gl_mtexcombine_arb = DynGL_HasExtension ("GL_ARB_texture_env_combine");
		gl_mtexcombine_ext = DynGL_HasExtension ("GL_EXT_texture_env_combine");
	}
	Com_Printf ("Checking for texenv combine: ");
	if (gl_mtex && gl_mtexcombine_arb)
	{
		Com_Printf ("GL_ARB_texture_env_combine.\n");
		gl_mtexcombine = true;
	}
	else if (gl_mtex && gl_mtexcombine_ext)
	{
		Com_Printf ("GL_EXT_texture_env_combine.\n");
		gl_mtexcombine = true;
	}
	else
		Com_Printf ("no.\n");

	if (!COM_CheckParm ("-nocva"))
		gl_cva = DynGL_HasExtension ("GL_EXT_compiled_vertex_array");

	Com_Printf ("Checking for compiled vertex arrays: %s.\n",
			gl_cva ? "GL_EXT_compiled_vertex_array" : "no");

	gl_secondary_color = DynGL_HasExtension ("GL_EXT_secondary_color");
	Com_Printf ("Checking for GL_EXT_secondary_color: %s.\n",
			gl_secondary_color ? "Yes" : "No");

	gl_nv_register_combiners = DynGL_HasExtension ("GL_NV_register_combiners");
	Com_Printf ("Checking for GL_NV_register_combiners: %s.\n",
			gl_nv_register_combiners ? "Yes" : "No");
}

void
GL_Info_f (void)
{
	Com_Printf ("Frame Buffer: %d bpp, %d-%d-%d-%d, %s buffered\n",
			buffer_size, fb_size[0], fb_size[1], fb_size[2], fb_size[3],
			doublebuffer ? "double" : "single");
	Com_Printf ("Accum Buffer: %d-%d-%d-%d\n", accum_size[0], accum_size[1],
			accum_size[2], accum_size[3]);
	Com_Printf ("Depth bits: %d. Stencil bits: %d\n", depth_size, stencil_size);
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
GLInfo_Init (void)
{
	gl_vendor = qglGetString (GL_VENDOR);
	gl_renderer = qglGetString (GL_RENDERER);

	gl_version = qglGetString (GL_VERSION);
	gl_extensions = qglGetString (GL_EXTENSIONS);

	GLInfo_CheckDriverQuirks ();
	GLInfo_CheckExtensions ();

	SDL_GL_GetAttribute (SDL_GL_RED_SIZE, &fb_size[0]);
	SDL_GL_GetAttribute (SDL_GL_GREEN_SIZE, &fb_size[1]);
	SDL_GL_GetAttribute (SDL_GL_BLUE_SIZE, &fb_size[2]);
	SDL_GL_GetAttribute (SDL_GL_ALPHA_SIZE, &fb_size[3]);
	SDL_GL_GetAttribute (SDL_GL_DOUBLEBUFFER, &doublebuffer);
	SDL_GL_GetAttribute (SDL_GL_BUFFER_SIZE, &buffer_size);
	SDL_GL_GetAttribute (SDL_GL_DEPTH_SIZE, &depth_size);
	SDL_GL_GetAttribute (SDL_GL_STENCIL_SIZE, &stencil_size);
	SDL_GL_GetAttribute (SDL_GL_ACCUM_RED_SIZE, &accum_size[0]);
	SDL_GL_GetAttribute (SDL_GL_ACCUM_GREEN_SIZE, &accum_size[1]);
	SDL_GL_GetAttribute (SDL_GL_ACCUM_BLUE_SIZE, &accum_size[2]);
	SDL_GL_GetAttribute (SDL_GL_ACCUM_ALPHA_SIZE, &accum_size[3]);

	Com_Printf ("Frame Buffer: %d bpp, %d-%d-%d-%d, %s buffered\n",
			buffer_size, fb_size[0], fb_size[1], fb_size[2], fb_size[3],
			doublebuffer ? "double" : "single");
	Com_Printf ("Accum Buffer: %d-%d-%d-%d\n", accum_size[0], accum_size[1],
			accum_size[2], accum_size[3]);
	Com_Printf ("Depth bits: %d. Stencil bits: %d\n", depth_size, stencil_size);

	Cmd_AddCommand ("gl_info", &GL_Info_f);
}
