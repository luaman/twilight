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
// cl_ents.c -- entity parsing and management
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "pmove.h"
#include "view.h"
#include "sys.h"

int			cl_num_static_entities;
entity_t	cl_static_entities[MAX_STATIC_ENTITIES];

int			cl_num_tmp_entities;
entity_t	cl_tmp_entities[MAX_ENTITIES];

entity_t	cl_network_entities[MAX_EDICTS];
entity_t	cl_player_entities[MAX_CLIENTS];

static int	entity_frame = 0;

extern cvar_t *cl_predict_players;
extern cvar_t *cl_solid_players;

extern int  cl_spikeindex, cl_playerindex, cl_flagindex;

extern void CL_ParseBaseline (entity_state_t *es);
extern entity_t   *CL_NewTempEntity (void);

static struct predicted_player {
	int         flags;
	qboolean    active;
	vec3_t      origin;					// predicted origin
} predicted_players[MAX_CLIENTS];

//============================================================

/*
===============
CL_AllocDlight

===============
*/
dlight_t   *
CL_AllocDlight (int key)
{
	int         i;
	dlight_t   *dl;

// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof (*dl));
				dl->key = key;
				dl->color[0] = 0.2f; 
				dl->color[1] = 0.1f; 
				dl->color[2] = 0.0f;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time) {
			memset (dl, 0, sizeof (*dl));
			dl->key = key;
			dl->color[0] = 0.2f; 
			dl->color[1] = 0.1f; 
			dl->color[2] = 0.0f;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
	dl->color[0] = 0.2f; 
	dl->color[1] = 0.1f; 
	dl->color[2] = 0.0f;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
void
CL_NewDlight (int key, vec3_t org, int effects)
{
	dlight_t   *dl = CL_AllocDlight (key);

	if (effects & EF_BRIGHTLIGHT)
		dl->radius = 400 + (Q_rand () & 31);
	else
		dl->radius = 200 + (Q_rand () & 31);

	dl->die = cl.time + 0.1;
	VectorCopy (org, dl->origin);

	dl->color[0] = 0.44;
	dl->color[1] = 0.34;
	dl->color[2] = 0.24;

	if (effects & EF_RED)
		dl->color[0] = 0.86;
	if (effects & EF_BLUE)
		dl->color[2] = 0.86;

	if (!(effects & (EF_LIGHTMASK - EF_DIMLIGHT))) {
		dl->color[0] += 0.20;
		dl->color[1] += 0.10;
	}

	dl->color[0] = bound ( 0, dl->color[0], 1 );
	dl->color[1] = bound ( 0, dl->color[1], 1 );
	dl->color[2] = bound ( 0, dl->color[2], 1 );
}


/*
===============
CL_DecayLights

===============
*/
void
CL_DecayLights (void)
{
	int         i;
	dlight_t   *dl;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= host_frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
==============
CL_BoundDlight
==============
*/
void
CL_BoundDlight (dlight_t *dl, vec3_t org)
{
	pmtrace_t tr;

	if (gl_flashblend->ivalue || gl_oldlights->ivalue)
		return;

	memset (&tr, 0, sizeof(tr));
	VectorCopy (dl->origin, tr.endpos);
	PM_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, org, dl->origin, &tr);
	VectorCopy (tr.endpos, dl->origin);
}

/*
=========================================================================

PACKET ENTITY PARSING / LINKING

=========================================================================
*/

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
void
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


/*
=================
FlushEntityPacket
=================
*/
void
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
CL_ParsePacketEntities

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

	cl.validsequence = cls.netchan.incoming_sequence;

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

float
CL_CalcBlend (float from_time, float to_time, float interval)
{
	float f;

	f = to_time - from_time;

	return bound(0, f / interval, 1);
}

qboolean
CL_Update_Origin (entity_t *ent, vec3_t origin, float origin_time)
{
	qboolean changed = false;

	if (!ent->times) {
		VectorCopy(origin, ent->from.origin);
		VectorCopy(origin, ent->to.origin);
		ent->from.origin_time = origin_time;
		ent->to.origin_time = origin_time;
	}

	if (!VectorCompare(ent->to.origin, origin)) {
		changed = true;

		VectorCopy(ent->to.origin, ent->from.origin);
		ent->from.origin_time = ent->to.origin_time;

		VectorCopy(origin, ent->to.origin);
	}
	ent->to.origin_time = origin_time;

	VectorCopy (ent->to.origin, ent->cur.origin);

	return changed;
}

void
CL_Lerp_Origin (entity_t *ent, vec3_t origin, float origin_time)
{
	int f;

	f = CL_CalcBlend (ent->from.origin_time, ent->to.origin_time, .1);
	Lerp_Vectors (ent->from.origin, f, ent->to.origin, ent->cur.origin);
}

qboolean
CL_UpdateAndLerp_Origin (entity_t *ent, vec3_t origin, float origin_time)
{
	qboolean changed;

	changed = CL_Update_Origin(ent, origin, origin_time);
	CL_Lerp_Origin(ent, origin, origin_time);

	return changed;
}

qboolean
CL_Update_Angles (entity_t *ent, vec3_t angles, float angles_time)
{
	qboolean changed = false;

	if (!ent->times) {
		VectorCopy(angles, ent->from.angles);
		VectorCopy(angles, ent->to.angles);
		ent->from.angles_time = angles_time;
		ent->to.angles_time = angles_time;
	}

	if (!VectorCompare(ent->to.angles, angles)) {
		changed = true;

		VectorCopy(ent->to.angles, ent->from.angles);
		ent->from.angles_time = ent->to.angles_time;

		VectorCopy(angles, ent->to.angles);
	}
	ent->to.angles_time = angles_time;

	VectorCopy (ent->to.angles, ent->cur.angles);

	return changed;
}

void
CL_Lerp_Angles (entity_t *ent, vec3_t angles, float angles_time)
{
	int f;

	f = CL_CalcBlend (ent->from.angles_time, ent->to.angles_time, .1);
	Lerp_Angles (ent->from.angles, f, ent->to.angles, ent->cur.angles);
}

qboolean
CL_UpdateAndLerp_Angles (entity_t *ent, vec3_t angles, float angles_time)
{
	qboolean changed = false;

	changed = CL_Update_Angles(ent, angles, angles_time);
	CL_Lerp_Angles(ent, angles, angles_time);

	return changed;
}

qboolean
CL_Update_Frame (entity_t *e, int frame, float frame_time)
{
	qboolean changed = false;
	aliashdr_t *paliashdr;

	if (!e->times) {
		e->cur.frame = e->from.frame = e->to.frame = frame;
		e->cur.frame_time = e->from.frame_time = e->to.frame_time = frame_time;
	}

	if (e->to.frame != frame) {
		changed = true;

		e->from.frame = e->to.frame;
		e->from.frame_time = e->to.frame_time;

		e->cur.frame = e->to.frame = frame;
	}
	e->cur.frame_time = e->to.frame_time = frame_time;

	if ((unsigned int) e->to.frame >= (unsigned int) e->model->numframes) {
		Com_DPrintf("INVALID FRAME %d FOR MODEL %s!\n", e->to.frame,
				e->model->name);
		e->cur.frame = e->to.frame = e->from.frame = 0;
	}

	if (e->model->type == mod_alias) {
		paliashdr = (aliashdr_t *) Mod_Extradata (e->model);

		if (paliashdr->frames[e->from.frame].numposes > 1)
			e->from.frame_interval = paliashdr->frames[e->from.frame].interval;
		else
			e->from.frame_interval = 0.1;

		if (paliashdr->frames[e->to.frame].numposes > 1)
			e->to.frame_interval = paliashdr->frames[e->to.frame].interval;
		else
			e->to.frame_interval = 0.1;
	}

	e->frame_blend = 1;

	return changed;
}

void
CL_Lerp_Frame (entity_t *e, int frame, float frame_time)
{
	if (e->from.frame_interval || e->to.frame_interval)
		e->frame_blend = CL_CalcBlend (e->from.frame_time, e->to.frame_time,
				(e->from.frame_interval + e->to.frame_interval) / 2);
}

qboolean
CL_UpdateAndLerp_Frame (entity_t *ent, int frame, float frame_time)
{
	qboolean changed = true;

	changed = CL_Update_Frame(ent, frame, frame_time);
	CL_Lerp_Frame(ent, frame, frame_time);

	return changed;
}


/*
===============
CL_LinkPacketEntities

===============
*/
void
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

	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	autorotate = ANGLEMOD (100 * cl.time);

	for (pnum = 0; pnum < pack->num_entities; pnum++) {
		state = &pack->entities[pnum];

		if (state->effects & EF_LIGHTMASK) {
			// spawn light flashes, even ones coming from invisible objects
			CL_NewDlight (state->number, state->origin, state->effects);
		}

		if (state->number >= MAX_EDICTS) {
			Host_EndGame ("ERROR! Entity number >= MAX_EDICTS!!\n");
		}

		ent = &cl_network_entities[state->number];

		if ((ent->entity_frame != (entity_frame - 1)) ||
				(ent->modelindex != state->modelindex)) {
			memset (ent, 0, sizeof (*ent));
		} else {
			ent->times++;
		}

		if (!state->modelindex)
			continue;		// Yes, it IS correct, go away.

		ent->modelindex = state->modelindex;
		ent->model = model = cl.model_precache[state->modelindex];	// Model.

		moved = CL_UpdateAndLerp_Origin (ent, state->origin, cls.realtime);
		CL_UpdateAndLerp_Angles (ent, state->angles, cls.realtime);
		CL_UpdateAndLerp_Frame (ent, state->frame, cls.realtime);

		ent->skinnum = state->skinnum;
		ent->effects = state->effects;
		ent->entity_frame = entity_frame;

		// set colormap
		if (state->colormap && (state->colormap < MAX_CLIENTS)
			&& state->modelindex == cl_playerindex) {
			ent->colormap = &cl.players[state->colormap - 1].colormap;
		} else {
			ent->colormap = NULL;
		}

		flags = model->flags;

		// rotate binary objects locally
		if (flags & EF_ROTATE) {
			flags &= ~EF_ROTATE;
			VectorSet (ent->cur.angles, 0, autorotate, 0);
		}

		V_AddEntity ( ent );

		// add automatic particle trails
		if (!model->flags || !ent->times || !moved)
			continue;

		if (flags & EF_ROCKET) {
			flags &= ~EF_ROCKET;
			R_RocketTrail (ent->from.origin, ent->to.origin);
			dl = CL_AllocDlight (state->number);
			VectorCopy (ent->cur.origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.1;
		}

		if (flags)
			R_ParticleTrail (ent->from.origin, ent->to.origin, flags);
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
projectile_t cl_projectiles[MAX_PROJECTILES];
int         cl_num_projectiles;

extern int  cl_spikeindex;

void
CL_ClearProjectiles (void)
{
	cl_num_projectiles = 0;
	memset (cl_projectiles, 0, sizeof(cl_projectiles));
}

/*
=====================
CL_ParseProjectiles

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

/*
=============
CL_LinkProjectiles

=============
*/
void
CL_LinkProjectiles (void)
{
	int         i;
	projectile_t *pr;
	entity_t   *ent;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++) {
		// grab an entity to fill in
		ent = CL_NewTempEntity ();
		if (!ent)
			break;						// object list is full

		if (pr->modelindex < 1)
			continue;
		ent->model = cl.model_precache[pr->modelindex];

		CL_Update_Origin (ent, pr->origin, cls.realtime);
		CL_Update_Angles (ent, pr->angles, cls.realtime);
		CL_Update_Frame (ent, 0, cls.realtime);
	}
}

/*
=====================
CL_ParseStatic

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

	if ((cl_num_static_entities + 1) >= MAX_STATIC_ENTITIES)
		Host_EndGame ("Too many static entities");
	ent = &cl_static_entities[cl_num_static_entities++];

	// copy it to the current state
	memset (ent, 0, sizeof(*ent));

	ent->model = cl.model_precache[es.modelindex];
	ent->to.frame = ent->from.frame = ent->cur.frame = es.frame;
	ent->skinnum = es.skinnum;

	VectorCopy (es.origin, ent->to.origin);
	VectorCopy (es.angles, ent->to.angles);
	VectorCopy (es.origin, ent->cur.origin);
	VectorCopy (es.angles, ent->cur.angles);
	VectorCopy (es.origin, ent->from.origin);
	VectorCopy (es.angles, ent->from.angles);
}


//========================================

/*
===================
CL_ParsePlayerinfo
===================
*/
extern int  parsecountmod;
extern double parsecounttime;
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
CL_AddFlagModels

Called when the CTF flags are set
================
*/
void
CL_AddFlagModels (entity_t *ent, int team)
{
	int         i;
	float       f;
	vec3_t      v_forward, v_right, v_up, origin, angles;
	entity_t   *newent;

	if (cl_flagindex == -1)
		return;

	f = 14;
	if (ent->cur.frame >= 29 && ent->cur.frame <= 40) {
		if (ent->cur.frame >= 29 && ent->cur.frame <= 34) {	// axpain
			if (ent->cur.frame == 29)
				f = f + 2;
			else if (ent->cur.frame == 30)
				f = f + 8;
			else if (ent->cur.frame == 31)
				f = f + 12;
			else if (ent->cur.frame == 32)
				f = f + 11;
			else if (ent->cur.frame == 33)
				f = f + 10;
			else if (ent->cur.frame == 34)
				f = f + 4;
		} else if (ent->cur.frame >= 35 && ent->cur.frame <= 40) {	// pain
			if (ent->cur.frame == 35)
				f = f + 2;
			else if (ent->cur.frame == 36)
				f = f + 10;
			else if (ent->cur.frame == 37)
				f = f + 10;
			else if (ent->cur.frame == 38)
				f = f + 8;
			else if (ent->cur.frame == 39)
				f = f + 4;
			else if (ent->cur.frame == 40)
				f = f + 2;
		}
	} else if (ent->cur.frame >= 103 && ent->cur.frame <= 118) {
		if (ent->cur.frame >= 103 && ent->cur.frame <= 104)
			f = f + 6;					// nailattack
		else if (ent->cur.frame >= 105 && ent->cur.frame <= 106)
			f = f + 6;					// light 
		else if (ent->cur.frame >= 107 && ent->cur.frame <= 112)
			f = f + 7;					// rocketattack
		else if (ent->cur.frame >= 112 && ent->cur.frame <= 118)
			f = f + 7;					// shotattack
	}

	newent = CL_NewTempEntity ();
	newent->model = cl.model_precache[cl_flagindex];
	newent->skinnum = team;

	AngleVectors (ent->cur.angles, v_forward, v_right, v_up);
	v_forward[2] = -v_forward[2];		// reverse z component
	for (i = 0; i < 3; i++)
		origin[i] = ent->cur.origin[i] - f * v_forward[i] + 22 * v_right[i];
	origin[2] -= 16;

	VectorCopy (ent->cur.angles, angles);
	angles[2] -= 45;

	CL_UpdateAndLerp_Origin (newent, origin, cls.realtime);
	CL_UpdateAndLerp_Angles (newent, angles, cls.realtime);
	CL_UpdateAndLerp_Frame (newent, 0, cls.realtime);
}

/*
=============
CL_LinkPlayers

Create visible entities in the correct position
for all current players
=============
*/
void
CL_LinkPlayers (void)
{
	int         j;
	player_info_t *info;
	player_state_t *state;
	player_state_t exact;
	double      playertime;
	entity_t   *ent;
	int         msec;
	frame_t    *frame;
	int         oldphysent;
	vec3_t      org, angles;

	playertime = cls.realtime - cls.latency + 0.02;
	playertime = min (playertime, cls.realtime);

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, info = cl.players, state = frame->playerstate;
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
				(!gl_flashblend->ivalue || j != cl.playernum))
		{
			if (j == cl.playernum)
				VectorCopy (cl.simorg, org);
			else
				VectorCopy (state->origin, org);
			CL_NewDlight (j, org, state->effects);
		}

		// the player object never gets added
		if (j == cl.playernum)
			continue;

		if (!state->modelindex)
			continue;

		if (!Cam_DrawPlayer (j))
			continue;

		V_AddEntity ( ent );

		ent->times++;

		ent->skinnum = state->skinnum;

		ent->model = cl.model_precache[state->modelindex];
		if (state->modelindex == cl_playerindex) {
			ent->colormap = &info->colormap;	// Use custom colormap.
			ent->skin = info->skin;				// Use custom skin.
		} else {
			ent->colormap = NULL;
			ent->skin = NULL;
		}

		// 
		// angles
		// 
		angles[PITCH] = -state->viewangles[PITCH] / 3;
		angles[YAW] = state->viewangles[YAW];
		angles[ROLL] = V_CalcRoll(ent->cur.angles,state->velocity) * 4;

		// only predict half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || (!cl_predict_players->ivalue))
		{
			CL_UpdateAndLerp_Origin (ent, state->origin, cls.realtime);
		} else {
			// predict players movement
			state->command.msec = min (state->command.msec, 255);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->command, false);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent->cur.origin);
			CL_UpdateAndLerp_Origin (ent, exact.origin, cls.realtime);
		}

		CL_UpdateAndLerp_Angles (ent, angles, cls.realtime);
		CL_UpdateAndLerp_Frame (ent, state->frame, cls.realtime);

		if (state->effects & EF_FLAG1)
			CL_AddFlagModels (ent, 0);
		else if (state->effects & EF_FLAG2)
			CL_AddFlagModels (ent, 1);
	}
}

//======================================================================

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
===============
*/
void
CL_SetSolidEntities (void)
{
	int         i;
	frame_t    *frame;
	packet_entities_t *pak;
	entity_state_t *state;

	pmove.physents[0].model = cl.worldmodel;
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (cl.model_precache[state->modelindex]->hulls[1].firstclipnode) {
			pmove.physents[pmove.numphysent].model =
				cl.model_precache[state->modelindex];
			VectorCopy (state->origin, pmove.physents[pmove.numphysent].origin);
			pmove.numphysent++;
		}
	}

}

/*
===
Calculate the new position of players, without other player clipping

We do this to set up real player prediction.
Players are predicted twice, first without clipping other players,
then with clipping against them.
This sets up the first phase.
===
*/
void
CL_SetUpPlayerPrediction (qboolean dopred)
{
	int         j;
	player_state_t *state;
	player_state_t exact;
	double      playertime;
	int         msec;
	frame_t    *frame;
	struct predicted_player *pplayer;

	playertime = cls.realtime - cls.latency + 0.02;
	if (playertime > cls.realtime)
		playertime = cls.realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0, pplayer = predicted_players, state = frame->playerstate;
		 j < MAX_CLIENTS; j++, pplayer++, state++) {

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;					// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy (cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].
						playerstate[cl.playernum].origin, pplayer->origin);
		} else {
			// only predict half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 ||
				(!cl_predict_players->ivalue) || !dopred)
			{
				VectorCopy (state->origin, pplayer->origin);
			} else {
				// predict players movement
				state->command.msec = min (state->command.msec, 255);

				CL_PredictUsercmd (state, &exact, &state->command, false);
				VectorCopy (exact.origin, pplayer->origin);
			}
		}
	}
}

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
Note that CL_SetUpPlayerPrediction() must be called first!
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void
CL_SetSolidPlayers (int playernum)
{
	int         j;
	extern vec3_t player_mins;
	extern vec3_t player_maxs;
	struct predicted_player *pplayer;
	physent_t  *pent;

	if (!cl_solid_players->ivalue)
		return;

	pent = pmove.physents + pmove.numphysent;

	for (j = 0, pplayer = predicted_players; j < MAX_CLIENTS; j++, pplayer++) {

		if (!pplayer->active)
			continue;					// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (pplayer->flags & PF_DEAD)
			continue;					// dead players aren't solid

		pent->model = 0;
		VectorCopy (pplayer->origin, pent->origin);
		VectorCopy (player_mins, pent->mins);
		VectorCopy (player_maxs, pent->maxs);
		pmove.numphysent++;
		pent++;
	}
}

void
CL_LinkStaticEntites (void)
{
	int		i;

	for (i = 0; i < cl_num_static_entities; i++) {
		cl_static_entities[i].times++;

		CL_Update_Angles (&cl_static_entities[i], cl_static_entities[i].to.angles, cls.realtime);
		CL_Update_Origin (&cl_static_entities[i], cl_static_entities[i].to.origin, cls.realtime);
		CL_Update_Frame (&cl_static_entities[i], cl_static_entities[i].to.frame, cls.realtime);
		V_AddEntity ( &cl_static_entities[i] );
	}
}

/*
===============
CL_EmitEntities

Builds the visedicts array for cl.time

Made up of: clients, packet_entities, nails, and tents
===============
*/
void
CL_EmitEntities (void)
{
	if (cls.state != ca_active)
		return;
	if (!cl.validsequence)
		return;

	V_ClearEntities ();

	cl_num_tmp_entities = 0;

	entity_frame++;

	CL_LinkPlayers ();
	CL_LinkProjectiles ();
	CL_LinkStaticEntites ();
	CL_LinkPacketEntities ();
	CL_UpdateTEnts ();
}

