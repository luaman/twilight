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

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "mathlib.h"
#include "pmove.h"
#include "strlib.h"
#include "host.h"

#define	PM_SPECTATORMAXSPEED	500
#define	PM_STOPSPEED			100
#define	PM_MAXSPEED				320
#define BUTTON_JUMP				2
#define BUTTON_ATTACK			1
#define MAX_ANGLE_TURN			10

static vec3_t desired_position;			// where the camera wants to be
static qboolean locked = false;
static int  oldbuttons;

// track high fragger
cvar_t		*cl_hightrack;
cvar_t		*cl_chasecam;

qboolean	cam_forceview;
vec3_t		cam_viewangles;
double		cam_lastviewtime;

int			spec_track = 0;				// player# of who we are tracking
int			autocam = CAM_NONE;

// returns true if weapon model should be drawn in camera mode
qboolean
Cam_DrawViewModel (void)
{
	if (!cl.spectator)
		return true;

	return (autocam && locked && cl_chasecam->ivalue);
}

// returns true if we should draw this player, we don't if we are chase camming
qboolean
Cam_DrawPlayer (int playernum)
{
	if (cl.spectator && autocam && locked && cl_chasecam->ivalue &&
		spec_track == playernum)
		return false;
	return true;
}

static void
Cam_Unlock (void)
{
	if (autocam) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "ptrack");
		autocam = CAM_NONE;
		locked = false;
	}
}

static void
Cam_Lock (int playernum)
{
	char	st[40];

	snprintf (st, sizeof (st), "ptrack %i", playernum);
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, st);
	spec_track = playernum;
	cam_forceview = true;
	locked = false;
}

static trace_t *
Cam_DoTrace (vec3_t vec1, vec3_t vec2)
{
	VectorCopy (vec1, pmove.origin);
	return PM_PlayerMove (pmove.origin, vec2);
}

// Returns distance or 9999 if invalid for some reason
static float
Cam_TryFlyby (player_state_t * self, player_state_t * player, vec3_t vec,
			  qboolean checkvis)
{
	vec3_t		v;
	trace_t		*trace;
	float		len;

	Vector2Angles (vec, v);
	VectorCopy (v, pmove.angles);
	VectorNormalizeFast (vec);
	VectorMA (player->origin, 800, vec, v);

	// v is endpos
	// fake a player move
	trace = Cam_DoTrace (player->origin, v);

	if ( /* trace.inopen || */ trace->inwater)
		return 9999;

	VectorCopy (trace->endpos, vec);
	len = VectorDistance (trace->endpos, player->origin);

	if (len < 32 || len > 800)
		return 9999;

	if (checkvis) {
		trace = Cam_DoTrace (self->origin, vec);
		if (trace->fraction != 1 || trace->inwater)
			return 9999;

		len = VectorDistance (trace->endpos, self->origin);
	}

	return len;
}

// Is player visible?
static qboolean
Cam_IsVisible (player_state_t * player, vec3_t vec)
{
	trace_t		*trace;
	vec3_t      v;
	float       d;

	trace = Cam_DoTrace (player->origin, vec);

	if (trace->fraction != 1 || /* trace.inopen || */ trace->inwater)
		return false;

	// check distance, don't let the player get too far away or too close
	d = VectorSubtract (player->origin, vec, v);
	d = DotProduct (v,v);

	return (d >= (16*16));
}

static qboolean
InitFlyby (player_state_t * self, player_state_t * player, int checkvis)
{
	float       f, max;
	vec3_t      vec, vec2;
	vec3_t      forward, right, up;

	VectorCopy (player->viewangles, vec);
	vec[0] = 0;
	AngleVectors (vec, forward, right, up);
//  for (i = 0; i < 3; i++)
//      forward[i] *= 3;

	max = 1000;
	VectorAdd (forward, up, vec2);
	VectorAdd (vec2, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, up, vec2);
	VectorSubtract (vec2, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (forward, right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (forward, up, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (forward, up, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorAdd (up, right, vec2);
	VectorSubtract (vec2, forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorSubtract (up, right, vec2);
	VectorSubtract (vec2, forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// invert
	VectorInverse (forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorCopy (forward, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// invert
	VectorInverse (right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	VectorCopy (right, vec2);
	if ((f = Cam_TryFlyby (self, player, vec2, checkvis)) < max) {
		max = f;
		VectorCopy (vec2, vec);
	}
	// ack, can't find him
	if (max >= 1000) {
//      Cam_Unlock();
		return false;
	}
	locked = true;
	VectorCopy (vec, desired_position);
	return true;
}

static void
Cam_CheckHighTarget (void)
{
	int         i, j, max;
	user_info_t *s;

	j = -1;
	for (i = 0, max = -9999; i < ccl.max_users; i++) {
		s = &ccl.users[i];
		if (s->name[0] && !(s->flags & USER_SPECTATOR) && s->frags > max) {
			max = s->frags;
			j = i;
		}
	}
	if (j >= 0) {
		if (!locked || ccl.users[j].frags > ccl.users[spec_track].frags)
			Cam_Lock (j);
	} else
		Cam_Unlock ();
}

// ZOID
//
// Take over the user controls and track a player.
// We find a nice position to watch the player and move there
void
Cam_Track (usercmd_t *cmd)
{
	player_state_t *player, *self;
	frame_t    *frame;
	vec3_t      vec;
	float       len;

	if (!cl.spectator)
		return;

	if (cl_hightrack->ivalue && !locked)
		Cam_CheckHighTarget ();

	if (!autocam || ccls.state != ca_active)
		return;

	if (locked && (!ccl.users[spec_track].name[0]
				   || (ccl.users[spec_track].flags & USER_SPECTATOR))) {
		locked = false;
		if (cl_hightrack->ivalue)
			Cam_CheckHighTarget ();
		else
			Cam_Unlock ();
		return;
	}

	frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	player = frame->playerstate + spec_track;
	self = frame->playerstate + ccl.player_num;

	if (!locked || !Cam_IsVisible (player, desired_position)) {
		if (!locked || host.time - cam_lastviewtime > 0.1) {
			if (!InitFlyby (self, player, true))
				InitFlyby (self, player, false);
			cam_lastviewtime = host.time;
		}
	} else
		cam_lastviewtime = host.time;

	// couldn't track for some reason
	if (!locked || !autocam)
		return;

	if (cl_chasecam->ivalue) {
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;

		VectorCopy (player->viewangles, cl.viewangles);
		VectorCopy (player->origin, desired_position);
		if (memcmp (&desired_position, &self->origin, sizeof (desired_position))
			!= 0) {
			MSG_WriteByte (&cls.netchan.message, clc_tmove);
			MSG_WriteCoord (&cls.netchan.message, desired_position[0]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[1]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[2]);
			// move there locally immediately
			VectorCopy (desired_position, self->origin);
		}
		self->weaponframe = player->weaponframe;

	} else {
		// Ok, move to our desired position and set our angles to view
		// the player
		VectorSubtract (desired_position, self->origin, vec);
		len = DotProduct (vec,vec);
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
		if (len > (16*16)) {					// close enough?
			MSG_WriteByte (&cls.netchan.message, clc_tmove);
			MSG_WriteCoord (&cls.netchan.message, desired_position[0]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[1]);
			MSG_WriteCoord (&cls.netchan.message, desired_position[2]);
		}
		// move there locally immediately
		VectorCopy (desired_position, self->origin);

		VectorSubtract (player->origin, desired_position, vec);
		Vector2Angles (vec, cl.viewangles);
		cl.viewangles[0] = -cl.viewangles[0];
	}
}


void
Cam_FinishMove (usercmd_t *cmd)
{
	int         i;
	user_info_t *s;
	int         end;

	if (ccls.state != ca_active)
		return;

	if (!cl.spectator)					// only in spectator mode
		return;

	if (cmd->buttons & BUTTON_ATTACK) {
		if (!(oldbuttons & BUTTON_ATTACK)) {

			oldbuttons |= BUTTON_ATTACK;
			autocam++;

			if (autocam > CAM_TRACK) {
				Cam_Unlock ();
				VectorCopy (cl.viewangles, cmd->angles);
				return;
			}
		} else
			return;
	} else {
		oldbuttons &= ~BUTTON_ATTACK;
		if (!autocam)
			return;
	}

	if (autocam && cl_hightrack->ivalue) {
		Cam_CheckHighTarget ();
		return;
	}

	if (locked) {
		if ((cmd->buttons & BUTTON_JUMP) && (oldbuttons & BUTTON_JUMP))
			return;						// don't pogo stick

		if (!(cmd->buttons & BUTTON_JUMP)) {
			oldbuttons &= ~BUTTON_JUMP;
			return;
		}
		oldbuttons |= BUTTON_JUMP;		// don't jump again until released
	}
//  Com_Printf("Selecting track target...\n");

	if (locked && autocam)
		end = (spec_track + 1) % MAX_CLIENTS;
	else
		end = spec_track;
	i = end;
	do {
		s = &ccl.users[i];
		if (s->name[0] && !(s->flags & USER_SPECTATOR)) {
			Cam_Lock (i);
			return;
		}
		i = (i + 1) % MAX_CLIENTS;
	} while (i != end);
	// stay on same guy?
	i = spec_track;
	s = &ccl.users[i];
	if (s->name[0] && !(s->flags & USER_SPECTATOR)) {
		Cam_Lock (i);
		return;
	}
	Com_Printf ("No target found ...\n");
	autocam = locked = false;
}

void
Cam_Reset (void)
{
	autocam = CAM_NONE;
	spec_track = 0;
}

void
CL_InitCam (void)
{
	cl_hightrack = Cvar_Get ("cl_hightrack", "0", CVAR_NONE, NULL);
	cl_chasecam = Cvar_Get ("cl_chasecam", "0", CVAR_NONE, NULL);
}

