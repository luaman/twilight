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

#define	MAX_GLTEXTURES	1024

#include "mathlib.h"
#include "dyngl.h"

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
extern int gl_tmus;

extern int fb_size[4];
extern int accum_size[4];
extern int doublebuffer, buffer_size, depth_size, stencil_size;

extern void GLInfo_Init (void);

#endif // __GL_INFO_H
