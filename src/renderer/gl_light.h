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

void GL_UpdateLightmap (model_t *mod, msurface_t *fa, matrix4x4_t *invmatrix);

#endif // __GL_RLIGHT_H

