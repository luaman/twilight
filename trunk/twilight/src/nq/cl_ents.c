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

static entity_t *traceline_entity[MAX_EDICTS];
static int traceline_entities;

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
		model = ent->common.model;
		// look for embedded brush models only
		if (model && model->name[0] == '*')
		{
			if (model->type == mod_brush)
			{
				traceline_entity[traceline_entities++] = ent;
				Mod_MinsMaxs (model, ent->common.origin, ent->common.angles, ent->common.mins, ent->common.maxs);
			}
		}
	}
}

void
CL_Update_Matrices (entity_t *ent)
{
	CL_Update_Matrices_C (&ent->common);
}

void
CL_Update_Matrices_C (entity_common_t *cent)
{
	Matrix4x4_CreateFromQuakeEntity(&cent->matrix,cent->origin,cent->angles,1);
	if (cent->model && cent->model->alias)
	{
		aliashdr_t	*alias = cent->model->alias;
		Matrix4x4_ConcatTranslate(&cent->matrix, alias->scale_origin);
		Matrix4x4_ConcatScale3(&cent->matrix, alias->scale);
	}
	Matrix4x4_Invert_Simple(&cent->invmatrix, &cent->matrix);
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
		VectorCopy (origin, ent->common.origin);

		VectorCopy (angles, ent->msg_angles[1]);
		VectorCopy (angles, ent->msg_angles[0]);
		VectorCopy (angles, ent->common.angles);

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
			VectorCopy (origin, ent->common.origin);
			VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
			VectorCopy (angles, ent->msg_angles[0]);
			VectorCopy (angles, ent->common.angles);
			changed = true;
		}
	}

	if (changed)
	{
		if (ent->common.model && ent->common.model->alias)
			ent->common.angles[PITCH] = -ent->common.angles[PITCH];

		if (!ent->common.lerping)
			CL_Update_Matrices (ent);
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
		ent->common.lerping = true;
		lerp = (cl.time - ent->lerp_start_time) / ent->lerp_delta_time;
		if (lerp < 1)
		{
			VectorMA (ent->msg_origins[1], lerp, odelta, ent->common.origin);
			if (adelta[0] < -180) adelta[0] += 360;else if (adelta[0] >= 180) adelta[0] -= 360;
			if (adelta[1] < -180) adelta[1] += 360;else if (adelta[1] >= 180) adelta[1] -= 360;
			if (adelta[2] < -180) adelta[2] += 360;else if (adelta[2] >= 180) adelta[2] -= 360;
			VectorMA (ent->msg_angles[1], lerp, adelta, ent->common.angles);
		} else {
			VectorCopy (ent->msg_origins[0], ent->common.origin);
			VectorCopy (ent->msg_angles[0], ent->common.angles);
		}
		if (ent->common.model && ent->common.model->alias)
			ent->common.angles[PITCH] = -ent->common.angles[PITCH];

		CL_Update_Matrices (ent);
	}
}

qboolean
CL_Update_Frame (entity_t *e, int frame, float frame_time)
{
	return CL_Update_Frame_C (&e->common, frame, frame_time);
}

qboolean
CL_Update_Frame_C (entity_common_t *ce, int frame, float frame_time)
{
	qboolean	changed = false;
	aliashdr_t	*paliashdr;
	int			i;

	if (!ce->frame_time[0]) {
		changed = true;

		ce->frame[0] = ce->frame[1] = frame;
		ce->frame_time[0] = ce->frame_time[1] = frame_time;
		ce->frame_frac[0] = 1;
		ce->frame_frac[1] = 0;
	} else if (ce->frame[0] != frame) {
		changed = true;

		ce->frame[1] = ce->frame[0];
		ce->frame_time[1] = ce->frame_time[0];

		ce->frame[0] = frame;
		ce->frame_time[0] = frame_time;
		goto CL_Update_Frame_frac;
	} else {
CL_Update_Frame_frac:
		ce->frame_frac[0] = (cl.time - ce->frame_time[0]) * 10;
		ce->frame_frac[0] = bound(0, ce->frame_frac[0], 1);
		ce->frame_frac[1] = 1 - ce->frame_frac[0];
	}

	if (!ce->model) {
		ce->frame[0] = ce->frame[1] = 0;
		return changed;
	}

	if ((Uint) ce->frame[0] >= (Uint) ce->model->numframes) {
		Com_DPrintf("INVALID FRAME %d FOR MODEL %s!\n", ce->frame[0],
				ce->model->name);
		ce->frame[0] = ce->frame[1] = 0;
	}

	if (ce->model->type == mod_alias) {
		paliashdr = ce->model->alias;

		for (i = 0; i < 2; i++) {
			if (paliashdr->frames[ce->frame[i]].numposes > 1)
				ce->frame_interval[i] =paliashdr->frames[ce->frame[i]].interval;
			else
				ce->frame_interval[i] = 0.1;
		}
	}

	return changed;
}
