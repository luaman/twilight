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

#ifndef __R_LIQUID_H
#define __R_LIQUID_H

#include "cvar.h"
#include "dyngl.h"
#include "mathlib.h"
#include "model.h"

extern cvar_t *r_wateralpha;
void R_Liquid_Init_Cvars(void);
void R_Draw_Liquid_Chain(model_t *mod, chain_head_t *chain, qboolean arranged);

#endif // __R_LIQUID_H
