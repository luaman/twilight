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

#include "mathlib.h"
#include "dyngl.h"

extern double	r_time;			// Current time.
extern double	r_frametime;	// Time since last frame.
extern Uint		r_framecount;	// Current frame.
extern vec3_t	r_origin;
extern float	d_8tofloattable[256][4];
extern Uint32	d_palette_raw[256];
extern Uint32	d_palette_base[256];
extern Uint32	d_palette_fb[256];
extern Uint32	d_palette_base_team[256];
extern Uint32	d_palette_top[256];
extern Uint32	d_palette_bottom[256];
extern Uint32	d_palette_top_bottom[256];
extern int		gl_solid_format;
extern int		gl_alpha_format;
extern int		gl_filter_mag;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define d_palette_empty	0x000000FF
#else
#define d_palette_empty	0xFF000000
#endif



// for glColor4fv
extern GLfloat whitev[4];

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;
extern qboolean gl_cva;
extern qboolean gl_mtex;
extern qboolean gl_mtexcombine;
extern qboolean gl_secondary_color;
extern qboolean gl_nv_register_combiners;
extern qboolean gl_sgis_mipmap;
extern int gl_tmus;

extern int fb_size[4];
extern int accum_size[4];
extern int doublebuffer, buffer_size, depth_size, stencil_size;

extern void GLInfo_Init (void);

#endif // __GL_INFO_H
