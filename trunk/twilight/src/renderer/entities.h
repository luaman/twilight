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

#ifndef __ENTITIES_H
#define __ENTITIES_H

#include "mathlib.h"
#include "dyngl.h"
#include "qtypes.h"
#include "matrixlib.h"
#include "gl_info.h"
#include "model.h"
#include "palette.h"

#define MAX_ENTITIES	1024

typedef struct entity_common_s {
	// Origin and angles.
	vec3_t			origin, angles;

	// Matrix and invmatrix.
	matrix4x4_t		matrix, invmatrix;

	// NULL = no model.
	int				frame[2];
	float			frame_frac[2];
	float			frame_time[2];
	float			frame_interval[2];
	qboolean		lerping;

	// Model.
	struct model_s	*model;

	// Bounding boxes.
	vec3_t			mins;
	vec3_t			maxs;

	// Skin of the model, if any.
	skin_t			*skin;
	int				skinnum;

	// Colormap for the model, if any.
	colormap_t		*colormap;

	// For trails: (FIXME: Better place?
	vec3_t			trail_old_org;
	float			trail_len;
	int				trail_times;

	// For model light lerping.
	vec3_t			last_light;
	// For torches.
	float			time_left;

	float			syncbase;
} entity_common_t;

void R_AddEntity(entity_common_t *ent);
void R_ClearEntities(void);
void R_VisEntities(void);
void R_DrawSkyEntities(void);
void R_DrawOpaqueEntities(void);
void R_DrawAddEntities(void);

#endif // __ENTITIES_H
