/*
	$RCSfile$

	Copyright (C) 1999  Sam Lantinga
	Copyright (C) 2001  Joseph Carter

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

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"
#include "SDL.h"

#include <string.h>

#include "entities.h"
#include "host.h"
#include "sky.h"
#include "gl_info.h"
#include "gl_alias.h"
#include "gl_brush.h"
#include "gl_sprite.h"
#include "gl_main.h"
#include "vis.h"

static qboolean need_sky;

void
R_AddEntity (entity_common_t *ent)
{
	if ( r.num_entities >= MAX_ENTITIES ) {
		Host_EndGame ("ERROR! Out of entitys!");
	}

	r.entities[r.num_entities++] = ent;
}

void
R_ClearEntities (void)
{
	r.num_entities = 0;
}

void
R_VisEntities (void)
{
	need_sky = R_VisBrushModels ();
#if 0
	R_VisAliasModels ();
	R_VisSpriteModels ();
#endif
}

void
R_DrawSkyEntities (void)
{
	if (sky_type != SKY_FAST && need_sky) {
		R_DrawBrushDepthSkies ();
		if (sky_type == SKY_BOX)
			Sky_Box_Draw ();
		else if (sky_type == SKY_SPHERE)
			Sky_Sphere_Draw ();
	}
}

void
R_DrawOpaqueEntities (void)
{
	R_DrawOpaqueBrushModels ();
	R_DrawOpaqueAliasModels (r.entities, r.num_entities, false);
	R_DrawOpaqueSpriteModels ();
}

void
R_DrawAddEntities (void)
{
	R_DrawAddBrushModels ();
#if 0
	R_DrawAddAliasModels ();
	R_DrawAddSpriteModels ();
#endif
}
