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

#ifndef __R_SKY_H
#define __R_SKY_H

#include "image/image.h"
#include "model.h"
#include "qtypes.h"

typedef enum {
	SKY_SPHERE, SKY_BOX, SKY_FAST,
} sky_type_t;

extern cvar_t *r_skyname;
extern sky_type_t	sky_type;

void Sky_Fast_Draw_Chain(model_t *mod, chain_head_t *chain);
void Sky_Depth_Draw_Chain(model_t *mod, chain_head_t *chain);
void Sky_Sphere_Draw(void);
void Sky_Box_Draw(void);
void Sky_InitSky (image_t *img);
void Sky_Init(void);
void Sky_Shutdown (void);
void Sky_Init_Cvars(void);

#endif // __R_SKY_H
