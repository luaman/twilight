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
#include "collision.h"

#define	MAX_PHYSENTS	32

typedef struct
{
	vec3_t			origin;

	// only for bsp models
	struct model_s	*model;

	// only for non-bsp models
	vec3_t			mins, maxs;

	// for client or server to identify
	int				info;
	// Don't collide with objects with the same id, unless id == -1.
	int				id;
} physent_t;


typedef struct
{
	// player state
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	int			oldbuttons;
	float		waterjumptime;
	qboolean	dead;
	int			spectator;
	int			player_id;

	// world state
	int			numphysent;

	// 0 should be the world
	physent_t	physents[MAX_PHYSENTS];

	// input
	usercmd_t	cmd;

	// results
	int			numtouch;
	physent_t	*touch[MAX_PHYSENTS];

	physent_t	*groundent;
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
extern vec3_t player_mins;
extern vec3_t player_maxs;
struct hull_s;

void PlayerMove (void);
void Pmove_Init (void);

int PM_PointContents (vec3_t point);
qboolean PM_TestPlayerPosition (vec3_t point);
trace_t *PM_PlayerMove (vec3_t start, vec3_t stop);

#endif // __PMOVE_H

