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
#include "sys.h"
#include "model.h"

model_t *r_worldmodel;
double r_time, r_frametime; // Current time, and time since last frame.
double r_realtime;			// Current real time, NOT affected by pausing.
Uint r_framecount;          // Current frame.

GLfloat whitev[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

int fb_size[4];
int accum_size[4];
int doublebuffer, buffer_size, depth_size, stencil_size;

cvar_t *gl_affinemodels;
cvar_t *gl_nocolors;
cvar_t *gl_im_animation;
cvar_t *gl_particletorches;
cvar_t *gl_cull;

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
qboolean gl_sgis_mipmap = false;
qboolean gl_vbo = false;
int gl_tmus = 1;

/*
	CheckDriverQuirks

	Check for buggy OpenGL drivers.
*/

static void
GLInfo_CheckDriverQuirks (void)
{
	/*
	if (strstr (gl_vendor, "NVIDIA")) {		// nVidia drivers.
		DynGL_BadExtension ("GL_EXT_compiled_vertex_array");
		Com_Printf ("Disabling GL_EXT_compiled_vertex_array due to buggy nVidia driver.\n");
	}
	*/

	if (!strcmp (gl_vendor, "ATI Technologies Inc."))	// ATI drivers.
	{
		// LordHavoc: Damm asked for it to be disabled on all driver versions for now
		DynGL_BadExtension ("GL_SGIS_generate_mipmap");
		Com_Printf ("Disabling GL_SGIS_generate_mipmap due to buggy ATI drivers.");
		// LordHavoc: kept for future use
		/*
		if (!strcmp (gl_version, "1.3.3224 Win2000 Release"))
		{
			DynGL_BadExtension ("GL_SGIS_generate_mipmap");
			Com_Printf ("Disabling GL_SGIS_generate_mipmap due to buggy ATI driver version.");
		}
		*/
	}
	if (!strcmp (gl_renderer, "Mesa DRI Voodoo3 20010501 x86/MMX")) {
		Com_Printf ("Disabiling glLoadTransposeMatrixf and glLoadTransposeMatrixf due to buggy DRI drivers.\n");
		DynGL_BadFunction("glLoadTransposeMatrixf", Sys_Error);
		DynGL_BadFunction("glMultTransposeMatrixf", Sys_Error);
	}
}


/*
	CheckExtensions

	Check for the OpenGL extensions we use
*/

static void
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
		Com_Printf ("No.\n");

	if (!COM_CheckParm ("-nocva"))
		gl_cva = DynGL_HasExtension ("GL_EXT_compiled_vertex_array");

	Com_Printf ("Checking for GL_EXT_compiled_vertex_array: %s.\n",
			gl_cva ? "Yes" : "No");

	gl_secondary_color = DynGL_HasExtension ("GL_EXT_secondary_color");
	Com_Printf ("Checking for GL_EXT_secondary_color: %s.\n",
			gl_secondary_color ? "Yes" : "No");

	if (!COM_CheckParm ("-nocombiners"))
		gl_nv_register_combiners=DynGL_HasExtension("GL_NV_register_combiners");
	Com_Printf ("Checking for GL_NV_register_combiners: %s.\n",
			gl_nv_register_combiners ? "Yes" : "No");

	if (!COM_CheckParm ("-noautomip"))
		gl_sgis_mipmap = DynGL_HasExtension ("GL_SGIS_generate_mipmap");

	Com_Printf ("Checking for GL_SGIS_generate_mipmap: %s.\n",
			gl_sgis_mipmap ? "Yes" : "No");


	if (COM_CheckParm ("-vbo"))
		gl_vbo = DynGL_HasExtension ("GL_ARB_vertex_buffer_object");

	Com_Printf ("Checking for GL_ARB_vertex_buffer_object: %s.\n",
			gl_vbo ? "Yes" : "No");
}

static void
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
GLInfo_Init_Cvars
===============
*/
void
GLInfo_Init_Cvars (void)
{
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);
	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_ARCHIVE, NULL);
	gl_particletorches = Cvar_Get ("gl_particletorches", "0", CVAR_ARCHIVE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
}

/*
===============
GLInfo_Init
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
