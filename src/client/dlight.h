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

#ifndef __DLIGHT_H
#define __DLIGHT_H

#include "qtypes.h"
#include "cvar.h"

#define MAX_DLIGHTS 32
// Note that you can't simply change this value (yet)

typedef struct {
	int			key;					// so entities can reuse same entry
	vec3_t		origin;
	float		radius;
	float		die;					// stop lighting after this time
	float		decay;					// drop this each second
	float		minlight;				// don't add when contributing less
	float		color[3];
} dlight_t;

extern cvar_t *r_dynamic;				// don't rename (compatibility)

extern dlight_t cl_dlights[MAX_DLIGHTS];

extern dlight_t *CCL_AllocDlight (int key);
extern void CCL_DecayLights (void);
extern void CCL_NewDlight (int key, vec3_t org, int effects);

#endif // __DLIGHT_H

