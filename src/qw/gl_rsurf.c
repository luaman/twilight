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

int lightmap_bytes;				// 1, 3, or 4
int lightmap_shift;

static Uint32 blocklights[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 3];
extern Uint8 templight[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 4];

static qboolean drawfullbrights = false;

static Uint r_pvsframecount = 1;

static int dlightdivtable[0x8000];

vec3_t modelorg;

static texture_t *R_TextureAnimation (texture_t *base);

void
R_InitSurf (void)
{
	int			i;

	dlightdivtable[0] = 4194304;
	for (i = 1;i < 0x8000;i++)
		dlightdivtable[i] = 4194304 / (i << 7);
}

/*
================
R_RenderPolys
================
*/
static inline void
R_RenderPolys (glpoly_t *p, qboolean t0, qboolean t1)
{
	for (; p; p = p->next) {
		c_brush_polys++;

		memcpy (v_array_p, p->v, sizeof(vertex_t) * p->numverts);
		if (t0)
			memcpy (tc0_array_p, p->tc, sizeof(texcoord_t) * p->numverts);
		if (t1)
			memcpy (tc1_array_p, p->ltc, sizeof(texcoord_t) * p->numverts);

		TWI_PreVDrawCVA (0, p->numverts);
		qglDrawArrays (GL_POLYGON, 0, p->numverts);
		TWI_PostVDrawCVA ();
	}
}

static void
R_RenderFullbrights (brushhdr_t *brush)
{
	Uint			 i, j;
	chain_head_t	*chain;
	chain_item_t	*c;
	texture_t		*st;
	qboolean		 bound;

	if (!drawfullbrights || !gl_fb_bmodels->ivalue)
		return;

	qglDepthMask (GL_FALSE);	// don't bother writing Z

	qglEnable (GL_BLEND);

	for (i = 0; i < brush->numtextures; i++) {
		chain = brush->tex_chains[i];
		if (!chain || !(chain->flags & CHAIN_FB)
				|| (chain->visframe != r_framecount))
			continue;

		c = chain->items;
		for (j = 0, bound = false; j < chain->n_items; j++) {
			if (c[j].visframe == r_framecount) {
				if (!bound) {
					bound = true;
					st = R_TextureAnimation (chain->texture);
					qglBindTexture (GL_TEXTURE_2D, st->fb_texturenum);
				}
				R_RenderPolys (c[j].surf->polys, true, false);
			}
		}
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
static int
R_AddDynamicLights (msurface_t *surf)
{
	Uint			lnum;
	int				i, lit;
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

	// clamp radius to avoid exceeding 0x8000 entry division table
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

		surf = model->extra.brush->surfaces + node->firstsurface;
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
	R_StainNode(model->extra.brush->nodes +
			model->extra.brush->hulls[0].firstclipnode,
			model, origin, radius, icolor);

	// look for embedded bmodels
	for (n = 1; n < MAX_EDICTS; n++)
	{
		ent = &cl_network_entities[n];
		model = ent->model;
		if (model && model->name[0] == '*' && model->type == mod_brush)
		{
			softwaretransformforentity (ent->cur.origin, ent->cur.angles);
			softwareuntransform (origin, org);
			R_StainNode(model->extra.brush->nodes +
					model->extra.brush->hulls[0].firstclipnode,
					model, org, radius, icolor);
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
GL_BuildLightmap (msurface_t *surf, brushhdr_t *brush)
{
	int			i, j, size3, stride;
	Uint8	   *lightmap, *dest, *stain;
	Uint32		scale, *bl;

	// Bind your textures early and often - or at least early
	qglBindTexture(GL_TEXTURE_2D, 
			brush->lightblock->b[surf->lightmap_texnum].chain.l_texnum);

	// Reset stuff here
	surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
	surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
	surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
	surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

	size3 = surf->smax * surf->tmax * 3;

	lightmap = surf->samples;

	// set to full bright if no light data
	if (!cl.worldmodel->extra.brush->lightdata)
		memset (blocklights, 255, size3	* sizeof(Uint32));
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
GL_UpdateLightmap (msurface_t *fa, brushhdr_t *brush)
{
	if (fa->texinfo->flags & TEX_SPECIAL)
		return;

	if (!r_dynamic->ivalue)
		return;

	if (fa->dlightframe == r_framecount // dynamic lighting
			|| fa->cached_dlight // previously lit
			|| d_lightstylevalue[fa->styles[0]] != fa->cached_light[0]
			|| d_lightstylevalue[fa->styles[1]] != fa->cached_light[1]
			|| d_lightstylevalue[fa->styles[2]] != fa->cached_light[2]
			|| d_lightstylevalue[fa->styles[3]] != fa->cached_light[3])
		GL_BuildLightmap(fa, brush);
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
R_BlendLightmaps (brushhdr_t *brush)
{
	Uint			 i, j;
	chain_head_t	*chain;
	chain_item_t	*c;
	msurface_t		*s;
	glpoly_t		*p;
	qboolean		 bound;
	lightsubblock_t	*sub;

	// don't bother writing Z
	qglDepthMask (GL_FALSE);

	qglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	qglEnable (GL_BLEND);

	for (sub = brush->lightblock->b, i = 0; i < brush->lightblock->num;
			i++, sub++)
	{
		chain = &sub->chain;
		bound = false;
		if ((chain->visframe != r_framecount))
			continue;

		c = chain->items;
		for (j = 0, bound = false; j < chain->n_items; j++)
			if (c[j].visframe == r_framecount) {
				s = c[j].surf;
				if (!bound) {
					bound = true;
					qglBindTexture (GL_TEXTURE_2D, chain->l_texnum);
				}
				for (p = s->polys; p; p = p->next) {
					memcpy(v_array_p, p->v, sizeof(vertex_t) * p->numverts);
					memcpy(tc0_array_p, p->ltc, sizeof(texcoord_t)*p->numverts);

					TWI_PreVDrawCVA (0, p->numverts);
					qglDrawArrays (GL_POLYGON, 0, p->numverts);
					TWI_PostVDrawCVA ();
				}
			}
	}

	qglDisable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// back to normal Z buffering
	qglDepthMask (GL_TRUE);
}


/*
================
DrawTextureChains
================
*/
static void
DrawTextureChains (brushhdr_t *brush, qboolean transform)
{
	Uint			i, j;
	msurface_t		*s;
	texture_t		*st;
	qboolean		bound;
	chain_head_t	*chain;
	chain_item_t	*c;
	lightsubblock_t	*sub;

	// LordHavoc: upload lightmaps early
	for (sub = brush->lightblock->b, i = 0; i < brush->lightblock->num;
			i++, sub++)
	{
		chain = &sub->chain;
		if (!chain->n_items || (chain->visframe != r_framecount))
			continue;

		c = chain->items;
		for (j = 0; j < chain->n_items; j++)
			if (c[j].visframe == r_framecount)
				GL_UpdateLightmap(c[j].surf, brush);
	}

	if (!draw_skybox) {
		if (r_fastsky->ivalue)
			R_Draw_Fast_Sky_Chain (&brush->sky_chain, modelorg);
		else
			R_Draw_Old_Sky_Chain (&brush->sky_chain, modelorg);
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

		for (i = 0; i < brush->numtextures; i++) {
			chain = brush->tex_chains[i];
			if (!chain || !chain->n_items || !(chain->flags & CHAIN_NORMAL)
					|| (chain->visframe != r_framecount))
				continue;

			c = chain->items;
			for (j = 0, bound = false; j < chain->n_items; j++)
				if (c[j].visframe == r_framecount) {
					s = c[j].surf;
					if (!bound) {
						bound = true;
						st = R_TextureAnimation (chain->texture);
						qglActiveTextureARB (GL_TEXTURE0_ARB);
						qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
						qglActiveTextureARB (GL_TEXTURE1_ARB);
					}
					qglBindTexture(GL_TEXTURE_2D, brush->lightblock->b[s->lightmap_texnum].chain.l_texnum);
					R_RenderPolys (s->polys, true, true);
				}
		}

		qglDisable (GL_TEXTURE_2D);

		qglTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		for (i = 0; i < brush->numtextures; i++) {
			chain = brush->tex_chains[i];
			if (!chain || !chain->n_items || !(chain->flags & CHAIN_NORMAL)
					|| (chain->visframe != r_framecount))
				continue;

			c = chain->items;
			for (j = 0, bound = false; j < chain->n_items; j++)
				if (c[j].visframe == r_framecount) {
					s = c[j].surf;
					if (!bound) {
						bound = true;
						st = R_TextureAnimation (chain->texture);
						qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
					}
					R_RenderPolys (s->polys, true, false);
				}
		}

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		R_BlendLightmaps (cl.worldmodel->extra.brush);
	}

	// If the water is solid, draw here, if not, then later.
	if (r_wateralpha->fvalue == 1)
		R_DrawWaterTextureChains (cl.worldmodel->extra.brush, transform);
}

/*
================
R_DrawWaterTextureChains
================
*/
void
R_DrawWaterTextureChains (brushhdr_t *brush, qboolean transform)
{
	unsigned int	i;
	float			wateralpha = r_wateralpha->fvalue;
	chain_head_t	*chain;

	qglColor4f (1.0, 1.0, 1.0, wateralpha);

	for (i = 0; i < brush->numtextures; i++) {
		chain = brush->tex_chains[i];
		if (!chain || !(chain->flags & CHAIN_LIQUID)
				|| (chain->visframe != r_framecount))
			continue;

		R_Draw_Liquid_Chain (chain, 0, transform);
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
	Uint			i;
	vec3_t			mins, maxs;
	brushhdr_t		*brush;
	entity_t		*e;

	R_Draw_Depth_Sky_Chain (&cl.worldmodel->extra.brush->sky_chain, modelorg);

	for (i = 0; i < r_refdef.num_entities; i++)
	{
		e = r_refdef.entities[i];
		brush = e->model->extra.brush;
	
		if (e->model->type == mod_brush)
		{
			Mod_MinsMaxs (e->model, e->cur.origin, e->cur.angles, mins, maxs);

			if (R_CullBox (mins, maxs))
				return;

			qglPushMatrix ();
			qglTranslatef(e->cur.origin[0], e->cur.origin[1], e->cur.origin[2]);
			qglRotatef (e->cur.angles[1], 0, 0, 1);
			qglRotatef (e->cur.angles[0], 0, 1, 0);
			qglRotatef (e->cur.angles[2], 1, 0, 0);

			R_Draw_Depth_Sky_Chain (&brush->sky_chain, modelorg);

			qglPopMatrix ();
		}
	}
}


/*
=================
R_VisBrushModel
=================
*/
void
R_VisBrushModel (entity_t *e)
{
	Uint			i;
	vec3_t			mins, maxs;
	msurface_t		*psurf;
	float			dot;
	model_t			*clmodel = e->model;
	brushhdr_t		*brush = clmodel->extra.brush;

	Mod_MinsMaxs (clmodel, e->cur.origin, e->cur.angles, mins, maxs);

	if (R_CullBox (mins, maxs))
		return;

	softwaretransformforbrushentity (e->cur.origin, e->cur.angles);
	softwareuntransform(r_origin, modelorg);

	/*
	 * LordHavoc: decide which surfs are visible and update lightmaps, then
	 * render afterward
	 */
	for (i = 0, psurf = &brush->surfaces[brush->firstmodelsurface];
			i < brush->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		dot = PlaneDiff (modelorg, psurf->plane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
				|| (!(psurf->flags & SURF_PLANEBACK)
					&& (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = r_framecount;
			if (psurf->tex_chain) {
				psurf->tex_chain->visframe = r_framecount;
				psurf->tex_chain->head->visframe = r_framecount;
			}
			if (psurf->light_chain) {
				psurf->light_chain->visframe = r_framecount;
				psurf->light_chain->head->visframe = r_framecount;
			}
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
	Uint			k;
	vec3_t			mins, maxs;
	model_t			*clmodel = e->model;
	brushhdr_t		*brush = clmodel->extra.brush;

	Mod_MinsMaxs (clmodel, e->cur.origin, e->cur.angles, mins, maxs);

	if (R_CullBox (mins, maxs))
		return;

	softwaretransformforbrushentity (e->cur.origin, e->cur.angles);
	softwareuntransform(r_origin, modelorg);

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (brush->firstmodelsurface && !gl_flashblend->ivalue)
		for (k = 0; k < r_numdlights; k++)
			R_MarkLightsNoVis (&r_dlight[k], 1 << k,
					brush->nodes + brush->hulls[0].firstclipnode);

	qglPushMatrix ();

	qglTranslatef (e->cur.origin[0], e->cur.origin[1], e->cur.origin[2]);

	qglRotatef (e->cur.angles[1], 0, 0, 1);
	qglRotatef (e->cur.angles[0], 0, 1, 0);
	qglRotatef (e->cur.angles[2], 1, 0, 0);

	// for transpoly water
	softwaretransformforbrushentity (e->cur.origin, e->cur.angles);

	DrawTextureChains (brush, true);

	R_RenderFullbrights (brush);

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
	int				 c, side;
	mplane_t		*plane;
	msurface_t		*surf, **mark;
	mleaf_t			*pleaf;
	double			 dot;

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

		for (c = 0; c < pleaf->nummarksurfaces; c++)
			(mark[c])->visframe = r_framecount;

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
	if (node->numsurfaces) {
		surf = cl.worldmodel->extra.brush->surfaces + node->firstsurface;
		side = (dot < 0) ? SURF_PLANEBACK : 0;

		for (c = 0; c < node->numsurfaces; c++) {
			if (surf[c].visframe == r_framecount) {
				if (side ^ (surf[c].flags & SURF_PLANEBACK))
					continue;       // wrong side

				if (surf[c].tex_chain) {
					surf[c].tex_chain->visframe = r_framecount;
					surf[c].tex_chain->head->visframe = r_framecount;
				}
				if (surf[c].light_chain) {
					surf[c].light_chain->visframe = r_framecount;
					surf[c].light_chain->head->visframe = r_framecount;
				}
			}
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
		for (i = 0; i < cl.worldmodel->extra.brush->numleafs; i++)
		{
			node = (mnode_t *) &cl.worldmodel->extra.brush->leafs[i + 1];
			do {
				if (node->pvsframe == r_pvsframecount)
					break;
				node->pvsframe = r_pvsframecount;
				node = node->parent;
			} while (node);
		}
	} else {
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		for (i = 0; i < cl.worldmodel->extra.brush->numleafs; i++)
		{
			if (vis[i >> 3] & (1 << (i & 7)))
			{
				node = (mnode_t *) &cl.worldmodel->extra.brush->leafs[i + 1];
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
R_VisWorld
=============
*/
void
R_VisWorld (void)
{
	VectorCopy (r_origin, modelorg);

	R_MarkLeaves ();
	R_RecursiveWorldNode (cl.worldmodel->extra.brush->nodes);
}

/*
=============
R_DrawWorld
=============
*/
void
R_DrawWorld (void)
{
	R_PushDlights ();

	DrawTextureChains (cl.worldmodel->extra.brush, false);

	R_RenderFullbrights (cl.worldmodel->extra.brush);
}
