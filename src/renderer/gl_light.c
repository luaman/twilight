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

#include <math.h>

#include "quakedef.h"
#include "collision.h"
#include "cvar.h"
#include "dlight.h"
#include "textures.h"
#include "image.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "strlib.h"
#include "view.h"
#include "gl_info.h"
#include "gl_arrays.h"
#include "vis.h"
#include "gl_light.h"
#include "sys.h"
#include "surface.h"
#include "gl_main.h"
#include "gen_textures.h"

static Uint32 blocklights[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 3];

void
R_DrawCoronas (void)
{
	Uint		i;
	float		scale, viewdist, dist;
	vec4_t		brightness;
	vec3_t		diff;
	rdlight_t	*rd;
	GTF_texture_t	*tex = &GTF_texture[GTF_corona];

	qglDisable (GL_DEPTH_TEST);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
	qglBindTexture (GL_TEXTURE_2D, GTF_texnum);
	qglEnableClientState (GL_COLOR_ARRAY);

	VectorSet2 (tc_array_v(0), tex->s1, tex->t1);
	VectorSet2 (tc_array_v(1), tex->s1, tex->t2);
	VectorSet2 (tc_array_v(2), tex->s2, tex->t2);
	VectorSet2 (tc_array_v(3), tex->s2, tex->t1);
	viewdist = DotProduct (r.origin, r.vpn);

	// Need brighter coronas if we don't have dynamic lightmaps
	if (gl_flashblend->ivalue)
		VectorSet4 (brightness, 1.0f / 131072.0f, 1.0f / 131072.0f,
				1.0f / 131072.0f, 1.0f);
	else
		VectorSet4 (brightness, 1.0f / 262144.0f, 1.0f / 262144.0f,
				1.0f / 262144.0f, 1.0f);

	for (i = 0; i < r.numdlights; i++)
	{
		rd = r.dlight + i;
		dist = (DotProduct (rd->origin, r.vpn) - viewdist);
		if (dist >= 24.0f)
		{
			// trace to a point just barely closer to the eye
			VectorSubtract(rd->origin, r.vpn, diff);
			if (TraceLine (r.worldmodel, r.origin, diff, NULL, NULL) == 1)
			{
				TWI_FtoUBMod (rd->light, c_array_v(0), brightness, 4);
				VectorCopy4 (c_array_v(0), c_array_v(1));
				VectorCopy4 (c_array_v(0), c_array_v(2));
				VectorCopy4 (c_array_v(0), c_array_v(3));

				scale = rd->cullradius * 0.25f;
				VectorTwiddle (rd->origin, r.vright, -1, r.vup, -1, scale,
						v_array_v(0));
				VectorTwiddle (rd->origin, r.vright, -1, r.vup, 1, scale,
						v_array_v(1));
				VectorTwiddle (rd->origin, r.vright, 1, r.vup, 1, scale,
						v_array_v(2));
				VectorTwiddle (rd->origin, r.vright, 1, r.vup, -1, scale,
						v_array_v(3));
				TWI_PreVDraw (0, 4);
				qglDrawArrays (GL_QUADS, 0, 4);
				TWI_PostVDraw ();			
			}
		}
	}

	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor4fv (whitev);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglEnable (GL_DEPTH_TEST);
}

void
R_AnimateLight (void)
{
	int         i, j, k;

//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int) (ccl.time * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++) {
		if (!ccl.lightstyles[j].length) {
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % ccl.lightstyles[j].length;
		k = ccl.lightstyles[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}


/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

static float       bubble_sintable[17], bubble_costable[17];

int dlightdivtable[32768];

void
GL_Light_Tables_Init (void)
{
	float       a;
	int         i;
	float       *bub_sin = bubble_sintable,
				*bub_cos = bubble_costable;

	// additional accuracy here
	// NOTE: We REALLY want to use the real sin and cos in this case.
	for (i = 16; i >= 0; i--) {
		a = i * (M_PI / 8.0);
		*bub_sin++ = sin (a);
		*bub_cos++ = cos (a);
	}

	dlightdivtable[0] = 4194304;
	for (i = 1;i < 32768;i++)
		dlightdivtable[i] = 4194304 / (i << 7);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

void
R_MarkLightsNoVis (vec3_t lightorigin, rdlight_t *rd,
		int bit, model_t *mod, mnode_t *node)
{
	float ndist, maxdist;
	msurface_t *surf;
	int i;
	int d, impacts, impactt;
	float dist, dist2, impact[3];

	/* for comparisons to minimum acceptable light */
	maxdist = rd->cullradius2;

loc0:
	if (node->contents < 0)
		return;

	ndist = PlaneDiff(lightorigin, node->plane);

	if (ndist > rd->cullradius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -rd->cullradius)
	{
		node = node->children[1];
		goto loc0;
	}

	/* mark the polygons */
	surf = mod->brush->surfaces + node->firstsurface;
	for (i = 0;i < node->numsurfaces;i++, surf++)
	{
		if (surf->visframe != vis_framecount)
			continue;
		dist = ndist;
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;

		if (dist < -0.25f)
			continue;

		dist2 = dist * dist;
		if (dist2 >= maxdist)
			continue;

		if (node->plane->type < 3)
		{
			VectorCopy(lightorigin, impact);
			impact[node->plane->type] -= dist;
		}
		else
		{
			impact[0] = lightorigin[0] - surf->plane->normal[0] * dist;
			impact[1] = lightorigin[1] - surf->plane->normal[1] * dist;
			impact[2] = lightorigin[2] - surf->plane->normal[2] * dist;
		}

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];

		d = bound(0, impacts, surf->extents[0] + 16) - impacts;
		dist2 += d * d;
		if (dist2 > maxdist)
			continue;

		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		d = bound(0, impactt, surf->extents[1] + 16) - impactt;
		dist2 += d * d;
		if (dist2 > maxdist)
			continue;

		if (surf->dlightframe != r.framecount)
		{ /* not dynamic until now */
			surf->dlightbits = 0;
			surf->dlightframe = r.framecount;
		}
		surf->dlightbits |= bit;
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_MarkLightsNoVis (lightorigin, rd, bit, mod, node->children[0]);
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

void
R_MarkLights (rdlight_t *rd, int bit, model_t *model, matrix4x4_t *invmatrix)
{
	mleaf_t		*pvsleaf;
	vec3_t		lightorigin;
	Uint		leafnum;
	int			i, k, m, c;
	msurface_t	*surf, **mark;
	mleaf_t		*leaf;
	Uint8		*in;
	int			row;
	float		low[3], high[3], dist, maxdist;
	static Uint	lightframe = 0;
	int			d, impacts, impactt;
	float		dist2, impact[3];

	if (invmatrix)
		Matrix4x4_Transform(invmatrix, rd->origin, lightorigin);
	else
		VectorCopy(rd->origin, lightorigin);
	lightframe++;

	pvsleaf = Mod_PointInLeaf (lightorigin, model);
	if (pvsleaf == NULL)
		return;
	in = pvsleaf->compressed_vis;
	if (!in || gl_oldlights->ivalue)
	{ /* told not to use pvs, or there's no pvs to use */
		R_MarkLightsNoVis(lightorigin, rd, bit, model, model->brush->nodes + model->hulls[0].firstclipnode);
		return;
	}

	VectorSlide(lightorigin, -rd->cullradius, low);
	VectorSlide(lightorigin, rd->cullradius, high);

	/* for comparisons to minimum acceptable light */
	maxdist = rd->cullradius2;

	row = (model->brush->numleafs+7)>>3;

	k = 0;
	while (k < row)
	{
		c = *in++;
		if (!c)
		{
			k += *in++;
			continue;
		}
		for (i = 0;i < 8;i++)
		{
			if (!(c & (1<<i)))
				continue;
			/* warning to the clumsy: numleafs is one less than it should
			 * be, it only counts leafs with vis bits (skips leaf 0) */
			leafnum = (k << 3)+i+1;
			if (leafnum > model->brush->numleafs)
				return;
			leaf = &model->brush->leafs[leafnum];
			if (leaf->mins[0] > high[0] || leaf->maxs[0] < low[0]
					|| leaf->mins[1] > high[1] || leaf->maxs[1] < low[1]
					|| leaf->mins[2] > high[2] || leaf->maxs[2] < low[2])
				continue;
			if ((m = leaf->nummarksurfaces))
			{
				mark = leaf->firstmarksurface;
				do
				{
					surf = *mark++;
					/* If not visible in current frame, or already marked
					 * because it was in another leaf we passed, skip. */
					if (surf->lightframe == lightframe)
						continue;
					surf->lightframe = lightframe;
					if (surf->visframe != vis_framecount)
						continue;
					dist = PlaneDiff(lightorigin, surf->plane);
					if (surf->flags & SURF_PLANEBACK)
						dist = -dist;
					/* LordHavoc: make sure it is infront of
					 * the surface and not too far away */
					if (dist >= rd->cullradius || (dist <= -0.25f))
						continue;

					dist2 = dist * dist;

					if (surf->plane->type < 3)
					{
						VectorCopy(lightorigin, impact);
						impact[surf->plane->type] -= dist;
					} else
						VectorMA(lightorigin, -dist, surf->plane->normal, impact);

					impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
					d = bound(0, impacts, surf->extents[0] + 16) - impacts;
					dist2 += d * d;
					if (dist2 > maxdist)
						continue;

					impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
					d = bound(0, impactt, surf->extents[1] + 16) - impactt;
					dist2 += d * d;
					if (dist2 > maxdist)
						continue;

					if (surf->dlightframe != r.framecount)
					{ /* not dynamic until now */
						surf->dlightbits = 0;
						surf->dlightframe = r.framecount;
					}
					surf->dlightbits |= bit;
				}
				while (--m);
			}
		}
		k++;
	}
}


void
R_PushDlights (void)
{
	Uint		i;
	rdlight_t	*l;

	if (gl_flashblend->ivalue)
		return;

	l = r.dlight;

	for (i = 0; i < r.numdlights; i++, l++)
		R_MarkLights (l, 1 << i, r.worldmodel, NULL);
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t   *lightplane;
vec3_t      lightspot;

static int
RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start,
		vec3_t end)
{
	float		front, back, frac;
	vec3_t		mid;

	if (node->contents < 0)
		// didn't hit anything
		return false;

	while (1)
	{
		if (node->contents < 0)
			// didn't hit anything
			return false;

		// calculate mid point
		front = PlaneDiff (start, node->plane);
		back = PlaneDiff (end, node->plane);

		if ((back < 0) == (front < 0))
		{
			node = node->children[front < 0];
			continue;
		}

		break;
	}
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
	// go down front side
	if (RecursiveLightPoint (color, node->children[front < 0], start, mid))
		// hit something
		return true;
	else
	{
		int i, ds, dt;
		msurface_t *surf;

		// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = node->plane;
		surf = r.worldmodel->brush->surfaces + node->firstsurface;

		for (i = 0; i < node->numsurfaces; i++, surf++)
		{
			if (!(surf->flags & SURF_LIGHTMAP))
				// no lightmaps
				continue;

			ds = (int) ((float) DotProduct (mid, surf->texinfo->vecs[0])
					+ surf->texinfo->vecs[0][3]);
			if (ds < surf->texturemins[0])
				continue;
			dt = (int) ((float) DotProduct (mid, surf->texinfo->vecs[1])
					+ surf->texinfo->vecs[1][3]);
			if (dt < surf->texturemins[1])
				continue;

			ds -= surf->texturemins[0];
			if (ds > surf->extents[0])
				continue;
			dt -= surf->texturemins[1];
			if (dt > surf->extents[1])
				continue;

			if (surf->samples)
			{
				Uint8 *lightmap;
				int maps, line3, dsfrac = ds & 15, 
					dtfrac = dt & 15, 
					r00 = 0, g00 = 0, b00 = 0, 
					r01 = 0, g01 = 0, b01 = 0, 
					r10 = 0, g10 = 0, b10 = 0, 
					r11 = 0, g11 = 0, b11 = 0;
				float scale;

				line3 = surf->smax*3;

				// LordHavoc: *3 for color
				lightmap = surf->samples + ((dt>>4)
						* surf->smax + (ds>>4))*3;
				for (maps = 0;
						maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
						maps++)
				{
					scale = (float)d_lightstylevalue[surf->styles[maps]]
						* 1.0 / 256.0;
					r00 += (float)lightmap[0] * scale;
					g00 += (float)lightmap[1] * scale;
					b00 += (float)lightmap[2] * scale;
					r01 += (float)lightmap[3] * scale;
					g01 += (float)lightmap[4] * scale;
					b01 += (float)lightmap[5] * scale;
					r10 += (float)lightmap[line3  ] * scale;
					g10 += (float)lightmap[line3+1] * scale;
					b10 += (float)lightmap[line3+2] * scale;
					r11 += (float)lightmap[line3+3] * scale;
					g11 += (float)lightmap[line3+4] * scale;
					b11 += (float)lightmap[line3+5] * scale;
					lightmap += surf->smax * surf->tmax *3;
				}

				/*
				// LordHavoc: here's the readable version of the interpolation
				// code, not quite as easy for the compiler to optimize...

				// dsfrac is the X position in the lightmap pixel, * 16
				// dtfrac is the Y position in the lightmap pixel, * 16
				// r00 is top left corner, r01 is top right corner
				// r10 is bottom left corner, r11 is bottom right corner
				// g and b are the same layout.
				// r0 and r1 are the top and bottom intermediate results

				// first we interpolate the top two points, to get the top
				// edge sample
				r0 = (((r01-r00) * dsfrac) >> 4) + r00;
				g0 = (((g01-g00) * dsfrac) >> 4) + g00;
				b0 = (((b01-b00) * dsfrac) >> 4) + b00;
				// then we interpolate the bottom two points, to get the
				// bottom edge sample
				r1 = (((r11-r10) * dsfrac) >> 4) + r10;
				g1 = (((g11-g10) * dsfrac) >> 4) + g10;
				b1 = (((b11-b10) * dsfrac) >> 4) + b10;
				// then we interpolate the top and bottom samples to get the
				// middle sample (the one which was requested)
				r = (((r1-r0) * dtfrac) >> 4) + r0;
				g = (((g1-g0) * dtfrac) >> 4) + g0;
				b = (((b1-b0) * dtfrac) >> 4) + b0;
				*/

				color[0] += (float)((int)((((((((r11-r10) * dsfrac) >> 4)
											+ r10)-((((r01-r00) * dsfrac) >> 4)
												+ r00)) * dtfrac) >> 4)
							+ ((((r01-r00) * dsfrac) >> 4) + r00)));
				color[1] += (float)((int)((((((((g11-g10) * dsfrac) >> 4)
											+ g10)-((((g01-g00) * dsfrac) >> 4)
												+ g00)) * dtfrac) >> 4)
							+ ((((g01-g00) * dsfrac) >> 4) + g00)));
				color[2] += (float)((int)((((((((b11-b10) * dsfrac) >> 4)
											+ b10)-((((b01-b00) * dsfrac) >> 4)
												+ b00)) * dtfrac) >> 4)
							+ ((((b01-b00) * dsfrac) >> 4) + b00)));
			}
			return true; // success
		}

		// go down back side
		return RecursiveLightPoint (color, node->children[front >= 0],
				mid, end);
	}
}

void
R_LightPoint (vec3_t p, vec3_t out)
{
	vec3_t end;

	if (!r.worldmodel->brush->lightdata)
	{
		out[0] = out[1] = out[2] = 1;
		return;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	out[0] = out[1] = out[2] = 0;

	RecursiveLightPoint (out, r.worldmodel->brush->nodes, p, end);

	return;
}


static int
R_AddDynamicLights (msurface_t *surf, matrix4x4_t *invmatrix)
{
	Uint			lnum;
	int				i, lit;
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

	for (lnum = 0; lnum < r.numdlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << (lnum & 31))))
			continue;                   // not lit by this light

		rd = &r.dlight[lnum];

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


/*
===============
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
		if (surf->dlightframe == r.framecount)
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
#if 1
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
#else
			{
				Uint16		max1[4] = {255, 255, 255, 255};

				for (i = 0; i < surf->tmax; i++, dest += stride)
				{
					for (j = 0; j < surf->smax; j++) {
						asm ("\n"
								"pxor					%%mm2, %%mm2\n"
								"movq					%1, %%mm0\n"
								"movq					%2, %%mm1\n"
								"psrld					$9, %%mm0\n"
								"psrld					$9, %%mm1\n"
								"packssdw				%%mm1, %%mm0\n"
								"pminsw					%4, %%mm0\n"
								"movd					%3, %%mm1\n"
								"punpcklbw				%%mm2, %%mm1\n"
								"pmullw					%%mm0, %%mm1\n"
								"psrlw					$8, %%mm1\n"
								"packuswb				%%mm2, %%mm1\n"
								"movd					%%mm1, %0\n"
								: "=m" (*(Uint32 *)dest) :
								"m" (*(Uint64 *)(&bl[0])),
								"m" (*(Uint64 *)(&bl[2])),
								"m" (*(Uint32 *)stain),
								"m" (*(Uint64 *)max1)
								: "mm0", "mm1", "mm2");
						bl += 3;
						dest += 3;
						stain += 3;
					}
				}
			}
			asm volatile ("emms\n");
#endif

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

void
GL_UpdateLightmap (model_t *mod, msurface_t *fa, matrix4x4_t *invmatrix)
{
	if (!r_dynamic->ivalue)
		return;

	if (fa->dlightframe == r.framecount // dynamic lighting
			|| fa->cached_dlight // previously lit
			|| d_lightstylevalue[fa->styles[0]] != fa->cached_light[0]
			|| d_lightstylevalue[fa->styles[1]] != fa->cached_light[1]
			|| d_lightstylevalue[fa->styles[2]] != fa->cached_light[2]
			|| d_lightstylevalue[fa->styles[3]] != fa->cached_light[3])
		GL_BuildLightmap(mod, fa, invmatrix);
}


void
R_BuildLightList (void)
{
	int i;
	dlight_t *cd;
	rdlight_t *rd;

	r.numdlights = 0;

	if (!r_dynamic->ivalue)
		return;

	for (i = 0; i < MAX_DLIGHTS; i++)
	{
		cd = ccl.dlights + i;
		if (cd->radius <= 0 || cd->die < ccl.time)
			continue;

		rd = &r.dlight[r.numdlights++];
		VectorCopy (cd->origin, rd->origin);
		VectorScale (cd->color, cd->radius * 128.0f, rd->light);
		rd->light[3] = 1.0f;
		rd->cullradius = (1.0f / 128.0f) * VectorLength(rd->light);
		rd->cullradius = max(2048.0f, rd->cullradius);  // avoid overflow

		rd->cullradius2 = rd->cullradius * rd->cullradius;
		rd->lightsubtract = 1.0f / rd->cullradius2;
	}
}

