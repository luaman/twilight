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
// refresh.h -- public interface to refresh functions

#ifndef __RENDER_H
#define __RENDER_H

#include "vid.h"
#include "model.h"

#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct colormap_s {
	vec3_t	top;
	vec3_t	bottom;
} colormap_t;

typedef struct entity_save_s {
	vec3_t	origin;
	float	origin_time;
	float	origin_interval;
	vec3_t	angles;
	float	angles_time;
	float	angles_interval;
	int		frame;
	float	frame_time;
	float	frame_interval;
} entity_save_t;

typedef struct entity_s {
	entity_save_t	from;
	entity_save_t	to;
	entity_save_t	cur;

	struct model_s	*model;				// NULL = no model
	int				skinnum;
	int				effects;

	int				modelindex;
	int				entity_frame;
	vec3_t			last_light;
	float			time_left;
	unsigned int	times;

	float			frame_blend;
	skin_t			*skin;				// Skin other then model.
	colormap_t		*colormap;			// Colormap for the model, if any.
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

void        R_Init_Cvars (void);
void        R_Init (void);
void        R_InitTextures (void);
void        R_RenderView (void);		// must set r_refdef first
								// called whenever r_refdef or vid change
void        R_InitSky (texture_t *mt);	// called at level load

void        R_NewMap (void);


void        R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void        R_RocketTrail (vec3_t start, vec3_t end);
void        R_ParticleTrail (vec3_t start, vec3_t end, int type);

void        R_BlobExplosion (vec3_t org);
void        R_ParticleExplosion (vec3_t org);
void        R_LavaSplash (vec3_t org);
void        R_TeleportSplash (vec3_t org);

//
// gl_rlight.c
//
#define	MAX_DLIGHTS		32

typedef struct {
	int         key;					// so entities can reuse same entry
	vec3_t      origin;
	float       radius;
	float       die;					// stop lighting after this time
	float       decay;					// drop this each second
	float       minlight;				// don't add when contributing less
	float       color[3];
} dlight_t;

void        R_MarkLightsNoVis (dlight_t *light, int bit, mnode_t *node);
void		R_MarkLights (dlight_t *light, int bit, model_t *model);
void        R_AnimateLight (void);
void        R_RenderDlights (void);
int         R_LightPoint (vec3_t p);
void        R_PushDlights (void);

void        R_InitParticles (void);
void        R_ClearParticles (void);
void        R_MoveParticles (void);
void        R_DrawParticles (void);
void        R_DrawWaterSurfaces (void);

#endif // __RENDER_H

