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
		if (model && model->name[0] == '*')
		{
			if (model->type == mod_brush)
			{
				traceline_entity[traceline_entities++] = ent;
				Mod_MinsMaxs (model, ent->origin, ent->angles, ent->mins, ent->maxs);
			}
		}
	}
}

qboolean
CL_Update_OriginAngles (entity_t *ent, vec3_t origin, vec3_t angles, float time)
{
	vec3_t		odelta, adelta;
	qboolean	changed = false;

	if (!ent->lerp_start_time)
	{
		VectorCopy (origin, ent->msg_origins[1]);
		VectorCopy (origin, ent->msg_origins[0]);
		VectorCopy (origin, ent->origin);

		VectorCopy (angles, ent->msg_angles[1]);
		VectorCopy (angles, ent->msg_angles[0]);
		VectorCopy (angles, ent->angles);

		ent->lerp_start_time = time;
		ent->lerp_delta_time = 0;
		changed = true;
	} else {
		VectorSubtract (origin, ent->msg_origins[0], odelta);
		VectorSubtract (angles, ent->msg_angles[0], adelta);

		if (DotProduct(odelta, odelta) + DotProduct(adelta, adelta) > 0.01)
		{
			ent->lerp_delta_time = bound (0, time - ent->lerp_start_time, 1);
			ent->lerp_start_time = time;

			VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
			VectorCopy (origin, ent->msg_origins[0]);
			VectorCopy (origin, ent->origin);
			VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
			VectorCopy (angles, ent->msg_angles[0]);
			VectorCopy (angles, ent->angles);
			changed = true;
		}
	}

	if (changed)
	{
		if (ent->model && ent->model->alias)
			ent->angles[PITCH] = -ent->angles[PITCH];

		if (!ent->lerping)
		{
			Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin, ent->angles, 1);
			if (ent->model && ent->model->alias)
			{
				aliashdr_t	*alias = ent->model->alias;
				Matrix4x4_ConcatTranslate(&ent->matrix, alias->scale_origin);
				Matrix4x4_ConcatScale3(&ent->matrix, alias->scale);
			}
			Matrix4x4_Invert_Simple(&ent->invmatrix, &ent->matrix);
		}
	}

	return changed;
}

void
CL_Lerp_OriginAngles (entity_t *ent)
{
	vec3_t		odelta, adelta;
	float		lerp;

	VectorSubtract (ent->msg_origins[0], ent->msg_origins[1], odelta);
	VectorSubtract (ent->msg_angles[0], ent->msg_angles[1], adelta);

	// Now lerp it.
	if (ent->lerp_delta_time)
	{
		ent->lerping = true;
		lerp = (cl.time - ent->lerp_start_time) / ent->lerp_delta_time;
		if (lerp < 1)
		{
			VectorMA (ent->msg_origins[1], lerp, odelta, ent->origin);
			if (adelta[0] < -180) adelta[0] += 360;else if (adelta[0] >= 180) adelta[0] -= 360;
			if (adelta[1] < -180) adelta[1] += 360;else if (adelta[1] >= 180) adelta[1] -= 360;
			if (adelta[2] < -180) adelta[2] += 360;else if (adelta[2] >= 180) adelta[2] -= 360;
			VectorMA (ent->msg_angles[1], lerp, adelta, ent->angles);
		} else {
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		}
		if (ent->model && ent->model->alias)
			ent->angles[PITCH] = -ent->angles[PITCH];

		Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin, ent->angles, 1);
		if (ent->model && ent->model->alias)
		{
			aliashdr_t	*alias = ent->model->alias;
			Matrix4x4_ConcatTranslate(&ent->matrix, alias->scale_origin);
			Matrix4x4_ConcatScale3(&ent->matrix, alias->scale);
		}
		Matrix4x4_Invert_Simple(&ent->invmatrix, &ent->matrix);
	}
}

qboolean
CL_Update_Frame (entity_t *e, int frame, float frame_time)
{
	qboolean	changed = false;
	aliashdr_t	*paliashdr;
	int			i;

	if (!e->frame_time[0]) {
		changed = true;

		e->frame[0] = e->frame[1] = frame;
		e->frame_time[0] = e->frame_time[1] = frame_time;
		e->frame_frac[0] = 1;
		e->frame_frac[1] = 0;
	} else if (e->frame[0] != frame) {
		changed = true;

		e->frame[1] = e->frame[0];
		e->frame_time[1] = e->frame_time[0];

		e->frame[0] = frame;
		e->frame_time[0] = frame_time;
		goto CL_Update_Frame_frac;
	} else {
CL_Update_Frame_frac:
		e->frame_frac[0] = (cl.time - e->frame_time[0]) * 10;
		e->frame_frac[0] = bound(0, e->frame_frac[0], 1);
		e->frame_frac[1] = 1 - e->frame_frac[0];
	}

	if (!e->model) {
		e->frame[0] = e->frame[1] = 0;
		return changed;
	}

	if ((Uint) e->frame[0] >= (Uint) e->model->numframes) {
		Com_DPrintf("INVALID FRAME %d FOR MODEL %s!\n", e->frame[0],
				e->model->name);
		e->frame[0] = e->frame[1] = 0;
	}

	if (e->model->type == mod_alias) {
		paliashdr = e->model->alias;

		for (i = 0; i < 2; i++) {
			if (paliashdr->frames[e->frame[i]].numposes > 1)
				e->frame_interval[i] = paliashdr->frames[e->frame[i]].interval;
			else
				e->frame_interval[i] = 0.1;
		}
	}

	return changed;
}
