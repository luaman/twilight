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

#ifndef __RENDER_H
#define __RENDER_H

#include "renderer/dyngl.h"
#include "mathlib.h"
#include "model.h"
#include "protocol.h"
#include "renderer/video.h"
#include "renderer/vis.h"
#include "renderer/gl_info.h"
#include "quakedef.h"
#include "matrixlib.h"
#include "renderer/r_part.h"
#include "renderer/alias.h"
#include "renderer/entities.h"
#include "renderer/gl_light.h"
#include "renderer/brush.h"
#include "renderer/gl_main.h"

typedef struct entity_s {
	// model changed
	qboolean		forcelink;

	// to fill in defaults in updates
	entity_state_t	baseline;

	// time of last update
	double			msgtime;

	float			lerp_start_time, lerp_delta_time;

	// last two updates (0 is newest) 
	vec3_t			msg_origins[2];

	// last two updates (0 is newest)
	vec3_t			msg_angles[2];

	// light, particals, etc
	int				effects;

	// last frame this entity was found in an active leaf
	int				visframe;

	entity_common_t	common;
} entity_t;

#endif // __RENDER_H

