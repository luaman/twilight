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

#ifndef __R_PART_H
#define __R_PART_H

#include "qtypes.h"
#include "gl_info.h"
#include "entities.h"

void R_InitParticles(void);
void R_EntityParticles(entity_common_t *ent);
void R_ClearParticles(void);
void R_ReadPointFile_f(void);
void R_ParseParticleEffect(void);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_BlobExplosion(vec3_t org);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_LavaSplash(vec3_t org);
void R_Torch(entity_common_t *ent, qboolean torch2);
void R_RailTrail(vec3_t start, vec3_t end);
void R_Lightning(vec3_t start, vec3_t end, float die);
void R_ParticleTrail(entity_common_t *ent);
void R_MoveParticles(void);
void R_DrawParticles(void);

#endif // __R_PART_H

