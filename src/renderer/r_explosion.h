/*
	$RCSfile$

	Copyright (C) 2002  Joseph Carter

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

#ifndef __R_EXPLOSION_H
#define __R_EXPLOSION_H

#include "qtypes.h"

void R_Explosion_Shutdown (void);
void r_explosion_newmap(void);
void R_Explosion_Init(void);
void R_NewExplosion(vec3_t org);
void R_MoveExplosions(void);
void R_DrawExplosions(void);

#endif // __R_EXPLOSION_H

