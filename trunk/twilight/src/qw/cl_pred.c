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

#include "SDL.h"

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "keys.h"
#include "mathlib.h"
#include "pmove.h"

cvar_t     *cl_nopred;
cvar_t     *cl_pushlatency;


/*
==============
CL_PredictUsercmd
==============
*/
void
CL_PredictUsercmd (int id, player_state_t * from, player_state_t * to,
		usercmd_t *u, qboolean spectator)
{
#if 1
	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t   split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (id, from, &temp, &split, spectator);
		CL_PredictUsercmd (id, &temp, to, &split, spectator);
		return;
	}
#endif

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	pmove.player_id = id;
	pmove.oldbuttons = from->oldbuttons;
	pmove.waterjumptime = from->waterjumptime;
	pmove.dead = ccl.stats[STAT_HEALTH] <= 0;
	pmove.spectator = spectator;

	pmove.cmd = *u;

	PlayerMove ();

	to->waterjumptime = pmove.waterjumptime;
	to->oldbuttons = pmove.oldbuttons;
	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	to->groundent = pmove.groundent;

	to->weaponframe = from->weaponframe;
}



/*
==============
CL_PredictMove
Predict our own movement.
==============
*/
void
CL_PredictMove (void)
{
	int         i;
	float       f;
	frame_t    *from, *to = NULL;

	if (cl_pushlatency->fvalue > 0)
		Cvar_Set (cl_pushlatency, "0");

	if (cl.paused)
		return;

	ccl.oldtime = ccl.time;
	ccl.time = cls.realtime - cls.latency - cl_pushlatency->fvalue * 0.001;
	if (ccl.time > cls.realtime)
		ccl.time = cls.realtime;

	r_time = ccl.time;
	r_frametime = ccl.time - ccl.oldtime;

	if (ccl.intermission)
		return;

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >=
		UPDATE_BACKUP - 1)
		return;

	VectorCopy (cl.viewangles, cl.simangles);

	// this is the last frame received from the server
	from = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];

	if (cl_nopred->ivalue) {
		VectorCopy (from->playerstate[ccl.player_num].velocity, cl.simvel);
		VectorCopy (from->playerstate[ccl.player_num].origin, cl.simorg);
		return;
	}

	for (i = 1; i < UPDATE_BACKUP - 1 && cls.netchan.incoming_sequence + i <
		 cls.netchan.outgoing_sequence; i++) {
		to = &cl.frames[(cls.netchan.incoming_sequence + i) & UPDATE_MASK];
		CL_PredictUsercmd (ccl.player_num, &from->playerstate[ccl.player_num],
			&to->playerstate[ccl.player_num], &to->cmd, cl.spectator);
		if (to->senttime >= ccl.time)
			break;
		from = to;
	}

	if (i == UPDATE_BACKUP - 1 || !to)
		return;				// net hasn't deliver packets in a long time...

	// now interpolate some fraction of the final frame
	if (to->senttime == from->senttime)
		f = 0;
	else {
		f = (ccl.time - from->senttime) / (to->senttime - from->senttime);
		f = bound (0, f, 1);
	}

	if (VectorDistance_fast (from->playerstate[ccl.player_num].origin,
				to->playerstate[ccl.player_num].origin) > (128 * 128)) {
		// teleported, so don't lerp
		VectorCopy (to->playerstate[ccl.player_num].velocity, cl.simvel);
		VectorCopy (to->playerstate[ccl.player_num].origin, cl.simorg);
		return;
	}

	Lerp_Vectors (from->playerstate[ccl.player_num].origin,
		f, to->playerstate[ccl.player_num].origin, cl.simorg);
	Lerp_Vectors (from->playerstate[ccl.player_num].velocity,
		f, to->playerstate[ccl.player_num].velocity, cl.simvel);
}


/*
==============
CL_InitPrediction
==============
*/
void
CL_InitPrediction (void)
{
	cl_nopred = Cvar_Get ("cl_nopred", "0", CVAR_NONE, NULL);
	cl_pushlatency = Cvar_Get ("pushlatency", "-999", CVAR_NONE, NULL);
}
