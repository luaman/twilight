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

#include "qtypes.h"
#include "model.h"

extern void R_Draw_Old_Sky_Chain (chain_head_t *chain, vec3_t origin);
extern void R_Draw_Fast_Sky_Chain (chain_head_t *chain, vec3_t origin);
extern void R_Draw_Depth_Sky_Chain (chain_head_t *chain, vec3_t origin);
extern void R_DrawSkyBox (void);
extern void R_Init_Sky (void);
extern void R_Init_Sky_Cvars (void);
extern void R_InitSky (texture_t *unused, Uint8 *pixels);

extern qboolean draw_skybox;

#endif // __R_SKY_H
