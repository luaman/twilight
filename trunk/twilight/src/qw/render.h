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

#include "dyngl.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "model.h"
#include "wad.h"
#include "video.h"
#include "vis.h"
#include "gl_info.h"
#include "gl_arrays.h"
#include "entities.h"
#include "gl_light.h"
#include "gl_brush.h"
#include "gl_main.h"


typedef struct entity_s {
	int				effects;

	int				modelindex;
	int				entity_frame;
	unsigned int	times;

	float			lerp_start_time, lerp_delta_time;

	// Last two updates. (0 is the newest)
	vec3_t			msg_origins[2];

	// Last two updates. (0 is the newest)
	vec3_t			msg_angles[2];

	entity_common_t	common;
} entity_t;

#endif // __RENDER_H

