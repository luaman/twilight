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
#include "matrixlib.h"
#include "mod_alias.h"
#include "model.h"
#include "palette.h"

typedef struct colormap_s {
	vec4_t	top;
	vec4_t	bottom;
} colormap_t;

typedef struct entity_common_s {
	// Origin and angles.
	vec3_t			origin, angles;

	// Matrix and invmatrix.
	matrix4x4_t		matrix, invmatrix;

	// NULL = no model.
	int				frame[2];
	float			frame_frac[2];
	float			frame_time[2];
	float			frame_interval[2];
	qboolean		lerping;

	// Model.
	struct model_s	*model;

	// Bounding boxes.
	vec3_t			mins;
	vec3_t			maxs;

	// Skin of the model, if any.
	skin_t			*skin;
	int				skinnum;

	// Colormap for the model, if any.
	colormap_t		*colormap;

	// For trails: (FIXME: Better place?
	vec3_t			trail_old_org;
	float			trail_len;
	int				trail_times;

	// For model light lerping.
	vec3_t			last_light;
	// For torches.
	float			time_left;

	// FIXME: HACK! HACK! HACK!
	struct entity_s *real_ent;
} entity_common_t;

extern vec3_t	r_origin, vpn, vright, vup;
extern double	r_time;			// Current time.
extern double	r_frametime;	// Time since last frame.
extern Uint		r_framecount;	// Current frame.
extern model_t	*r_worldmodel;	// World model.
extern vec3_t	r_origin;

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

extern void GLInfo_Init_Cvars (void);
extern void GLInfo_Init (void);

#endif // __GL_INFO_H
