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

#include "quakedef.h"
#include "bspfile.h"
#include "common.h"
#include "mathlib.h"
#include "model.h"
#include "pmove.h"
#include "sys.h"
#include "strlib.h"
#include "collision.h"
#include "crc.h"

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

/*
===================
Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void
PM_InitBoxHull (void)
{
	int         i;
	int         side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0; i < 6; i++) {
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side ^ 1] = i + 1;
		else
			box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1;
	}

}


/*
===================
To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
static hull_t     *
PM_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}


static int
PM_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	float       d;
	dclipnode_t *node;
	mplane_t   *plane;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("PM_HullPointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		d = PlaneDiff(p, plane);
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

int
PM_PointContents (vec3_t p)
{
	float       d;
	dclipnode_t *node;
	mplane_t   *plane;
	hull_t     *hull;
	int         num;

	hull = &pmove.physents[0].model->hulls[0];

	num = hull->firstclipnode;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("PM_PointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		d = PlaneDiff(p, plane);
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

/*
================
Returns false if the given player position is not valid (in solid)
================
*/
qboolean
PM_TestPlayerPosition (vec3_t pos)
{
	int         i;
	physent_t  *pe;
	vec3_t      mins, maxs, test;
	hull_t     *hull;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		if ((pe->id != -1) && (pe->id == pmove.player_id))
			continue;

		// get the clipping hull
		if (pe->model)
			hull = &pmove.physents[i].model->hulls[1];
		else {
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = PM_HullForBox (mins, maxs);
		}

		VectorSubtract (pos, pe->origin, test);

		if (PM_HullPointContents (hull, hull->firstclipnode, test) ==
			CONTENTS_SOLID)
			return false;
	}

	return true;
}

trace_t *
PM_PlayerMove (vec3_t start, vec3_t end)
{
	static trace_t	traces[2];
	trace_t		*cur, *shortest;
	vec3_t      start_l, end_l;
	hull_t     *hull;
	int         i, trace = 0;
	physent_t  *pe;
	vec3_t      mins, maxs;

	cur = &traces[trace];
	shortest = NULL;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		if ((pe->id != -1) && (pe->id == pmove.player_id))
			continue;
		// get the clipping hull
		if (pe->model)
			hull = &pmove.physents[i].model->hulls[1];
		else {
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = PM_HullForBox (mins, maxs);
		}

		VectorSubtract (start, pe->origin, start_l);
		VectorSubtract (end, pe->origin, end_l);

		// trace a line through the apropriate clipping hull
		TraceLine_Raw (hull, start_l, end_l, cur);

		// did we clip the move?
		if (!shortest || (cur->fraction < shortest->fraction)) {
			VectorAdd (cur->endpos, pe->origin, cur->endpos);
			cur->ent = pe;
			// Swap spare and cur.
			shortest = cur;
			cur = &traces[++trace % 2];
		}
	}

	return shortest;
}
