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

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdlib.h>	/* for rand() */

#include "quakedef.h"
#include "client.h"
#include "dlight.h"
#include "model.h"
#include "mathlib.h"
#include "strlib.h"
#include "sound/sound.h"
#include "renderer/explosion.h"
#include "host.h"
#include "renderer/r_part.h"

#define	MAX_BEAMS	8
typedef struct {
	int				entity;
	qboolean		lightning;
	struct model_s	*model;
	float			endtime;
	vec3_t			start, end, diff;
} beam_t;

static beam_t      cl_beams[MAX_BEAMS];

#define	MAX_EXPLOSIONS	8
typedef struct {
	vec3_t      origin;
	float       start;
	model_t    *model;
} explosion_t;

static explosion_t cl_explosions[MAX_EXPLOSIONS];
static cvar_t		*cl_lightning_xbeam;


static sfx_t      *cl_sfx_wizhit;
static sfx_t      *cl_sfx_knighthit;
static sfx_t      *cl_sfx_tink1;
static sfx_t      *cl_sfx_ric1;
static sfx_t      *cl_sfx_ric2;
static sfx_t      *cl_sfx_ric3;
static sfx_t      *cl_sfx_r_exp3;

static model_t	   *cl_bolt1_mod = NULL;
static model_t	   *cl_bolt2_mod = NULL;
static model_t	   *cl_bolt3_mod = NULL;


void
CL_TEnts_Init_Cvars (void)
{
	cl_lightning_xbeam = Cvar_Get ("cl_lightning_xbeam","1",CVAR_ARCHIVE, NULL);
}

void
CL_TEnts_Init (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
}

void
CL_ClearTEnts (void)
{
	memset (&cl_beams, 0, sizeof (cl_beams));
	memset (&cl_explosions, 0, sizeof (cl_explosions));
}

static void
CL_ParseBeam (model_t *m, qboolean lightning)
{
	int         ent;
	vec3_t      start, end;
	beam_t     *b;
	int         i;

	ent = MSG_ReadShort ();

	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();

	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

// override any beam with the same entity
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (b->entity == ent) {
			b->model = m;
			b->endtime = ccl.time + 0.3;
			b->lightning = lightning;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			if (b->entity == cl.viewentity)
				VectorSubtract(ccl.player_origin, start, b->diff);
			return;
		}
	}

// find a free beam
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (!b->model || b->endtime < ccl.time) {
			b->entity = ent;
			b->model = m;
			b->endtime = ccl.time + 0.3;
			b->lightning = lightning;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			if (b->entity == cl.viewentity)
				VectorSubtract(ccl.player_origin, start, b->diff);
			return;
		}
	}

	Com_Printf ("CL_ParseBeam: beam list overflow!\n");
}

void
CL_ParseTEnt (void)
{
	int         type;
	vec3_t      pos;
	dlight_t   *dl;
	int         rnd;
	int         cnt;

	type = MSG_ReadByte ();
	switch (type) {
		case TE_WIZSPIKE:				// spike hitting wall
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 20, 30);
			R_Stain (pos, 32, 40, 96, 40, 32, 96, 128, 96, 32);
			S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
			break;

		case TE_KNIGHTSPIKE:			// spike hitting wall
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 226, 20);
			R_Stain (pos, 32, 64, 64, 64, 32, 192, 192, 192, 32);
			S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
			break;

		case TE_SPIKE:					// spike hitting wall
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 0, 10);
			R_Stain (pos, 32, 64, 64, 64, 32, 192, 192, 192, 32);

			if (rand () % 5)
				S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
			else {
				rnd = rand () & 3;
				if (rnd == 1)
					S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
				else if (rnd == 2)
					S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
				else
					S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
			}
			break;

		case TE_SUPERSPIKE:			// super spike hitting wall
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 0, 20);
			R_Stain (pos, 32, 64, 64, 64, 32, 192, 192, 192, 32);

			if (rand () % 5)
				S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
			else {
				rnd = rand () & 3;
				if (rnd == 1)
					S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
				else if (rnd == 2)
					S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
				else
					S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
			}
			break;

		case TE_EXPLOSION:				// rocket explosion
			// particles
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_NewExplosion (pos);

			// light
			dl = CCL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 350;
			dl->die = ccl.time + 0.5;
			dl->decay = 300;
			dl->color[0] = 1.25f;
			dl->color[1] = 1.0f;
			dl->color[2] = 0.5f;
			R_Stain (pos, 96, 64, 64, 64, 128, 192, 192, 192, 192);

			// sound
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);

			break;

		case TE_TAREXPLOSION:			// tarbaby explosion
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_BlobExplosion (pos);
			R_Stain (pos, 96, 64, 64, 64, 128, 192, 192, 192, 192);

			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			break;

		case TE_LIGHTNING1:			// lightning bolts
			if (!cl_bolt1_mod)
				cl_bolt1_mod = Mod_ForName ("progs/bolt.mdl", FLAG_RENDER | FLAG_CRASH);
			CL_ParseBeam (cl_bolt1_mod, 1);
			break;

		case TE_LIGHTNING2:			// lightning bolts
			if (!cl_bolt2_mod)
				cl_bolt2_mod = Mod_ForName ("progs/bolt2.mdl", FLAG_RENDER | FLAG_CRASH);
			CL_ParseBeam (cl_bolt2_mod, 1);
			break;

		case TE_LIGHTNING3:			// lightning bolts
			if (!cl_bolt3_mod)
				cl_bolt3_mod = Mod_ForName ("progs/bolt3.mdl", FLAG_RENDER | FLAG_CRASH);
			CL_ParseBeam (cl_bolt3_mod, 0);
			break;

		case TE_LAVASPLASH:
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_LavaSplash (pos);
			break;

		case TE_TELEPORT:
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			dl = CCL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 1000;
			dl->die = ccl.time + 99;
			dl->decay = 3000;
			dl->color[0] = 1.25f;
			dl->color[1] = 1.25f;
			dl->color[2] = 1.25f;
			break;

		case TE_GUNSHOT:				// bullet hitting wall
			cnt = MSG_ReadByte ();
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 0, 20 * cnt);
			R_Stain (pos, 5 * cnt, 64, 64, 64, 72, 192, 192, 192, 72);
			break;

		case TE_BLOOD:					// bullets hitting body
			cnt = MSG_ReadByte ();
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 73, 20 * cnt);
			break;

		case TE_LIGHTNINGBLOOD:		// lightning hitting body
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			R_RunParticleEffect (pos, vec3_origin, 225, 50);
			break;

		default:
			Host_EndGame ("CL_ParseTEnt: bad type");
	}
}


entity_t   *
CL_NewTempEntity (void)
{
	entity_t   *ent;

	if ((cl_num_tmp_entities + 1) >= MAX_ENTITIES)
		Host_EndGame ("Out of entities!");

	ent = &cl_tmp_entities[cl_num_tmp_entities++];

	R_AddEntity ( &ent->common );

	memset (ent, 0, sizeof (*ent));

	return ent;
}


static void
CL_UpdateBeams (void)
{
	int         i;
	beam_t     *b;
	vec3_t      dist, org;
	float       d;
	entity_t   *ent;
	vec3_t		ang;

// update lightning
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (!b->model || b->endtime < ccl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity) {
			VectorAdd (ccl.player_origin, b->diff, b->start);
		}

		if (b->lightning) {
			R_Lightning (b->start, b->end, 0);
			continue;
		}

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);
		Vector2Angles (dist, ang);

		// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalize (dist);

		while (d > 0) {
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			ang[2] = rand () % 360;
			ent->common.model = b->model;
			CL_Update_OriginAngles(ent, org, ang, ccl.time);
			CL_Update_Frame(ent, 0, ccl.time);

			for (i = 0; i < 3; i++)
				org[i] += dist[i] * 30;
			d -= 30;
		}
	}

}

static void
CL_UpdateExplosions (void)
{
	int         i;
	int         f;
	explosion_t *ex;
	entity_t   *ent;

	for (i = 0, ex = cl_explosions; i < MAX_EXPLOSIONS; i++, ex++) {
		if (!ex->model)
			continue;
		f = 10 * (ccl.time - ex->start);
		if (f >= ex->model->numframes) {
			ex->model = NULL;
			continue;
		}

		ent = CL_NewTempEntity ();
		if (!ent)
			return;
		ent->common.model = ex->model;
		CL_Update_OriginAngles(ent, ex->origin, vec3_origin, ccl.time);
		CL_Update_Frame (ent, f, ccl.time);
	}
}

void
CL_UpdateTEnts (void)
{
	CL_UpdateBeams ();
	CL_UpdateExplosions ();
}
