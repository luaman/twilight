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

#ifndef __VID_H
#define __VID_H

#include "qtypes.h"
#include "cvar.h"

typedef struct vrect_s
{
	int			x, y;
	int			width, height;
} vrect_t;

typedef struct
{
	Uint32		width;
	Uint32		height;
	Uint32		width_2d;
	Uint32		height_2d;

	Uint32		bpp;
} viddef_t;

extern viddef_t	vid;
extern qboolean VID_Inited;
extern float mouse_x;
extern float mouse_y;
extern int sdl_flags;

void GL_EndRendering(void);
void Size_Changed2D(cvar_t *cvar);
void VID_Init_Cvars(void);
void VID_Init (void);
void VID_Shutdown(void);

#endif // __VID_H
