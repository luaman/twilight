/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

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

	$Id$
*/

#ifndef __GL_INFO_H
#define __GL_INFO_H

#include "dyngl.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "mod_alias.h"
#include "model.h"
#include "palette.h"

#define Check_GL_Error()		do {								\
	int _err;														\
	if ((_err = qglGetError())) {									\
		fprintf(stderr, "%s %d (%s): Error: %d\n",__FILE__,__LINE__,__FUNCTION__,_err);\
		fflush(NULL);												\
	}																\
} while (0)

typedef struct colormap_s {
	vec4_t	top;
	vec4_t	bottom;
} colormap_t;

extern cvar_t *gl_affinemodels;
extern cvar_t *gl_nocolors;
extern cvar_t *gl_im_animation;
extern cvar_t *gl_particletorches;
extern cvar_t *gl_cull;

// for glColor4fv
extern GLfloat whitev[4];

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;
extern int gl_cva;
extern int gl_mtex;
extern int gl_mtexcombine;
extern int gl_secondary_color;
extern int gl_nv_register_combiners;
extern int gl_sgis_mipmap;
extern int gl_vbo;
extern int gl_ext_anisotropy;
extern int gl_tmus;
extern int gl_lightmap_format;


#define GLA_WIREFRAME		BIT(0)
#define GLA_WATERALPHA		BIT(1)

extern Uint32 gl_allow;


extern int fb_size[4];
extern int accum_size[4];
extern int doublebuffer, buffer_size, depth_size, stencil_size;

extern void GLInfo_Init_Cvars (void);
extern void GLInfo_Init (void);
extern void GLInfo_Shutdown (void);

#endif // __GL_INFO_H
