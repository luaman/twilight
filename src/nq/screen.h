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

#ifndef __SCREEN_H
#define __SCREEN_H

#include "qtypes.h"
#include "video.h"

extern float	scr_con_current;
extern viddef_t	vid;
extern qboolean	scr_disabled_for_loading;
extern float	scr_centertime_off;
extern int		fps_count, fps_capped0, fps_capped1;

void SCR_CenterPrint(const char *str);
void SCR_Init_Cvars(void);
void SCR_Init(void);
void SCR_BeginLoadingPlaque(void);
void SCR_EndLoadingPlaque(void);
int MipColor(int r, int g, int b);
void SCR_UpdateScreen(void);

#endif // __SCREEN_H

