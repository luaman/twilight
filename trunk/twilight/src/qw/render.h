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
#include "gl_model.h"

#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s {
	struct mleaf_s *leaf;
	struct efrag_s *leafnext;
	struct entity_s *entity;
	struct efrag_s *entnext;
} efrag_t;


typedef struct entity_s {
	int         keynum;					// for matching entities in different
	// frames
	vec3_t      origin;
	vec3_t      angles;
	struct model_s *model;				// NULL = no model
	int         frame;
	Uint8      *colormap;
	int         skinnum;				// for Alias models

	struct player_info_s *scoreboard;	// identify player

	struct efrag_s *efrag;				// linked list of efrags (FIXME)
	int         visframe;				// last frame this entity was
										// found in an active leaf
										// only used for static objects

	// Animation interpolation
	float       frame_start_time;
	float       frame_interval;
	int         pose1; 
	int         pose2;

	vec3_t		last_light;
} entity_t;

typedef struct {
	vrect_t     vrect;					// subwindow in video for refresh

	vec3_t      vieworg;
	vec3_t      viewangles;

	float       fov_x, fov_y;
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

void        R_AddEfrags (entity_t *ent);
void        R_RemoveEfrags (entity_t *ent);

void        R_NewMap (void);


void        R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void        R_RocketTrail (vec3_t start, vec3_t end, int type);

void        R_BlobExplosion (vec3_t org);
void        R_ParticleExplosion (vec3_t org);
void        R_LavaSplash (vec3_t org);
void        R_TeleportSplash (vec3_t org);

void        R_PushDlights (void);
void        R_InitParticles (void);
void        R_ClearParticles (void);
void        R_DrawParticles (void);
void        R_DrawWaterSurfaces (void);

#endif // __RENDER_H

