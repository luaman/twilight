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

#ifndef __R_SURFACE_H
#define __R_SURFACE_H

#include "model.h"

void CountSubdividedGLPolyFromEdges (msurface_t *surf, model_t *model);
void CountGLPolyFromEdges (msurface_t *surf, model_t *model);
void BuildSubdividedGLPolyFromEdges (msurface_t *surf, model_t *model);
void BuildGLPolyFromEdges (msurface_t *surf, model_t *model, int *count);
void SetupLightmapSettings ();

#endif // __R_SURFACE_H
