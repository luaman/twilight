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

#ifndef __QTYPES_H
#define __QTYPES_H

#include "SDL_types.h"

// KJB Undefined true and false defined in SciTech's DEBUG.H header
#undef true
#undef false

typedef enum { false, true } qboolean;

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

// plane_t structure
typedef struct mplane_s {
	vec3_t      normal;
	float       dist;
	Uint8       type;					// for texture axis selection and fast
	// side tests
	Uint8       signbits;				// signx + signy<<1 + signz<<1
	Uint8       pad[2];
} mplane_t;

#endif // __QTYPES_H

