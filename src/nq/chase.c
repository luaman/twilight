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
#include "client.h"
#include "cvar.h"
#include "mathlib.h"
#include "world.h"

cvar_t	*chase_active;
static cvar_t	*chase_back;
static cvar_t	*chase_up;
static cvar_t	*chase_right;

static vec3_t	chase_dest;


void
Chase_Init_Cvars (void)
{
	chase_back = Cvar_Get ("chase_back", "100", CVAR_NONE, NULL);
	chase_up = Cvar_Get ("chase_up", "16", CVAR_NONE, NULL);
	chase_right = Cvar_Get ("chase_right", "0", CVAR_NONE, NULL);
	chase_active = Cvar_Get ("chase_active", "0", CVAR_NONE, NULL);
}

void
Chase_Init (void)
{
}

void
Chase_Update (void)
{
	int		i;
	float	dist;
	vec3_t	forward, up, right, dest, stop, normal;

	// if can't see player, reset
	AngleVectors (cl.viewangles, forward, right, up);

	// calc exact destination
	for (i = 0; i < 3; i++)
		chase_dest[i] = r.origin[i] - forward[i] * chase_back->fvalue - right[i] * chase_right->fvalue;

	chase_dest[2] = r.origin[2] + chase_up->fvalue;

	// find the spot the player is looking at
	VectorMA (r.origin, 4096, forward, dest);
	TraceLine (ccl.worldmodel, r.origin, dest, stop, normal);

	// calculate pitch to look at the same spot from camera
	VectorSubtract (stop, r.origin, stop);
	dist = DotProduct (stop, forward);

	if (dist < 1)
		dist = 1;

	r.angles[PITCH] = -Q_atan (stop[2] / dist) / M_PI * 180;

	// move towards destination
	TraceLine (ccl.worldmodel, r.origin, chase_dest, stop, normal);

	VectorCopy (stop, chase_dest);

	// move towards destination
	VectorCopy (chase_dest, r.origin);
}

