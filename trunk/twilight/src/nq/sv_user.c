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
#include "cvar.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "net.h"
#include "server.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "world.h"

edict_t			*sv_player;

cvar_t			*sv_edgefriction;
cvar_t			*sv_idealpitchscale;
cvar_t			*sv_maxspeed;
cvar_t			*sv_accelerate;

static vec3_t forward, right, up;

static vec3_t		wishdir;
static float		wishspeed;

// world
static float		*angles;
static float		*origin;
static float		*velocity;

static qboolean	onground;

static usercmd_t	cmd;


#define	MAX_FORWARD	6
void
SV_SetIdealPitch (void)
{
	float       angleval, sinval, cosval;
	trace_t     tr;
	vec3_t      top, bottom;
	float       z[MAX_FORWARD];
	int         i, j;
	int         step, dir, steps;

	if (!((int) sv_player->v.flags & FL_ONGROUND))
		return;

	angleval = sv_player->v.angles[YAW] * M_PI * 2 / 360;
	sinval = Q_sin (angleval);
	cosval = Q_cos (angleval);

	for (i = 0; i < MAX_FORWARD; i++) {
		top[0] = sv_player->v.origin[0] + cosval * (i + 3) * 12;
		top[1] = sv_player->v.origin[1] + sinval * (i + 3) * 12;
		top[2] = sv_player->v.origin[2] + sv_player->v.view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = SV_Move (top, vec3_origin, vec3_origin, bottom, 1, sv_player);
		if (tr.allsolid)
			return;						// looking at a wall, leave ideal the
										// way is was

		if (tr.fraction == 1)
			return;						// near a dropoff

		z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (j = 1; j < i; j++) {
		step = z[j] - z[j - 1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
			return;						// mixed changes

		steps++;
		dir = step;
	}

	if (!dir) {
		sv_player->v.idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	sv_player->v.idealpitch = -dir * sv_idealpitchscale->fvalue;
}


static void
SV_UserFriction (void)
{
	float      *vel;
	float       speed, newspeed, control;
	vec3_t      start, stop;
	float       friction;
	trace_t     trace;

	vel = velocity;

	speed = VectorLength2(vel);
	if (!speed)
		return;

// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
	start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
	start[2] = origin[2] + sv_player->v.mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction->fvalue * sv_edgefriction->fvalue;
	else
		friction = sv_friction->fvalue;

// apply friction   
	control = speed < sv_stopspeed->fvalue ? sv_stopspeed->fvalue : speed;
	newspeed = speed - host_frametime * control * friction;

	if (newspeed < 0) {
		VectorClear (vel);
	}
	else {
		newspeed /= speed;
		VectorScale (vel, newspeed, vel);
	}
}

static void
SV_Accelerate (void)
{
	int         i;
	float       addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate->fvalue * host_frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishdir[i];
}

static void
SV_AirAccelerate (vec3_t wishveloc)
{
	int         i;
	float       addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate->fvalue * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishveloc[i];
}


static void
DropPunchAngle (void)
{
	float	len;
	eval_t	*val;

	len = VectorNormalize (sv_player->v.punchangle);

	len -= 10 * host_frametime;
	if (len < 0)
		len = 0;
	VectorScale (sv_player->v.punchangle, len, sv_player->v.punchangle);

	if ((val = GETEDICTFIELDVALUE(sv_player, eval_punchvector)))
	{
		len = VectorNormalize (val->vector);
		
		len -= 20 * host_frametime;
		if (len < 0)
			len = 0;
		VectorScale (val->vector, len, val->vector);
	}
}

static void
SV_FreeMove (void)
{
	int			i;
	float		wishspeed;

	AngleVectors (sv_player->v.v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		velocity[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	velocity[2] += cmd.upmove;

	wishspeed = VectorLength (velocity);
	if (wishspeed > sv_maxspeed->fvalue)
	{
		VectorScale (velocity, sv_maxspeed->fvalue / wishspeed, velocity);
		wishspeed = sv_maxspeed->fvalue;
	}
}

static void
SV_WaterMove (void)
{
	int         i;
	vec3_t      wishvel;
	float       speed, newspeed, wishspeed, addspeed, accelspeed;

//
// user intentions
//
	AngleVectors (sv_player->v.v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;				// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength (wishvel);
	if (wishspeed > sv_maxspeed->fvalue) {
		VectorScale (wishvel, sv_maxspeed->fvalue / wishspeed, wishvel);
		wishspeed = sv_maxspeed->fvalue;
	}
	wishspeed *= 0.7;

//
// water friction
//
	speed = VectorLength (velocity);
	if (speed) {
		newspeed = speed - host_frametime * speed * sv_friction->fvalue;
		if (newspeed < 0)
			newspeed = 0;
		VectorScale (velocity, newspeed / speed, velocity);
	} else
		newspeed = 0;

//
// water acceleration
//
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalizeFast (wishvel);
	accelspeed = sv_accelerate->fvalue * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishvel[i];
}

static void
SV_WaterJump (void)
{
	if (sv.time > sv_player->v.teleport_time || !sv_player->v.waterlevel) {
		sv_player->v.flags = (int) sv_player->v.flags & ~FL_WATERJUMP;
		sv_player->v.teleport_time = 0;
	}
	sv_player->v.velocity[0] = sv_player->v.movedir[0];
	sv_player->v.velocity[1] = sv_player->v.movedir[1];
}


static void
SV_AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;

	AngleVectors (sv_player->v.angles, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv.time < sv_player->v.teleport_time && fmove < 0)
		fmove = 0;

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	if ((int) sv_player->v.movetype != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);
	if (wishspeed > sv_maxspeed->fvalue)
	{
		wishspeed = sv_maxspeed->fvalue / wishspeed;
		VectorScale (wishvel, wishspeed, wishvel);
		wishspeed = sv_maxspeed->fvalue;
	}

	if (sv_player->v.movetype == MOVETYPE_NOCLIP)
		VectorCopy (wishvel, velocity);
	else if (onground)
	{
		SV_UserFriction ();
		SV_Accelerate ();
	}
	else
		// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
}

/*
===================
the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
static void
SV_ClientThink (void)
{
	vec3_t		v_angle;

	if (sv_player->v.movetype == MOVETYPE_NONE)
		return;

	onground = (int) sv_player->v.flags & FL_ONGROUND;

	origin = sv_player->v.origin;
	velocity = sv_player->v.velocity;

	DropPunchAngle ();

	// if dead, behave differently
	if (sv_player->v.health <= 0)
		return;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	cmd = host_client->cmd;
	angles = sv_player->v.angles;

	VectorAdd (sv_player->v.v_angle, sv_player->v.punchangle, v_angle);
	angles[ROLL] = V_CalcRoll (sv_player->v.angles, sv_player->v.velocity) * 4;
	if (!sv_player->v.fixangle)
	{
		angles[PITCH] = -v_angle[PITCH] / 3;
		angles[YAW] = v_angle[YAW];
	}

	if ((int) sv_player->v.flags & FL_WATERJUMP)
	{
		SV_WaterJump ();
		return;
	}

	// Player is (somehow) outside of the map
	if (SV_TestEntityPosition (sv_player)
			|| sv_player->v.movetype == MOVETYPE_FLY
			|| sv_player->v.movetype == MOVETYPE_NOCLIP)
	{
		SV_FreeMove ();
		return;
	}

	// walk
	if ((sv_player->v.waterlevel >= 2)
		&& (sv_player->v.movetype != MOVETYPE_NOCLIP))
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}


static void
SV_ReadClientMove (usercmd_t *move)
{
	int		i, bits;
	float	total;
	vec3_t	angle;
	eval_t	*val;

	// read ping time
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES]
		= sv.time - MSG_ReadFloat ();
	host_client->num_pings++;

	for (i = 0, total = 0; i < NUM_PING_TIMES; i++)
		total += host_client->ping_times[i];

	host_client->ping = total / NUM_PING_TIMES; // can be used for prediction
	host_client->latency = 0;

	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_ping)))
		val->_float = host_client->ping * 1000.0;

	// read current angles  
	for (i = 0; i < 3; i++)
		angle[i] = MSG_ReadAngle ();

	VectorCopy (angle, host_client->edict->v.v_angle);

	// read movement
	move->forwardmove = MSG_ReadShort ();
	move->sidemove = MSG_ReadShort ();
	move->upmove = MSG_ReadShort ();
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_movement)))
	{
		val->vector[0] = move->forwardmove;
		val->vector[1] = move->sidemove;
		val->vector[2] = move->upmove;
	}

	// read buttons
	bits = MSG_ReadByte ();
	host_client->edict->v.button0 = bits & 1;
	host_client->edict->v.button2 = (bits & 2) >> 1;
	host_client->edict->v.button1 = (bits & 4) >> 2;
	// LordHavoc: added 6 new buttons
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button3))) val->_float = ((bits >> 2) & 1);
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button4))) val->_float = ((bits >> 3) & 1);
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button5))) val->_float = ((bits >> 4) & 1);
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button6))) val->_float = ((bits >> 5) & 1);
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button7))) val->_float = ((bits >> 6) & 1);
	if ((val = GETEDICTFIELDVALUE (host_client->edict, eval_button8))) val->_float = ((bits >> 7) & 1);

	i = MSG_ReadByte ();
	if (i)
		host_client->edict->v.impulse = i;
}

/*
===================
Returns false if the client should be killed
===================
*/
static qboolean
SV_ReadClientMessage (void)
{
	int			ret;
	int			cmd;
	const char	*s;

	do {
	  nextmsg:
		ret = NET_GetMessage (host_client->netconnection);
		if (ret == -1) {
			Sys_Printf ("SV_ReadClientMessage: NET_GetMessage failed\n");
			return false;
		}
		if (!ret)
			return true;

		MSG_BeginReading ();

		while (1) {
			if (!host_client->active)
				return false;			// a command caused an error

			if (msg_badread) {
				Sys_Printf ("SV_ReadClientMessage: badread\n");
				return false;
			}

			cmd = MSG_ReadChar ();

			switch (cmd) {
				case -1:
					goto nextmsg;		// end of message

				default:
					Sys_Printf ("SV_ReadClientMessage: unknown command char\n");
					return false;

				case clc_nop:
					break;

				case clc_stringcmd:
					s = MSG_ReadString ();
					if (host_client->privileged)
						ret = 2;
					else
						ret = 0;
					if (strncasecmp (s, "status", 6) == 0 ||
						strncasecmp (s, "god", 3) == 0 ||
						strncasecmp (s, "notarget", 8) == 0 ||
						strncasecmp (s, "fly", 3) == 0 ||
						strncasecmp (s, "name", 4) == 0 ||
						strncasecmp (s, "noclip", 6) == 0 ||
						strncasecmp (s, "say", 3) == 0 ||
						strncasecmp (s, "say_team", 8) == 0 ||
						strncasecmp (s, "tell", 4) == 0 ||
						strncasecmp (s, "color", 5) == 0 ||
						strncasecmp (s, "kill", 4) == 0 ||
						strncasecmp (s, "pause", 5) == 0 ||
						strncasecmp (s, "spawn", 5) == 0 ||
						strncasecmp (s, "begin", 5) == 0 ||
						strncasecmp (s, "prespawn", 8) == 0 ||
						strncasecmp (s, "kick", 4) == 0 ||
						strncasecmp (s, "ping", 4) == 0 ||
						strncasecmp (s, "give", 4) == 0 ||
						strncasecmp (s, "ban", 3) == 0)
					{
							ret = 1;
							Cmd_ExecuteString (s, src_client);
					}
					else
					{
						if (ret == 2)
							Cbuf_InsertText (s);
						else
							Com_DPrintf ("%s tried to %s\n", host_client->name, s);
					}
					break;

				case clc_disconnect:
//              Sys_Printf ("SV_ReadClientMessage: client disconnected\n");
					return false;

				case clc_move:
					SV_ReadClientMove (&host_client->cmd);
					break;
			}
		}
	} while (ret == 1);

	return true;
}


void
SV_RunClients (void)
{
	Uint32	i;

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!SV_ReadClientMessage ()) {
			SV_DropClient (false);		// client misbehaved...
			continue;
		}

		if (!host_client->spawned) {
			// clear client movement until a new packet is received
			memset (&host_client->cmd, 0, sizeof (host_client->cmd));
			continue;
		}
		// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
			SV_ClientThink ();
	}
}
