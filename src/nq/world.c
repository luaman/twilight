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
#include "strlib.h"
#include "mathlib.h"
#include "server.h"
#include "sys.h"
#include "world.h"

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/


typedef struct {
	vec3_t      boxmins, boxmaxs;		// enclose the test object along entire 
										// move
	float      *mins, *maxs;			// size of the moving object
	vec3_t      mins2, maxs2;			// size when clipping against mosnters
	float      *start, *end;
	trace_t     trace;
	int         type;
	edict_t    *passedict;
} moveclip_t;


int         SV_HullPointContents (hull_t *hull, int num, vec3_t p);

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void
SV_InitBoxHull (void)
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
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t     *
SV_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}



/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t     *
SV_HullForEntity (edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset)
{
	model_t    *model;
	vec3_t      size;
	vec3_t      hullmins, hullmaxs;
	hull_t     *hull;

// decide which clipping hull to use, based on the size
	if (ent->v.solid == SOLID_BSP) {	// explicit hulls in the BSP model
		if (ent->v.movetype != MOVETYPE_PUSH)
			Sys_Error ("SOLID_BSP without MOVETYPE_PUSH");

		model = sv.models[(int) ent->v.modelindex];

		if (!model || model->type != mod_brush)
			Sys_Error ("SOLID_BSP with a non bsp model");

		VectorSubtract (maxs, mins, size);
		if (size[0] < 3)
			hull = &model->hulls[0];
		else if (size[0] <= 32)
			hull = &model->hulls[1];
		else
			hull = &model->hulls[2];

// calculate an offset value to center the origin
		VectorSubtract (hull->clip_mins, mins, offset);
		VectorAdd (offset, ent->v.origin, offset);
	} else {
		// create a temp hull from bounding box sizes
		VectorSubtract (ent->v.mins, maxs, hullmins);
		VectorSubtract (ent->v.maxs, mins, hullmaxs);
		hull = SV_HullForBox (hullmins, hullmaxs);

		VectorCopy (ent->v.origin, offset);
	}


	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s {
	int         axis;					// -1 = leaf node
	float       dist;
	struct areanode_s *children[2];
	link_t      trigger_edicts;
	link_t      solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

static areanode_t sv_areanodes[AREA_NODES];
static int  sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *
SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t *anode;
	vec3_t      size;
	vec3_t      mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth + 1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void
SV_ClearWorld (void)
{
	SV_InitBoxHull ();

	memset (sv_areanodes, 0, sizeof (sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void
SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;							// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


/*
====================
SV_TouchLinks
====================
*/
void
SV_TouchLinks (edict_t *ent, areanode_t *node)
{
	link_t     *l, *next;
	edict_t    *touch;
	int         old_self, old_other;

// touch linked edicts
	for (l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA (l);
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
			|| ent->v.absmin[1] > touch->v.absmax[1]
			|| ent->v.absmin[2] > touch->v.absmax[2]
			|| ent->v.absmax[0] < touch->v.absmin[0]
			|| ent->v.absmax[1] < touch->v.absmin[1]
			|| ent->v.absmax[2] < touch->v.absmin[2])
			continue;
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG (touch);
		pr_global_struct->other = EDICT_TO_PROG (ent);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram (touch->v.touch, "");

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if (ent->v.absmax[node->axis] > node->dist)
		SV_TouchLinks (ent, node->children[0]);
	if (ent->v.absmin[node->axis] < node->dist)
		SV_TouchLinks (ent, node->children[1]);
}


/*
===============
SV_FindTouchedLeafs

===============
*/
void
SV_FindTouchedLeafs (edict_t *ent, mnode_t *node)
{
	mplane_t   *splitplane;
	mleaf_t    *leaf;
	int         sides;
	int         leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

// add an efrag if the node is a leaf

	if (node->contents < 0) {
		if (ent->num_leafs == MAX_ENT_LEAFS)
			return;

		leaf = (mleaf_t *) node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}
// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE (ent->v.absmin, ent->v.absmax, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs (ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs (ent, node->children[1]);
}

/*
===============
SV_LinkEdict

===============
*/
void
SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;
	model_t		*mod;

	if (ent->area.prev)
		SV_UnlinkEdict (ent);			// unlink from old position

	if (ent == sv.edicts)
		return;							// don't add the world

	if (ent->free)
		return;

// set the abs box
	mod = sv.models[(int) ent->v.modelindex];
#if 1
	if (mod && (ent->v.solid == SOLID_BSP))
		Mod_MinsMaxs (mod, ent->v.origin, ent->v.angles, ent->v.absmin, ent->v.absmax);
	else
#endif
	{
		VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
		VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);
	}


//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int) ent->v.flags & FL_ITEM) {
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	} else {							// because movement is clipped an
										// epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}

// link to PVS leafs
	ent->num_leafs = 0;
	if (ent->v.modelindex)
		SV_FindTouchedLeafs (ent, sv.worldmodel->nodes);

	if (ent->v.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1) {
		if (node->axis == -1)
			break;
		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;						// crosses the node
	}

// link it in   

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);

// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers)
		SV_TouchLinks (ent, sv_areanodes);
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_HullPointContents

==================
*/
int
SV_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	float       d;
	dclipnode_t *node;
	mplane_t   *plane;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("SV_HullPointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		d = PlaneDiff (p, plane);

		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}


/*
==================
SV_PointContents

==================
*/
int
SV_PointContents (vec3_t p)
{
	int         cont;

	cont = SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return cont;
}

int
SV_TruePointContents (vec3_t p)
{
	return SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
edict_t    *
SV_TestEntityPosition (edict_t *ent)
{
	trace_t     trace;

	trace =
		SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, 0,
				 ent);

	if (trace.startsolid)
		return sv.edicts;

	return NULL;
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
//#define	DIST_EPSILON	(0.03125)
#define DIST_EPSILON (0.125)

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

// LordHavoc: FIXME: this is not thread safe, if threading matters here, pass
// this as a struct to RecursiveHullCheck, RecursiveHullCheck_Impact, etc...
RecursiveHullCheckTraceInfo_t RecursiveHullCheckInfo;
#define RHC RecursiveHullCheckInfo

void SV_RecursiveHullCheck_Impact (mplane_t *plane, int side)
{
	// LordHavoc: using doubles for extra accuracy
	double t1, t2, frac;

	// LordHavoc: now that we have found the impact, recalculate the impact
	// point from scratch for maximum accuracy, with an epsilon bias on the
	// surface distance
	frac = plane->dist;
	if (side)
	{
		frac -= DIST_EPSILON;
		VectorNegate (plane->normal, RHC.trace->plane.normal);
		RHC.trace->plane.dist = -plane->dist;
	}
	else
	{
		frac += DIST_EPSILON;
		VectorCopy (plane->normal, RHC.trace->plane.normal);
		RHC.trace->plane.dist = plane->dist;
	}

	if (plane->type < 3)
	{
		t1 = RHC.start[plane->type] - frac;
		t2 = RHC.start[plane->type] + RHC.dist[plane->type] - frac;
	}
	else
	{
		t1 = plane->normal[0] * RHC.start[0] + plane->normal[1] * RHC.start[1] + plane->normal[2] * RHC.start[2] - frac;
		t2 = plane->normal[0] * (RHC.start[0] + RHC.dist[0]) + plane->normal[1] * (RHC.start[1] + RHC.dist[1]) + plane->normal[2] * (RHC.start[2] + RHC.dist[2]) - frac;
	}

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0f);

	RHC.trace->fraction = frac;
	RHC.trace->endpos[0] = RHC.start[0] + frac * RHC.dist[0];
	RHC.trace->endpos[1] = RHC.start[1] + frac * RHC.dist[1];
	RHC.trace->endpos[2] = RHC.start[2] + frac * RHC.dist[2];
}

int SV_RecursiveHullCheck (int num, double p1f, double p2f, double p1[3], double p2[3])
{
	dclipnode_t	*node;
	double		mid[3];
	int			side;
	double		midf;
	// LordHavoc: FIXME: this is not thread safe...  if threading matters here,
	// remove the static prefixes
	static int ret;
	static mplane_t *plane;
	static double t1, t2, frac;

	// LordHavoc: a goto!  everyone flee in terror... :)
loc0:
	// check for empty
	if (num < 0)
	{
		RHC.trace->endcontents = num;
		if (RHC.trace->startcontents)
		{
			if (num == RHC.trace->startcontents)
				RHC.trace->allsolid = false;
			else
			{
				// if the first leaf is solid, set startsolid
				if (RHC.trace->allsolid)
					RHC.trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
		else
		{
			if (num != CONTENTS_SOLID)
			{
				RHC.trace->allsolid = false;
				if (num == CONTENTS_EMPTY)
					RHC.trace->inopen = true;
				else
					RHC.trace->inwater = true;
			}
			else
			{
				// if the first leaf is solid, set startsolid
				if (RHC.trace->allsolid)
					RHC.trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
	}

	// find the point distances
	node = RHC.hull->clipnodes + num;

	plane = RHC.hull->planes + node->planenum;
	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	// LordHavoc: rearranged the side/frac code
	if (t1 >= 0)
	{
		if (t2 >= 0)
		{
			num = node->children[0];
			goto loc0;
		}
		// put the crosspoint DIST_EPSILON pixels on the near side
		side = 0;
	}
	else
	{
		if (t2 < 0)
		{
			num = node->children[1];
			goto loc0;
		}
		// put the crosspoint DIST_EPSILON pixels on the near side
		side = 1;
	}

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0f);

	midf = p1f + ((p2f - p1f) * frac);
	mid[0] = RHC.start[0] + midf * RHC.dist[0];
	mid[1] = RHC.start[1] + midf * RHC.dist[1];
	mid[2] = RHC.start[2] + midf * RHC.dist[2];

	// front side first
	ret = SV_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);
	if (ret != HULLCHECKSTATE_EMPTY)
		return ret; // solid or done
	ret = SV_RecursiveHullCheck (node->children[!side], midf, p2f, mid, p2);
	if (ret != HULLCHECKSTATE_SOLID)
		return ret; // empty or done

	// front is air and back is solid, this is the impact point...
	SV_RecursiveHullCheck_Impact(RHC.hull->planes + node->planenum, side);

	return HULLCHECKSTATE_DONE;
}

float
TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t trace;
	double startd[3], endd[3];

	VectorCopy(start, startd);
	VectorCopy(end, endd);
	memset (&trace, 0, sizeof(trace));
	VectorCopy (end, trace.endpos);
	trace.fraction = 1;
	trace.startcontents = 0;
	VectorCopy(start, RecursiveHullCheckInfo.start);
	VectorSubtract(end, start, RecursiveHullCheckInfo.dist);
	RecursiveHullCheckInfo.hull = cl.worldmodel->hulls;
	RecursiveHullCheckInfo.trace = &trace;
	SV_RecursiveHullCheck (cl.worldmodel->hulls->firstclipnode, 0, 1, startd, endd);
	if (impact)
		VectorCopy (trace.endpos, impact);
	if (normal)
		VectorCopy (trace.plane.normal, normal);
	return trace.fraction;
}


/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t
SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs,
					 vec3_t end)
{
	trace_t     trace;
	vec3_t      offset;
	vec3_t      start_l, end_l;
	double      startd[3], endd[3];
	hull_t     *hull;

// fill in a default trace
	memset (&trace, 0, sizeof (trace_t));
	trace.fraction = 1;
	trace.allsolid = true;
	VectorCopy (end, trace.endpos);

// get the clipping hull
	hull = SV_HullForEntity (ent, mins, maxs, offset);

	VectorSubtract (start, offset, start_l);
	VectorSubtract (end, offset, end_l);

	// rotate start and end into the models frame of reference
	if (ent->v.solid == SOLID_BSP &&
		(ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2])) {
		vec3_t      forward, right, up;
		vec3_t      temp;

		AngleVectors (ent->v.angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	VectorCopy(start_l, startd);
	VectorCopy(end_l, endd);
	VectorCopy(startd, RecursiveHullCheckInfo.start);
	VectorSubtract(endd, startd, RecursiveHullCheckInfo.dist);
	RecursiveHullCheckInfo.hull = hull;
	RecursiveHullCheckInfo.trace = &trace;

// trace a line through the apropriate clipping hull
	SV_RecursiveHullCheck (hull->firstclipnode, 0, 1, startd, endd);

	// rotate endpos back to world frame of reference
	if (ent->v.solid == SOLID_BSP &&
		(ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2])) {
		vec3_t      a;
		vec3_t      forward, right, up;
		vec3_t      temp;

		if (trace.fraction != 1) {
			VectorInverse (ent->v.angles, a);
			AngleVectors (a, forward, right, up);

			VectorCopy (trace.endpos, temp);
			trace.endpos[0] = DotProduct (temp, forward);
			trace.endpos[1] = -DotProduct (temp, right);
			trace.endpos[2] = DotProduct (temp, up);

			VectorCopy (trace.plane.normal, temp);
			trace.plane.normal[0] = DotProduct (temp, forward);
			trace.plane.normal[1] = -DotProduct (temp, right);
			trace.plane.normal[2] = DotProduct (temp, up);
		}
	}

// fix trace up by the offset
	if (trace.fraction != 1)
		VectorAdd (trace.endpos, offset, trace.endpos);

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = ent;

	return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void
SV_ClipToLinks (areanode_t *node, moveclip_t * clip)
{
	link_t     *l, *next;
	edict_t    *touch;
	trace_t     trace;

// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
		next = l->next;
		touch = EDICT_FROM_AREA (l);
		if (touch->v.solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v.solid == SOLID_TRIGGER)
			Sys_Error ("Trigger in clipping list");

		if (clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->v.absmax[0]
			|| clip->boxmins[1] > touch->v.absmax[1]
			|| clip->boxmins[2] > touch->v.absmax[2]
			|| clip->boxmaxs[0] < touch->v.absmin[0]
			|| clip->boxmaxs[1] < touch->v.absmin[1]
			|| clip->boxmaxs[2] < touch->v.absmin[2])
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;					// points never interact

		// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->passedict) {
			if (PROG_TO_EDICT (touch->v.owner) == clip->passedict)
				continue;				// don't clip against own missiles
			if (PROG_TO_EDICT (clip->passedict->v.owner) == touch)
				continue;				// don't clip against owner
		}

		if ((int) touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2,
					clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins,
					clip->maxs, clip->end);
		if (trace.allsolid || trace.startsolid
			|| trace.fraction < clip->trace.fraction) {
			trace.ent = touch;
			if (clip->trace.startsolid) {
				clip->trace = trace;
				clip->trace.startsolid = true;
			} else
				clip->trace = trace;
		} else if (trace.startsolid)
			clip->trace.startsolid = true;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (clip->boxmaxs[node->axis] > node->dist)
		SV_ClipToLinks (node->children[0], clip);
	if (clip->boxmins[node->axis] < node->dist)
		SV_ClipToLinks (node->children[1], clip);
}


/*
==================
SV_MoveBounds
==================
*/
void
SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
			   vec3_t boxmins, vec3_t boxmaxs)
{
	int         i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
SV_Move
==================
*/
trace_t
SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type,
		 edict_t *passedict)
{
	moveclip_t  clip;
	int         i;

	memset (&clip, 0, sizeof (moveclip_t));

// clip to world
	clip.trace = SV_ClipMoveToEntity (sv.edicts, start, mins, maxs, end);

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

	if (type == MOVE_MISSILE) {
		for (i = 0; i < 3; i++) {
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	} else {
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}

// create the bounding box of the entire move
	SV_MoveBounds (start, clip.mins2, clip.maxs2, end, clip.boxmins,
				   clip.boxmaxs);

// clip to entities
	SV_ClipToLinks (sv_areanodes, &clip);

	return clip.trace;
}

