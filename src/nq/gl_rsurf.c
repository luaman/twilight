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
#include "sky.h"
#include "liquid.h"

extern int lightmap_bytes;				// 1, 3, or 4
extern int lightmap_shift;

extern Uint8 templight[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 4];
static Uint32 blocklights[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 3];

static int dlightdivtable[32768];

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
R_AddDynamicLights (msurface_t *surf, matrix4x4_t *invmatrix)
{
	int				i, lnum, lit;
	int				s, t, td, smax, tmax, smax3;
	int				red, green, blue;
	int				dist2, maxdist, maxdist2, maxdist3;
	int				impacts, impactt, subtract, k;
	int				sdtable[256];
	Uint			*bl;
	rdlight_t		*rd;
	float			dist;
	vec3_t			impact, local;

	lit = false;

	smax = surf->smax;
	tmax = surf->tmax;
	smax3 = smax * 3;

	for (lnum = 0; lnum < r_numdlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << (lnum & 31))))
			continue;                   // not lit by this light

		rd = &r_dlight[lnum];

		if (invmatrix)
			Matrix4x4_Transform(invmatrix, rd->origin, local);
		else
			VectorCopy (rd->origin, local);
		dist = PlaneDiff (local, surf->plane);
		
		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) rd->cullradius2 + LIGHTOFFSET;
		
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
		red = (rd->light[0] * (1.0/256.0));
		green = (rd->light[1] * (1.0/256.0));
		blue = (rd->light[2] * (1.0/256.0));
		subtract = (int) (rd->lightsubtract * 4194304.0f);
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
						k = dlightdivtable[(sdtable[s] + td) >> 7] - subtract;
						if (k > 0)
						{
							bl[0] += (red   * k);
							bl[1] += (green * k);
							bl[2] += (blue  * k);
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
	maxdist = (radius * radius) + 4096;

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

		surf = model->brush->surfaces + node->firstsurface;
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
	entity_common_t	*ent;
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
	R_StainNode(model->brush->nodes + model->hulls[0].firstclipnode,
			model, origin, radius, icolor);

	// look for embedded bmodels
	for (n = 1; n < MAX_EDICTS; n++)
	{
		ent = &cl_entities[n].common;
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			if (model->type == mod_brush)
			{
				Matrix4x4_Transform(&ent->invmatrix, origin, org);
				R_StainNode(model->brush->nodes + model->hulls[0].firstclipnode, model, org, radius, icolor);
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
GL_BuildLightmap (model_t *mod, msurface_t *surf, matrix4x4_t *invmatrix)
{
	int			 i, j, size3, stride;
	Uint8		*lightmap, *dest, *stain;
	Uint32		 scale, *bl, l32;
	brushhdr_t	*brush = mod->brush;

	// Bind your textures early and often - or at least early
	qglBindTexture (GL_TEXTURE_2D, brush->lightblock.chains[surf->lightmap_texnum].l_texnum);

	// Reset stuff here
	surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
	surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
	surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
	surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

	size3 = surf->smax * surf->tmax * 3;

	lightmap = surf->samples;

	// set to full bright if no light data
	if (!brush->lightdata)
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
			if (R_AddDynamicLights (surf, invmatrix))
				surf->cached_dlight = 1;

		// add all the lightmaps
		if (lightmap)
		{
			for (i = 0; i < MAXLIGHTMAPS && surf->styles[i] != 255; i++)
			{
				scale = d_lightstylevalue[surf->styles[i]];
				bl = blocklights;

				j = 0;
				for (; (j + 4) <= size3; j += 4) {
					l32 = *((Uint32 *) &lightmap[j]);
					l32 = LittleLong(l32);
					bl[j + 0] += ((l32 >> 0) & 0xFF) * scale;
					bl[j + 1] += ((l32 >> 8) & 0xFF) * scale;
					bl[j + 2] += ((l32 >> 16) & 0xFF) * scale;
					bl[j + 3] += ((l32 >> 24) & 0xFF) * scale;
				}
				for (; j < size3; j++)
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
					l32 = min ((bl[0] * stain[0]) >> lightmap_shift, 255);
					l32 |= min ((bl[1] * stain[1]) >> lightmap_shift, 255)<< 8;
					l32 |= min ((bl[2] * stain[2]) >> lightmap_shift, 255)<< 16;
					l32 |= 255 << 24;
					*((Uint32 *) dest) = l32;
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
GL_UpdateLightmap (model_t *mod, msurface_t *fa, matrix4x4_t *invmatrix)
{
	if (!r_dynamic->ivalue)
		return;

	if (fa->dlightframe == r_framecount // dynamic lighting
			|| fa->cached_dlight // previously lit
			|| d_lightstylevalue[fa->styles[0]] != fa->cached_light[0]
			|| d_lightstylevalue[fa->styles[1]] != fa->cached_light[1]
			|| d_lightstylevalue[fa->styles[2]] != fa->cached_light[2]
			|| d_lightstylevalue[fa->styles[3]] != fa->cached_light[3])
		GL_BuildLightmap(mod, fa, invmatrix);
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
static texture_t *
R_TextureAnimation (texture_t *base, int frame)
{
	int			relative;
	int			count;

	if (frame && base->alt_anims)
		base = base->alt_anims;

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

void
R_DrawBrushDepthSkies (void)
{
	int			 i;
	vec3_t		 mins, maxs, org;
	brushhdr_t	*brush;
	entity_common_t	*e;

	Sky_Depth_Draw_Chain (cl.worldmodel, &cl.worldmodel->brush->sky_chain);

	if (!r_drawentities->ivalue)
		return;

	for (i = 0; i < r_refdef.num_entities; i++)
	{
		e = r_refdef.entities[i];
		brush = e->model->brush;

		if (e->model->type == mod_brush)
		{
			Mod_MinsMaxs (e->model, e->origin, e->angles, mins, maxs);
			if (Vis_CullBox (mins, maxs))
				continue;

			Matrix4x4_Transform(&e->invmatrix, r_origin, org);

			qglPushMatrix ();
			qglMultTransposeMatrixf ((GLfloat *) &e->matrix);

			Sky_Depth_Draw_Chain (e->model, &brush->sky_chain);

			qglPopMatrix ();
		}
	}
}

/*
================
R_RenderBrushPolys
================
*/

static inline void
R_RenderBrushPolys (glpoly_t *p)
{
	for (;p; p = p->next)
	{
		c_brush_polys++;
		qglDrawArrays (GL_POLYGON, p->start, p->numverts);
	}
}

void
R_DrawLiquidTextureChains (model_t *mod, qboolean arranged)
{
	Uint			 i;
	brushhdr_t		*brush = mod->brush;
	chain_head_t	*chain;

	for (i = 0; i < brush->numtextures; i++)
	{
		chain = &brush->tex_chains[i];
		if (chain->visframe != vis_framecount
				|| !(chain->flags & CHAIN_LIQUID))
			continue;

		R_Draw_Liquid_Chain (mod, chain, arranged);
	}
}

/*
================
R_DrawTextureChains
================
*/
void
R_DrawTextureChains (model_t *mod, int frame,
		matrix4x4_t *matrix, matrix4x4_t *invmatrix)
{
	Uint			 i, j;
	texture_t		*st;
	chain_head_t	*chain;
	chain_item_t	*c;
	brushhdr_t		*brush = mod->brush;

	if (gl_vbo) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, brush->vbo_objects[VBO_VERTS]);
		qglVertexPointer (3, GL_FLOAT, 0, 0);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, brush->vbo_objects[VBO_TC0]);
		qglTexCoordPointer (2, GL_FLOAT, 0, 0);
	} else {
		qglVertexPointer (3, GL_FLOAT, 0, brush->verts);
		qglTexCoordPointer (2, GL_FLOAT, 0, brush->tcoords[0]);
	}
	
	if (matrix) {
		qglPushMatrix ();

		qglMultTransposeMatrixf ((GLfloat *) matrix);
	}

	// LordHavoc: upload lightmaps early
	for (i = 0; i < brush->lightblock.num; i++)
	{
		chain = &brush->lightblock.chains[i];
		if (chain->visframe != vis_framecount)
			continue;

		for (j = 0; j < chain->n_items; j++)
		{
			if (chain->items[j].visframe != vis_framecount)
				continue;
			GL_UpdateLightmap (mod, chain->items[j].surf, invmatrix);
		}
	}

	if (sky_type == SKY_FAST && brush->sky_chain.visframe == vis_framecount)
		Sky_Fast_Draw_Chain (mod, &brush->sky_chain);

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

		if (gl_vbo) {
			qglClientActiveTextureARB(GL_TEXTURE1_ARB);
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, brush->vbo_objects[VBO_TC1]);
			qglTexCoordPointer (2, GL_FLOAT, 0, 0);
			qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		} else {
			qglClientActiveTextureARB(GL_TEXTURE1_ARB);
			qglTexCoordPointer (2, GL_FLOAT, 0, brush->tcoords[1]);
			qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		}

		for (i = 0; i < brush->numtextures; i++)
		{
			chain = &brush->tex_chains[i];
			if ((chain->visframe != vis_framecount)
					|| !(chain->flags & CHAIN_NORMAL))
				continue;
			st = R_TextureAnimation (chain->texture, frame);
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			qglActiveTextureARB (GL_TEXTURE1_ARB);

			for (j = 0; j < chain->n_items; j++)
			{
				c = &chain->items[j];
				if (c->visframe != vis_framecount)
					continue;
				qglBindTexture (GL_TEXTURE_2D, brush->lightblock.chains[c->surf->lightmap_texnum].l_texnum);
				R_RenderBrushPolys (c->surf->polys);
			}
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

		for (i = 0; i < brush->numtextures; i++)
		{
			chain = &brush->tex_chains[i];
			if ((chain->visframe != vis_framecount)
					|| !(chain->flags & CHAIN_NORMAL))
				continue;
			st = R_TextureAnimation (chain->texture, frame);
			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);

			for (j = 0; j < chain->n_items; j++)
			{
				c = &chain->items[j];
				if (c->visframe != vis_framecount)
					continue;
				R_RenderBrushPolys (c->surf->polys);
			}
		}

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		// don't bother writing Z
		qglDepthMask (GL_FALSE);

		qglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);


		if (gl_vbo) {
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, brush->vbo_objects[VBO_TC1]);
			qglTexCoordPointer (2, GL_FLOAT, 0, 0);
		} else
			qglTexCoordPointer (2, GL_FLOAT, 0, brush->tcoords[1]);

		qglEnable (GL_BLEND);
		for (i = 0; i < brush->lightblock.num; i++)
		{
			chain = &brush->lightblock.chains[i];
			if (chain->visframe != vis_framecount)
				continue;

			for (j = 0; j < chain->n_items; j++)
			{
				if (chain->items[j].visframe != vis_framecount)
					continue;
				qglBindTexture (GL_TEXTURE_2D, chain->l_texnum);
				R_RenderBrushPolys (chain->items[j].surf->polys);
			}
		}

		if (gl_vbo) {
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, brush->vbo_objects[VBO_TC0]);
			qglTexCoordPointer (2, GL_FLOAT, 0, 0);
		} else
			qglTexCoordPointer (2, GL_FLOAT, 0, brush->tcoords[0]);

		qglDisable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// back to normal Z buffering
		qglDepthMask (GL_TRUE);
	}

	// Draw the fullbrights, if there are any
	if (gl_fb->ivalue)
	{
		qglDepthMask (GL_FALSE);	// don't bother writing Z
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

		for (i = 0; i < brush->numtextures; i++)
		{
			chain = &brush->tex_chains[i];
			if ((chain->visframe != vis_framecount)
					|| !(chain->flags & CHAIN_NORMAL))
				continue;
			st = R_TextureAnimation (chain->texture, frame);
			if (!st->fb_texturenum)
				continue;
			qglBindTexture (GL_TEXTURE_2D, st->fb_texturenum);

			for (j = 0; j < chain->n_items; j++)
			{
				c = &chain->items[j];
				if (c->visframe != vis_framecount)
					continue;
				R_RenderBrushPolys (c->surf->polys);
			}
		}

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_BLEND);
		qglDepthMask (GL_TRUE);
	}

	// If the water is solid, draw here, if not, then later.
	if (r_wateralpha->fvalue == 1)
		R_DrawLiquidTextureChains (mod, true);

	if (gl_vbo)
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	GLArrays_Reset_Vertex ();
	GLArrays_Reset_TC ();

	if (matrix)
		qglPopMatrix ();
}

/*
=================
R_VisBrushModel
=================
*/
void
R_VisBrushModel (entity_common_t *e)
{
	Uint			 i;
	msurface_t		*psurf;
	float			 dot;
	model_t			*mod = e->model;
	brushhdr_t		*brush = mod->brush;
	vec3_t			 org;

	Matrix4x4_Transform(&e->invmatrix, r_origin, org);
		
	/*
	 * LordHavoc: decide which surfs are visible and update lightmaps, then
	 * render afterward
	 */
	for (i = 0, psurf = &brush->surfaces[brush->firstmodelsurface];
			i < brush->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		dot = PlaneDiff (org, psurf->plane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
				|| (!(psurf->flags & SURF_PLANEBACK)
					&& (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = vis_framecount;
			if (psurf->tex_chain)
			{
				psurf->tex_chain->visframe = vis_framecount;
				psurf->tex_chain->head->visframe = vis_framecount;
			}
			if (psurf->light_chain)
			{
				psurf->light_chain->visframe = vis_framecount;
				psurf->light_chain->head->visframe = vis_framecount;
			}
		}
	}
}

/*
=================
R_DrawOpaqueBrushModel
=================
*/
void
R_DrawOpaqueBrushModel (entity_common_t *e)
{
	int				 k;
	model_t			*mod = e->model;
	vec3_t			lightorigin;

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (mod->brush->firstmodelsurface != 0 && !gl_flashblend->ivalue)
	{
		for (k = 0; k < r_numdlights; k++) {
			Matrix4x4_Transform(&e->invmatrix, r_dlight[k].origin, lightorigin);
			R_MarkLightsNoVis (lightorigin, &r_dlight[k], 1 << k, mod,
					mod->brush->nodes + mod->hulls[0].firstclipnode);
		}
	}

	R_DrawTextureChains(mod, e->frame[0], &e->matrix, &e->invmatrix);
}

/*
=================
R_DrawAddBrushModel
=================
*/
void
R_DrawAddBrushModel (entity_common_t *e)
{
	model_t			*mod = e->model;

	qglPushMatrix ();

	qglMultTransposeMatrixf ((GLfloat *) &e->matrix);

	R_DrawLiquidTextureChains (mod, false);

	qglPopMatrix ();
}
