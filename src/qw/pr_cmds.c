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

#include <stdio.h>

#include "quakedef.h"
#include "cmd.h"
#include "common.h"
#include "cvar.h"
#include "mathlib.h"
#include "model.h"
#include "progs.h"
#include "server.h"
#include "strlib.h"
#include "world.h"

#define RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))
#define RETURN_STRING(s) (((int *)pr_globals)[OFS_RETURN] = PR_SetString(s))

extern cvar_t *sv_aim;

static char engine_extensions[] =
"DP_QC_ETOS "
"DP_QC_FINDFLOAT "
"DP_QC_MINMAXBOUND "
"DP_QC_RANDOMVEC "
"DP_QC_SINCOSSQRTPOW "
"DP_QC_TRACEBOX "
"DP_REGISTERCVAR ";

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/

static char *
PF_VarString (int first)
{
	Uint			i;
	static char		out[256];

	out[0] = 0;
	for (i = first; i < pr_argc; i++)
		strcat (out, G_STRING ((OFS_PARM0 + i * 3)));

	return out;
}


/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
static void
PF_error (void)
{
	char       *s;
	edict_t    *ed;

	s = PF_VarString (0);
	Com_Printf ("======SERVER ERROR in %s:\n%s\n",
				PR_GetString (pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr_global_struct->self);
	ED_Print (ed);

	SV_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
static void
PF_objerror (void)
{
	char       *s;
	edict_t    *ed;

	s = PF_VarString (0);
	Com_Printf ("======OBJECT ERROR in %s:\n%s\n",
				PR_GetString (pr_xfunction->s_name), s);
	ed = PROG_TO_EDICT (pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	SV_Error ("Program error");
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
static void
PF_makevectors (void)
{
	AngleVectors (G_VECTOR (OFS_PARM0), pr_global_struct->v_forward,
				  pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
static void
PF_setorigin (void)
{
	edict_t    *e;
	float      *org;

	e = G_EDICT (OFS_PARM0);
	org = G_VECTOR (OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
static void
PF_setsize (void)
{
	edict_t    *e;
	float      *min, *max;

	e = G_EDICT (OFS_PARM0);
	min = G_VECTOR (OFS_PARM1);
	max = G_VECTOR (OFS_PARM2);
	VectorCopy (min, e->v.mins);
	VectorCopy (max, e->v.maxs);
	VectorSubtract (max, min, e->v.size);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
static void
PF_setmodel (void)
{
	edict_t    *e;
	char       *m, **check;
	int         i;
	model_t    *mod;

	e = G_EDICT (OFS_PARM0);
	m = G_STRING (OFS_PARM1);

// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp (*check, m))
			break;

	if (!*check)
		PR_RunError ("no precache: %s\n", m);

	e->v.model = PR_SetString (m);
	e->v.modelindex = i;

// if it is an inline model, get the size information for it
	if (m[0] == '*') {
		mod = Mod_ForName (m, FLAG_RENDER | FLAG_CRASH);
		VectorCopy (mod->normalmins, e->v.mins);
		VectorCopy (mod->normalmaxs, e->v.maxs);
		VectorSubtract (mod->normalmaxs, mod->normalmins, e->v.size);
		SV_LinkEdict (e, false);
	}

}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
static void
PF_bprint (void)
{
	char       *s;
	int         level;

	level = G_FLOAT (OFS_PARM0);

	s = PF_VarString (1);
	SV_BroadcastPrintf (level, "%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
static void
PF_sprint (void)
{
	char       *s;
	client_t   *client;
	int         entnum;
	int         level;

	entnum = G_EDICTNUM (OFS_PARM0);
	level = G_FLOAT (OFS_PARM1);

	s = PF_VarString (2);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Com_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	SV_ClientPrintf (client, level, "%s", s);
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
static void
PF_centerprint (void)
{
	char       *s;
	int         entnum;
	client_t   *cl;

	entnum = G_EDICTNUM (OFS_PARM0);
	s = PF_VarString (1);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Com_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen (s));
	ClientReliableWrite_String (cl, s);
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
static void
PF_normalize (void)
{
	float      *value1;
	vec3_t      newvalue;
	float       new;

	value1 = G_VECTOR (OFS_PARM0);

	new = DotProduct(value1,value1);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else {
		new = Q_sqrt (new);
		new = 1 / new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy (newvalue, G_VECTOR (OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
static void
PF_vlen (void)
{
	float      *value1;
	float       new;

	value1 = G_VECTOR (OFS_PARM0);

	new = DotProduct(value1,value1);

	G_FLOAT (OFS_RETURN) = (new) ? Q_sqrt(new) : 0;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
static void
PF_vectoyaw (void)
{
	float      *value1;
	float       yaw;

	value1 = G_VECTOR (OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else {
		yaw = Q_atan2 (value1[1], value1[0]) * 180 / M_PI;
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT (OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
static void
PF_vectoangles (void)
{
	float      *value1 = G_VECTOR (OFS_PARM0);
	vec3_t		v;

	Vector2Angles (value1, v);

	G_FLOAT (OFS_RETURN + 0) = v[0];
	G_FLOAT (OFS_RETURN + 1) = v[1];
	G_FLOAT (OFS_RETURN + 2) = v[2];
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
static void
PF_random (void)
{
	float       num;

	num = (rand () & 0x7fff) / ((float) 0x7fff);

	G_FLOAT (OFS_RETURN) = num;
}


/*
=================
PF_ambientsound

=================
*/
static void
PF_ambientsound (void)
{
	char      **check;
	char       *samp;
	float      *pos;
	float       vol, attenuation;
	int         i, soundnum;

	pos = G_VECTOR (OFS_PARM0);
	samp = G_STRING (OFS_PARM1);
	vol = G_FLOAT (OFS_PARM2);
	attenuation = G_FLOAT (OFS_PARM3);

// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
		if (!strcmp (*check, samp))
			break;

	if (!*check) {
		Com_Printf ("no precache: %s\n", samp);
		return;
	}
// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon, svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord (&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol * 255);
	MSG_WriteByte (&sv.signon, attenuation * 64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
static void
PF_sound (void)
{
	char       *sample;
	int         channel;
	edict_t    *entity;
	int         volume;
	float       attenuation;

	entity = G_EDICT (OFS_PARM0);
	channel = G_FLOAT (OFS_PARM1);
	sample = G_STRING (OFS_PARM2);
	volume = G_FLOAT (OFS_PARM3) * 255;
	attenuation = G_FLOAT (OFS_PARM4);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
static void
PF_break (void)
{
	Com_Printf ("break statement\n");
	*(int *) -4 = 0;					// dump to debugger
//  PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
static void
PF_traceline (void)
{
	float		*v1, *v2;
	trace_t		trace;
	int			nomonsters;
	edict_t		*ent;

	v1 = G_VECTOR (OFS_PARM0);
	v2 = G_VECTOR (OFS_PARM1);
	nomonsters = G_FLOAT (OFS_PARM2);
	ent = G_EDICT (OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG (trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG (sv.edicts);
}

/*
=================
PF_tracebox

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

tracebox (vector1, vector mins, vector maxs, vector2, tryents)
=================
*/
static void
PF_tracebox (void)
{
	float		*v1, *v2, *m1, *m2;
	trace_t		trace;
	int			nomonsters;
	edict_t		*ent;

	v1 = G_VECTOR(OFS_PARM0);
	m1 = G_VECTOR(OFS_PARM1);
	m2 = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(OFS_PARM5);

	trace = SV_Move (v1, m1, m2, v2,
			nomonsters ? MOVE_NOMONSTERS : MOVE_NORMAL, ent);
	
	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;

	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

//============================================================================

Uint8       checkpvs[MAX_MAP_LEAFS / 8];

static Uint
PF_newcheckclient (Uint check)
{
	Uint        i;
	Uint8      *pvs;
	edict_t    *ent;
	mleaf_t    *leaf;
	vec3_t      org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > MAX_CLIENTS)
		check = MAX_CLIENTS;

	if (check == MAX_CLIENTS)
		i = 1;
	else
		i = check + 1;

	for (;; i++) {
		if (i == MAX_CLIENTS + 1)
			i = 1;

		ent = EDICT_NUM (i);

		if (i == check)
			break;						// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int) ent->v.flags & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	memcpy (checkpvs, pvs, (sv.worldmodel->brush->numleafs + 7) >> 3);

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
int         c_invis, c_notvis;
static void
PF_checkclient (void)
{
	edict_t    *ent, *self;
	mleaf_t    *leaf;
	int         l;
	vec3_t      view;

// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1) {
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}
// return check if it might be visible  
	ent = EDICT_NUM (sv.lastcheck);
	if (ent->free || ent->v.health <= 0) {
		RETURN_EDICT (sv.edicts);
		return;
	}
// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT (pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->brush->leafs) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7)))) {
		c_notvis++;
		RETURN_EDICT (sv.edicts);
		return;
	}
// might be able to see it
	c_invis++;
	RETURN_EDICT (ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
static void
PF_stuffcmd (void)
{
	int         entnum;
	char       *str;
	client_t   *cl;

	entnum = G_EDICTNUM (OFS_PARM0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING (OFS_PARM1);

	cl = &svs.clients[entnum - 1];

	if (strcmp (str, "disconnect\n") == 0) {
		// so long and thanks for all the fish
		cl->drop = true;
		return;
	}

	ClientReliableWrite_Begin (cl, svc_stufftext, 2 + strlen (str));
	ClientReliableWrite_String (cl, str);
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
static void
PF_localcmd (void)
{
	char       *str;

	str = G_STRING (OFS_PARM0);
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void
PF_cvar (void)
{
	char       *str;
	cvar_t	   *var;

	str = G_STRING (OFS_PARM0);

	var = Cvar_Find (str);
	if (!var)
		var = Cvar_CreateTemp (str, "0");

	G_FLOAT (OFS_RETURN) = var->fvalue;
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
static void
PF_cvar_set (void)
{
	char       *name, *val;
	cvar_t	   *var;

	name = G_STRING (OFS_PARM0);
	val = G_STRING (OFS_PARM1);

	var = Cvar_Find (name);
	if (!var)
		Cvar_CreateTemp (name, val);
	else
		Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
static void
PF_findradius (void)
{
	edict_t		*ent, *chain;
	float		rad;
	float		*org;
	vec3_t		eorg;
	Uint		i, j;

	chain = (edict_t *) sv.edicts;

	org = G_VECTOR (OFS_PARM0);
	rad = G_FLOAT (OFS_PARM1);

	ent = NEXT_EDICT (sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT (ent)) {
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] =
				org[j] - (ent->v.origin[j] +
						  (ent->v.mins[j] + ent->v.maxs[j]) * 0.5);
		if (DotProduct(eorg,eorg) > rad*rad)
			continue;

		ent->v.chain = EDICT_TO_PROG (chain);
		chain = ent;
	}

	RETURN_EDICT (chain);
}


/*
=========
PF_dprint
=========
*/
static void
PF_dprint (void)
{
	Com_Printf ("%s", PF_VarString (0));
}

char        pr_string_temp[128];

static void
PF_ftos (void)
{
	float       v;

	v = G_FLOAT (OFS_PARM0);

	if (v == (int) v)
		snprintf (pr_string_temp, sizeof (pr_string_temp), "%d", (int) v);
	else
		snprintf (pr_string_temp, sizeof (pr_string_temp), "%5.1f", v);
	G_INT (OFS_RETURN) = PR_SetString (pr_string_temp);
}

static void
PF_fabs (void)
{
	float       v;

	v = G_FLOAT (OFS_PARM0);
	G_FLOAT (OFS_RETURN) = fabs (v);
}

static void
PF_vtos (void)
{
	snprintf (pr_string_temp, sizeof (pr_string_temp), "'%5.1f %5.1f %5.1f'",
			  G_VECTOR (OFS_PARM0)[0], G_VECTOR (OFS_PARM0)[1],
			  G_VECTOR (OFS_PARM0)[2]);
	G_INT (OFS_RETURN) = PR_SetString (pr_string_temp);
}

static void
PF_etos (void)
{
	snprintf (pr_string_temp, sizeof (pr_string_temp), "entity %i",
			G_EDICTNUM(OFS_PARM0));
	G_INT (OFS_RETURN) = PR_SetString (pr_string_temp);
}

static void
PF_Spawn (void)
{
	edict_t    *ed;

	ed = ED_Alloc ();
	RETURN_EDICT (ed);
}

static void
PF_Remove (void)
{
	edict_t    *ed;

	ed = G_EDICT (OFS_PARM0);
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
static void
PF_Find (void)
{
	Uint		e;
	int			f;
	char		*s, *t;
	edict_t		*ed;

	e = G_EDICTNUM (OFS_PARM0);
	f = G_INT (OFS_PARM1);
	s = G_STRING (OFS_PARM2);

	if (!s)
		PR_RunError ("PF_Find: bad search string");

	if (!*s)
	{
		RETURN_EDICT (sv.edicts);
		return;
	}

	for (e++; e < sv.num_edicts; e++) {
		ed = EDICT_NUM (e);
		if (ed->free)
			continue;
		t = E_STRING (ed, f);
		if (!t)
			continue;
		if (!strcmp (t, s)) {
			RETURN_EDICT (ed);
			return;
		}
	}

	RETURN_EDICT (sv.edicts);
}

static void
PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

static void
PF_precache_file (void)
{
	// precache_file is only used to copy files with qcc, it does nothing
	G_INT (OFS_RETURN) = G_INT (OFS_PARM0);
}

static void
PF_precache_sound (void)
{
	char       *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError
			("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING (OFS_PARM0);
	G_INT (OFS_RETURN) = G_INT (OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (!sv.sound_precache[i]) {
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp (sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

static void
PF_precache_model (void)
{
	char       *s;
	int         i;

	if (sv.state != ss_loading)
		PR_RunError
			("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING (OFS_PARM0);
	G_INT (OFS_RETURN) = G_INT (OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 0; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i]) {
			sv.model_precache[i] = s;
			return;
		}
		if (!strcmp (sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}


static void
PF_coredump (void)
{
	ED_PrintEdicts ();
}

static void
PF_traceon (void)
{
	pr_trace = true;
}

static void
PF_traceoff (void)
{
	pr_trace = false;
}

static void
PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM (OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
static void
PF_walkmove (void)
{
	edict_t    *ent;
	float       yaw, dist;
	vec3_t      move;
	dfunction_t *oldf;
	int         oldself;

	ent = PROG_TO_EDICT (pr_global_struct->self);
	yaw = G_FLOAT (OFS_PARM0);
	dist = G_FLOAT (OFS_PARM1);

	if (!((int) ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		G_FLOAT (OFS_RETURN) = 0;
		return;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = Q_cos (yaw) * dist;
	move[1] = Q_sin (yaw) * dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT (OFS_RETURN) = SV_movestep (ent, move, true);


// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
static void
PF_droptofloor (void)
{
	edict_t    *ent;
	vec3_t      end;
	trace_t     trace;

	ent = PROG_TO_EDICT (pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT (OFS_RETURN) = 0;
	else {
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int) ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG (trace.ent);
		G_FLOAT (OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
static void
PF_lightstyle (void)
{
	int         style;
	char       *val;
	client_t   *client;
	int         j;

	style = G_FLOAT (OFS_PARM0);
	val = G_STRING (OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		if (client->state == cs_spawned) {
			ClientReliableWrite_Begin (client, svc_lightstyle,
									   strlen (val) + 3);
			ClientReliableWrite_Char (client, style);
			ClientReliableWrite_String (client, val);
		}
}

static void
PF_rint (void)
{
	float       f;

	f = G_FLOAT (OFS_PARM0);
	if (f > 0)
		G_FLOAT (OFS_RETURN) = (int) (f + 0.5);
	else
		G_FLOAT (OFS_RETURN) = (int) (f - 0.5);
}

static void
PF_floor (void)
{
	G_FLOAT (OFS_RETURN) = floor (G_FLOAT (OFS_PARM0));
}

static void
PF_ceil (void)
{
	G_FLOAT (OFS_RETURN) = ceil (G_FLOAT (OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
static void
PF_checkbottom (void)
{
	edict_t    *ent;

	ent = G_EDICT (OFS_PARM0);

	G_FLOAT (OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
static void
PF_pointcontents (void)
{
	float      *v;

	v = G_VECTOR (OFS_PARM0);

	G_FLOAT (OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
static void
PF_nextent (void)
{
	Uint		i;
	edict_t		*ent;

	i = G_EDICTNUM (OFS_PARM0);
	while (1) {
		i++;
		if (i == sv.num_edicts) {
			RETURN_EDICT (sv.edicts);
			return;
		}
		ent = EDICT_NUM (i);
		if (!ent->free) {
			RETURN_EDICT (ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
static void
PF_aim (void)
{
	edict_t		*ent, *check, *bestent;
	vec3_t		start, dir, end, bestdir;
	Uint		i, j;
	trace_t		tr;
	float		dist, bestdist;
	float		speed;
	char		*noaim;

	ent = G_EDICT (OFS_PARM0);
	speed = G_FLOAT (OFS_PARM1);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

// noaim option
	i = NUM_FOR_EDICT (ent);
	if (i > 0 && i < MAX_CLIENTS) {
		noaim = Info_ValueForKey (svs.clients[i - 1].userinfo, "noaim");
		if (Q_atoi (noaim) > 0) {
			VectorCopy (pr_global_struct->v_forward, G_VECTOR (OFS_RETURN));
			return;
		}
	}
// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && ((edict_t *) tr.ent)->v.takedamage == DAMAGE_AIM
		&& (!teamplay->ivalue || ent->v.team <= 0
			|| ent->v.team != ((edict_t *) tr.ent)->v.team)) {
		VectorCopy (pr_global_struct->v_forward, G_VECTOR (OFS_RETURN));
		return;
	}
// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim->fvalue;
	bestent = NULL;

	check = NEXT_EDICT (sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, check = NEXT_EDICT (check)) {
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay->ivalue && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;					// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = check->v.origin[j]
				+ 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalizeFast (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;					// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check) {			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent) {
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalizeFast (end);
		VectorCopy (end, G_VECTOR (OFS_RETURN));
	} else {
		VectorCopy (bestdir, G_VECTOR (OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void
PF_changeyaw (void)
{
	edict_t    *ent;
	float       ideal, current, move, speed;

	ent = PROG_TO_EDICT (pr_global_struct->self);
	current = ANGLEMOD (ent->v.angles[1]);
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current) {
		if (move >= 180)
			move = move - 360;
	} else {
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0) {
		if (move > speed)
			move = speed;
	} else {
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[1] = ANGLEMOD (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0				// unreliable to all
#define	MSG_ONE			1				// reliable to one (msg_entity)
#define	MSG_ALL			2				// reliable to all
#define	MSG_INIT		3				// write to the init string
#define	MSG_MULTICAST	4				// for multicast()

sizebuf_t  *
WriteDest (void)
{
	int         dest;

	dest = G_FLOAT (OFS_PARM0);
	switch (dest) {
		case MSG_BROADCAST:
			return &sv.datagram;

		case MSG_ONE:
			SV_Error ("Shouldn't be at MSG_ONE");

		case MSG_ALL:
			return &sv.reliable_datagram;

		case MSG_INIT:
			if (sv.state != ss_loading)
				PR_RunError
					("PF_Write_*: MSG_INIT can only be written in spawn functions");
			return &sv.signon;

		case MSG_MULTICAST:
			return &sv.multicast;

		default:
			PR_RunError ("WriteDest: bad destination");
			break;
	}

	return NULL;
}

static client_t *
Write_GetClient (void)
{
	int         entnum;
	edict_t    *ent;

	ent = PROG_TO_EDICT (pr_global_struct->msg_entity);
	entnum = NUM_FOR_EDICT (ent);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("WriteDest: not a client");
	return &svs.clients[entnum - 1];
}


static void
PF_WriteByte (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Byte (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteByte (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteChar (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Char (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteChar (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteShort (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Short (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteShort (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteLong (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 4);
		ClientReliableWrite_Long (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteLong (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteAngle (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 1);
		ClientReliableWrite_Angle (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteAngle (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteCoord (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Coord (cl, G_FLOAT (OFS_PARM1));
	} else
		MSG_WriteCoord (WriteDest (), G_FLOAT (OFS_PARM1));
}

static void
PF_WriteString (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 1 + strlen (G_STRING (OFS_PARM1)));
		ClientReliableWrite_String (cl, G_STRING (OFS_PARM1));
	} else
		MSG_WriteString (WriteDest (), G_STRING (OFS_PARM1));
}


static void
PF_WriteEntity (void)
{
	if (G_FLOAT (OFS_PARM0) == MSG_ONE) {
		client_t   *cl = Write_GetClient ();

		ClientReliableCheckBlock (cl, 2);
		ClientReliableWrite_Short (cl, G_EDICTNUM (OFS_PARM1));
	} else
		MSG_WriteShort (WriteDest (), G_EDICTNUM (OFS_PARM1));
}

//=============================================================================

int         SV_ModelIndex (char *name);

static void
PF_makestatic (void)
{
	edict_t    *ent;
	int         i;

	ent = G_EDICT (OFS_PARM0);

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	MSG_WriteByte (&sv.signon, SV_ModelIndex (PR_GetString (ent->v.model)));

	MSG_WriteByte (&sv.signon, ent->v.frame);
	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord (&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle (&sv.signon, ent->v.angles[i]);
	}

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
static void
PF_setspawnparms (void)
{
	edict_t    *ent;
	int         i;
	client_t   *client;

	ent = G_EDICT (OFS_PARM0);
	i = NUM_FOR_EDICT (ent);
	if (i < 1 || i > MAX_CLIENTS)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
static void
PF_changelevel (void)
{
	char       *s;
	static int  last_spawncount;

// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	s = G_STRING (OFS_PARM0);
	Cbuf_AddText (va ("map %s\n", s));
}


/*
==============
PF_logfrag

logfrag (killer, killee)
==============
*/
static void
PF_logfrag (void)
{
	edict_t    *ent1, *ent2;
	int         e1, e2;
	char       *s;

	ent1 = G_EDICT (OFS_PARM0);
	ent2 = G_EDICT (OFS_PARM1);

	e1 = NUM_FOR_EDICT (ent1);
	e2 = NUM_FOR_EDICT (ent2);

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	s = va ("\\%s\\%s\\\n", svs.clients[e1 - 1].name,
			svs.clients[e2 - 1].name);

	SZ_Print (&svs.log[svs.logsequence & 1], s);
	if (sv_fraglogfile) {
		fprintf (sv_fraglogfile, s);
		fflush (sv_fraglogfile);
	}
}


/*
==============
PF_infokey

string(entity e, string key) infokey
==============
*/
static void
PF_infokey (void)
{
	edict_t    *e;
	int         e1;
	char       *value;
	char       *key;
	static char ov[256];

	e = G_EDICT (OFS_PARM0);
	e1 = NUM_FOR_EDICT (e);
	key = G_STRING (OFS_PARM1);

	if (e1 == 0) {
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey (localinfo, key);
	} else if (e1 <= MAX_CLIENTS) {
		if (!strcmp (key, "ip"))
			value =
				strcpy (ov,
						NET_BaseAdrToString (svs.clients[e1 - 1].netchan.
											 remote_address));
		else if (!strcmp (key, "ping")) {
			int         ping = SV_CalcPing (&svs.clients[e1 - 1]);

			snprintf (ov, sizeof (ov), "%d", ping);
			value = ov;
		} else
			value = Info_ValueForKey (svs.clients[e1 - 1].userinfo, key);
	} else
		value = "";

	RETURN_STRING (value);
}

/*
==============
PF_stof

float(string s) stof
==============
*/
static void
PF_stof (void)
{
	char       *s;

	s = G_STRING (OFS_PARM0);

	G_FLOAT (OFS_RETURN) = Q_atof (s);
}


/*
==============
PF_multicast

void(vector where, float set) multicast
==============
*/
static void
PF_multicast (void)
{
	float      *o;
	int         to;

	o = G_VECTOR (OFS_PARM0);
	to = G_FLOAT (OFS_PARM1);

	SV_Multicast (o, to);
}

static void
PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}   

static void
PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}   

static void
PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}   

static void
PF_randomvec (void)
{
	vec3_t		temp;
	
	do
	{
		temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	}
	while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

static void
PF_registercvar (void)
{
	char		*name, *value;

	name = G_STRING(OFS_PARM0);
	value = G_STRING(OFS_PARM1);
	G_FLOAT(OFS_RETURN) = 0;

	// Do nothing if Cvar already exists
	if (Cvar_Find (name))
		return;

	// Don't create a Cvar which overlaps a command
	if (Cmd_Exists (name))
	{
		Com_Printf ("PF_registercvar: %s is a command\n", name);
		return;
	}

	if (!Cvar_Get (name, value, CVAR_TEMP, NULL))
	{
		Com_Printf ("PF_registercvar: %s could not be created\n", name);
		return;
	}

	G_FLOAT(OFS_RETURN) = 1;
}

/*
=================
PF_min

returns the minimum of two or more supplied floats
=================
*/
static void
PF_min (void)
{
	Uint		i;
	float		f;

	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc > 2)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < pr_argc; i++)
			if (G_FLOAT((OFS_PARM0+i*3)) < f)
				f = G_FLOAT((OFS_PARM0+i*3));

		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_RunError ("min: must supply at least 2 floats\n");
}

/*
=================
PF_max

returns the maximum of two or more supplied floats
=================
*/
static void
PF_max (void)
{
	Uint		i;
	float		f;

	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < pr_argc; i++)
			if (G_FLOAT((OFS_PARM0+i*3)) > f)
				f = G_FLOAT((OFS_PARM0+i*3));

		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_RunError("max: must supply at least 2 floats\n");
}

/*
=================
PF_bound

returns float bounded within a supplied range
=================
*/
static void
PF_bound (void)
{
	G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

/*
=================
PF_pow

returns x raised to the y power
=================
*/
static void
PF_pow (void)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

/*
=================
PF_findfloat

=================
*/
static void
PF_findfloat (void)
{
	Uint		e;
	int			f;
	float		s;
	edict_t		*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);

	for (e++; e < sv.num_edicts; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		if (E_FLOAT(ed,f) == s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

static void
PF_checkextension (void)
{
	int			len;
	char		*name, *e, *start;

	name = G_STRING(OFS_PARM0);
	len = strlen(name);
	for (e = engine_extensions; *e; e++)
	{
		while (*e == ' ')
			e++;
		if (!*e)
			break;
		start = e;
		while (*e && *e != ' ')
			e++;
		if (e - start == len)
			if (!strncasecmp (start, name, len))
			{
				G_FLOAT(OFS_RETURN) = true;
				return;
			}
	}

	G_FLOAT(OFS_RETURN) = false;
}

#define NULL10 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
#define NULL100 NULL10, NULL10, NULL10, NULL10, NULL10, NULL10, NULL10, \
		NULL10, NULL10, NULL10

builtin_t pr_builtin[] =
{
	NULL,					// #000 runtime error
	PF_makevectors,			// #001
	PF_setorigin,			// #002
	PF_setmodel,			// #003
	PF_setsize,				// #004
	NULL,					// #005 setabssize (never implemented)
	PF_break,				// #006
	PF_random,				// #007
	PF_sound,				// #008
	PF_normalize,			// #009

	PF_error,				// #010
	PF_objerror,			// #011
	PF_vlen,				// #012
	PF_vectoyaw,			// #013
	PF_Spawn,				// #014
	PF_Remove,				// #015
	PF_traceline,			// #016
	PF_checkclient,			// #017
	PF_Find,				// #018
	PF_precache_sound,		// #019

	PF_precache_model,		// #020
	PF_stuffcmd,			// #021
	PF_findradius,			// #022
	PF_bprint,				// #023
	PF_sprint,				// #024
	PF_dprint,				// #025
	PF_ftos,				// #026
	PF_vtos,				// #027
	PF_coredump,			// #028
	PF_traceon,				// #029

	PF_traceoff,			// #030
	PF_eprint,				// #031
	PF_walkmove,			// #032
	NULL,					// #033 ???
	PF_droptofloor,			// #034
	PF_lightstyle,			// #035
	PF_rint,				// #036
	PF_floor,				// #037
	PF_ceil,				// #038
	NULL,					// #039

	PF_checkbottom,			// #040
	PF_pointcontents,		// #041
	NULL,					// #042
	PF_fabs,				// #043
	PF_aim,					// #044
	PF_cvar,				// #045
	PF_localcmd,			// #046
	PF_nextent,				// #047
	NULL,					// #048 PF_particle in NQ
	PF_changeyaw,			// #049

	NULL,					// #050
	PF_vectoangles,			// #051
	PF_WriteByte,			// #052
	PF_WriteChar,			// #053
	PF_WriteShort,			// #054
	PF_WriteLong,			// #055
	PF_WriteCoord,			// #056
	PF_WriteAngle,			// #057
	PF_WriteString,			// #058
	PF_WriteEntity,			// #059

	PF_sin,					// #060 DP: DP_QC_SINCOSSQRTPOW
	PF_cos,					// #061 DP: DP_QC_SINCOSSQRTPOW
	PF_sqrt,				// #062 DP: DP_QC_SINCOSSQRTPOW
	NULL,					// #063 Q2: PF_changepitch
	NULL,					// #064 DP: PF_tracetoss
	PF_etos,				// #065 DP: DP_QC_ETOS
	NULL,					// #066 Q2: PF_watermove (don't implement)
	SV_MoveToGoal,			// #067
	PF_precache_file,		// #068
	PF_makestatic,			// #069

	PF_changelevel,			// #070
	NULL,					// #071
	PF_cvar_set,			// #072
	PF_centerprint,			// #073
	PF_ambientsound,		// #074
	PF_precache_model,		// #075
	PF_precache_sound,		// #076
	PF_precache_file,		// #077
	PF_setspawnparms,		// #078

	// QW functions (overlap with QSG "tutorials")
	PF_logfrag,				// #079
	PF_infokey,				// #080
	PF_stof,				// #081
	PF_multicast,			// #082

	// Used by various QSG "tutorials"
	NULL,					// #083
	NULL,					// #084
	NULL,					// #085
	NULL,					// #086
	NULL,					// #087
	NULL,					// #088
	NULL,					// #089

	// DarkPlaces extensions (1)
	PF_tracebox,			// #090 PF_tracebox
	PF_randomvec,			// #091 PF_randomvec
	NULL,					// #092 PF_getlight
	PF_registercvar,        // #093 PF_registercvar
	PF_min,					// #094 PF_min
	PF_max,                 // #095 PF_max
	PF_bound,               // #096 PF_bound
	PF_pow,                 // #097 DP_QC_SINCOSSQRTPOW
	PF_findfloat,			// #098 PF_findfloat
	PF_checkextension,      // #099 PF_checkextension

	// Used by someone for something probably...
	NULL100,				// #100 - 199
	NULL100,				// #200 - 299
	NULL100,				// #300 - 399

/*
	// DarkPlaces extensions (2)
	PF_copyentity,			// #400
	PF_setcolor,			// #401
	PF_findchain,			// #402
	PF_findchainfloat,		// #403
	PF_effect,				// #404
	PF_te_blood,			// #405
	PF_te_bloodshower,		// #406
	PF_te_explosionrgb,		// #407
	PF_te_particlecube,		// #408
	PF_te_particlerain,		// #409
	PF_te_particlesnow,		// #410
	PF_te_spark,			// #411
	PF_te_gunshotquad,		// #412
	PF_te_spikequad,		// #413
	PF_te_superspikequad,	// #414
	PF_te_explosionquad,	// #415
	PF_te_smallflash,		// #416
	PF_te_customflash,		// #417
	PF_te_gunshot,			// #418
	PF_te_spike,			// #419
	PF_te_superspike,		// #420
	PF_te_explosion,		// #421
	PF_te_tarexplosion,		// #422
	PF_te_wizspike,			// #423
	PF_te_knightspike,		// #424
	PF_te_lavasplash,		// #425
	PF_te_teleport,			// #426
	PF_te_explosion2,		// #427
	PF_te_lightning1,		// #428
	PF_te_lightning2,		// #429
	PF_te_lightning3,		// #430
	PF_te_beam,				// #431
	PF_vectorvectors,		// #432
	PF_te_plasmaburn,		// #433
*/
};

builtin_t *pr_builtins = pr_builtin;
Uint pr_numbuiltins = sizeof (pr_builtin) / sizeof (pr_builtin[0]);

