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

#include "quakedef.h"
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

glpoly_t	*fullbright_polys[MAX_GLTEXTURES];
qboolean	drawfullbrights = false;

void        R_RenderDynamicLightmaps (msurface_t *fa);
void		DrawGLPoly (glpoly_t *p);

void inline
R_UploadModifiedLightmap (int lightmapnum)
{
	glRect_t	*theRect;
	
	lightmap_modified[lightmapnum] = false;
	theRect = &lightmap_rectchange[lightmapnum];
	qglTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t,
					 BLOCK_WIDTH, theRect->h, gl_lightmap_format,
					 GL_UNSIGNED_BYTE,
					 lightmaps + (lightmapnum * BLOCK_HEIGHT +
								  theRect->t) * BLOCK_WIDTH *
					 lightmap_bytes);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
}

void 
R_RenderFullbrights (void)
{
	int         i, j;
	glpoly_t   *p;
	float		*v;

	if (!drawfullbrights || !gl_fb_bmodels->value)
		return;

	qglDepthMask (GL_FALSE);	// don't bother writing Z

	qglEnable (GL_BLEND);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;

		qglBindTexture (GL_TEXTURE_2D, i);

		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qglTexCoord2f (v[3], v[4]);
				qglVertex3fv (v);
			}
			qglEnd ();
		}

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
					br = irad - idist;
					switch (lightmap_bytes) {
						case 1:
							*dest += br * ((cl_dlights[lnum].color[0] +
											cl_dlights[lnum].color[1] +
											cl_dlights[lnum].color[2]) / 3);
							break;
						case 3:
						case 4:
							dest[0] += (int)(br * cl_dlights[lnum].color[0]);
							dest[1] += (int)(br * cl_dlights[lnum].color[1]);
							dest[2] += (int)(br * cl_dlights[lnum].color[2]);
							break;
					}
				}
				dest += lightmap_bytes;
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
	if (!cl.worldmodel->lightdata) {
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

			switch (lightmap_bytes) {
				case 1:
					for (i = 0; i < size; i++)
						bl[i] += lightmap[i] * scale;

					lightmap += size;
					break;
				case 3:
				case 4:
					for (i = 0; i < size; i++) {
						bl[0] += lightmap[0] * scale;
						bl[1] += lightmap[1] * scale;
						bl[2] += lightmap[2] * scale;
						bl += 3;
						lightmap += 3;
					}
					break;
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

/*
================
R_BlendLightmaps
================
*/
void
R_BlendLightmaps (void)
{
	int         i, j;
	glpoly_t	*p;
	vec3_t		nv;
	float		*v;
	float		intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	qglDepthMask (GL_FALSE);					// don't bother writing Z

	qglBlendFunc (GL_ZERO, GL_SRC_COLOR);

	qglEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		if (lightmap_modified[i])
			R_UploadModifiedLightmap (i);
		for (; p; p = p->chain) {
			if (p->flags & SURF_UNDERWATER && r_waterwarp->value) {
				qglBegin (GL_TRIANGLE_FAN);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					qglTexCoord2f (v[5], v[6]);
					nv[0] =	v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
					nv[1] =	v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
					nv[2] = v[2];
					qglVertex3fv (nv);
				}
				qglEnd ();
			} else {
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
R_RenderBrushPolyMTex
================
*/
static void
R_RenderBrushPolyMTex (msurface_t *fa, texture_t *t)
{
	int		i;
	float	*v;
	vec3_t	nv;
	float	intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	c_brush_polys++;

	qglBindTexture (GL_TEXTURE_2D, lightmap_textures + fa->lightmaptexturenum);

	R_RenderDynamicLightmaps (fa);
	if (lightmap_modified[fa->lightmaptexturenum])
		R_UploadModifiedLightmap (fa->lightmaptexturenum);
	
	if (fa->flags & SURF_UNDERWATER && r_waterwarp->value) {
		qglBegin (GL_TRIANGLE_FAN);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[2] = v[2];
			qglVertex3fv (nv);
		}
		qglEnd ();
	} else {
		qglBegin (GL_POLYGON);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			qglVertex3fv (v);
		}
		qglEnd ();
	}

	// add the poly to the proper fullbright chain

	if (t->fb_texturenum) {
		fa->polys->fb_chain = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		drawfullbrights = true;
	}
}

/*
================
R_RenderBrushPoly
================
*/
static void
R_RenderBrushPoly (msurface_t *fa, texture_t *t)
{
	int		i;
	vec3_t	nv;
	float	*v;
	float	intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	c_brush_polys++;

	if (fa->flags & SURF_UNDERWATER && r_waterwarp->value) {
		qglBegin (GL_TRIANGLE_FAN);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglTexCoord2f (v[3], v[4]);
			nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[2] = v[2];
			qglVertex3fv (nv);
		}
		qglEnd ();
	} else {
		qglBegin (GL_POLYGON);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglTexCoord2f (v[3], v[4]);
			qglVertex3fv (v);
		}
		qglEnd ();
	}

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
R_DrawWaterSurfaces
================
*/
void
R_DrawWaterSurfaces (void)
{
	int         i;
	msurface_t *s;
	texture_t  *t;

	// 
	// go back to the world matrix
	// 

	qglLoadMatrixf (r_world_matrix);

	if (r_wateralpha->value < 1.0) {
		qglEnable (GL_BLEND);
		qglColor4f (1, 1, 1, r_wateralpha->value);
	}

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

	if (r_wateralpha->value < 1.0) {
		qglColor3f (1, 1, 1);
		qglDisable (GL_BLEND);
	}
}

/*
================
DrawTextureChainsMTex
================
*/
void
DrawTextureChainsMTex (void)
{
	int         i;
	msurface_t *s;
	texture_t  *t, *st;

	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		st = R_TextureAnimation (s->texinfo->texture);
		if (i == skytexturenum)
			R_DrawSkyChain (s);
		else {
			if ((s->flags & SURF_DRAWTURB))
				continue;				// draw translucent water later

			if (s->flags & SURF_DRAWSKY) {
				for (; s; s = s->texturechain) {
					EmitBothSkyLayers (s);
				}
				return;
			}

			qglEnable (GL_BLEND);
			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglEnable (GL_TEXTURE_2D);

			for (; s; s = s->texturechain) {
				R_RenderBrushPolyMTex (s, st);
			}

			qglDisable (GL_TEXTURE_2D);
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglDisable (GL_BLEND);
		}

		t->texturechain = NULL;
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
	texture_t  *t, *st;

	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		st = R_TextureAnimation (s->texinfo->texture);
		if (i == skytexturenum)
			R_DrawSkyChain (s);
		else {
			if ((s->flags & SURF_DRAWTURB))
				continue;				// draw translucent water later

			if (s->flags & SURF_DRAWSKY) {
				for (; s; s = s->texturechain) {
					EmitBothSkyLayers (s);
				}
				return;
			}

			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s, st);
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
	texture_t	*t;

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
			if (psurf->flags & SURF_DRAWSKY) {
				EmitBothSkyLayers (psurf);
			} else {
				t = R_TextureAnimation(psurf->texinfo->texture);
				qglBindTexture (GL_TEXTURE_2D, t->gl_texturenum);
				R_RenderBrushPoly (psurf, t);
			}
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

		if (dot < 0)
			side = SURF_PLANEBACK;
		else
			side = 0;
		for (; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			// don't backface underwater surfaces, because they warp
			if (r_waterwarp->value &&
					!(((r_viewleaf->contents == CONTENTS_EMPTY
								&& (surf->flags & SURF_UNDERWATER))
							|| (r_viewleaf->contents != CONTENTS_EMPTY
								&& !(surf->flags & SURF_UNDERWATER)))
						&& !(surf->flags & SURF_DONTWARP))
					&& ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
				continue;			// wrong side

			// if sorting by texture, just store it out
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
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

	R_RecursiveWorldNode (cl.worldmodel->nodes);

	if (gl_mtexable) {
		DrawTextureChainsMTex ();
	} else {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		DrawTextureChains ();
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		R_BlendLightmaps ();
	}

	R_RenderFullbrights ();

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	R_DrawSkyBox ();
	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
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
	poly = Hunk_Alloc (sizeof (glpoly_t)); 
	poly->verts = Hunk_Alloc (lnumverts * sizeof (pvertex_t));
	
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
			VectorNormalizeFast (v1);
			VectorSubtract (next, prev, v2);
			VectorNormalizeFast (v2);

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

