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
// disable data conversion warnings

#ifndef __GLQUAKE_H
#define __GLQUAKE_H

#include "mathlib.h"

#ifdef _WIN32
#if _MSC_VER >= 800	/* MSVC 4.0 */
#pragma warning(disable : 4244)			// MIPS
#pragma warning(disable : 4136)			// X86
#pragma warning(disable : 4051)			// ALPHA
#endif
#include <windows.h>
#endif

#define	MAX_GLTEXTURES	1024

#include "opengl.h"
#include "dynogl.h"

#include "wad.h"
#include "render.h"

qboolean GLF_Init (void);

void        GL_EndRendering (void);

extern int  texture_extension_number;

extern float gldepthmin, gldepthmax;

void GL_Upload32 (unsigned *data, unsigned width, unsigned height,
		qboolean mipmap, qboolean alpha);
void GL_Upload8 (Uint8 *data, int width, int height, qboolean mipmap, 
						int alpha, unsigned *ttable);

extern int  glx, gly;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					// 1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


void        R_ReadPointFile_f (void);
texture_t  *R_TextureAnimation (texture_t *base);

//====================================================


extern entity_t *currententity;
extern int  r_visframecount;			// ??? what difs?
extern int  r_framecount;
extern int  c_brush_polys, c_alias_polys;


//
// view origin
//
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

//
// screen size info
//
extern refdef_t r_refdef;
extern mleaf_t *r_viewleaf, *r_oldviewleaf;
extern texture_t *r_notexture_mip;
extern int  d_lightstylevalue[256];		// 8.8 fraction of base light value

extern int  particletexture;
extern int  netgraphtexture;			// netgraph texture
extern int  playertextures;

extern int  skyboxtexnum;

extern int  skytexturenum;				// index in cl.loadmodel, not gl
										// texture object

extern struct cvar_s *r_norefresh;
extern struct cvar_s *r_drawentities;
extern struct cvar_s *r_drawviewmodel;
extern struct cvar_s *r_speeds;
extern struct cvar_s *r_waterwarp;
extern struct cvar_s *r_shadows;
extern struct cvar_s *r_wateralpha;
extern struct cvar_s *r_dynamic;
extern struct cvar_s *r_novis;
extern struct cvar_s *r_netgraph;
extern struct cvar_s *r_skybox;
extern struct cvar_s *r_fastsky;
extern struct cvar_s *r_lightlerp;

extern struct cvar_s *gl_clear;
extern struct cvar_s *gl_cull;
extern struct cvar_s *gl_affinemodels;
extern struct cvar_s *gl_polyblend;
extern struct cvar_s *gl_keeptjunctions;
extern struct cvar_s *gl_flashblend;
extern struct cvar_s *gl_nocolors;
extern struct cvar_s *gl_finish;
extern struct cvar_s *gl_im_animation;
extern struct cvar_s *gl_fb_models;
extern struct cvar_s *gl_fb_bmodels;
extern struct cvar_s *gl_oldlights;
extern struct cvar_s *gl_colorlights;
extern struct cvar_s *gl_particletorches;

extern int  gl_lightmap_format;
extern int  gl_solid_format;
extern int  gl_alpha_format;
extern qboolean colorlights;

extern struct cvar_s *gl_max_size;
extern struct cvar_s *gl_playermip;

extern float r_world_matrix[16];

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;

void        R_TranslatePlayerSkin (int playernum);

extern qboolean gl_mtexable;
extern qboolean gl_mtexcombine_arb;
extern qboolean gl_mtexcombine_ext;

// Vertex array stuff.

#define MAX_VERTEX_ARRAYS	1024
#define MAX_VERTEX_INDICES	(MAX_VERTEX_ARRAYS * 3)
extern GLfloat	tc_array[MAX_VERTEX_ARRAYS][2];
extern GLfloat	v_array[MAX_VERTEX_ARRAYS][3];
extern GLfloat	c_array[MAX_VERTEX_ARRAYS][4];

extern GLuint vindices[MAX_VERTEX_INDICES];

extern GLuint v_index, i_index;

/*
extern varray_t2f_c4f_v4f_t varray[MAX_VERTEX_ARRAYS];
typedef struct varray_t2f_c4f_v4f_s {
	GLfloat		texcoord[2];
	GLfloat		color[4];
	GLfloat		vertex[4];
} varray_t2f_c4f_v4f_t;
*/

//
// gl_warp.c
//
void        EmitBothSkyLayers (msurface_t *fa);
void        EmitWaterPolys (msurface_t *fa);
void        R_DrawSkyChain (msurface_t *s);

//
// gl_draw.c
//
extern int	gl_filter_min;
extern int	gl_filter_max;

//
// gl_rmain.c
//
qboolean    R_CullBox (vec3_t mins, vec3_t maxs);

//
// gl_refrag.c
//
void        R_StoreEfrags (efrag_t **ppefrag);

//
// gl_rsurf.c
//
void        R_DrawBrushModel (entity_t *e);
void        R_DrawWorld (void);
void        GL_BuildLightmaps (void);

//
// gl_ngraph.c
//
void        R_NetGraph (void);

#endif // __GLQUAKE_H
