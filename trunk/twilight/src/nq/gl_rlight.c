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
#include "cvar.h"
#include "glquake.h"
#include "light.h"
#include "mathlib.h"
#include "strlib.h"
#include "view.h"

rdlight_t r_dlight[MAX_DLIGHTS];
int r_numdlights = 0;
int r_dlightframecount;

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
		rd->cullradius = (1.0f / 128.0f) * sqrt (DotProduct (rd->light, rd->light));

		// clamp radius to avoid overflowing division table in lightmap code
		if (rd->cullradius > 2048.0f)
			rd->cullradius = 2048.0f;

		rd->cullradius2 = rd->cullradius * rd->cullradius;
		rd->lightsubtract = 1.0f / rd->cullradius2;
		r_numdlights++;
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


void
R_RenderDlight (rdlight_t *light)
{
	int     i, j, vcenter, vlast = -1;
	vec3_t  v, v_right, v_up, c;
	float	*bub_sin = bubble_sintable,
			*bub_cos = bubble_costable;
	float   rad = light->cullradius * 0.35, length;

	VectorSubtract (light->origin, r_origin, v);
	length = VectorNormalize (v);

	VectorCopy (light->light, c);
	VectorNormalizeFast (c);

	if (length < rad)
	{
		// view is inside the dlight
		AddLightBlend (c, light->cullradius * 0.0003);
		return;
	}

	v_right[0] = v[1];
	v_right[1] = -v[0];
	v_right[2] = 0;
	VectorNormalizeFast (v_right);
	CrossProduct (v_right, v, v_up);

	if (length - rad > 8)
		VectorScale (v, rad, v);
	else {
		// make sure the light bubble will not be clipped by
		// near z clip plane
		VectorScale (v, length-8, v);
	}
	VectorSubtract (light->origin, v, v);

	VectorSet3 (v_array_v(v_index), v[0], v[1], v[2]);
	c_array(v_index, 0) = c[0];
	c_array(v_index, 1) = c[1];
	c_array(v_index, 2) = c[2];
	c_array(v_index, 3) = 1.0;
	vcenter = v_index;
	v_index++;

	for (i = 16; i >= 0; i--, bub_sin++, bub_cos++) 
	{
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (v_right[j] * (*bub_cos) +
				+ v_up[j] * (*bub_sin)) * rad;

		VectorSet4 (c_array_v(v_index), 0.0, 0.0, 0.0, 0.0);
		VectorSet3 (v_array_v(v_index), v[0], v[1], v[2]);
		if (vlast != -1) {
			vindices[i_index + 0] = vcenter;
			vindices[i_index + 1] = vlast;
			vindices[i_index + 2] = v_index;
			i_index += 3;
		}
		vlast = v_index;
		v_index++;

		if (((v_index + 3) >= MAX_VERTEX_ARRAYS) ||
				((i_index + 3) >= MAX_VERTEX_INDICES)) {
			TWI_PreVDrawCVA (0, v_index);
			qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
			TWI_PostVDrawCVA ();
			v_index = 0;
			i_index = 0;
			memcpy(v_array_v(v_index), v_array_v(vcenter),sizeof(v_array_v(0)));
			memcpy(c_array_v(v_index), c_array_v(vcenter),sizeof(c_array_v(0)));
			vcenter = v_index++;
			memcpy(v_array_v(v_index), v_array_v(vlast), sizeof(v_array_v(0)));
			memcpy(c_array_v(v_index), c_array_v(vlast), sizeof(c_array_v(0)));
			vlast = v_index++;
		}
	}
}

/*
=============
R_RenderDlights
=============
*/
void
R_RenderDlights (void)
{
	int				i;
	rdlight_t		*l;

	if (!gl_flashblend->ivalue)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnableClientState (GL_COLOR_ARRAY);

	l = r_dlight;
	for (i = 0; i < r_numdlights; i++, l++)
		R_RenderDlight (l);

	if (v_index || i_index) {
		TWI_PreVDrawCVA (0, v_index);
		qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}

	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
	qglDisableClientState (GL_COLOR_ARRAY);
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
R_MarkLightsNoVis (rdlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	float		l, maxdist;
	int			j, s, t;
	vec3_t		impact;

	while (node->contents >= 0)
	{
		splitplane = node->plane;
		dist = PlaneDiff (light->origin, splitplane);

		if (dist > light->cullradius)
		{
			node = node->children[0];
			continue;
		}

		if (dist < -light->cullradius)
		{
			node = node->children[1];
			continue;
		}

		// mark the polygons
		maxdist = light->cullradius2;
		surf = cl.worldmodel->surfaces + node->firstsurface;

		for (i = 0; i < node->numsurfaces; i++, surf++)
		{
			if (surf->visframe != r_framecount)
				continue;
			for (j = 0; j < 3; j++)
				impact[j] = light->origin[j] - surf->plane->normal[j]*dist;

			// clamp center of light to corner and check brightness
			l = DotProduct (impact, surf->texinfo->vecs[0])
				+ surf->texinfo->vecs[0][3] - surf->texturemins[0];
			s = l+0.5;
			if (s < 0)
				s = 0;
			else if (s > surf->extents[0])
				s = surf->extents[0];
			s = l - s;
			l = DotProduct (impact, surf->texinfo->vecs[1])
				+ surf->texinfo->vecs[1][3] - surf->texturemins[1];
			t = l+0.5;
			if (t < 0)
				t = 0;
			else if (t > surf->extents[1])
				t = surf->extents[1];
			t = l - t;
			// compare to minimum light
			if ((s*s+t*t+dist*dist) < maxdist)
			{
				if (surf->dlightframe != r_framecount) // not dynamic until now
				{
					surf->dlightbits = bit;
					surf->dlightframe = r_framecount;
				}
				else // already dynamic
					surf->dlightbits |= bit;
			}
		}

		if (node->children[0]->contents >= 0)
		{
			if (node->children[1]->contents >= 0)
			{
				R_MarkLightsNoVis (light, bit, node->children[0]);
				node = node->children[1];
				continue;
			}
			else
			{
				node = node->children[0];
				continue;
			}
		}
		else if (node->children[1]->contents >= 0)
		{
			node = node->children[1];
			continue;
		}

		return;
	}
}

/*
=============
R_MarkLights
=============
*/
void
R_MarkLights (rdlight_t *light, int bit, model_t *model)
{
	mleaf_t *pvsleaf = Mod_PointInLeaf (light->origin, model);
	int				i, k, l, m, c;
	msurface_t	   *surf, **mark;
	mleaf_t		   *leaf;
	Uint8		   *in = pvsleaf->compressed_vis;
	int				row = (model->numleafs+7)>>3;
	float			low[3], high[3], radius, dist, maxdist;

	if (!pvsleaf->compressed_vis || gl_oldlights->ivalue)
	{
		// no vis info, so make all visible
		R_MarkLightsNoVis(light, bit, model->nodes
				+ model->hulls[0].firstclipnode);
		return;
	}

	r_dlightframecount++;

	radius = light->cullradius;

	// for comparisons to maximum light distance
	maxdist = light->cullradius2;

	low[0] = light->origin[0] - radius;
	low[1] = light->origin[1] - radius;
	low[2] = light->origin[2] - radius;
	high[0] = light->origin[0] + radius;
	high[1] = light->origin[1] + radius;
	high[2] = light->origin[2] + radius;

	k = 0;
	while (k < row)
	{
		c = *in++;
		if (c)
		{
			l = model->numleafs + 1 - (k << 3);
			l = min (l, 8);
			for (i=0 ; i<l ; i++)
			{
				if (c & (1<<i))
				{
					leaf = &model->leafs[(k << 3)+i+1];
					if (leaf->visframe != r_framecount)
						continue;
					if (leaf->contents == CONTENTS_SOLID)
						continue;
					// if out of the light radius, skip
					if (leaf->mins[0] > high[0]
						|| leaf->maxs[0] < low[0]
						|| leaf->mins[1] > high[1]
						|| leaf->maxs[1] < low[1]
						|| leaf->mins[2] > high[2]
						|| leaf->maxs[2] < low[2])
						continue;
					if ((m = leaf->nummarksurfaces))
					{
						mark = leaf->firstmarksurface;
						do {
							surf = *mark++;

							if (surf->lightframe == r_dlightframecount)
								continue;
							surf->lightframe = r_dlightframecount;
							if (surf->visframe != r_framecount)
								continue;

							dist = PlaneDiff(light->origin, surf->plane);
							if (surf->flags & SURF_PLANEBACK)
								dist = -dist;
							// LordHavoc: make sure it is infront of the
							// surface and not too far away
							if (dist >= -0.25f && (dist < radius))
							{
								int d;
								float dist2, impact[3];

								dist2 = dist * dist;

								impact[0] = light->origin[0]
									- surf->plane->normal[0] * dist;
								impact[1] = light->origin[1]
									- surf->plane->normal[1] * dist;
								impact[2] = light->origin[2]
									- surf->plane->normal[2] * dist;

								d = DotProduct (impact, surf->texinfo->vecs[0])
									+ surf->texinfo->vecs[0][3]
									- surf->texturemins[0];

								if (d < 0)
								{
									dist2 += d * d;
									if (dist2 >= maxdist)
										continue;
								} else {
									d -= surf->extents[0] + 16;
									if (d > 0)
									{
										dist2 += d * d;
										if (dist2 >= maxdist)
											continue;
									}
								}

								d = DotProduct (impact, surf->texinfo->vecs[1])
									+ surf->texinfo->vecs[1][3]
									- surf->texturemins[1];

								if (d < 0)
								{
									dist2 += d * d;
									if (dist2 >= maxdist)
										continue;
								} else {
									d -= surf->extents[1] + 16;
									if (d > 0)
									{
										dist2 += d * d;
										if (dist2 >= maxdist)
											continue;
									}
								}

								if (surf->dlightframe != r_framecount)
								{
									// not dynamic until now
									surf->dlightbits = 0;
									surf->dlightframe = r_framecount;
								}
								surf->dlightbits |= bit;
							}
						} while (--m);
					}
				}
			}
			k++;
			continue;
		}

		k += *in++;
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
		R_MarkLights (l, 1 << i, cl.worldmodel);
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
		surf = cl.worldmodel->surfaces + node->firstsurface;

		for (i = 0; i < node->numsurfaces; i++, surf++)
		{
			if (surf->flags & SURF_DRAWTILED)
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

				line3 = ((surf->extents[0]>>4)+1)*3;

				// LordHavoc: *3 for color
				lightmap = surf->samples + ((dt>>4)
						* ((surf->extents[0]>>4)+1) + (ds>>4))*3;
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

	if (!cl.worldmodel->lightdata)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
		return 255;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

	RecursiveLightPoint (lightcolor, cl.worldmodel->nodes, p, end);

	return 255;
}

