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
#include "client.h"
#include "collision.h"
#include "cvar.h"
#include "draw.h"
#include "gl_textures.h"
#include "image.h"
#include "light.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "strlib.h"
#include "view.h"

rdlight_t r_dlight[MAX_DLIGHTS];
int r_numdlights = 0;

static int corona_texture;

void
R_InitLightTextures (void)
{
	float		dx, dy;
	int			x, y, a;
	Uint8		pixels[32][32][4];
	image_t		img;
	
	for (y = 0; y < 32; y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0; x < 32; x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = ((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / 1.2f))
				* 32.0f / (1.0f / (1.0f + 0.2));
			a = bound(0, a, 255);
			pixels[y][x][0] = 255;
			pixels[y][x][1] = 255;
			pixels[y][x][2] = 255;
			pixels[y][x][3] = a;
		}
	}
	img.width = 32;
	img.height = 32;
	img.type = IMG_RGBA;
	img.pixels = (Uint8 *)pixels;
	corona_texture = R_LoadTexture ("dlcorona", &img, NULL, TEX_ALPHA);
}

void
R_DrawCoronas (void)
{
	int			i;
	float		scale, viewdist, dist;
	vec4_t		brightness;
	vec3_t		diff;
	rdlight_t	*rd;

	qglDisable (GL_DEPTH_TEST);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
	qglBindTexture (GL_TEXTURE_2D, corona_texture);
	qglEnableClientState (GL_COLOR_ARRAY);

	VectorSet2 (tc_array_v(0), 0.0f, 0.0f);
	VectorSet2 (tc_array_v(1), 0.0f, 1.0f);
	VectorSet2 (tc_array_v(2), 1.0f, 1.0f);
	VectorSet2 (tc_array_v(3), 1.0f, 0.0f);
	viewdist = DotProduct (r_origin, vpn);

	// Need brighter coronas if we don't have dynamic lightmaps
	if (gl_flashblend->ivalue)
		VectorSet4 (brightness, 1.0f / 131072.0f, 1.0f / 131072.0f,
				1.0f / 131072.0f, 1.0f);
	else
		VectorSet4 (brightness, 1.0f / 262144.0f, 1.0f / 262144.0f,
				1.0f / 262144.0f, 1.0f);

	for (i = 0; i < r_numdlights; i++)
	{
		rd = r_dlight + i;
		dist = (DotProduct (rd->origin, vpn) - viewdist);
		if (dist >= 24.0f)
		{
			// trace to a point just barely closer to the eye
			VectorSubtract(rd->origin, vpn, diff);
			if (TraceLine (cl.worldmodel, r_origin, diff, NULL, NULL) == 1)
			{
				TWI_FtoUBMod (rd->light, c_array_v(0), brightness, 4);
				VectorCopy4 (c_array_v(0), c_array_v(1));
				VectorCopy4 (c_array_v(0), c_array_v(2));
				VectorCopy4 (c_array_v(0), c_array_v(3));

				scale = rd->cullradius * 0.25f;
				VectorTwiddle (rd->origin, vright, -1, vup, -1, scale,
						v_array_v(0));
				VectorTwiddle (rd->origin, vright, -1, vup, 1, scale,
						v_array_v(1));
				VectorTwiddle (rd->origin, vright, 1, vup, 1, scale,
						v_array_v(2));
				VectorTwiddle (rd->origin, vright, 1, vup, -1, scale,
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

/*
==================
R_AnimateLight
==================
*/
void
R_AnimateLight (void)
{
	int         i, j, k;

//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int) (cl.time * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++) {
		if (!cl_lightstyle[j].length) {
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}


/*
==================
R_BuildLightList
==================
*/
void
R_BuildLightList (void)
{
	int			i;
	dlight_t	*cd;
	rdlight_t	*rd;

	r_numdlights = 0;

	if (!r_dynamic->ivalue)
		return;

	for (i = 0; i < MAX_DLIGHTS; i++)
	{
		cd = cl_dlights + i;
		if (cd->radius <= 0 || cd->die < cl.time)
			continue;

		rd = &r_dlight[r_numdlights++];
		VectorCopy (cd->origin, rd->origin);
		VectorScale (cd->color, cd->radius * 128.0f, rd->light);
		rd->light[3] = 1.0f;
		rd->cullradius = (1.0f / 128.0f) * sqrt (DotProduct (rd->light, rd->light));

		// clamp radius to avoid overflowing division table in lightmap code
		if (rd->cullradius > 2048.0f)
			rd->cullradius = 2048.0f;

		rd->cullradius2 = rd->cullradius * rd->cullradius;
		rd->lightsubtract = 1.0f / rd->cullradius2;
	}
}


/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void
AddLightBlend (vec3_t v, float a2)
{
	float       a;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	a2 = a2 / a;
	a = 1 - a2;

	v_blend[0] = v_blend[0] * a + v[0] * a2;
	v_blend[1] = v_blend[1] * a + v[1] * a2;
	v_blend[2] = v_blend[2] * a + v[2] * a2;
}

float       bubble_sintable[17], bubble_costable[17];

void
R_InitBubble (void)
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
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLightsNoVis
=============
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

		if (surf->dlightframe != r_framecount)
		{ /* not dynamic until now */
			surf->dlightbits = 0;
			surf->dlightframe = r_framecount;
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

/*
=============
R_MarkLights
=============
*/
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

					if (surf->dlightframe != r_framecount)
					{ /* not dynamic until now */
						surf->dlightbits = 0;
						surf->dlightframe = r_framecount;
					}
					surf->dlightbits |= bit;
				}
				while (--m);
			}
		}
		k++;
	}
}


/*
=============
R_PushDlights
=============
*/
void
R_PushDlights (void)
{
	int			i;
	rdlight_t	*l;

	if (gl_flashblend->ivalue)
		return;

	l = r_dlight;

	for (i = 0; i < r_numdlights; i++, l++)
		R_MarkLights (l, 1 << i, cl.worldmodel, NULL);
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t   *lightplane;
vec3_t      lightspot;

int
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
		surf = cl.worldmodel->brush->surfaces + node->firstsurface;

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

vec3_t lightcolor;

int R_LightPoint (vec3_t p)
{
	vec3_t end;

	if (!cl.worldmodel->brush->lightdata)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
		return 255;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

	RecursiveLightPoint (lightcolor, cl.worldmodel->brush->nodes, p, end);

	return 255;
}

