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
#include "host.h"

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

static areanode_t *
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

void
SV_ClearWorld (void)
{
	Collision_Init ();

	memset (sv_areanodes, 0, sizeof (sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
}


void
SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;							// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


static void
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


static void
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
		leafnum = leaf - sv.worldmodel->brush->leafs - 1;

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

void
SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;
	model_t		*model;

	// unlink from old position
	if (ent->area.prev)
		SV_UnlinkEdict (ent);

	// don't add the world
	if (ent == sv.edicts)
		return;

	if (ent->free)
		return;

	// set the abs box
	if (ent->v.solid == SOLID_BSP)
	{
		if (ent->v.modelindex < 0 || ent->v.modelindex > MAX_MODELS)
			PR_RunError("SOLID_BSP with invalid modelindex!\n");
		model = sv.models[(int) ent->v.modelindex];
		if (model != NULL)
		{
			if (model->type != mod_brush)
				PR_RunError("SOLID_BSP with non-BSP model\n");

			// Can't use Mod_MinsMaxs here because of avelocity
			if (ent->v.angles[0] || ent->v.angles[2] || ent->v.avelocity[0]
					|| ent->v.avelocity[2])
			{
				VectorAdd (ent->v.origin, model->rotatedmins, ent->v.absmin);
				VectorAdd (ent->v.origin, model->rotatedmaxs, ent->v.absmax);
			}
			else if (ent->v.angles[1] || ent->v.avelocity[1])
			{
				VectorAdd (ent->v.origin, model->yawmins, ent->v.absmin);
				VectorAdd (ent->v.origin, model->yawmaxs, ent->v.absmax);
			}
			else
			{
				VectorAdd (ent->v.origin, model->normalmins, ent->v.absmin);
				VectorAdd (ent->v.origin, model->normalmaxs, ent->v.absmax);
			}
		}
		else
		{
			// SOLID_BSP with no model is valid, mainly because some QC
			// setup code does so temporarily
			VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
			VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);
		}
	}
	else
	{
		VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
		VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);
	}

	// to make items easier to pick up and allow them to be grabbed off
	// of shelves, the abs sizes are expanded
	if ((int) ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
		ent->v.absmax[2] += 1;
	}
	else
	{
		// because movement is clipped an epsilon away from an actual edge,
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
		SV_FindTouchedLeafs (ent, sv.worldmodel->brush->nodes);

	if (ent->v.solid == SOLID_NOT)
		return;

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			// crosses the node
			break;
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

int
SV_PointContents (vec3_t p)
{
	return (Mod_PointInLeaf (p, sv.worldmodel))->contents;
}

//===========================================================================

/*
============
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


/*
==================
Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t
SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs,
					 vec3_t end)
{
	int i;
	trace_t trace;
	model_t *model = NULL;

	if ((int) ent->v.solid == SOLID_BSP) {
		if (ent->v.movetype != MOVETYPE_PUSH)
			Host_Error("SV_ClipMoveToEntity: SOLID_BSP without MOVETYPE_PUSH");

		i = ent->v.modelindex;
		model = sv.models[i];
		if ((i >= MAX_MODELS) || !model)
			PR_RunError("SV_ClipMoveToEntity: invalid modelindex\n");

		if (model->type != mod_brush) {
			Com_Printf("SV_ClipMoveToEntity: SOLID_BSP with a non bsp model, entity dump:\n");
			ED_Print (ent);
			Host_Error("SV_ClipMoveToEntity: SOLID_BSP with a non bsp model\n");
		}
	}

	Collision_ClipTrace (&trace, ent, model, ent->v.origin, ent->v.angles,
			ent->v.mins, ent->v.maxs, start, mins, maxs, end);

	return trace;
}

//===========================================================================

/*
====================
Mins and maxs enclose the entire area swept by the move
====================
*/
static void
SV_ClipToLinks (areanode_t *node, moveclip_t * clip)
{
	link_t     *l;
	edict_t    *touch;
	trace_t     trace;
	volatile int i = 0;

// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = l->next, i++) {
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

		if (clip->passedict)
		{
			if (clip->passedict->v.size[0] && !touch->v.size[0])
				// points never interact
				continue;

			if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				// don't clip against own missiles
				continue;

			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				// don't clip against owner
				continue;

			// LordHavoc: corpse code
			if (clip->passedict->v.solid == SOLID_CORPSE
					&& (touch->v.solid == SOLID_SLIDEBOX
						|| touch->v.solid == SOLID_CORPSE))
				continue;
			if (clip->passedict->v.solid == SOLID_SLIDEBOX
					&& touch->v.solid == SOLID_CORPSE)
				continue;
		}
		
		// might intersect, so do an exact clip
		if ((int) touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2,
					clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins,
					clip->maxs, clip->end);
		// LordHavoc: take the 'best' answers from the new trace and combine
		// with existing data
		if (trace.allsolid)
			clip->trace.allsolid = true;
		if (trace.startsolid)
		{
			clip->trace.startsolid = true;
			if (!clip->trace.ent)
				clip->trace.ent = trace.ent;
		}
		if (trace.inopen)
			clip->trace.inopen = true;
		if (trace.inwater)
			clip->trace.inwater = true;
		if (trace.fraction < clip->trace.fraction)
		{
			clip->trace.fraction = trace.fraction;
			VectorCopy(trace.endpos, clip->trace.endpos);
			clip->trace.plane = trace.plane;
			clip->trace.endcontents = trace.endcontents;
			clip->trace.ent = trace.ent;
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (clip->boxmaxs[node->axis] > node->dist)
		SV_ClipToLinks (node->children[0], clip);
	if (clip->boxmins[node->axis] < node->dist)
		SV_ClipToLinks (node->children[1], clip);
}


static void
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

