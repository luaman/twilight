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

#include "qtypes.h"
#include "gl_info.h"

typedef struct {
	int         key;                    // so entities can reuse same entry
	vec3_t      origin;
	float       radius;
	float       die;                    // stop lighting after this time
	float       decay;                  // drop this each second
	float       minlight;               // don't add when contributing less
	float       color[3];
} dlight_t;

extern int dlightdivtable[32768];
extern rdlight_t r_dlight[32];
extern int r_numdlights;
extern mplane_t *lightplane;
extern vec3_t lightspot;
extern vec3_t lightcolor;

void R_InitSurf(void);
void R_InitLightTextures(void);
void R_DrawCoronas(void);
void R_AnimateLight(void);
void R_BuildLightList(void);
void R_InitBubble(void);
void R_MarkLightsNoVis(vec3_t lightorigin, rdlight_t *rd, int bit, model_t *mod, mnode_t *node);
void R_MarkLights(rdlight_t *rd, int bit, model_t *model, matrix4x4_t *invmatrix);
void R_PushDlights(void);
int R_LightPoint(vec3_t p);
void GL_UpdateLightmap(model_t *mod, msurface_t *fa, matrix4x4_t *invmatrix);

#endif // __GL_RLIGHT_H

