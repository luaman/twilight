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

#ifndef __HUD_H
#define __HUD_H

#include "cvar.h"

extern int sb_lines;            // scan lines to draw
extern cvar_t *cl_sbar;

void		HUD_Changed (cvar_t *unused);

void		HUD_Init_Cvars (void);
void		HUD_Init (void);
void		HUD_Shutdown (void);

// call whenever any of the client stats represented on the sbar changes

void		HUD_Draw (void);

// called every frame by screen

void		HUD_IntermissionOverlay (void);

// called each frame after the level has been completed

void		HUD_FinaleOverlay (void);

#endif // __HUD_H

