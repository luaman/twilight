/*
	$RCSfile$
	OpenGL Texture management.

	Copyright (C) 2002  Zephaniah E. Hull.

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

    "$Id$";
*/

#ifndef __gl_main_h
#define __gl_main_h

#include "qtypes.h"
#include "cvar.h"
#include "entities.h"

extern Uint c_brush_polys;
extern Uint c_alias_polys;
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;
extern refdef_t r_refdef;
extern texture_t *r_notexture;
extern texture_t *r_notexture_water;
extern int d_lightstylevalue[256];
extern cvar_t *r_drawentities;
extern cvar_t *r_drawviewmodel;
extern cvar_t *r_dynamic;
extern cvar_t *r_stainmaps;
extern cvar_t *gl_clear;
extern cvar_t *gl_polyblend;
extern cvar_t *gl_flashblend;
extern cvar_t *gl_playermip;
extern cvar_t *gl_finish;
extern cvar_t *gl_im_transform;
extern cvar_t *gl_oldlights;
extern cvar_t *gl_colorlights;
extern qboolean colorlights;

void R_Clear (void);
void R_RenderView(void);
void R_Init_Cvars(void);
void R_Init(void);
void R_NewMap(void);

#endif // __gl_main_h

