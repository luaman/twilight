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

#include <stdio.h>
#include <string.h> /* for malloc() */

#include "model.h"
#include "cvar.h"
#include "qtypes.h"
#include "mathlib.h"
#include "sys.h"
#include "gl_info.h"

Uint8 templight[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 4];

int lightmap_bytes;				// 1, 3, or 4
int lightmap_shift;
int gl_lightmap_format = GL_RGB;
extern qboolean colorlights;

extern cvar_t *gl_colorlights;

//static msurface_t *warpface;

cvar_t *gl_subdivide_size;

/*
===============
Surf_Init_Cvars
===============
 */
void
Surf_Init_Cvars (void)
{
	gl_subdivide_size =Cvar_Get("gl_subdivide_size", "128", CVAR_ARCHIVE, NULL);
}

#if 0
static void
BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int         i, j;
	float      *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

static void
CountSubdividePolygon (brushhdr_t *brush, int numverts, float *verts,
		memzone_t *zone)
{
	int         i, j, k;
	vec3_t      mins, maxs;
	float       m;
	float      *v;
	vec3_t      front[64], back[64];
	int         f, b;
	float       dist[64];
	float       frac;

	if (!numverts)
		Sys_Error ("numverts = 0!");
	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size->ivalue
			* floor (m / gl_subdivide_size->ivalue + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		CountSubdividePolygon (brush, f, front[0], zone);
		CountSubdividePolygon (brush, b, back[0], zone);
		return;
	}

	brush->numsets += numverts;
}

/*
================
Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void
CountSubdividedGLPolyFromEdges (msurface_t *surf, model_t *model)
{
	vec3_t		 verts[64];
	int			 numverts;
	int			 i;
	int			 lindex;
	float		*vec;
	brushhdr_t	*brush = model->brush;
	memzone_t	*zone = model->zone;

	warpface = surf;

	// 
	// convert edges back to a normal polygon
	// 
	numverts = 0;
	for (i = 0; i < surf->numedges; i++) {
		lindex = brush->surfedges[surf->firstedge + i];

		if (lindex > 0)
			vec = brush->vertices[brush->edges[lindex].v[0]].v;
		else
			vec = brush->vertices[brush->edges[-lindex].v[1]].v;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	CountSubdividePolygon (brush, numverts, verts[0], zone);
}

static void
SubdividePolygon (brushhdr_t *brush, Uint numverts, Uint *verts,
		memzone_t *zone)
{
	int         i, j, k, v;
	vec3_t      mins, maxs;
	float       m;
	vec3_t      front[64], back[64];
	int         f, b;
	float       dist[64];
	float       frac;
	glpoly_t   *poly;

	if (!numverts)
		Sys_Error ("numverts = 0!");
	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);
	v = 0;

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size->ivalue
			* floor (m / gl_subdivide_size->ivalue + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (brush, f, front[0], zone);
		SubdividePolygon (brush, b, back[0], zone);
		return;
	}

	poly = Zone_Alloc (zone, sizeof (glpoly_t)); 
	poly->tc = Zone_Alloc (zone, numverts * sizeof (texcoord_t));
	poly->v = Zone_Alloc (zone, numverts * sizeof (vertex_t));

	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy (verts, poly->v[i].v);
		poly->tc[i].v[0] = DotProduct (verts, warpface->texinfo->vecs[0]);
		poly->tc[i].v[1] = DotProduct (verts, warpface->texinfo->vecs[1]);
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void
BuildSubdividedGLPolyFromEdges (msurface_t *surf, model_t *model)
{
	Uint		 verts[64];
	int			 numverts;
	int			 i;
	int			 lindex;
	float		*vec;
	brushhdr_t	*brush = model->brush;
	memzone_t	*zone = model->zone;

	warpface = surf;

	// 
	// convert edges back to a normal polygon
	// 
	numverts = 0;
	for (i = 0; i < surf->numedges; i++) {
		lindex = brush->surfedges[surf->firstedge + i];

		if (lindex > 0)
			verts[numverts++] = brush->edges[lindex].v[0];
		else
			verts[numverts++] = brush->edges[-lindex].v[0];
	}

	SubdividePolygon (brush, numverts, verts, zone);
}
#endif

/*
================
CountGLPolyFromEdges
================
 */
void
CountGLPolyFromEdges (msurface_t *surf, model_t *model)
{
	brushhdr_t	*brush = model->brush;

	brush->numsets += surf->numedges;
}

/*
================
BuildGLPolyFromEdges
================
 */
void
BuildGLPolyFromEdges (msurface_t *surf, model_t *model, int *count)
{
	int			 i, vert, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			 vertpage;
	float		*vec;
	float		 s, t;
	glpoly_t	*poly;
	brushhdr_t	*brush = model->brush;
	memzone_t	*zone = model->zone;
	mtexinfo_t	*texinfo = surf->texinfo;

	// reconstruct the polygon
	pedges = brush->edges;
	lnumverts = surf->numedges;
	vertpage = 0;

	/*
	 * draw texture
	 */
	poly = Zone_Alloc (zone, sizeof (glpoly_t)); 

	poly->next = surf->polys;
	surf->polys = poly;
	poly->numverts = lnumverts;
	poly->start = *count;

	for (i = 0; i < lnumverts; i++, (*count)++) {
		lindex = brush->surfedges[surf->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vert = r_pedge->v[0];
		} else {
			r_pedge = &pedges[-lindex];
			vert = r_pedge->v[1];
		}
		vec = brush->vertices[vert].v;
		VectorCopy (vec, brush->verts[*count].v);

		s = DotProduct (vec, texinfo->vecs[0]) + texinfo->vecs[0][3];
		s /= texinfo->texture->width;

		t = DotProduct (vec, texinfo->vecs[1]) + texinfo->vecs[1][3];
		t /= texinfo->texture->height;

		B_TC(brush, 0, *count, 0) = s;
		B_TC(brush, 0, *count, 1) = t;

		/*
		 * lightmap texture coordinates
		 */
		s = DotProduct (vec, texinfo->vecs[0]) + texinfo->vecs[0][3];
		s -= surf->texturemins[0];
		s += surf->light_s << 4;
		s += 8;
		s /= LIGHTBLOCK_WIDTH << 4;			// texinfo->texture->width;

		t = DotProduct (vec, texinfo->vecs[1]) + texinfo->vecs[1][3];
		t -= surf->texturemins[1];
		t += surf->light_t << 4;
		t += 8;
		t /= LIGHTBLOCK_HEIGHT << 4;		// texinfo->texture->height;
		B_TC(brush, 1, *count, 0) = s;
		B_TC(brush, 1, *count, 1) = t;
	}
}

void
SetupLightmapSettings ()
{
	r_framecount = 1;
	if (gl_mtexcombine)
		lightmap_shift = 9;
	else
		lightmap_shift = 8;

	lightmap_shift += 8;			// For stainmaps.
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
}


qboolean
AllocLightBlockForSurf (int *allocated, int num, msurface_t *surf,
		memzone_t *zone)
{
	int              w, h, i, j, best, best2;

	zone = zone;

	/* We need to be aligned to 4 bytes due to GL. */
	surf->alignedwidth = surf->smax;
	while ((surf->alignedwidth * lightmap_bytes) & 3)
		surf->alignedwidth++;
	w = surf->alignedwidth;
	h = surf->tmax;

	best = LIGHTBLOCK_HEIGHT;

	for (i = 0; i < LIGHTBLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (allocated[i + j] >= best)
				break;
			if (allocated[i + j] > best2)
				best2 = allocated[i + j];
		}

		if (j == w)
		{
			// this is a valid spot
			surf->light_s = i;
			surf->light_t = best = best2;
		}
	}

	if (best + h > LIGHTBLOCK_HEIGHT)
		return false;

	for (i = 0; i < w; i++)
		allocated[surf->light_s + i] = best + h;

	surf->lightmap_texnum = num;
	return true;
}
