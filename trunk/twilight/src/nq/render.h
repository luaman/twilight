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

#include "protocol.h"
#include "vid.h"
#include "model.h"

#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct colormap_s {
	vec3_t	top;
	vec3_t	bottom;
} colormap_t;

typedef struct entity_s {
	qboolean    forcelink;				// model changed

	entity_state_t baseline;			// to fill in defaults in updates

	double      msgtime;				// time of last update
	vec3_t      msg_origins[2];			// last two updates (0 is newest) 
	vec3_t      origin;
	vec3_t      msg_angles[2];			// last two updates (0 is newest)
	vec3_t      angles;
	struct model_s *model;				// NULL = no model
	int         frame;
	float       syncbase;				// for client-side animations
	colormap_t	*colormap;
	int         effects;				// light, particals, etc
	int         skinnum;				// for Alias models
	int         visframe;				// last frame this entity was
	// found in an active leaf

	vec3_t		last_light;
	float		time_left;
	struct model_s *lastmodel;
} entity_t;

typedef struct {
	vrect_t     vrect;					// subwindow in video for refresh

	vec3_t      vieworg;
	vec3_t      viewangles;

	float       fov_x, fov_y;

	int			num_entities;
	entity_t	*entities[MAX_EDICTS];
} refdef_t;


//
// refresh
//
extern int  reinit_surfcache;


extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern struct texture_s *r_notexture_mip;


void        R_Init_Cvars (void);
void        R_Init (void);
void        R_InitTextures (void);
void        R_RenderView (void);		// must set r_refdef first
void        R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect);

								// called whenever r_refdef or vid change
void        R_InitSky (struct texture_s *mt);	// called at level load

void        R_NewMap (void);


void        R_ParseParticleEffect (void);
void        R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void        R_RocketTrail (vec3_t start, vec3_t end);
void        R_ParticleTrail (vec3_t start, vec3_t end, int type);

void        R_EntityParticles (entity_t *ent);
void        R_BlobExplosion (vec3_t org);
void        R_ParticleExplosion (vec3_t org);
void        R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void        R_LavaSplash (vec3_t org);
void        R_TeleportSplash (vec3_t org);
void		R_RailTrail (vec3_t start, vec3_t end);

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

void        R_PushDlights (void);
void		R_MarkLights (dlight_t *light, int bit, model_t *model);
void		R_MarkLightsNoVis (dlight_t *light, int bit, mnode_t *node);
void        R_RenderDlights (void);
void        R_AnimateLight (void);
int         R_LightPoint (vec3_t p);

void        R_InitParticles (void);
void        R_ClearParticles (void);
void        R_MoveParticles (void);
void        R_DrawParticles (void);
void        R_DrawWaterSurfaces (void);

//
// surface cache related
//
extern int  reinit_surfcache;			// if 1, surface cache is currently
										// empty and
extern qboolean r_cache_thrash;			// set if thrashing the surface cache

int         D_SurfaceCacheForRes (int width, int height);
void        D_FlushCaches (void);
void        D_DeleteSurfaceCache (void);
void        D_InitCaches (void *buffer, int size);
void        R_SetVrect (vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

#endif // __RENDER_H

