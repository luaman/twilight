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
// r_light.c
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
#include "cvar.h"
#include "glquake.h"
#include "view.h"

int         r_dlightframecount;


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
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void
AddLightBlend (float r, float g, float b, float a2)
{
	float       a;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	a2 = a2 / a;

	v_blend[0] = v_blend[0] * (1 - a2) + r * a2;
	v_blend[1] = v_blend[1] * (1 - a2) + g * a2;
	v_blend[2] = v_blend[2] * (1 - a2) + b * a2;
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
	for (i = 16; i >= 0; i--) {
		a = i / 16.0 * M_PI * 2;
		*bub_sin++ = Q_sin (a);
		*bub_cos++ = Q_cos (a);
	}
}


void
R_RenderDlight (dlight_t *light)
{
	int     i, j;
	vec3_t  v, v_right, v_up;
	float	*bub_sin = bubble_sintable, 
			*bub_cos = bubble_costable;
	float   rad = light->radius * 0.35, length;

	VectorSubtract (light->origin, r_origin, v);
	length = VectorNormalize (v);

	if (length < rad) {				// view is inside the dlight
		AddLightBlend (1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	qglBegin (GL_TRIANGLE_FAN);
	qglColor3fv (light->color);

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

	qglVertex3fv (v);
	qglColor3f (0, 0, 0);

	for (i = 16; i >= 0; i--, bub_sin++, bub_cos++) 
	{
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (v_right[j] * (*bub_cos) +
				+ v_up[j] * (*bub_sin)) * rad;

		qglVertex3fv (v);
	}

	qglEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void
R_RenderDlights (void)
{
	int         i;
	dlight_t   *l;

	if (!gl_flashblend->value)
		return;

	qglDepthMask (0);
	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ONE, GL_ONE);

	l = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, l++) {
		if (l->die < cl.time || !l->radius)
			continue;
		R_RenderDlight (l);
	}

	qglColor3f (1, 1, 1);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (1);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void 
R_MarkLightsNoVis (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	float		l, maxdist;
	int			j, s, t;
	vec3_t		impact;

loc0:
	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = PlaneDiff (light->origin, splitplane);

	if (dist > light->radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (dist < -light->radius)
	{
		node = node->children[1];
		goto loc0;
	}
		
// mark the polygons
	maxdist = light->radius*light->radius;
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		for (j=0 ; j<3 ; j++)
			impact[j] = light->origin[j] - surf->plane->normal[j]*dist;

		// clamp center of light to corner and check brightness
		l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		s = l+0.5;
		if (s < 0) 
			s = 0;
		else if (s > surf->extents[0]) 
			s = surf->extents[0];
		s = l - s;
		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
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
R_MarkLights (dlight_t *light, int bit, model_t *model)
{
	mleaf_t *pvsleaf = Mod_PointInLeaf (light->origin, model);
	
	if (!pvsleaf->compressed_vis)
	{
		// no vis info, so make all visible
		R_MarkLightsNoVis(light, bit, model->nodes + model->hulls[0].firstclipnode);
		return;
	}
	else
	{
		int		i, k, l, m, c;
		msurface_t *surf, **mark;
		mleaf_t *leaf;
		byte	*in = pvsleaf->compressed_vis;
		int		row = (model->numleafs+7)>>3;
		float	low[3], high[3], radius, dist, maxdist;

		r_dlightframecount++;

		radius = light->radius * 2;

		low[0] = light->origin[0] - radius;
		low[1] = light->origin[1] - radius;
		low[2] = light->origin[2] - radius;
		high[0] = light->origin[0] + radius;
		high[1] = light->origin[1] + radius;
		high[2] = light->origin[2] + radius;

		// for comparisons to minimum acceptable light
		maxdist = radius*radius;

		k = 0;
		while (k < row)
		{
			c = *in++;
			if (c)
			{
				l = model->numleafs - (k << 3);
				if (l > 8)
					l = 8;
				for (i=0 ; i<l ; i++)
				{
					if (c & (1<<i))
					{
						leaf = &model->leafs[(k << 3)+i+1];
						if (leaf->visframe != r_visframecount)
							continue;
						if (leaf->contents == CONTENTS_SOLID)
							continue;
						// if out of the light radius, skip
						if (leaf->minmaxs[0] > high[0] || leaf->minmaxs[3+0] < low[0]
						 || leaf->minmaxs[1] > high[1] || leaf->minmaxs[3+1] < low[1]
						 || leaf->minmaxs[2] > high[2] || leaf->minmaxs[3+2] < low[2])
							continue; 
						if ((m = leaf->nummarksurfaces))
						{
							mark = leaf->firstmarksurface;
							do
							{
								surf = *mark++;

								if (surf->lightframe == r_dlightframecount)
									continue;

								surf->lightframe = r_dlightframecount;
								dist = PlaneDiff(light->origin, surf->plane);
								if (surf->flags & SURF_PLANEBACK)
									dist = -dist;
								// LordHavoc: make sure it is infront of the surface and not too far away
								if (dist >= -0.25f && (dist < radius))
								{
									int d;
									float dist2, impact[3];

									dist2 = dist * dist;

									impact[0] = light->origin[0] - surf->plane->normal[0] * dist;
									impact[1] = light->origin[1] - surf->plane->normal[1] * dist;
									impact[2] = light->origin[2] - surf->plane->normal[2] * dist;

									d = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];

									if (d < 0)
									{
										dist2 += d * d;
										if (dist2 >= maxdist)
											continue;
									}
									else
									{
										d -= surf->extents[0] + 16;
										if (d > 0)
										{
											dist2 += d * d;
											if (dist2 >= maxdist)
												continue;
										}
									}

									d = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

									if (d < 0)
									{
										dist2 += d * d;
										if (dist2 >= maxdist)
											continue;
									}
									else
									{
										d -= surf->extents[1] + 16;
										if (d > 0)
										{
											dist2 += d * d;
											if (dist2 >= maxdist)
												continue;
										}
									}

									if (surf->dlightframe != r_framecount) // not dynamic until now
									{
										surf->dlightbits = 0;
										surf->dlightframe = r_framecount;
									}
									surf->dlightbits |= bit;
								}
							}
							while (--m);
						}
					}
				}
				k++;
				continue;
			}
		
			k += *in++;
		}
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
	int         i;
	dlight_t   *l;

	if (gl_flashblend->value)
		return;

	l = cl_dlights;

	for (i = 0; i < MAX_DLIGHTS; i++, l++) {
		if (l->die < cl.time || !l->radius)
			continue;
		R_MarkLights (l, 1 << i, cl.worldmodel);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t   *lightplane;
vec3_t      lightspot;

int
RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int         r;
	float       front, back, frac;
	int         side;
	mplane_t   *plane;
	vec3_t      mid;
	msurface_t *surf;
	int         s, t, ds, dt;
	int         i;
	mtexinfo_t *tex;
	byte       *lightmap;
	unsigned    scale;
	int         maps;

	if (node->contents < 0)
		return -1;						// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = PlaneDiff (start, plane);
	back = PlaneDiff (end, plane);
	side = front < 0;

	if ((back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

// go down front side   
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;						// hit something

	if ((back < 0) == side)
		return -1;						// didn't hit anuthing

// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		if (surf->flags & SURF_DRAWTILED)
			continue;					// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap) {

			lightmap += dt * ((surf->extents[0] >> 4) + 1) + ds;

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
				 maps++) {
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0] >> 4) + 1) *
					((surf->extents[1] >> 4) + 1);
			}

			r >>= 8;
		}

		return r;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

int
R_LightPoint (vec3_t p)
{
	vec3_t      end;
	int         r;

	if (!cl.worldmodel->lightdata)
		return 255;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (cl.worldmodel->nodes, p, end);

	if (r == -1)
		r = 0;

	return r;
}
