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

#ifndef __GL_BRUSH_H
#define __GL_BRUSH_H

#include "entities.h"
#include "gl_info.h"
#include "model.h"
#include "qtypes.h"

void R_Stain(vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);
void R_DrawBrushDepthSkies(void);
void R_DrawLiquidTextureChains(model_t *mod, qboolean arranged);
void R_DrawTextureChains(model_t *mod, int frame, matrix4x4_t *matrix, matrix4x4_t *invmatrix);
void R_VisBrushModel(entity_common_t *e);
void R_DrawOpaqueBrushModel(entity_common_t *e);
void R_DrawAddBrushModel(entity_common_t *e);
qboolean R_VisBrushModels(void);
void R_DrawOpaqueBrushModels(void);
void R_DrawAddBrushModels(void);

#endif // __GL_BRUSH_H

