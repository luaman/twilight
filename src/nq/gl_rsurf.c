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
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdlib.h>

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128
#define	MAX_LIGHTMAPS	256

static int lightmap_bytes;				// 1, 3, or 4
static int lightmap_shift;

static int lightmap_textures;

static Uint32 blocklights[BLOCK_WIDTH * BLOCK_HEIGHT * 3];
static Uint8 templight[BLOCK_WIDTH * BLOCK_HEIGHT * 4];

static glpoly_t *lightmap_polys[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

static int r_pvsframecount = 1;

static int dlightdivtable[32768];

vec3_t modelorg;

void
R_InitSurf (void)
{
	int			i;

	dlightdivtable[0] = 4194304;
	for (i = 1;i < 32768;i++)
		dlightdivtable[i] = 4194304 / (i << 7);
}

/*
===============
R_AddDynamicLights
===============
*/
static int
R_AddDynamicLights (msurface_t *surf)
{
	int				i, lnum, lit;
	int				s, t, td, smax, tmax, smax3;
	int				red, green, blue;
	int				dist2, maxdist, maxdist2, maxdist3;
	int				impacts, impactt, subtract;
	int				sdtable[256];
	unsigned int	*bl;
	float			dist;
	vec3_t			impact, local;
	Sint64			k;

	lit = false;

	smax = surf->smax;
	tmax = surf->tmax;
	smax3 = smax * 3;

	for (lnum = 0; lnum < r_numdlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << (lnum & 31))))
			continue;                   // not lit by this light

		VectorCopy (r_dlight[lnum].origin, local);
		dist = PlaneDiff (local, surf->plane);
		
		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) r_dlight[lnum].cullradius2 + LIGHTOFFSET;
		
		dist2 = dist * dist;
		dist2 += LIGHTOFFSET;
		if (dist2 >= maxdist)
			continue;

		if (surf->plane->type < 3)
		{
			impact[0] = local[0];
			impact[1] = local[1];
			impact[2] = local[2];
			impact[surf->plane->type] -= dist;
		}
		else
		{
			impact[0] = local[0] - surf->plane->normal[0] * dist;
			impact[1] = local[1] - surf->plane->normal[1] * dist;
			impact[2] = local[2] - surf->plane->normal[2] * dist;
		}

		impacts = DotProduct (impact, surf->texinfo->vecs[0])
			+ surf->texinfo->vecs[0][3] - surf->texturemins[0];
		impactt = DotProduct (impact, surf->texinfo->vecs[1])
			+ surf->texinfo->vecs[1][3] - surf->texturemins[1];

		s = bound(0, impacts, smax * 16) - impacts;
		t = bound(0, impactt, tmax * 16) - impactt;
		i = s * s + t * t + dist2;
		if (i > maxdist)
			continue;

		// reduce calculations
		for (s = 0, i = impacts; s < smax; s++, i -= 16)
			sdtable[s] = i * i + dist2;

		maxdist3 = maxdist - dist2;

		// convert to 8.8 blocklights format
		red = r_dlight[lnum].light[0];
		green = r_dlight[lnum].light[1];
		blue = r_dlight[lnum].light[2];
		subtract = (int) (r_dlight[lnum].lightsubtract * 4194304.0f);
		bl = blocklights;

		i = impactt;
		for (t = 0; t < tmax; t++, i -= 16)
		{
			td = i * i;
			// make sure some part of it is visible on this line
			if (td < maxdist3)
			{
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++)
				{
					if (sdtable[s] < maxdist2)
					{
						k = dlightdivtable[(sdtable[s] + td) >> 7]
							- subtract;
						if (k > 0)
						{
							bl[0] += (red   * k) >> 8;
							bl[1] += (green * k) >> 8;
							bl[2] += (blue  * k) >> 8;
							lit = true;
						}
					}
					bl += 3;
				}
			}
			else // skip line
				bl += smax3;
		}
	}
	return lit;
}

inline qboolean
R_StainBlendTexel (Sint64 k, int *icolor, Uint8 *bl)
{
	int			ratio, a;
	int			cr, cg, cb, ca;

	ratio = rand() & 255;
	ca = (((icolor[7] - icolor[3]) * ratio) >> 8) + icolor[3];
	a = (ca * k) >> 8;

	if (a > 0)
	{
		a = bound(0, a, 256);
		cr = (((icolor[4] - icolor[0]) * ratio) >> 8) + icolor[0];
		cg = (((icolor[5] - icolor[1]) * ratio) >> 8) + icolor[1];
		cb = (((icolor[6] - icolor[2]) * ratio) >> 8) + icolor[2];
		bl[0] = (Uint8) ((((cr - (int) bl[0]) * a) >> 8) + (int) bl[0]);
		bl[1] = (Uint8) ((((cg - (int) bl[1]) * a) >> 8) + (int) bl[1]);
		bl[2] = (Uint8) ((((cb - (int) bl[2]) * a) >> 8) + (int) bl[2]);
		return true;
	}
	else
		return false;
}

/*
===============
R_StainNode
===============
*/
void
R_StainNode (mnode_t *node, model_t *model, vec3_t origin, float radius,
		int icolor[8])
{
	float			ndist; 
	msurface_t		*surf, *endsurf;
	int				i, stained;
	int				s, t, td, smax, tmax, smax3;
	int				dist2, maxdist, maxdist2, maxdist3;
	int				impacts, impactt, subtract;
	int				sdtable[256];
	Uint8			*bl; 
	vec3_t			impact;
	Sint64			k;

	// for comparisons to minimum acceptable light
	// compensate for 4096 offset
	maxdist = radius * radius + 4096;

	// clamp radius to avoid exceeding 32768 entry division table
	maxdist = min (maxdist, 4194304);

	subtract = (int) ((1.0f / maxdist) * 4194304.0f);

loc0:
	if (node->contents < 0)
		return;
	ndist = PlaneDiff(origin, node->plane);
	if (ndist > radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -radius)
	{
		node = node->children[1];
		goto loc0;
	}

	dist2 = ndist * ndist;
	dist2 += 4096.0f;
	if (dist2 < maxdist)
	{
		maxdist3 = maxdist - dist2;

		if (node->plane->type < 3)
		{
			impact[0] = origin[0];
			impact[1] = origin[1];
			impact[2] = origin[2];
			impact[node->plane->type] -= ndist;
		}
		else
		{
			impact[0] = origin[0] - node->plane->normal[0] * ndist;
			impact[1] = origin[1] - node->plane->normal[1] * ndist;
			impact[2] = origin[2] - node->plane->normal[2] * ndist;
		}

		surf = model->surfaces + node->firstsurface;
		endsurf = surf + node->numsurfaces;
		for (; surf < endsurf; surf++)
		{
			if (surf->stainsamples)
			{
				smax = surf->smax;
				tmax = surf->tmax;

				impacts = DotProduct (impact, surf->texinfo->vecs[0])
					+ surf->texinfo->vecs[0][3] - surf->texturemins[0];
				impactt = DotProduct (impact, surf->texinfo->vecs[1])
					+ surf->texinfo->vecs[1][3] - surf->texturemins[1];

				s = bound(0, impacts, smax * 16) - impacts;
				t = bound(0, impactt, tmax * 16) - impactt;
				i = s * s + t * t + dist2;
				if (i > maxdist)
					continue;

				// reduce calculations
				for (s = 0, i = impacts; s < smax; s++, i -= 16)
					sdtable[s] = i * i + dist2;

				// convert to 8.8 blocklights format
				bl = surf->stainsamples;
				smax3 = smax * 3;
				stained = false;

				i = impactt;
				for (t = 0;t < tmax;t++, i -= 16)
				{
					td = i * i;
					// make sure some part of it is visible on this line
					if (td < maxdist3)
					{
						maxdist2 = maxdist - td;
						for (s = 0; s < smax; s++)
						{
							if (sdtable[s] < maxdist2)
							{
								k = dlightdivtable[(sdtable[s] + td) >> 7]
									- subtract;
								if (k > 0)
									if (R_StainBlendTexel (k, icolor, bl))
										stained = true;
							}
							bl += 3;
						}
					}
					else // skip line
						bl += smax3;
				}

				// force lightmap upload
				if (stained)
					surf->cached_dlight = true;
			}
		}
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_StainNode(node->children[0], model, origin, radius, icolor);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->contents >= 0)
	{
		node = node->children[1];
		goto loc0;
	}
}


/*
===============
R_Stain
===============
*/
void
R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1,
		int cr2, int cg2, int cb2, int ca2)
{
	int			icolor[8];
	int			n;
	entity_t	*ent;
	vec3_t		org;
	model_t		*model;

	if (!r_stainmaps->ivalue)
		return;

	icolor[0] = cr1;
	icolor[1] = cg1;
	icolor[2] = cb1;
	icolor[3] = ca1;
	icolor[4] = cr2;
	icolor[5] = cg2;
	icolor[6] = cb2;
	icolor[7] = ca2;

	model = cl.worldmodel;
	softwaretransformidentity();
	R_StainNode(model->nodes + model->hulls[0].firstclipnode, model,
			origin, radius, icolor);

	// look for embedded bmodels
	for (n = 1; n < MAX_EDICTS; n++)
	{
		ent = &cl_entities[n];
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			if (model->type == mod_brush)
			{
				softwaretransformforentity (ent->origin, ent->angles);
				softwareuntransform (origin, org);
				R_StainNode(model->nodes + model->hulls[0].firstclipnode,
						model, org, radius, icolor);
			}
		}
	}
}


/*
===============
R_BuildLightmap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void
GL_BuildLightmap (msurface_t *surf)
{
	int			i, j, size3, stride;
	Uint8	   *lightmap, *dest, *stain;
	Uint32		scale, *bl;

	// Bind your textures early and often - or at least early
	qglBindTexture (GL_TEXTURE_2D, lightmap_textures
			+ surf->lightmaptexturenum);

	// Reset stuff here
	surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
	surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
	surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
	surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

	size3 = surf->smax * surf->tmax * 3;

	lightmap = surf->samples;

	// set to full bright if no light data
	if (!cl.worldmodel->lightdata)
	{
		bl = blocklights;
		for (i = 0;i < size3;i++)
			bl[i] = 255*256;			
	}
	else
	{
		// clear to no light
		memset (blocklights, 0, size3 * sizeof(Uint32));

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			if (R_AddDynamicLights (surf))
				surf->cached_dlight = 1;

		// add all the lightmaps
		if (lightmap)
		{
			for (i = 0; i < MAXLIGHTMAPS && surf->styles[i] != 255; i++)
			{
				scale = d_lightstylevalue[surf->styles[i]];
				bl = blocklights;

				for (j = 0; j < size3; j++)
					bl[j] += lightmap[j] * scale;
				lightmap += j;
			}
		}
	}

	bl = blocklights;
	dest = templight;
	stain = surf->stainsamples;

	// bound, invert, and shift
	stride = surf->alignedwidth * lightmap_bytes;

	switch (gl_lightmap_format)
	{
		case GL_RGB:
			stride -= surf->smax * 3;
			for (i = 0; i < surf->tmax; i++, dest += stride)
			{
				for (j = 0; j < surf->smax; j++)
				{
					dest[0] = min ((bl[0] * stain[0]) >> lightmap_shift, 255);
					dest[1] = min ((bl[1] * stain[1]) >> lightmap_shift, 255);
					dest[2] = min ((bl[2] * stain[2]) >> lightmap_shift, 255);
					bl += 3;
					dest += 3;
					stain += 3;
				}
			}

			break;

		case GL_RGBA:
			stride -= surf->smax * 4;
			for (i = 0; i < surf->tmax; i++, dest += stride)
			{
				for (j = 0; j < surf->smax; j++)
				{
					dest[0] = min ((bl[0] * stain[0]) >> lightmap_shift, 255);
					dest[1] = min ((bl[1] * stain[1]) >> lightmap_shift, 255);
					dest[2] = min ((bl[2] * stain[2]) >> lightmap_shift, 255);
					dest[3] = 255;
					bl += 3;
					dest += 4;
					stain += 3;
				}
			}

			break;

		case GL_LUMINANCE:
			stride -= surf->smax;

			for (i = 0; i < surf->tmax; i++, dest += stride)
			{
				for (j = 0; j < surf->smax; j++)
				{
					// 85 / 256 == 0.33203125, close enough
					scale = ((bl[0] + bl[1] + bl[2]) * 85) >> lightmap_shift;
					*dest++ = min (scale, 255);
					bl += 3;
				}
			}
			break;

		default:
			Sys_Error ("Bad lightmap format - your compiler sucks!");
	}

	qglTexSubImage2D (GL_TEXTURE_2D, 0, surf->light_s, surf->light_t,
			surf->alignedwidth, surf->tmax, gl_lightmap_format,
			GL_UNSIGNED_BYTE, templight);
}

static void
GL_UpdateLightmap (msurface_t *fa)
{
	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	if (fa->lightmappedframe != r_framecount)
	{
		fa->lightmappedframe = r_framecount;
		fa->polys->lchain = lightmap_polys[fa->lightmaptexturenum];
		lightmap_polys[fa->lightmaptexturenum] = fa->polys;
	}

	if (!r_dynamic->ivalue)
		return;

	if (fa->dlightframe == r_framecount // dynamic lighting
			|| fa->cached_dlight // previously lit
			|| d_lightstylevalue[fa->styles[0]] != fa->cached_light[0]
			|| d_lightstylevalue[fa->styles[1]] != fa->cached_light[1]
			|| d_lightstylevalue[fa->styles[2]] != fa->cached_light[2]
			|| d_lightstylevalue[fa->styles[3]] != fa->cached_light[3])
		GL_BuildLightmap(fa);
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
static texture_t *
R_TextureAnimation (texture_t *base)
{
	int			relative;
	int			count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
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


/*
================
R_BlendLightmaps
================
*/
static void
R_BlendLightmaps (void)
{
	int			i;
	glpoly_t   *p;

	// don't bother writing Z
	qglDepthMask (GL_FALSE);

	qglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	qglEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		for (; p; p = p->lchain) {
			memcpy (v_array_p, p->v, sizeof(vertex_t) * p->numverts);
			memcpy (tc0_array_p, p->ltc, sizeof(texcoord_t) * p->numverts);

			TWI_PreVDrawCVA (0, p->numverts);
			qglDrawArrays (GL_POLYGON, 0, p->numverts);
			TWI_PostVDrawCVA ();
		}
	}

	qglDisable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// back to normal Z buffering
	qglDepthMask (GL_TRUE);
}

/*
================
R_RenderBrushPolyMTex
================
*/
static void
R_RenderBrushPolyMTex (msurface_t *fa, texture_t *t)
{
	c_brush_polys++;

	qglBindTexture(GL_TEXTURE_2D, lightmap_textures + fa->lightmaptexturenum);

	memcpy (v_array_p, fa->polys->v, sizeof(vertex_t) * fa->polys->numverts);
	memcpy (tc0_array_p, fa->polys->tc,
			sizeof(texcoord_t) * fa->polys->numverts);
	memcpy (tc1_array_p, fa->polys->ltc,
			sizeof(texcoord_t) * fa->polys->numverts);

	TWI_PreVDrawCVA (0, fa->polys->numverts);
	qglDrawArrays (GL_POLYGON, 0, fa->polys->numverts);
	TWI_PostVDrawCVA ();
}

/*
================
R_RenderBrushPoly
================
*/
static void
R_RenderBrushPoly (msurface_t *fa, texture_t *t)
{
	c_brush_polys++;

	memcpy (v_array_p, fa->polys->v, sizeof(vertex_t) * fa->polys->numverts);
	memcpy (tc0_array_p, fa->polys->tc,
			sizeof(texcoord_t) * fa->polys->numverts);

	TWI_PreVDrawCVA (0, fa->polys->numverts);
	qglDrawArrays (GL_POLYGON, 0, fa->polys->numverts);
	TWI_PostVDrawCVA ();
}

/*
================
DrawTextureChains
================
*/
static void
DrawTextureChains ()
{
	unsigned int	i;
	msurface_t		*s;
	texture_t		*t, *st;

	// LordHavoc: upload lightmaps early
	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
			continue;
		for (; s; s = s->texturechain)
			GL_UpdateLightmap(s);
	}

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		st = R_TextureAnimation (s->texinfo->texture);
		if (s->flags & SURF_DRAWSKY)
		{
			R_DrawSkyChain (s);
			t->texturechain = NULL;
		}
	}

	if (gl_mtex)
	{
		if (gl_mtexcombine)
		{
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 4);
		}
		else
		{
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qglEnable (GL_TEXTURE_2D);

		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s || (s->flags & SURF_DRAWTURB))
				continue;
			st = R_TextureAnimation (t);
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			qglActiveTextureARB (GL_TEXTURE1_ARB);

			for (; s; s = s->texturechain)
				R_RenderBrushPolyMTex (s, st);
		}

		qglDisable (GL_TEXTURE_2D);

		if (gl_mtexcombine)
			qglTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s || (s->flags & SURF_DRAWTURB))
				continue;
			st = R_TextureAnimation (t);

			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s, st);
		}

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		R_BlendLightmaps();
	}

	// If the water is solid, draw here, if not, then later.
	if (r_wateralpha->fvalue == 1)
		R_DrawWaterTextureChains ();

	// Draw the fullbrights, if there are any
	if (!gl_fb_bmodels->ivalue)
	{
		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			s = cl.worldmodel->textures[i]->texturechain;
			if (!s || (s->flags & SURF_DRAWTURB))
				continue;
			cl.worldmodel->textures[i]->texturechain = NULL;
		}
		return;
	}

	qglDepthMask (GL_FALSE);	// don't bother writing Z
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s || (s->flags & SURF_DRAWTURB))
			continue;
		st = R_TextureAnimation (t);

		if (st->fb_texturenum)
		{
			qglBindTexture (GL_TEXTURE_2D, st->fb_texturenum);
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s, st);
		}

		t->texturechain = NULL;
	}

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable (GL_BLEND);
	qglDepthMask (GL_TRUE);
}

/*
================
R_DrawWaterTextureChains
================
*/
void
R_DrawWaterTextureChains ()
{
	unsigned int	i;
	msurface_t		*s;
	texture_t		*t, *st;
	float			wateralpha = r_wateralpha->fvalue;

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!(s && (s->flags & SURF_DRAWTURB)))
			continue;
		st = R_TextureAnimation (t);

		qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
		for (; s; s = s->texturechain)
			EmitWaterPolys (s, st, false, wateralpha);

		t->texturechain = NULL;
	}

	if (wateralpha != 1.0f)
		qglColor4fv (whitev);
}


/*
=================
R_DrawBrushModelSkies
=================
*/
void
R_DrawBrushModelSkies (void)
{
	int				i, j;
	vec3_t			mins, maxs;
	msurface_t		*psurf;
	float			dot;
	model_t			*clmodel;
	entity_t		*e;

	for (i = 0; i < r_refdef.num_entities; i++)
	{
		e = r_refdef.entities[i];
		clmodel = e->model;
	
		if (e->model->type == mod_brush)
		{
			Mod_MinsMaxs (clmodel, e->origin, e->angles, mins, maxs);

			if (R_CullBox (mins, maxs))
				return;

			memset (lightmap_polys, 0, sizeof (lightmap_polys));

			softwaretransformforbrushentity (e->origin, e->angles);
			softwareuntransform(r_origin, modelorg);

			qglPushMatrix ();
			qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
			qglRotatef (e->angles[1], 0, 0, 1);
			qglRotatef (e->angles[0], 0, 1, 0);
			qglRotatef (e->angles[2], 1, 0, 0);

			// If any sky surfs are visible, draw them now
			psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			for (j = 0; j < clmodel->nummodelsurfaces; j++)
			{
				// find which side of the node we are on
				dot = PlaneDiff (modelorg, psurf->plane);

				// draw the polygon
				if ((psurf->flags & SURF_DRAWSKY)
						&& (((psurf->flags & SURF_PLANEBACK)
								&& (dot < -BACKFACE_EPSILON))
							|| (!(psurf->flags & SURF_PLANEBACK)
								&& (dot > BACKFACE_EPSILON))))
				{
					// FIXME: Do this better
					EmitBothSkyLayers (psurf);
					R_DrawSkyBoxChain (psurf);
				}
				psurf->visframe = -1;
				psurf++;
			}
			qglPopMatrix ();
		}
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
	int				i, k, texnum;
	vec3_t			mins, maxs;
	msurface_t		*psurf;
	float			dot, wateralpha = r_wateralpha->fvalue;
	model_t			*clmodel = e->model;
	texture_t		*t, *st;

	Mod_MinsMaxs (clmodel, e->origin, e->angles, mins, maxs);

	if (R_CullBox (mins, maxs))
		return;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	softwaretransformforbrushentity (e->origin, e->angles);
	softwareuntransform(r_origin, modelorg);

	/*
	 * LordHavoc: decide which surfs are visible and update lightmaps, then
	 * render afterward
	 */
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		dot = PlaneDiff (modelorg, psurf->plane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
				|| (!(psurf->flags & SURF_PLANEBACK)
					&& (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = r_framecount;
		}
		else
			psurf->visframe = -1;
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend->ivalue)
	{
		for (k = 0; k < r_numdlights; k++)
			R_MarkLightsNoVis (&r_dlight[k], 1 << k,
					clmodel->nodes + clmodel->hulls[0].firstclipnode);
	}

	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			i < clmodel->nummodelsurfaces; i++, psurf++)
		if (psurf->visframe == r_framecount)
			GL_UpdateLightmap(psurf);

	qglPushMatrix ();

	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (e->angles[0], 0, 1, 0);
	qglRotatef (e->angles[2], 1, 0, 0);

	// for transpoly water
	softwaretransformforbrushentity (e->origin, e->angles);

	/*
	 * draw texture
	 */
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			if (psurf->flags & SURF_DRAWSKY)
				psurf->visframe = -1;
		}
	}

	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			if (psurf->flags & SURF_DRAWTURB)
			{
				st = R_TextureAnimation (psurf->texinfo->texture);
				qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
				EmitWaterPolys (psurf, st, true, wateralpha);
				psurf->visframe = -1;
			}
		}
	}

	if (gl_mtexcombine)
	{
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
		qglTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 4);
		qglEnable (GL_TEXTURE_2D);
	}
	else if (gl_mtex)
	{
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglEnable (GL_TEXTURE_2D);
	}
	else
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	texnum = -1;
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			t = R_TextureAnimation(psurf->texinfo->texture);
			if (gl_mtex)
			{
				if (texnum != t->gl_texturenum)
				{
					texnum = t->gl_texturenum;
					qglActiveTextureARB (GL_TEXTURE0_ARB);
					qglBindTexture (GL_TEXTURE_2D, texnum);
					qglActiveTextureARB (GL_TEXTURE1_ARB);
				}
				R_RenderBrushPolyMTex (psurf, t);
			}
			else
			{
				if (texnum != t->gl_texturenum)
				{
					texnum = t->gl_texturenum;
					qglBindTexture (GL_TEXTURE_2D, texnum);
				}
				R_RenderBrushPoly (psurf, t);
			}
		}
	}

	if (gl_mtex)
	{
		if (gl_mtexcombine)
			qglTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		R_BlendLightmaps ();
	}

	if (gl_fb_bmodels->ivalue)
	{
		qglDepthMask (GL_FALSE);			// don't bother writing Z
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

		for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
				i < clmodel->nummodelsurfaces; i++, psurf++)
		{
			if (psurf->visframe == r_framecount)
			{
				t = R_TextureAnimation(psurf->texinfo->texture);
				if (t->fb_texturenum)
				{
					if (texnum != t->fb_texturenum)
					{
						texnum = t->fb_texturenum;
						qglBindTexture (GL_TEXTURE_2D, texnum);
					}
					R_RenderBrushPoly (psurf, t);
				}
			}
		}

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_BLEND);
		qglDepthMask (GL_TRUE);
	}

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
static void
R_RecursiveWorldNode (mnode_t *node)
{
	int				c, side;
	mplane_t	   *plane;
	msurface_t	   *surf, **mark;
	mleaf_t		   *pleaf;
	double			dot;

	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->pvsframe != r_pvsframecount)
		return;
	if (R_CullBox (node->mins, node->maxs))
		return;

	// mark node/leaf as visible for MarkLights
	node->visframe = r_framecount;

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

		return;
	}
	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;
	dot = PlaneDiff (modelorg, plane);
	side = dot < 0;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

	// draw stuff
	c = node->numsurfaces;

	if (c) {
		surf = cl.worldmodel->surfaces + node->firstsurface;
		side = (dot < 0) ? SURF_PLANEBACK : 0;

		for (; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

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
===============
R_MarkLeaves
===============
*/
static void
R_MarkLeaves (void)
{
	Uint8		   *vis;
	mnode_t		   *node;
	unsigned int	i;

	if (r_oldviewleaf == r_viewleaf && !r_novis->ivalue)
		return;

	r_pvsframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis->ivalue)
	{
		for (i = 0; i < cl.worldmodel->numleafs; i++)
		{
			node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
			do {
				if (node->pvsframe == r_pvsframecount)
					break;
				node->pvsframe = r_pvsframecount;
				node = node->parent;
			} while (node);
		}
	} else {
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		for (i = 0; i < cl.worldmodel->numleafs; i++)
		{
			if (vis[i >> 3] & (1 << (i & 7)))
			{
				node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
				do {
					if (node->pvsframe == r_pvsframecount)
						break;
					node->pvsframe = r_pvsframecount;
					node = node->parent;
				} while (node);
			}
		}
	}
}

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

	VectorCopy (r_origin, modelorg);
	currententity = &ent;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	R_MarkLeaves ();
	R_RecursiveWorldNode (cl.worldmodel->nodes);

	R_PushDlights ();

	DrawTextureChains ();
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
static int
AllocBlock (int w, int h, int *x, int *y)
{
	int			i, j;
	int			best, best2;
	int			texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			
			if (j == w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		/*
		 * LordHavoc: clear texture to blank image, fragments are uploaded
		 * using subimage
		 */
		if (!allocated[texnum][0])
		{
			memset(templight, 0, sizeof(templight));
			qglBindTexture(GL_TEXTURE_2D, lightmap_textures + texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, colorlights ? 3 : 1, BLOCK_WIDTH,
					BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE,
					templight);
		}

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Host_Error ("AllocBlock: full");
	return 0;
}


static mvertex_t *r_pcurrentvertbase;
static model_t *currentmodel;


/*
================
BuildSurfaceDisplayList
================
*/
static void
BuildSurfaceDisplayList (msurface_t *fa)
{
	int				i, lindex, lnumverts;
	medge_t		   *pedges, *r_pedge;
	int				vertpage;
	float		   *vec;
	float			s, t;
	glpoly_t	   *poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	/*
	 * draw texture
	 */
	poly = Hunk_Alloc (sizeof (glpoly_t));
	poly->tc = Hunk_Alloc (lnumverts * sizeof (texcoord_t));
	poly->ltc = Hunk_Alloc (lnumverts * sizeof (texcoord_t));
	poly->v = Hunk_Alloc (lnumverts * sizeof (vertex_t));

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
		VectorCopy (vec, poly->v[i].v);

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		poly->tc[i].v[0] = s;
		poly->tc[i].v[1] = t;

		/*
		 * lightmap texture coordinates
		 */
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s << 4;
		s += 8;
		s /= BLOCK_WIDTH << 4;			// fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t << 4;
		t += 8;
		t /= BLOCK_HEIGHT << 4;			// fa->texinfo->texture->height;

		poly->ltc[i].v[0] = s;
		poly->ltc[i].v[1] = t;
	}
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
static void
GL_CreateSurfaceLightmap (msurface_t *surf)
{
	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	/*
	 * LordHavoc: TexSubImage2D needs data aligned on 4 byte boundaries
	 * unless I specify glPixelStorei(GL_UNPACK_ALIGNMENT, 1), I suspect 4
	 * byte may be faster anyway, so it is aligned on 4 byte boundaries...
	 */
	surf->alignedwidth = surf->smax;
	while ((surf->alignedwidth * lightmap_bytes) & 3)
		surf->alignedwidth++;

	surf->lightmaptexturenum =
		AllocBlock (surf->alignedwidth, surf->tmax, &surf->light_s, &surf->light_t);
	GL_BuildLightmap(surf);
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
	Uint32		i, j;
	model_t	   *m;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					// no dlightcache

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	if (gl_mtexcombine)
		lightmap_shift = 9;
	else if (gl_mtex)
		lightmap_shift = 7;
	else
		lightmap_shift = 8;

	lightmap_shift += 8;

	switch (gl_colorlights->ivalue)
	{
		case 0:
			gl_lightmap_format = GL_LUMINANCE;
			lightmap_bytes = 1;
			colorlights = false;
			break;

		default:
		case 1:
			gl_lightmap_format = GL_RGB;
			lightmap_bytes = 3;
			colorlights = true;
			break;

		case 2:
			gl_lightmap_format = GL_RGBA;
			lightmap_bytes = 4;
			colorlights = true;
			break;
	}

	for (j = 1; j < MAX_MODELS; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}
}

