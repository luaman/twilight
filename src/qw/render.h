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
#include "transform.h"
#include "wad.h"
#include "vid.h"

#define	MAX_GLTEXTURES	1024
#define	TOP_RANGE		16				// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct colormap_s {
	vec4_t		top;
	vec4_t		bottom;
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

extern struct texture_s *r_notexture;
extern struct texture_s *r_notexture_water;

extern entity_t r_worldentity;

void R_Init_Cvars (void);
void R_Init (void);
void R_InitTextures (void);

// must set r_refdef first
// called whenever r_refdef or vid change
void R_RenderView (void);

// called at level load
void R_InitSky (struct texture_s *mt, Uint8 *pixels);

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


// for glColor4fv
extern GLfloat whitev[4];


qboolean GLF_Init (void);
void GL_EndRendering (void);

extern int texture_extension_number;

int GL_MangleImage8 (Uint8 *in, Uint8 *out, int width, int height, short mask,
		        Uint8 to, qboolean bleach);
void GL_Upload32 (Uint32 *data, int width, int height, int flags);
void GL_Upload8 (Uint8 *data, int width, int height,unsigned *ttable,
		int flags);

extern int glx, gly;

/*
 * normalizing factor so player model works out to about 1 pixel per triangle
 */
#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
#define MAX_LBM_HEIGHT				480

#define BACKFACE_EPSILON			0.01


void R_ReadPointFile_f (void);

//============================================================================


extern entity_t *currententity;
extern int r_framecount;
extern int c_brush_polys, c_alias_polys;


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

extern int netgraphtexture;				// netgraph texture
extern int playertextures;

extern int skyboxtexnum;

extern int skytexturenum;				// in cl.loadmodel, not GL texture

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
extern struct cvar_s *gl_fb_models;
extern struct cvar_s *gl_fb_bmodels;
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

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;

extern qboolean gl_cva;
extern qboolean gl_mtex;
extern qboolean gl_mtexcombine;

// Vertex array stuff.

extern texcoord_t	*tc0_array_p;
extern texcoord_t	*tc1_array_p;
extern vertex_t		*v_array_p;
extern colorf_t		*cf_array_p;
extern colorub_t	*cub_array_p;

#define tc_array_v(x) tc0_array_p[x].v
#define tc_array(x,y) tc0_array_p[x].v[y]
#define tc0_array_v(x) tc0_array_p[x].v
#define tc0_array(x,y) tc0_array_p[x].v[y]
#define tc1_array_v(x) tc1_array_p[x].v
#define tc1_array(x,y) tc1_array_p[x].v[y]
#define v_array_v(x) v_array_p[x].v
#define v_array(x,y) v_array_p[x].v[y]
#define c_array_v(x) cub_array_p[x].v
#define c_array(x,y) cub_array_p[x].v[y]
#define cub_array_v(x) cub_array_p[x].v
#define cub_array(x,y) cub_array_p[x].v[y]
#define cf_array_v(x) cf_array_p[x].v
#define cf_array(x,y) cf_array_p[x].v[y]

extern GLuint *vindices;

extern GLint	v_index, i_index;
extern qboolean	va_locked;
extern GLint	MAX_VERTEX_ARRAYS, MAX_VERTEX_INDICES;
extern memzone_t *vzone;

extern float_int_t *FtoUB_tmp;

extern void inline
TWI_FtoUBMod (GLfloat *in, GLubyte *out, vec4_t *mod, int num)
{
	int		i;

	// shift float to have 8bit fraction at base of number
	for (i = 0; i < num; i += 4) {
		FtoUB_tmp[i    ].f = (in[i    ] * (*mod)[0]) + 32768.0f;
		FtoUB_tmp[i + 1].f = (in[i + 1] * (*mod)[1]) + 32768.0f;
		FtoUB_tmp[i + 2].f = (in[i + 2] * (*mod)[2]) + 32768.0f;
		FtoUB_tmp[i + 3].f = (in[i + 3] * (*mod)[3]) + 32768.0f;
	}

	// then read as integer and kill float bits...
	for (i = 0; i < num; i += 4) {
		out[i    ] = (Uint8) min(FtoUB_tmp[i    ].i & 0x7FFFFF, 255);
		out[i + 1] = (Uint8) min(FtoUB_tmp[i + 1].i & 0x7FFFFF, 255);
		out[i + 2] = (Uint8) min(FtoUB_tmp[i + 2].i & 0x7FFFFF, 255);
		out[i + 3] = (Uint8) min(FtoUB_tmp[i + 3].i & 0x7FFFFF, 255);
	}
}

extern void inline
TWI_FtoUB (GLfloat *in, GLubyte *out, int num)
{
	int		i;

	// shift float to have 8bit fraction at base of number
	for (i = 0; i < num; i += 4) {
		FtoUB_tmp[i    ].f = in[i    ] + 32768.0f;
		FtoUB_tmp[i + 1].f = in[i + 1] + 32768.0f;
		FtoUB_tmp[i + 2].f = in[i + 2] + 32768.0f;
		FtoUB_tmp[i + 3].f = in[i + 3] + 32768.0f;
	}

	// then read as integer and kill float bits...
	for (i = 0; i < num; i += 4) {
		out[i    ] = (Uint8) min(FtoUB_tmp[i    ].i & 0x7FFFFF, 255);
		out[i + 1] = (Uint8) min(FtoUB_tmp[i + 1].i & 0x7FFFFF, 255);
		out[i + 2] = (Uint8) min(FtoUB_tmp[i + 2].i & 0x7FFFFF, 255);
		out[i + 3] = (Uint8) min(FtoUB_tmp[i + 3].i & 0x7FFFFF, 255);
	}
}

extern void inline TWI_PreVDrawCVA (GLint min, GLint max)
{
	if (gl_cva) {
		qglLockArraysEXT (min, max);
		va_locked = 1;
	}
}

extern void inline TWI_PostVDrawCVA ()
{
	if (va_locked)
		qglUnlockArraysEXT ();
}

// May be used for NV_VAR or something...
extern void inline TWI_PreVDraw (GLint min, GLint max)
{
}

extern void inline TWI_PostVDraw ()
{
}

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
 * gl_rmain.c
 */
qboolean R_CullBox (vec3_t mins, vec3_t maxs);

/*
 * gl_rsurf.c
 */
void R_DrawBrushModel (entity_t *e);
void R_DrawBrushModelSkies (void);
void R_DrawWorld (void);
void R_DrawWaterTextureChains (void);
void GL_BuildLightmaps (void);

/*
 * gl_ngraph.c
 */
void R_NetGraph (void);

#endif // __RENDER_H

