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

#include <SDL.h>

#include "twiconfig.h"

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "dlight.h"
#include "host.h"
#include "mathlib.h"
#include "pmove.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "renderer/r_part.h"

int			cl_num_static_entities;
entity_t	cl_static_entities[MAX_STATIC_ENTITIES];

int			cl_num_tmp_entities;
entity_t	cl_tmp_entities[MAX_ENTITIES];

entity_t	cl_network_entities[MAX_EDICTS];
entity_t	cl_player_entities[MAX_CLIENTS];

static int	entity_frame = 0;

/*
=========================================================================

PACKET ENTITY PARSING / LINKING

=========================================================================
*/

/*
==================
Can go from either a baseline or a previous packet_entity
==================
*/
static void
CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits)
{
	int         i;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {			// read in the low order bits
		i = MSG_ReadByte ();
		bits |= i;
	}

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte ();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte ();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte ();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte ();

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord ();

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle ();

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord ();

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle ();

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord ();

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle ();

	if (bits & U_SOLID) {
		// FIXME
	}
}


static void
FlushEntityPacket (void)
{
	int         word;
	entity_state_t olde, newe;

	Com_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof (olde));

	cl.validsequence = 0;				// can't render a frame
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {				// something didn't parse right...
			Host_EndGame ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;						// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

/*
==================
An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void
CL_ParsePacketEntities (qboolean delta)
{
	int         oldpacket, newpacket;
	packet_entities_t *oldp, *newp, dummy;
	int         oldindex, newindex;
	int         word, newnum, oldnum;
	qboolean    full;
	Uint8       from;

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) {
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
			Com_DPrintf ("WARNING: from mismatch\n");
	} else
		oldpacket = -1;

	full = false;
	if (oldpacket != -1) {
		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			FlushEntityPacket ();
			return;
		}
		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
	} else {
		// this is a full update that we can
		// start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	// first update is the final signon stage
	if ((cl.validsequence = cls.netchan.incoming_sequence) &&
			ccls.state == ca_onserver) {		
		ccls.state = ca_active;
		SDL_WM_SetCaption (va("Twilight QWCL: %s", cls.servername),
				"Twilight QWCL");
	}

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {
			// something didn't parse right...
			Host_EndGame ("msg_badread in packetentities");
			return;
		}

		if (!word) {
			while (oldindex < oldp->num_entities) {
				// copy all the rest of the entities from the old packet
				if (newindex >= MAX_PACKET_ENTITIES)
					Host_EndGame
						("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word & 511;
		oldnum =
			oldindex >=
			oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while (newnum > oldnum) {
			if (full) {
				Com_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				return;
			}
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_PACKET_ENTITIES)
				Host_EndGame
					("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum =
				oldindex >=
				oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) {			// new from baseline
			if (word & U_REMOVE) {
				if (full) {
					cl.validsequence = 0;
					Com_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					return;
				}
				continue;
			}
			if (newindex >= MAX_PACKET_ENTITIES)
				Host_EndGame
					("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			CL_ParseDelta (&cl_baselines[newnum], &newp->entities[newindex],
						   word);
			newindex++;
			continue;
		}

		if (newnum == oldnum) {
			// delta from previous
			if (full) {
				cl.validsequence = 0;
				Com_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {
				oldindex++;
				continue;
			}
			CL_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex],
						   word);
			newindex++;
			oldindex++;
		}

	}

	newp->num_entities = newindex;
}

void
CL_Update_Matrices (entity_t *ent)
{
	CL_Update_Matrices_C (&ent->common);
}

void
CL_Update_Matrices_C (entity_common_t *ent)
{
	Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin, ent->angles, 1);
	if (ent->model && ent->model->alias)
	{
		aliashdr_t  *alias = ent->model->alias;
		Matrix4x4_ConcatTranslate(&ent->matrix, alias->scale_origin);
		Matrix4x4_ConcatScale3(&ent->matrix, alias->scale);
	}
	Matrix4x4_Invert_Simple(&ent->invmatrix, &ent->matrix);
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
		lerp = (ccl.time - ent->lerp_start_time) / ent->lerp_delta_time;
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
CL_Update_Frame_C (entity_common_t *e, int frame, float frame_time)
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
		e->frame_frac[0] = (ccl.time - e->frame_time[0]) * 10;
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


static void
CL_LinkPacketEntities (void)
{
	entity_t			*ent;
	packet_entities_t	*pack;
	entity_state_t		*state;
	model_t				*model;
	float				autorotate;
	int					pnum, flags;
	dlight_t			*dl;
	qboolean			moved;
	vec3_t				angles;

	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	autorotate = ANGLEMOD (100 * ccl.time);

	for (pnum = 0; pnum < pack->num_entities; pnum++) {
		state = &pack->entities[pnum];

		if (state->effects & EF_LIGHTMASK) {
			// spawn light flashes, even ones coming from invisible objects
			CCL_NewDlight (state->number, state->origin, state->effects);
		}

		if (state->number >= MAX_EDICTS) {
			Host_EndGame ("ERROR! Entity number >= MAX_EDICTS!!\n");
		}

		ent = &cl_network_entities[state->number];

		if ((ent->entity_frame != (entity_frame - 1)) ||
				(ent->modelindex != state->modelindex) ||
				(VectorDistance_fast(ent->msg_origins[0], state->origin) > sq(512))) {
			memset (ent, 0, sizeof (*ent));
		} else
			ent->times++;

		if (!state->modelindex)
			continue;		// Yes, it IS correct, go away.

		ent->modelindex = state->modelindex;
		ent->common.model = model = ccl.model_precache[state->modelindex];

		ent->common.skinnum = state->skinnum;
		ent->effects = state->effects;
		ent->entity_frame = entity_frame;

		// set colormap
		if (state->colormap && (state->colormap < ccl.max_users)
			&& state->modelindex == cl_playerindex) {
			ent->common.colormap = &ccl.users[state->colormap - 1].color_map;
		} else {
			ent->common.colormap = NULL;
		}

		flags = model->flags;

		VectorCopy(state->angles, angles);

		// rotate binary objects locally
		if (flags & EF_ROTATE) {
			flags &= ~EF_ROTATE;
			angles[YAW] = autorotate;
		}

		moved = CL_Update_OriginAngles (ent, state->origin, angles, ccl.time);
		CL_Lerp_OriginAngles (ent);
		CL_Update_Frame (ent, state->frame, ccl.time);

		R_AddEntity ( &ent->common );

		// add automatic particle trails
		if (!model->flags || !ent->times || !moved)
			continue;

		if (flags & EF_ROCKET) {
			dl = CCL_AllocDlight (state->number);
			VectorCopy (ent->common.origin, dl->origin);
			dl->radius = 200;
			dl->die = ccl.time + 0.1;
			VectorSet (dl->color, 1.0f, 0.6f, 0.2f);
		}

		if (flags)
			R_ParticleTrail (&ent->common);
	}
}


/*
=========================================================================

PROJECTILE PARSING / LINKING

=========================================================================
*/

typedef struct {
	int         modelindex;
	vec3_t      origin;
	vec3_t      angles;
} projectile_t;

#define	MAX_PROJECTILES	32
static projectile_t cl_projectiles[MAX_PROJECTILES];
static int         cl_num_projectiles;

void
CL_ClearProjectiles (void)
{
	cl_num_projectiles = 0;
	memset (cl_projectiles, 0, sizeof(cl_projectiles));
}

/*
=====================
Nails are passed as efficient temporary entities
=====================
*/
void
CL_ParseProjectiles (void)
{
	int         i, c, j;
	Uint8       bits[6];
	projectile_t *pr;

	c = MSG_ReadByte ();
	for (i = 0; i < c; i++) {
		for (j = 0; j < 6; j++)
			bits[j] = MSG_ReadByte ();

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles++];
		pr->modelindex = cl_spikeindex;
		pr->origin[0] = ((bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = 360 * (bits[4] >> 4) / 16;
		pr->angles[1] = 360 * bits[5] / 256;
	}
}

static void
CL_LinkProjectiles (void)
{
	int         i;
	projectile_t *pr;
	entity_t   *ent;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++) {
		if (pr->modelindex < 1)
			continue;
		// grab an entity to fill in
		ent = CL_NewTempEntity ();
		if (!ent)
			break;						// object list is full

		ent->common.model = ccl.model_precache[pr->modelindex];

		CL_Update_OriginAngles (ent, pr->origin, pr->angles, ccl.time);
		CL_Update_Frame (ent, 0, ccl.time);
	}
}

/*
=====================
Static entities are non-interactive world objects
like torches
=====================
*/
void
CL_ParseStatic (void)
{
	entity_t   *ent;
	entity_state_t es;

	CL_ParseBaseline (&es);
	if (!es.modelindex)
		return;

	if ((cl_num_static_entities + 1) >= MAX_STATIC_ENTITIES)
		Host_EndGame ("Too many static entities");
	ent = &cl_static_entities[cl_num_static_entities++];

	// copy it to the current state
	memset (ent, 0, sizeof(*ent));

	ent->common.model = ccl.model_precache[es.modelindex];
	ent->common.frame[0] = ent->common.frame[1] = es.frame;
	ent->common.skinnum = es.skinnum;

	VectorCopy (es.origin, ent->msg_origins[1]);
	VectorCopy (es.angles, ent->msg_angles[1]);
	VectorCopy (es.origin, ent->msg_origins[0]);
	VectorCopy (es.angles, ent->msg_angles[0]);
	VectorCopy (es.origin, ent->common.origin);
	VectorCopy (es.angles, ent->common.angles);
}


//========================================

void
CL_ParsePlayerinfo (void)
{
	int         msec;
	int         flags;
	player_info_t *info;
	player_state_t *state;
	int         num;
	int         i;

	num = MSG_ReadByte ();
	if (num > MAX_CLIENTS)
		Host_EndGame ("CL_ParsePlayerinfo: bad num");

	info = &cl.players[num];

	state = &cl.frames[parsecountmod].playerstate[num];

	state->number = num;
	flags = state->flags = MSG_ReadShort ();

	state->messagenum = cl.parsecount;
	state->origin[0] = MSG_ReadCoord ();
	state->origin[1] = MSG_ReadCoord ();
	state->origin[2] = MSG_ReadCoord ();

	state->frame = MSG_ReadByte ();

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	if (flags & PF_MSEC) {
		msec = MSG_ReadByte ();
		state->state_time = parsecounttime - msec * 0.001;
	} else
		state->state_time = parsecounttime;

	if (flags & PF_COMMAND)
		MSG_ReadDeltaUsercmd (&nullcmd, &state->command);

	for (i = 0; i < 3; i++) {
		if (flags & (PF_VELOCITY1 << i))
			state->velocity[i] = MSG_ReadShort ();
		else
			state->velocity[i] = 0;
	}

	state->modelindex = (flags & PF_MODEL) ? 
		MSG_ReadByte() : cl_playerindex;
	state->skinnum = (flags & PF_SKINNUM) ? 
		MSG_ReadByte() : 0;
	state->effects = (flags & PF_EFFECTS) ? 
		MSG_ReadByte() : 0;
	state->weaponframe = (flags & PF_WEAPONFRAME) ? 
		MSG_ReadByte() : 0;

	VectorCopy (state->command.angles, state->viewangles);
}


/*
================
Called when the CTF flags are set
================
*/
static void
CL_AddFlagModels (entity_t *ent, int team)
{
	int         i;
	float       f;
	vec3_t      v_forward, v_right, v_up, origin, angles;
	entity_t   *newent;

	if (cl_flagindex == -1)
		return;

	f = 14;
	if (ent->common.frame[0] >= 29 && ent->common.frame[0] <= 40) {
		if (ent->common.frame[0] >= 29 && ent->common.frame[0] <= 34) {	// axpain
			if (ent->common.frame[0] == 29)
				f = f + 2;
			else if (ent->common.frame[0] == 30)
				f = f + 8;
			else if (ent->common.frame[0] == 31)
				f = f + 12;
			else if (ent->common.frame[0] == 32)
				f = f + 11;
			else if (ent->common.frame[0] == 33)
				f = f + 10;
			else if (ent->common.frame[0] == 34)
				f = f + 4;
		} else if (ent->common.frame[0] >= 35 && ent->common.frame[0] <= 40) {	// pain
			if (ent->common.frame[0] == 35)
				f = f + 2;
			else if (ent->common.frame[0] == 36)
				f = f + 10;
			else if (ent->common.frame[0] == 37)
				f = f + 10;
			else if (ent->common.frame[0] == 38)
				f = f + 8;
			else if (ent->common.frame[0] == 39)
				f = f + 4;
			else if (ent->common.frame[0] == 40)
				f = f + 2;
		}
	} else if (ent->common.frame[0] >= 103 && ent->common.frame[0] <= 118) {
		if (ent->common.frame[0] >= 103 && ent->common.frame[0] <= 104)
			f = f + 6;					// nailattack
		else if (ent->common.frame[0] >= 105 && ent->common.frame[0] <= 106)
			f = f + 6;					// light 
		else if (ent->common.frame[0] >= 107 && ent->common.frame[0] <= 112)
			f = f + 7;					// rocketattack
		else if (ent->common.frame[0] >= 112 && ent->common.frame[0] <= 118)
			f = f + 7;					// shotattack
	}

	newent = CL_NewTempEntity ();
	newent->common.model = ccl.model_precache[cl_flagindex];
	newent->common.skinnum = team;

	AngleVectors (ent->common.angles, v_forward, v_right, v_up);
	v_forward[2] = -v_forward[2];		// reverse z component
	for (i = 0; i < 3; i++)
		origin[i] = ent->common.origin[i] - f * v_forward[i] + 22 * v_right[i];
	origin[2] -= 16;

	VectorCopy (ent->common.angles, angles);
	angles[2] -= 45;

	CL_Update_OriginAngles (newent, origin, angles, ccl.time);
	CL_Update_Frame (newent, 0, ccl.time);
}

/*
=============
Create visible entities in the correct position
for all current players
=============
*/
static void
CL_LinkPlayers (void)
{
	int         j;
	user_info_t *info;
	player_state_t *state;
	player_state_t exact;
	double      playertime;
	entity_t   *ent;
	int         msec;
	frame_t    *frame;
	vec3_t      org, angles;

	playertime = ccl.time - cls.latency + 0.02;
	playertime = min (playertime, ccl.time);

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, info = ccl.users, state = frame->playerstate;
			j < MAX_CLIENTS;
			j++, info++, state++)
	{
		ent = &cl_player_entities[j];

		// If not present this frame, skip it
		if (state->messagenum != cl.parsecount) {
			memset (ent, 0, sizeof(*ent));
			continue;
		}

		// spawn light flashes
		if ((state->effects & EF_LIGHTMASK) &&
				(!gl_flashblend->ivalue || j != ccl.player_num))
		{
			if (j == ccl.player_num)
				VectorCopy (ccl.player_origin, org);
			else
				VectorCopy (state->origin, org);
			CCL_NewDlight (j, org, state->effects);
		}

		// the player object never gets added
		if (j == ccl.player_num)
			continue;

		if (!state->modelindex)
			continue;

		if (!Cam_DrawPlayer (j))
			continue;

		R_AddEntity ( &ent->common );

		ent->times++;

		ent->common.skinnum = state->skinnum;

		ent->common.model = ccl.model_precache[state->modelindex];
		if (state->modelindex == cl_playerindex) {
			ent->common.colormap = &info->color_map;	// Use custom colormap.
			ent->common.skin = info->skin;				// Use custom skin.
		} else {
			ent->common.colormap = NULL;
			ent->common.skin = NULL;
		}

		// 
		// angles
		// 
		angles[PITCH] = -state->viewangles[PITCH] / 3;
		angles[YAW] = state->viewangles[YAW];
		angles[ROLL] = V_CalcRoll(ent->common.angles, state->velocity) * 4;

		// only predict half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players->ivalue)) {
			CL_Update_OriginAngles (ent, state->origin, angles, ccl.time);
		} else {
			// predict players movement
			state->command.msec = min (state->command.msec, 255);

			CL_PredictUsercmd (j, state, &exact, &state->command, false);
			VectorCopy (exact.origin, ent->common.origin);
			CL_Update_OriginAngles (ent, exact.origin, angles, ccl.time);
		}

//		CL_Lerp_OriginAngles (ent);
		CL_Update_Frame (ent, state->frame, ccl.time);

		if (state->effects & EF_FLAG1)
			CL_AddFlagModels (ent, 0);
		else if (state->effects & EF_FLAG2)
			CL_AddFlagModels (ent, 1);
	}
}

//======================================================================

/*
===============
Builds all the pmove physents for the current frame
===============
*/
void
CL_SetSolidEntities (void)
{
	int					i;
	frame_t				*frame;
	packet_entities_t	*pak;
	entity_state_t		*state;
	physent_t			*pent;
	model_t				*model;

	pmove.physents[0].model = ccl.worldmodel;
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].id = -1;
	pmove.physents[0].info = __LINE__;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!(model = ccl.model_precache[state->modelindex]))
			continue;
		if (!model->brush)
			continue;
		if (!model->brush->hulls[1].firstclipnode)
			continue;
		if (pmove.numphysent >= MAX_PHYSENTS)
			return;

		pent = &pmove.physents[pmove.numphysent++];
		pent->model = model;
		VectorCopy (state->origin, pent->origin);
		pent->id = -1;
		pent->info = __LINE__;
	}
}

/*
===============
Builds all the pmove physents for the current frame
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void
CL_SetSolidPlayers ()
{
	int				i;
	player_state_t	*state;
	physent_t		*pent;
	frame_t			*frame;

	if (!cl_solid_players->ivalue || !cl_predict_players->ivalue)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (i = 0, state = frame->playerstate; i < MAX_CLIENTS; i++, state++) {
		if (pmove.numphysent >= MAX_PHYSENTS)
			return;
		// If not present this frame, skip it
		if (state->messagenum != cl.parsecount)
			continue;

		if (!state->modelindex)
			continue;

		if (state->flags & PF_DEAD)
			continue;					// dead players aren't solid

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		pent = &pmove.physents[pmove.numphysent++];
		if (i == ccl.player_num)
			VectorCopy (cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
						playerstate[ccl.player_num].origin, pent->origin);
		else
			VectorCopy (state->origin, pent->origin);
		pent->id = i;
		pent->model = NULL;
		pent->info = __LINE__;
		VectorCopy (player_mins, pent->mins);
		VectorCopy (player_maxs, pent->maxs);
	}
}

static void
CL_LinkStaticEntites (void)
{
	int		i;

	for (i = 0; i < cl_num_static_entities; i++) {
		cl_static_entities[i].times++;

		CL_Update_OriginAngles (&cl_static_entities[i],
				cl_static_entities[i].msg_origins[0],
				cl_static_entities[i].msg_angles[0], ccl.time);
		CL_Update_Frame (&cl_static_entities[i],
				cl_static_entities[i].common.frame[0], ccl.time);
		R_AddEntity ( &cl_static_entities[i].common );
	}
}

/*
===============
Builds the visedicts array for ccl.time

Made up of: clients, packet_entities, nails, and tents
===============
*/
void
CL_EmitEntities (void)
{
	R_ClearEntities ();

	if (ccls.state != ca_active)
		return;

	if (!cl.validsequence)
		return;

	cl_num_tmp_entities = 0;

	entity_frame++;

	CL_LinkPlayers ();
	CL_LinkProjectiles ();
	CL_LinkStaticEntites ();
	CL_LinkPacketEntities ();
	CL_UpdateTEnts ();
}

