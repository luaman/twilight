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

#ifndef __GL_RLIGHT_H
#define __GL_RLIGHT_H

#include "cclient.h"
#include "gl_info.h"
#include "matrixlib.h"
#include "model.h"
#include "qtypes.h"

// Light sources are points, use this to pretend they are not
#define LIGHTOFFSET (32 * 32)

// Note that you can't simply change this value (yet)
#define MAX_DLIGHTS 32

typedef struct
{
	vec3_t	origin;

	// only for culling comparisons
	vec_t	cullradius;

	// only for culling comparisons, squared version
	vec_t	cullradius2;

	// the brightness of the light
	vec4_t	light;

	// to avoid sudden brightness change at cullradius, subtract this
	vec_t	lightsubtract;
} rdlight_t;

extern int dlightdivtable[32768];
extern mplane_t *lightplane;
extern vec3_t lightspot;

void GL_Light_Tables_Init (void);
void R_DrawCoronas(void);
void R_AnimateLight(void);
void R_BuildLightList(void);
void R_MarkLightsNoVis(vec3_t lightorigin, rdlight_t *rd, int bit, model_t *mod, mnode_t *node);
void R_MarkLights(rdlight_t *rd, int bit, model_t *model, matrix4x4_t *invmatrix);
void R_PushDlights(void);
void R_LightPoint (vec3_t p, vec3_t out);
void GL_UpdateLightmap(model_t *mod, msurface_t *fa, matrix4x4_t *invmatrix);
void R_BuildLightList (void);

#endif // __GL_RLIGHT_H

