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

#include "dyngl.h"
#include "mathlib.h"
#include "model.h"
#include "protocol.h"
#include "wad.h"
#include "vid.h"
#include "vis.h"
#include "gl_info.h"
#include "gl_arrays.h"
#include "quakedef.h"
#include "matrixlib.h"


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

	float			lerp_start_time, lerp_delta_time;

	// last two updates (0 is newest) 
	vec3_t			msg_origins[2];
	vec3_t			origin;

	// last two updates (0 is newest)
	vec3_t			msg_angles[2];
	vec3_t			angles;

	qboolean		lerping;

	matrix4x4_t		matrix;
	matrix4x4_t		invmatrix;

	// Bounding box
	vec3_t			mins;
	vec3_t			maxs;

	// NULL = no model
	struct model_s	*model;
	int				frame[2];
	float			frame_frac[2];
	float			frame_time[2];
	float			frame_interval[2];

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

extern struct texture_s *r_notexture;
extern struct texture_s *r_notexture_water;

void R_Init_Cvars (void);
void R_Init (void);
void R_InitTextures (void);

// must set r_refdef first
void R_RenderView (void);

// called whenever r_refdef or vid change
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect);

// called at level load
void R_InitSky (struct texture_s *mt, Uint8 *pixels);

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

// for glColor4fv
extern GLfloat whitev[4];


qboolean GLF_Init (void);
void GL_EndRendering (void);

int GL_MangleImage8 (Uint8 *in, Uint8 *out, int width, int height, short mask,
		        Uint8 to, qboolean bleach);
qboolean GL_Upload32 (Uint32 *data, int width, int height, int flags);
qboolean GL_Upload8 (Uint8 *data, int width, int height, Uint32 *palette,
		int flags);

extern int glx, gly;

// r_local.h -- private refresh defs

#define BACKFACE_EPSILON			0.01


void R_ReadPointFile_f (void);

//============================================================================


extern entity_t *currententity;
extern Uint r_framecount;
extern Uint c_brush_polys, c_alias_polys;


/*
 * view origin
 */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

/*
 * screen size info
 */
extern refdef_t r_refdef;
extern mleaf_t *r_viewleaf, *r_oldviewleaf;

extern vec3_t modelorg;

extern int d_lightstylevalue[256];		// 8.8 fraction of base light value

extern GLuint netgraphtexture;				// netgraph texture
extern GLuint playertextures;

extern GLuint skyboxtexnums[6];

extern struct cvar_s *r_norefresh;
extern struct cvar_s *r_drawentities;
extern struct cvar_s *r_drawviewmodel;
extern struct cvar_s *r_speeds;
extern struct cvar_s *r_waterwarp;
extern struct cvar_s *r_shadows;
extern struct cvar_s *r_wateralpha;
extern struct cvar_s *r_waterripple;
extern struct cvar_s *r_wireframe;
extern struct cvar_s *r_dynamic;
extern struct cvar_s *r_novis;
extern struct cvar_s *r_stainmaps;
extern struct cvar_s *r_netgraph;
extern struct cvar_s *r_skyname;
extern struct cvar_s *r_fastsky;

extern struct cvar_s *gl_clear;
extern struct cvar_s *gl_cull;
extern struct cvar_s *gl_affinemodels;
extern struct cvar_s *gl_polyblend;
extern struct cvar_s *gl_flashblend;
extern struct cvar_s *gl_nocolors;
extern struct cvar_s *gl_finish;
extern struct cvar_s *gl_im_animation;
extern struct cvar_s *gl_im_transform;
extern struct cvar_s *gl_fb;
extern struct cvar_s *gl_oldlights;
extern struct cvar_s *gl_colorlights;
extern struct cvar_s *gl_particletorches;
extern struct cvar_s *r_particles;

extern int gl_lightmap_format;
extern int gl_solid_format;
extern int gl_alpha_format;
extern qboolean colorlights;
extern int gl_wireframe;

extern struct cvar_s *gl_max_size;
extern struct cvar_s *gl_playermip;

/*
 * gl_warp.c
 */
void EmitBothSkyLayers (msurface_t *fa);
void EmitWaterPolys (msurface_t *fa, texture_t *tex, int transform,float alpha);
void R_DrawSkyChain (msurface_t *s);
extern void R_DrawSkyBoxChain (msurface_t *s);

/*
 * gl_draw.c
 */
extern int gl_filter_min;
extern int gl_filter_mag;

/*
 * gl_rsurf.c
 */
void R_VisBrushModel (entity_t *e);
void R_DrawOpaqueBrushModel (entity_t *e);
void R_DrawAddBrushModel (entity_t *e);
void R_DrawBrushDepthSkies (void);
void R_VisWorld (void);
void R_DrawWorld (void);
void R_DrawLiquidTextureChains (model_t *mod);
void R_DrawTextureChains (model_t *mod, vec3_t origin, int frame, matrix4x4_t *matrix, matrix4x4_t *invmatrix);
void GL_BuildLightmaps (void);

#endif // __RENDER_H

