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

#include "protocol.h"
#include "vid.h"
#include "model.h"

#define	MAXCLIPPLANES	11
#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct colormap_s {
	vec4_t		top;
	vec4_t		bottom;
} colormap_t;

typedef struct entity_s {
	// model changed
	qboolean		forcelink;

	// to fill in defaults in updates
	entity_state_t	baseline;

	// time of last update
	double			msgtime;

	// last two updates (0 is newest) 
	vec3_t			msg_origins[2];
	vec3_t			origin;

	// last two updates (0 is newest)
	vec3_t			msg_angles[2];
	vec3_t			angles;

	// Bounding box
	vec3_t			mins;
	vec3_t			maxs;

	// NULL = no model
	struct model_s	*model;
	int				frame;

	// for client-side animations
	float			syncbase;
	colormap_t		*colormap;

	// light, particals, etc
	int				effects;

	// for Alias models
	int				skinnum;

	// last frame this entity was found in an active leaf
	int				visframe;

	vec3_t			last_light;
	float			time_left;
	struct model_s	*lastmodel;
} entity_t;

typedef struct {
	vec3_t      vieworg;
	vec3_t      viewangles;

	float       fov_x, fov_y;

	int			num_entities;
	entity_t	*entities[MAX_EDICTS];
} refdef_t;


//
// refresh
//
extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern struct texture_s *r_notexture_mip;

void R_Init_Cvars (void);
void R_Init (void);
void R_InitTextures (void);

// must set r_refdef first
void R_RenderView (void);

// called whenever r_refdef or vid change
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect);

// called at level load
void R_InitSky (struct texture_s *mt);

void R_InitSurf (void);

void R_NewMap (void);


void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end);
void R_ParticleTrail (vec3_t start, vec3_t end, int type);

void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);
void R_RailTrail (vec3_t start, vec3_t end);

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

// It's a particle effect or something.  =)
void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1,
		int cr2, int cg2, int cb2, int ca2);

#endif // __RENDER_H

