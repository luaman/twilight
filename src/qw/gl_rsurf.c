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

*/
// r_surf.c: surface-related refresh code
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"


int         skytexturenum;

#ifndef GL_RGBA4
#define	GL_RGBA4	0
#endif


int         lightmap_bytes = 3;				// 1, 3, or 4

int         lightmap_textures;

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

unsigned    blocklights[BLOCK_WIDTH * BLOCK_HEIGHT * 3];

#define	MAX_LIGHTMAPS	256

int         active_lightmaps;

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

glpoly_t   *lightmap_polys[MAX_LIGHTMAPS];
qboolean    lightmap_modified[MAX_LIGHTMAPS];
glRect_t    lightmap_rectchange[MAX_LIGHTMAPS];

int         allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
Uint8       lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

// For gl_texsort 0
msurface_t *skychain = NULL;
msurface_t *waterchain = NULL;

glpoly_t	*fullbright_polys[MAX_GLTEXTURES];
qboolean	drawfullbrights = false;

void        R_RenderDynamicLightmaps (msurface_t *fa);
void		DrawGLPoly (glpoly_t *p);

void 
R_RenderFullbrights (void)
{
	int         i;
	glpoly_t   *p;
//	float			depthdelta;

	if (!drawfullbrights || !gl_fb_bmodels->value)
		return;

	qglDepthMask (GL_FALSE);	// don't bother writing Z

//	depthdelta = -1.0/8192;

	qglEnable (GL_BLEND);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;

		qglBindTexture (GL_TEXTURE_2D, i);

		for (p = fullbright_polys[i]; p; p = p->fb_chain)
			DrawGLPoly (p);

		fullbright_polys[i] = NULL;
	}

	qglDisable (GL_BLEND);
	
	qglDepthMask (GL_TRUE);

	drawfullbrights = false;
}

/*
===============
R_AddDynamicLights
===============
*/
void
R_AddDynamicLights (msurface_t *surf)
{
	int			lnum, br;
	float		dist;
	vec3_t		impact;
	int			local[2];
	int			s, t;
	mtexinfo_t	*tex = surf->texinfo; 
	int			sd, td, _sd, _td;
	int			irad, idist, iminlight;
	unsigned	*dest; 
	
	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;
		
		// not lit by this light 
		dist = PlaneDiff (cl_dlights[lnum].origin, surf->plane);
		irad = cl_dlights[lnum].radius - Q_fabs(dist);
		iminlight = cl_dlights[lnum].minlight;

		if (irad < iminlight)
			continue;

		irad *= 256; iminlight *= 256;
		iminlight = irad - iminlight; 
		
		VectorMA (cl_dlights[lnum].origin, -dist, surf->plane->normal, impact);

		local[0] = (DotProduct (impact, tex->vecs[0]) +
			tex->vecs[0][3] - surf->texturemins[0]) * 256;
		local[1] = (DotProduct (impact, tex->vecs[1]) +
			tex->vecs[1][3] - surf->texturemins[1]) * 256;
		
		_td = local[1];
		dest = blocklights;

		for (t = 0; t < surf->tmax; t++)
		{
			td = _td;
			_td -= 16*256;
			if (td < 0)
				td = -td;
			_sd = local[0];

			for (s = 0; s < surf->smax; s++)
			{
				sd = _sd;
				_sd -= 16*256;
				if (sd < 0)
					sd = -sd;

				if (sd > td)
					idist = sd + (td>>1);
				else
					idist = td + (sd>>1);

				if (idist < iminlight) {
					if (lightmap_bytes == 1) {
						*dest += irad - idist;
					}
					else {
						br = irad - idist;
						dest[0] += (int)(br * cl_dlights[lnum].color[0]);
						dest[1] += (int)(br * cl_dlights[lnum].color[1]);
						dest[2] += (int)(br * cl_dlights[lnum].color[2]);
					}
				}

				if (lightmap_bytes == 1)
					dest ++;
				else
					dest += 3;
			}
		} 
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void
R_BuildLightMap (msurface_t *surf, Uint8 *dest, int stride)
{
	int			i, j, size;
	Uint8		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);
	size = surf->smax * surf->tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (/* r_fullbright->value || */!cl.worldmodel->lightdata) {
		memset (blocklights, 255, size*lightmap_bytes*sizeof(int));
		goto store;
	}

	// clear to no light
	memset (blocklights, 0, size*lightmap_bytes*sizeof(int));

	// add all the lightmaps
	if (lightmap) {
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = blocklights;

			if (lightmap_bytes == 1)
			{
				for (i = 0; i < size; i++)
					bl[i] += lightmap[i] * scale;

				lightmap += size;
			}
			else {
				for (i = 0; i < size; i++) {
					bl[0] += lightmap[0] * scale;
					bl[1] += lightmap[1] * scale;
					bl[2] += lightmap[2] * scale;
					bl += 3;
					lightmap += 3;
				}
			}
		}
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

	// bound, invert, and shift
store:
	switch (gl_lightmap_format) {
		case GL_RGB:

			stride -= (surf->smax * 3);
			bl = blocklights;

			for (i = 0; i < surf->tmax; i++, dest += stride) {
				for (j=surf->smax; j; j--) {
					dest[0] = (Uint8) (min(bl[0], (128 * 255)) >> 7);
					dest[1] = (Uint8) (min(bl[1], (128 * 255)) >> 7);
					dest[2] = (Uint8) (min(bl[2], (128 * 255)) >> 7);
					bl+=3;
					dest+=3;
				}
			}

			break;

		case GL_RGBA:

			stride -= (surf->smax * 4);
			bl = blocklights;

			for (i = 0; i < surf->tmax; i++, dest += stride) {
				for (j=surf->smax; j; j--) {
					dest[0] = (Uint8) (min(bl[0], (128 * 255)) >> 7);
					dest[1] = (Uint8) (min(bl[1], (128 * 255)) >> 7);
					dest[2] = (Uint8) (min(bl[2], (128 * 255)) >> 7);
					dest[3] = 255;
					bl+=3;
					dest+=4;
				}
			}

			break;

		case GL_LUMINANCE:
			bl = blocklights;

			for (i = 0; i < surf->tmax; i++, dest += stride, bl += surf->smax)
				for (j = 0; j < surf->smax; j++)
					dest[j] = (Uint8) (min(bl[j], (128 * 255)) >> 7);

			break;

		default:
			Sys_Error ("Bad lightmap format");
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t  *
R_TextureAnimation (texture_t *base)
{
	int         reletive;
	int         count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	reletive = (int) (cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive) {
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern int  solidskytexture;
extern int  alphaskytexture;
extern float speedscale;							// for top sky and bottom sky

void        DrawGLWaterPoly (glpoly_t *p);			// draws the underwater poly
void        DrawGLWaterPolyLightmap (glpoly_t *p);	// draws the underwater poly lightmap
void        DrawGLWaterPolyMTex (glpoly_t *p);		// draws the underwater poly & lightmap (gl_mtexable only!)

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void
R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t   *p;
	float      *v;
	int         i;
	texture_t  *t;
	glRect_t   *theRect;

	// 
	// normal lightmaped poly
	// 

	if (!(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER))) {
		c_brush_polys++;
		R_RenderDynamicLightmaps (s);
		if (gl_mtexable) {
			qglEnable (GL_BLEND);
			p = s->polys;

			t = R_TextureAnimation (s->texinfo->texture);
			// Binds world to texture env 0
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);
			// Binds lightmap to texenv 1
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglBindTexture (GL_TEXTURE_2D, lightmap_textures + s->lightmaptexturenum);
			qglEnable(GL_TEXTURE_2D);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i]) {
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
								 BLOCK_WIDTH, theRect->h, gl_lightmap_format,
								 GL_UNSIGNED_BYTE,
								 lightmaps + (i * BLOCK_HEIGHT +
											  theRect->t) * BLOCK_WIDTH *
								 lightmap_bytes);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
				qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
				qglVertex3fv (v);
			}
			qglEnd ();
//			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglDisable(GL_TEXTURE_2D);
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglDisable(GL_BLEND);
		} else {
			p = s->polys;

			t = R_TextureAnimation (s->texinfo->texture);
			qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				qglTexCoord2f (v[3], v[4]);
				qglVertex3fv (v);
			}
			qglEnd ();

			qglBindTexture (GL_TEXTURE_2D, lightmap_textures + s->lightmaptexturenum);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i]) {
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
								 BLOCK_WIDTH, theRect->h, gl_lightmap_format,
								 GL_UNSIGNED_BYTE,
								 lightmaps + (i * BLOCK_HEIGHT +
											  theRect->t) * BLOCK_WIDTH *
								 lightmap_bytes);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			qglEnable (GL_BLEND);
			qglBlendFunc (GL_ZERO, GL_SRC_COLOR);
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				qglTexCoord2f (v[5], v[6]);
				qglVertex3fv (v);
			}
			qglEnd ();

			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglDisable (GL_BLEND);
		}

		if (t->fb_texturenum) {
			s->polys->fb_chain = fullbright_polys[t->fb_texturenum];
			fullbright_polys[t->fb_texturenum] = s->polys;
			drawfullbrights = true;
		}
		
		return;
	}
	// 
	// subdivided water surface warp
	// 

	if (s->flags & SURF_DRAWTURB) {
		qglBindTexture (GL_TEXTURE_2D, s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}
	// 
	// subdivided sky warp
	// 
	if (s->flags & SURF_DRAWSKY) {
		qglBindTexture (GL_TEXTURE_2D, solidskytexture);
		speedscale = realtime * 8;
		speedscale -= (int) speedscale & ~127;

		EmitSkyPolys (s);

		qglEnable (GL_BLEND);
		qglBindTexture (GL_TEXTURE_2D, alphaskytexture);
		speedscale = realtime * 16;
		speedscale -= (int) speedscale & ~127;
		EmitSkyPolys (s);

		qglDisable (GL_BLEND);
		return;
	}

	// 
	// underwater optionally warped with lightmap
	// 
	c_brush_polys++;
	R_RenderDynamicLightmaps (s);
	if (gl_mtexable) {
		p = s->polys;

		qglEnable (GL_BLEND);
		t = R_TextureAnimation (s->texinfo->texture);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglEnable (GL_TEXTURE_2D);
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + s->lightmaptexturenum);
		i = s->lightmaptexturenum;
		if (lightmap_modified[i]) {
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
							  BLOCK_WIDTH, theRect->h,
							  gl_lightmap_format, GL_UNSIGNED_BYTE,
							  lightmaps + (i * BLOCK_HEIGHT + theRect->t) *
							  BLOCK_WIDTH * lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (r_waterwarp->value > 0) {	// warping factor greater than 0
			DrawGLWaterPolyMTex(p);
		} else {						// no warping
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
					qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
					qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v);
			}
			qglEnd ();
		}
//		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);

		qglDisable (GL_BLEND);
	} else {
		p = s->polys;

		t = R_TextureAnimation (s->texinfo->texture);
		qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);
		if (r_waterwarp->value > 0) {	// water warp factor > 0, so warp
			DrawGLWaterPoly(p);
		} else {						// no water warp
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				qglTexCoord2f (v[3], v[4]);
				qglVertex3fv (v);
			}
			qglEnd ();
		}

		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + s->lightmaptexturenum);
		i = s->lightmaptexturenum;
		if (lightmap_modified[i]) {
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
							 BLOCK_WIDTH, theRect->h, gl_lightmap_format,
							 GL_UNSIGNED_BYTE,
							 lightmaps + (i * BLOCK_HEIGHT +
										  theRect->t) * BLOCK_WIDTH *
							 lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_ZERO, GL_SRC_COLOR);

		if (r_waterwarp->value > 0) {	// water warp factor > 0, so warp
			DrawGLWaterPolyLightmap (p);
		} else {						// no water warp
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				qglTexCoord2f (v[5], v[6]);
				qglVertex3fv (v);
			}
			qglEnd ();
		}
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		qglDisable (GL_BLEND);
	}
}


/*
================
DrawGLWaterPolyMTex

Warp the vertex coordinates
for gl->mtexable mode, draws
the lightmap as well
================
*/
void
DrawGLWaterPolyMTex (glpoly_t *p)
{
	int			i;
	float		*v;
	vec3_t		nv;
	float		intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	qglBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[2] = v[2];
			qglVertex3fv (nv);
	}
	qglEnd ();
}

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
for !gl->mtexable mode
================
*/
void
DrawGLWaterPoly (glpoly_t *p)
{
	int			i;
	float		*v;
	vec3_t		nv;
	float		intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	qglBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f (v[3], v[4]);
		nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
		nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
		nv[2] = v[2];
		qglVertex3fv (nv);
	}
	qglEnd ();
}

void
DrawGLWaterPolyLightmap (glpoly_t *p)
{
	int			i;
	float		*v;
	vec3_t		nv;
	float		intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	qglBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f (v[5], v[6]);
		nv[0] =	v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
		nv[1] =	v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
		nv[2] = v[2];
		qglVertex3fv (nv);
	}
	qglEnd ();
}

/*
================
DrawGLPoly
================
*/
void
DrawGLPoly (glpoly_t *p)
{
	int         i;
	float      *v;

	qglBegin (GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		qglTexCoord2f (v[3], v[4]);
		qglVertex3fv (v);
	}
	qglEnd ();
}


/*
================
R_BlendLightmaps
================
*/
void
R_BlendLightmaps (void)
{
	int         i, j;
	glpoly_t   *p;
	float      *v;
	glRect_t   *theRect;

//	if (r_fullbright->value)
//		return;
	if (!gl_texsort->value)
		return;

	qglDepthMask (GL_FALSE);					// don't bother writing Z

	qglBlendFunc (GL_ZERO, GL_SRC_COLOR);

	qglEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		if (lightmap_modified[i]) {
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
			qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
							 BLOCK_WIDTH, theRect->h, gl_lightmap_format,
							 GL_UNSIGNED_BYTE,
							 lightmaps + (i * BLOCK_HEIGHT +
										  theRect->t) * BLOCK_WIDTH *
							 lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		for (; p; p = p->chain) {
			if (p->flags & SURF_UNDERWATER && r_waterwarp->value)
				DrawGLWaterPolyLightmap (p);
			else {
				qglBegin (GL_POLYGON);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					qglTexCoord2f (v[5], v[6]);
					qglVertex3fv (v);
				}
				qglEnd ();
			}
		}
	}

	qglDisable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglDepthMask (GL_TRUE);					// back to normal Z buffering
}

/*
================
R_RenderBrushPoly
================
*/
void
R_RenderBrushPoly (msurface_t *fa)
{
	texture_t  *t;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY) {		// warp texture, no lightmaps
		EmitBothSkyLayers (fa);
		return;
	}

	t = R_TextureAnimation (fa->texinfo->texture);
	qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB) {	// warp texture, no lightmaps
		EmitWaterPolys (fa);
		return;
	}

	if (fa->flags & SURF_UNDERWATER && r_waterwarp->value)
		DrawGLWaterPoly (fa->polys);
	else
		DrawGLPoly (fa->polys);

	// add the poly to the proper lightmap chain

	if (t->fb_texturenum) {
		fa->polys->fb_chain = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		drawfullbrights = true;
	}

	R_RenderDynamicLightmaps(fa);
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void
R_RenderDynamicLightmaps (msurface_t *fa)
{
	Uint8      *base;
	int         maps;
	glRect_t   *theRect;

	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
	  dynamic:
		if (r_dynamic->value) {
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}

			if ((theRect->w + theRect->l) < (fa->light_s + fa->smax))
				theRect->w = (fa->light_s - theRect->l) + fa->smax;
			if ((theRect->h + theRect->t) < (fa->light_t + fa->tmax))
				theRect->h = (fa->light_t - theRect->t) + fa->tmax;
			base =
				lightmaps +
				fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH *
				BLOCK_HEIGHT;
			base +=
				fa->light_t * BLOCK_WIDTH * lightmap_bytes +
				fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}

/*
================
R_MirrorChain
================
*/
void
R_MirrorChain (msurface_t *s)
{
	if (mirror)
		return;
	mirror = true;
	mirror_plane = s->plane;
}


/*
================
R_DrawWaterSurfaces
================
*/
void
R_DrawWaterSurfaces (void)
{
	int         i;
	msurface_t *s;
	texture_t  *t;

	if (r_wateralpha->value == 1.0 && gl_texsort->value)
		return;

	// 
	// go back to the world matrix
	// 

	qglLoadMatrixf (r_world_matrix);

	if (r_wateralpha->value < 1.0) {
		qglEnable (GL_BLEND);
		qglColor4f (1, 1, 1, r_wateralpha->value);
	}

	if (!gl_texsort->value) {
		if (!waterchain)
			return;

		for (s = waterchain; s; s = s->texturechain) {
			qglBindTexture (GL_TEXTURE_2D, s->texinfo->texture->gl_texturenum);
			EmitWaterPolys (s);
		}

		waterchain = NULL;
	} else {

		for (i = 0; i < cl.worldmodel->numtextures; i++) {
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			if (!(s->flags & SURF_DRAWTURB))
				continue;

			// set modulate mode explicitly

			qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);

			for (; s; s = s->texturechain)
				EmitWaterPolys (s);

			t->texturechain = NULL;
		}

	}

	// QW has a condition for the qglColor3f, NQ doesn't, which is right?
	if (r_wateralpha->value < 1.0) {
		qglColor3f (1, 1, 1);
		qglDisable (GL_BLEND);
	}

}

/*
================
DrawTextureChains
================
*/
void
DrawTextureChains (void)
{
	int         i;
	msurface_t *s;
	texture_t  *t;

	if (!gl_texsort->value) {
		if (skychain) {
			R_DrawSkyChain (skychain);
			skychain = NULL;
		}

		return;
	}

	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (i == skytexturenum)
			R_DrawSkyChain (s);
		else if (i == mirrortexturenum && r_mirroralpha->value != 1.0) {
			R_MirrorChain (s);
			continue;
		} else {
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha->value != 1.0)
				continue;				// draw translucent water later
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s);
		}

		t->texturechain = NULL;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void
R_DrawBrushModel (entity_t *e)
{
	int         i;
	int         k;
	vec3_t      mins, maxs;
	msurface_t *psurf;
	float       dot;
	mplane_t   *pplane;
	model_t    *clmodel;
	qboolean    rotated;

	currententity = e;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	} else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t      temp;
		vec3_t      forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend->value) {
		for (k = 0; k < MAX_DLIGHTS; k++) {
			if ((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
				continue;

			R_MarkLightsNoVis (&cl_dlights[k], 1 << k, 
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	qglPushMatrix ();

	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (e->angles[0], 0, 1, 0);	// stupid quake bug
	qglRotatef (e->angles[2], 1, 0, 0);

	// 
	// draw texture
	// 
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = PlaneDiff (modelorg, pplane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (gl_texsort->value)
				R_RenderBrushPoly (psurf);
			else
				R_DrawSequentialPoly (psurf);
		}
	}

	R_BlendLightmaps ();

	R_RenderFullbrights ();

	qglPopMatrix ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void
R_RecursiveWorldNode (mnode_t *node)
{
	int         c, side;
	mplane_t   *plane;
	msurface_t *surf, **mark;
	mleaf_t    *pleaf;
	double      dot;

	if (node->contents == CONTENTS_SOLID)
		return;							// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->mins, node->maxs))
		return;

// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}
		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}
// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;
	dot = PlaneDiff (modelorg, plane);

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

// draw stuff
	c = node->numsurfaces;

	if (c) {
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for (; c; c--, surf++) {
				if (surf->visframe != r_framecount)
					continue;

				// don't backface underwater surfaces, because they warp
				if (!
					(((r_viewleaf->contents == CONTENTS_EMPTY
					   && (surf->flags & SURF_UNDERWATER))
					  || (r_viewleaf->contents != CONTENTS_EMPTY
						  && !(surf->flags & SURF_UNDERWATER)))
					 && !(surf->flags & SURF_DONTWARP))
					&& ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue;			// wrong side

				// if sorting by texture, just store it out
				if (gl_texsort->value) {
					if (!mirror
						|| surf->texinfo->texture !=
						cl.worldmodel->textures[mirrortexturenum]) {
						surf->texturechain =
							surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				} else if (surf->flags & SURF_DRAWSKY) {
					surf->texturechain = skychain;
					skychain = surf;
				} else if (surf->flags & SURF_DRAWTURB) {
					surf->texturechain = waterchain;
					waterchain = surf;
				} else
					R_DrawSequentialPoly (surf);

			}
		}

	}
// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}

void R_ClearSkyBox (void);
void R_DrawSkyBox (void);




/*
=============
R_DrawWorld
=============
*/
void
R_DrawWorld (void)
{
	entity_t    ent;

	memset (&ent, 0, sizeof (ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	if (gl_fb_bmodels->value)
		memset (fullbright_polys, 0, sizeof(fullbright_polys));

	R_ClearSkyBox ();

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	R_RecursiveWorldNode (cl.worldmodel->nodes);

	DrawTextureChains ();

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	R_BlendLightmaps ();

	R_RenderFullbrights ();

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	R_DrawSkyBox ();
}


/*
===============
R_MarkLeaves
===============
*/
void
R_MarkLeaves (void)
{
	Uint8      *vis;
	mnode_t    *node;
	int         i;
	Uint8       solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis->value)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis->value) {
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int
AllocBlock (int w, int h, int *x, int *y)
{
	int         i, j;
	int         best, best2;
	int         texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++) {
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w) {				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


mvertex_t  *r_pcurrentvertbase;
model_t    *currentmodel;

//int         nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void
BuildSurfaceDisplayList (msurface_t *fa)
{
	int         i, lindex, lnumverts;
	medge_t    *pedges, *r_pedge;
	int         vertpage;
	float      *vec;
	float       s, t;
	glpoly_t   *poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// 
	// draw texture
	// 
	poly =
		Hunk_Alloc (sizeof (glpoly_t) +
					(lnumverts - 4) * VERTEXSIZE * sizeof (float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// 
		// lightmap texture coordinates
		// 
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16;			// fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16;			// fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	// 
	// remove co-linear points - Ed
	// 
	if (!gl_keeptjunctions->value && !(fa->flags & SURF_UNDERWATER)) {
		for (i = 0; i < lnumverts; ++i) {
			vec3_t      v1, v2;
			float      *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract (this, prev, v1);
			VectorNormalize (v1);
			VectorSubtract (next, prev, v2);
			VectorNormalize (v2);

			// skip co-linear points
#define COLINEAR_EPSILON 0.001
			if ((Q_fabs (v1[0] - v2[0]) <= COLINEAR_EPSILON) &&
				(Q_fabs (v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
				(Q_fabs (v1[2] - v2[2]) <= COLINEAR_EPSILON)) {
				int         j;

				for (j = i + 1; j < lnumverts; ++j) {
					int         k;

					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
//				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void
GL_CreateSurfaceLightmap (msurface_t *surf)
{
	Uint8      *base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	surf->lightmaptexturenum =
		AllocBlock (surf->smax, surf->tmax, &surf->light_s, &surf->light_t);
	base =
		lightmaps +
		surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH * lightmap_bytes);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void
GL_BuildLightmaps (void)
{
	int         i, j;
	model_t    *m;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	if (gl_colorlights->value) {
		gl_lightmap_format = GL_RGB;
		lightmap_bytes = 3;
		colorlights = true;

		if ((i = COM_CheckParm ("-bpp")) != 0) {
			if (Q_atoi (com_argv[i + 1]) == 32) {
				gl_lightmap_format = GL_RGBA;
				lightmap_bytes = 4;
			}
		}
	}
	else {
		gl_lightmap_format = GL_LUMINANCE;
		lightmap_bytes = 1;
		colorlights = false;
	}

	for (j = 1; j < MAX_MODELS; j++) {
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			GL_CreateSurfaceLightmap (m->surfaces + i);
			if (m->surfaces[i].flags & SURF_DRAWTURB)
				continue;
			if (m->surfaces[i].flags & SURF_DRAWSKY)
				continue;
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	// 
	// upload all lightmaps that were filled
	// 
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;						// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		qglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH,
					  BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE,
					  lightmaps +
					  i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes);
	}
}
