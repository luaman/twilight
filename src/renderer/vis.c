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

#include "cvar.h"
#include "mathlib.h"
#include "model.h"
#include "qtypes.h"
#include "vis.h"

static cvar_t *r_novis;

static mplane_t frustum[4];

static Uint	 vis_pvsframecount;
Uint		 vis_framecount;
mleaf_t		*vis_viewleaf;
mleaf_t	*vis_oldviewleaf;

void
Vis_Init_Cvars (void)
{
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);
}

void
Vis_Init (void)
{
	vis_framecount = 1;
	vis_pvsframecount = 1;
}

void
Vis_Shutdown (void)
{
}

static int
SignbitsForPlane (mplane_t *out)
{
	int bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

void
Vis_NewVisParams (model_t *mod, vec3_t org, vec3_t up, vec3_t right,
		vec3_t point, float fov_x, float fov_y)
{
	int i;

	// current viewleaf
	vis_oldviewleaf = vis_viewleaf;
	vis_viewleaf = Mod_PointInLeaf (org, mod);

	vis_framecount++;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, up, point, -(90 - fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, up, point, 90 - fov_x / 2);
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector (frustum[2].normal, right, point, 90 - fov_y / 2);
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector (frustum[3].normal, right, point,-(90 - fov_y / 2));

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (org, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

qboolean
Vis_CullBox(vec3_t emins, vec3_t emaxs)
{
	int i;
	mplane_t *p;
	for (i = 0;i < 4;i++)
	{
		p = frustum + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

void
Vis_RecursiveWorldNode (mnode_t *node, model_t *mod, vec3_t org)
{
	int				 c, side;
	mplane_t		*plane;
	msurface_t		*surf, **mark;
	mleaf_t			*pleaf;
	double			 dot;

vis_worldnode:
	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->pvsframe != vis_pvsframecount)
		return;
	if (Vis_CullBox (node->mins, node->maxs))
		return;

	/* mark node/leaf as visible for MarkLights */
	node->visframe = vis_framecount;

	/* if a leaf node, draw stuff */
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;

		for (c = 0; c < pleaf->nummarksurfaces; c++, mark++)
			(*mark)->visframe = vis_framecount;

		return;
	}
	/* node is just a decision point, so go down the apropriate sides */

	/* find which side of the node we are on */
	plane = node->plane;
	dot = PlaneDiff (org, plane);
	side = dot < 0;

	/* recurse down the children, front side first */
	Vis_RecursiveWorldNode (node->children[side], mod, org);

	/* draw stuff */
	c = node->numsurfaces;

	if (c) {
		surf = mod->brush->surfaces + node->firstsurface;
		side = (dot < 0) ? SURF_PLANEBACK : 0;

		for (; c; c--, surf++) {
			if (surf->visframe != vis_framecount)
				continue;
			if (surf->tex_chain)
			{
				surf->tex_chain->visframe = vis_framecount;
				surf->tex_chain->head->visframe = vis_framecount;
			}
			if (surf->light_chain)
			{
				surf->light_chain->visframe = vis_framecount;
				surf->light_chain->head->visframe = vis_framecount;
			}
		}
	}

	/* recurse down the back side */
	node = node->children[!side];
	goto vis_worldnode;
}

void
Vis_MarkLeaves (model_t *mod)
{
	Uint8	*vis;
	mnode_t	*node;
	Uint	 i;

	if (vis_oldviewleaf == vis_viewleaf && !r_novis->ivalue)
		return;

	vis_pvsframecount++;
	vis_oldviewleaf = vis_viewleaf;

	if (r_novis->ivalue)
	{
		for (i = 0; i < mod->brush->numleafs; i++)
		{
			node = (mnode_t *) &mod->brush->leafs[i + 1];
			do {
				if (node->pvsframe == vis_pvsframecount)
					break;
				node->pvsframe = vis_pvsframecount;
				node = node->parent;
			} while (node);
		}
	} else {
		vis = Mod_LeafPVS (vis_viewleaf, mod);

		for (i = 0; i < mod->brush->numleafs; i++)
		{
			if (vis[i >> 3] & (1 << (i & 7)))
			{
				node = (mnode_t *) &mod->brush->leafs[i + 1];
				do {
					if (node->pvsframe == vis_pvsframecount)
						break;
					node->pvsframe = vis_pvsframecount;
					node = node->parent;
				} while (node);
			}
		}
	}
}

