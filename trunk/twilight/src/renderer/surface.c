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

#include "cvar.h"
#include "gl_info.h"
#include "gl_main.h"
#include "mathlib.h"
#include "model.h"
#include "qtypes.h"
#include "surface.h"
#include "sys.h"

Uint8 templight[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 4];

int lightmap_bytes;				// 1, 3, or 4
int lightmap_shift;
int gl_lightmap_format = GL_RGB;

cvar_t *gl_subdivide_size;
cvar_t *gl_colorlights;

void
Surf_Init_Cvars (void)
{
	gl_subdivide_size =Cvar_Get("gl_subdivide_size", "128", CVAR_ARCHIVE, NULL);
	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_NONE, NULL);
}

void
BuildGLPolyFromEdges (msurface_t *surf, model_t *model, Uint *count)
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

	if (surf->polys)
		Com_Printf("Well, we actually have more then one poly for the surface.\n");

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
SetupLightmapSettings (void)
{
	r.framecount = 1;
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
