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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

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
=================
CL_NudgePosition

If pmove.origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
void
CL_NudgePosition (void)
{
	vec3_t      base;
	int         x, y;

	if (PM_HullPointContents (&cl.model_precache[1]->hulls[1], 0, pmove.origin)
		== CONTENTS_EMPTY)
		return;

	VectorCopy (pmove.origin, base);
	for (x = -1; x <= 1; x++) {
		for (y = -1; y <= 1; y++) {
			pmove.origin[0] = base[0] + x * 1.0 / 8;
			pmove.origin[1] = base[1] + y * 1.0 / 8;
			if (PM_HullPointContents
				(&cl.model_precache[1]->hulls[1], 0,
				 pmove.origin) == CONTENTS_EMPTY)
				return;
		}
	}

	Com_DPrintf ("CL_NudgePosition: stuck\n");
}

/*
==============
CL_PredictUsercmd
==============
*/
void
CL_PredictUsercmd (player_state_t * from, player_state_t * to, usercmd_t *u,
				   qboolean spectator)
{
	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t   split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split, spectator);
		CL_PredictUsercmd (&temp, to, &split, spectator);
		return;
	}

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	pmove.oldbuttons = from->oldbuttons;
	pmove.waterjumptime = from->waterjumptime;
	pmove.dead = cl.stats[STAT_HEALTH] <= 0;
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
==============
*/
void
CL_PredictMove (void)
{
	int         i;
	float       f;
	frame_t    *from, *to = NULL;
	int         oldphysent;

	if (cl_pushlatency->value > 0)
		Cvar_Set (cl_pushlatency, "0");

	if (cl.paused)
		return;

	cl.oldtime = cl.time;
	cl.time = cls.realtime - cls.latency - cl_pushlatency->value * 0.001;
	if (cl.time > cls.realtime)
		cl.time = cls.realtime;

	if (cl.intermission)
		return;

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >=
		UPDATE_BACKUP - 1)
		return;

	VectorCopy (cl.viewangles, cl.simangles);

	// this is the last frame received from the server
	from = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];

	// we can now render a frame
	if (cls.state == ca_onserver) {		
		// first update is the final signon stage
		char        text[1024] = { 0 };

		cls.state = ca_active;
		snprintf (text, sizeof (text), "Twilight QWCL: %s", cls.servername);
		SDL_WM_SetCaption (text, "Twilight QWCL");
	}

	if (cl_nopred->value) {
		VectorCopy (from->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (from->playerstate[cl.playernum].origin, cl.simorg);
		return;
	}

	// predict forward until cl.time <= to->senttime
	oldphysent = pmove.numphysent;
	CL_SetSolidPlayers (cl.playernum);

	for (i = 1; i < UPDATE_BACKUP - 1 && cls.netchan.incoming_sequence + i <
		 cls.netchan.outgoing_sequence; i++) {
		to = &cl.frames[(cls.netchan.incoming_sequence + i) & UPDATE_MASK];
		CL_PredictUsercmd (&from->playerstate[cl.playernum],
			&to->playerstate[cl.playernum], &to->cmd, cl.spectator);
		if (to->senttime >= cl.time)
			break;
		from = to;
	}

	pmove.numphysent = oldphysent;

	if (i == UPDATE_BACKUP - 1 || !to)
		return;							// net hasn't deliver packets in a long 

	// 
	// time...

	// now interpolate some fraction of the final frame
	if (to->senttime == from->senttime)
		f = 0;
	else {
		f = (cl.time - from->senttime) / (to->senttime - from->senttime);
		f = bound (0, f, 1);
	}

	for (i = 0; i < 3; i++) {
		if (Q_fabs (from->playerstate[cl.playernum].origin[i] - to->playerstate[cl.playernum].origin[i]) > 128) {
			// teleported, so don't lerp
			VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
			VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
			return;
		}
	}

	Lerp_Vectors (from->playerstate[cl.playernum].origin,
		f, to->playerstate[cl.playernum].origin, cl.simorg);
	Lerp_Vectors (from->playerstate[cl.playernum].velocity,
		f, to->playerstate[cl.playernum].velocity, cl.simvel);
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
