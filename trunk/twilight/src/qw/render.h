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

#ifndef __RENDER_H
#define __RENDER_H

#include "vid.h"
#include "model.h"

#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct colormap_s {
	vec3_t		top;
	vec3_t		bottom;
} colormap_t;

typedef struct entity_save_s {
	vec3_t		origin;
	float		origin_time;
	float		origin_interval;
	vec3_t		angles;
	float		angles_time;
	float		angles_interval;
	int			frame;
	float		frame_time;
	float		frame_interval;
} entity_save_t;

typedef struct entity_s {
	entity_save_t	from;
	entity_save_t	to;
	entity_save_t	cur;

	// NULL = no model
	struct model_s	*model;
	int				skinnum;
	int				effects;

	int				modelindex;
	int				entity_frame;
	vec3_t			last_light;
	float			time_left;
	unsigned int	times;

	float			frame_blend;

	// Bounding box
	vec3_t			mins;
	vec3_t			maxs;

	// Skin other then model.
	skin_t			*skin;

	// Colormap for the model, if any.
	colormap_t		*colormap;
} entity_t;

#define MAX_ENTITIES	1024

typedef struct {
	vrect_t     vrect;					// subwindow in video for refresh

	vec3_t      vieworg;
	vec3_t      viewangles;

	float       fov_x, fov_y;

	int			num_entities;
	entity_t	*entities[MAX_ENTITIES];
} refdef_t;


//
// refresh
//
extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern entity_t r_worldentity;

void R_Init_Cvars (void);
void R_Init (void);
void R_InitTextures (void);

// must set r_refdef first
// called whenever r_refdef or vid change
void R_RenderView (void);

// called at level load
void R_InitSky (struct texture_s *mt);

void R_InitSurf (void);

void R_NewMap (void);


void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end);
void R_ParticleTrail (vec3_t start, vec3_t end, int type);

void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

//
// gl_rlight.c
//

typedef struct {
	int			key;					// so entities can reuse same entry
	vec3_t		origin;
	float		radius;
	float		die;					// stop lighting after this time
	float		decay;					// drop this each second
	float		minlight;				// don't add when contributing less
	float		color[3];
} dlight_t;

void R_InitParticles (void);
void R_ClearParticles (void);
void R_MoveParticles (void);
void R_DrawParticles (void);
void R_DrawWaterSurfaces (void);

// It's a particle effect or something.  =)
void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1,
		int cr2, int cg2, int cb2, int ca2);

#endif // __RENDER_H

