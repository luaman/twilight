/*
	$RCSfile$

	Copyright (C) 2002  Forest Hale

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

#include "quakedef.h"
#include "client.h"
#include "model.h"
#include "render.h"

entity_t *traceline_entity[MAX_EDICTS];
int traceline_entities;

/*
================
CL_ScanForBModels
================
*/
void
CL_ScanForBModels (void)
{
	int			i;
	entity_t	*ent;
	model_t		*model;

	traceline_entities = 0;
	for (i = 1; i < MAX_EDICTS; i++)
	{
		ent = &cl_entities[i];
		model = ent->model;
		// look for embedded brush models only
		if (model && model->name[0] == '*' && model->type == mod_brush)
		{
			traceline_entity[traceline_entities++] = ent;
			Mod_MinsMaxs(model, ent->origin, ent->angles, ent->mins, ent->maxs);
		}
	}
}

