/*
	$RCSfile$

	Copyright (C) 2002  Forest Hale

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

#ifndef __RENDERER_LIGHT_H
#define __RENDERER_LIGHT_H

#include "model.h"

// Light sources are points, use this to pretend they are not
#define LIGHTOFFSET 32 * 32

// Note that you can't simply change this value (yet)
#define MAX_DLIGHTS 32

typedef struct
{
	vec3_t				origin;

	// only for culling comparisons, squared version
	vec_t				cullradius2;

	// the brightness of the light
	vec4_t				light;

	// only for culling comparisons
	vec_t				cullradius;

	// to avoid sudden brightness change at cullradius, subtract this
	vec_t				lightsubtract;
}
rdlight_t;

extern int r_numdlights;
extern rdlight_t r_dlight[MAX_DLIGHTS];


void R_InitLightTextures (void);
void R_BuildLightList (void);
void R_MarkLights (rdlight_t *light, int bit, model_t *model);
void R_MarkLightsNoVis (rdlight_t *light, int bit, mnode_t *node);
void R_AnimateLight (void);
int R_LightPoint (vec3_t p);
void R_PushDlights (void);
void R_DrawCoronas (void);

#endif // __RENDERER_LIGHT_H

