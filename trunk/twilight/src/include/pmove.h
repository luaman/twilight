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

	$Id$
*/

#ifndef __PMOVE_H
#define __PMOVE_H

#include "model.h"
#include "protocol.h"

#define	MAX_PHYSENTS	32


typedef struct
{
	vec3_t		normal;
	float		dist;
} pmplane_t;

typedef struct
{
	// if true, plane is not valid
	qboolean	allsolid;

	// if true, the initial point was in a solid area
	qboolean	startsolid;
	qboolean	inopen, inwater;

	// time completed, 1.0 = didn't hit anything
	float		fraction;

	// final position
	vec3_t		endpos;

	// surface normal at impact
	pmplane_t	plane;

	// entity the surface is on
	int			ent;
} pmtrace_t;


typedef struct
{
	vec3_t			origin;

	// only for bsp models
	struct model_s	*model;

	// only for non-bsp models
	vec3_t			mins, maxs;

	// for client or server to identify
	int				info;
} physent_t;


typedef struct
{
	// for debugging prints
	int			sequence;

	// player state
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	int			oldbuttons;
	float		waterjumptime;
	qboolean	dead;
	int			spectator;

	// world state
	int			numphysent;

	// 0 should be the world
	physent_t	physents[MAX_PHYSENTS];

	// input
	usercmd_t	cmd;

	// results
	int			numtouch;
	int			touchindex[MAX_PHYSENTS];

	int			groundent;
	int			waterlevel;
	int			watertype;
} playermove_t;

typedef struct
{
	float		gravity;
	float		stopspeed;
	float		maxspeed;
	float		spectatormaxspeed;
	float		accelerate;
	float		airaccelerate;
	float		wateraccelerate;
	float		friction;
	float		waterfriction;
	float		entgravity;
} movevars_t;


extern movevars_t movevars;
extern playermove_t pmove;
struct hull_s;

void PlayerMove (void);
void Pmove_Init (void);

int PM_HullPointContents (struct hull_s *hull, int num, vec3_t p);

int PM_PointContents (vec3_t point);
qboolean PM_TestPlayerPosition (vec3_t point);
pmtrace_t PM_PlayerMove (vec3_t start, vec3_t stop);

qboolean PM_RecursiveHullCheck (struct hull_s *hull, int num, float p1f,
		float p2f, vec3_t p1, vec3_t p2, pmtrace_t *trace);

float TraceLine (model_t *mdl, vec3_t start, vec3_t end, vec3_t impact,
		vec3_t normal);
	

#endif // __PMOVE_H

