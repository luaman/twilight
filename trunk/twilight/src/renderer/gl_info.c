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

#include "cmd.h"
#include "common.h"
#include "dyngl.h"
#include "model.h"
#include "qtypes.h"
#include "sys.h"

GLfloat whitev[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static int fb_size[4];
static int accum_size[4];
static int doublebuffer, buffer_size, depth_size, stencil_size;

cvar_t *gl_affinemodels;
cvar_t *gl_nocolors;
cvar_t *gl_im_animation;
cvar_t *gl_particletorches;
cvar_t *gl_cull;

/*-----------------------------------------------------------------------*/

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static const char *gl_extensions;

int gl_cva = 0;
int gl_mtex = 0;
int gl_mtexcombine = 0;
int gl_secondary_color = 0;
int gl_nv_register_combiners = 0;
int gl_sgis_mipmap = 0;
int gl_vbo = 0;
int gl_ext_anisotropy = 0;
int gl_tmus = 1;

Uint32 gl_allow;

/*
	CheckDriverQuirks

	Check for buggy OpenGL drivers.
*/

static void
GLInfo_CheckDriverQuirks (void)
{
	if (!strcmp (gl_vendor, "NVIDIA Corporation"))	// nVidia drivers.
	{
		char	*num;
		float	ver;
		if ((num = strrchr (gl_version, ' '))) {
			ver = atof(num);
			if (ver < 44.96) {
				Com_Printf ("Not using GL_ARB_vertex_buffer_object by default due to driver issues. (Use -vbo to enable.)\n");
				gl_vbo = -2;
			}
		}
	}
	/*
	if (strstr (gl_vendor, "NVIDIA")) {		// nVidia drivers.
		DynGL_BadExtension ("GL_EXT_compiled_vertex_array");
		Com_Printf ("Disabling GL_EXT_compiled_vertex_array due to buggy nVidia driver.\n");
	}
	*/

	if (!strcmp (gl_vendor, "ATI Technologies Inc."))	// ATI drivers.
	{
		// LordHavoc: kept for future use
		if (!strcmp (gl_version, "1.3.3717 WinXP Release"))
		{
			DynGL_BadExtension ("GL_SGIS_generate_mipmap");
			Com_Printf ("Disabling GL_SGIS_generate_mipmap due to buggy ATI driver version.");
		}
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

#define CHECK_EXT(name,var)		{ \
	Com_Printf ("Checking for " name ": ");				\
	switch (var) {										\
		case -1:										\
			Com_Printf("Disabled. (Argument)\n");		\
			var = 0;									\
			break;										\
		case -2:										\
			Com_Printf("Disabled. (Drive Quirk)\n");	\
			var = 0;									\
			break;										\
		default:										\
			var = DynGL_HasExtension (name);			\
			if (var)									\
				Com_Printf ("Yes.\n");					\
			else										\
				Com_Printf ("No.\n");					\
			break;										\
	}													\
}

static void
GLInfo_CheckExtensions (void)
{
	int	gl_mtexcombine_arb = 0, gl_mtexcombine_ext = 0;

	if (COM_CheckParm ("-nomtex"))
		gl_mtex = -1;
	if (COM_CheckParm ("-nocva"))
		gl_cva = -1;
	if (COM_CheckParm ("-nocombiners"))
		gl_nv_register_combiners = -1;
	if (COM_CheckParm ("-noautomip"))
		gl_sgis_mipmap = -1;
	if (COM_CheckParm ("-novbo"))
		gl_vbo = -1;
	else if (COM_CheckParm ("-vbo"))
		gl_vbo = 0;
	if (COM_CheckParm ("-noanisotropy"))
		gl_ext_anisotropy = -1;
	if (COM_CheckParm ("-nomtexcombine"))
		gl_mtexcombine_arb = gl_mtexcombine_ext = -1;

	CHECK_EXT("GL_ARB_multitexture", gl_mtex);
	if (gl_mtex) {
		qglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_tmus);
		Com_Printf ("(%d TMUs)\n", gl_tmus);
		CHECK_EXT("GL_ARB_texture_env_combine", gl_mtexcombine_arb);
		CHECK_EXT("GL_EXT_texture_env_combine", gl_mtexcombine_ext);
		gl_mtexcombine = gl_mtexcombine_arb || gl_mtexcombine_ext;
	}

	CHECK_EXT("GL_EXT_compiled_vertex_array", gl_cva);
//	CHECK_EXT("GL_EXT_secondary_color", gl_secondary_color);
	CHECK_EXT("GL_NV_register_combiners", gl_nv_register_combiners);
	CHECK_EXT("GL_SGIS_generate_mipmap", gl_sgis_mipmap);
	CHECK_EXT("GL_ARB_vertex_buffer_object", gl_vbo);
	CHECK_EXT("GL_EXT_texture_filter_anisotropic", gl_ext_anisotropy);
}

#undef CHECK_EXT

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


void
GLInfo_Init_Cvars (void)
{
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);
	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_ARCHIVE, NULL);
	gl_particletorches = Cvar_Get ("gl_particletorches", "0", CVAR_ARCHIVE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
}

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

void
GLInfo_Shutdown (void)
{
	/*
	Cmd_RemoveCommand ("gl_info", &GL_Info_f);
	*/
}
