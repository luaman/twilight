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
// cl_main.c  -- client main loop
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
#include "strlib.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "model.h"
#include "host.h"
#include "input.h"
#include "mathlib.h"
#include "net.h"
#include "screen.h"
#include "server.h"
#include "sound.h"
#include "world.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t		*_cl_name;
cvar_t		*_cl_color;

cvar_t		*cl_shownet;
cvar_t		*cl_nolerp;

cvar_t		*cl_sbar;
cvar_t		*cl_hudswap;
cvar_t		*cl_maxfps;

cvar_t		*cl_mapname;

cvar_t		*show_fps;

client_static_t cls;
client_state_t cl;

// FIXME: put these on hunk?
entity_t	cl_entities[MAX_EDICTS];
entity_t	cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t	cl_dlights[MAX_DLIGHTS];

int			cl_numvisedicts;
entity_t	*cl_visedicts[MAX_VISEDICTS];

extern int	r_framecount;

/*
=====================
CL_ClearState

=====================
*/
void
CL_ClearState (void)
{
	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	SZ_Clear (&cls.message);

// clear other arrays   
	memset (cl_entities, 0, sizeof (cl_entities));
	memset (cl_dlights, 0, sizeof (cl_dlights));
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof (cl_temp_entities));
	memset (cl_beams, 0, sizeof (cl_beams));
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void
CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds (true);

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected) {
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void
CL_Disconnect_f (void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer (false);
}




/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void
CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;					// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;						// need all the signon messages before
	// playing
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void
CL_SignonReply (void)
{
	char        str[8192];

	Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
		case 1:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, "prespawn");
			break;

		case 2:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message,
							 va ("name \"%s\"\n", _cl_name->string));

			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message,
							 va ("color %i %i\n", 
								 ((int) _cl_color->value) >> 4,
								 ((int) _cl_color->value) & 15));

			MSG_WriteByte (&cls.message, clc_stringcmd);
			snprintf (str, sizeof (str), "spawn %s", cls.spawnparms);
			MSG_WriteString (&cls.message, str);
			break;

		case 3:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, "begin");
			Cache_Report ();			// print remaining memory
			break;

		case 4:
			SCR_EndLoadingPlaque ();	// allow normal screen updates
			break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void
CL_NextDemo (void)
{
	char        str[1024];

	if (cls.demonum == -1)
		return;							// don't play demos

	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	snprintf (str, sizeof (str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void
CL_PrintEntities_f (void)
{
	entity_t   *ent;
	int         i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++) {
		Con_Printf ("%3i:", i);
		if (!ent->model) {
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
					ent->model->name, ent->frame, ent->origin[0],
					ent->origin[1], ent->origin[2], ent->angles[0],
					ent->angles[1], ent->angles[2]);
	}
}

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
	float       time;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= time * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float
CL_LerpPoint (void)
{
	float       f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp->value || cls.timedemo || sv.active) {
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1) {						// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;

	if (frac < 0) {
		if (frac < -0.01) {
			cl.time = cl.mtime[1];
		}
		frac = 0;
	} else if (frac > 1) {
		if (frac > 1.01) {
			cl.time = cl.mtime[0];
		}
		frac = 1;
	}

	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	int         i, j;
	float       frac, f, d;
	vec3_t      delta;
	vec3_t      oldorg;
	dlight_t   *dl;
	trace_t		tr;
	extern		cvar_t *gl_flashblend, *gl_oldlights;

// determine partial update time    
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

//
// interpolate player info
//
	Lerp_Vectors (cl.mvelocity[1], frac, cl.mvelocity[0], cl.velocity);

	if (cls.demoplayback) {
		// interpolate the angles 
		for (j = 0; j < 3; j++) {
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
		}
	}


// start on the entity after the world
	for (i = 1, ent = cl_entities + 1; i < cl.num_entities; i++, ent++) {
		if (!ent->model)				// empty slot
			continue;
// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0]) {
			ent->model = NULL;
			ent->translate_start_time = 0;
			ent->rotate_start_time    = 0;
			VectorClear (ent->last_light);
			continue;
		}

		VectorCopy (ent->origin, oldorg);

		if (ent->forcelink) {			// the entity was not updated in the
			// last message
			// so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		} else {						// if the delta is large, assume a
			// teleport and don't lerp
			f = frac;
			for (j = 0; j < 3; j++) {
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
					f = 1;				// assume a teleportation, not a motion
			}

			if (f >= 1)
			{
				ent->translate_start_time = 0;
				ent->rotate_start_time    = 0;
				VectorClear (ent->last_light);
			}

			// interpolate the origin and angles
			for (j = 0; j < 3; j++) {
				ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f * d;
			}

		}

		if (ent->effects)
		{
			if (ent->effects & EF_BRIGHTFIELD)
				R_EntityParticles (ent);

			if (ent->effects & EF_MUZZLEFLASH) {
				// don't draw our own muzzle flash if flashblending
				if (i != cl.viewentity || chase_active->value || !gl_flashblend->value) {
					vec3_t      fv;

					dl = CL_AllocDlight (i);
					VectorCopy (ent->origin, dl->origin);
					AngleVectors (ent->angles, fv, NULL, NULL);
					VectorMA (dl->origin, 18, fv, dl->origin);

					if (!gl_flashblend->value && !gl_oldlights->value)
					{			
						memset (&tr, 0, sizeof(tr));

						VectorCopy (dl->origin, tr.endpos);

						SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, ent->origin, dl->origin, &tr);
						
						VectorCopy (tr.endpos, dl->origin);
					}

					dl->radius = 200 + (Q_rand () & 31);
					dl->minlight = 32;
					dl->die = cl.time + 0.1;
				}
			}

			// spawn light flashes, even ones coming from invisible objects
			if (ent->effects & EF_LIGHTMASK) {
				CL_NewDlight (i, ent->origin, ent->effects);
			}
		}

// rotate binary objects locally
		if (ent->model->flags)
		{
			if (ent->model->flags & EF_ROTATE) {
				ent->angles[1] = anglemod (100 * (cl.time + ent->syncbase));
			}
			if (ent->model->flags & EF_GIB)
				R_RocketTrail (oldorg, ent->origin, 2);
			else if (ent->model->flags & EF_ZOMGIB)
				R_RocketTrail (oldorg, ent->origin, 4);
			else if (ent->model->flags & EF_TRACER)
				R_RocketTrail (oldorg, ent->origin, 3);
			else if (ent->model->flags & EF_TRACER2)
				R_RocketTrail (oldorg, ent->origin, 5);
			else if (ent->model->flags & EF_ROCKET) {
				R_RocketTrail (oldorg, ent->origin, 0);
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;
			} else if (ent->model->flags & EF_GRENADE)
				R_RocketTrail (oldorg, ent->origin, 1);
			else if (ent->model->flags & EF_TRACER3)
				R_RocketTrail (oldorg, ent->origin, 6);
		}

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active->value)
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS) {
			cl_visedicts[cl_numvisedicts++] = ent;
		}
	}

	for (i = 0; i < cl.num_statics; i++) {
		cl_static_entities[i].visframe = r_framecount;
		if (cl_numvisedicts >= MAX_VISEDICTS)
			continue;
		cl_visedicts[cl_numvisedicts++] = &cl_static_entities[i];
	}
}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int
CL_ReadFromServer (void)
{
	int         ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	do {
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet->value)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

//
// bring the links up to date
//
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void
CL_SendCmd (void)
{
	usercmd_t   cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS) {
		// get basic movement from keyboard
		CL_BaseMove (&cmd);

		// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

		// send the unreliable message
		CL_SendMove (&cmd);

	}

	if (cls.demoplayback) {
		SZ_Clear (&cls.message);
		return;
	}
// send the reliable message
	if (!cls.message.cursize)
		return;							// no message at all

	if (!NET_CanSendMessage (cls.netcon)) {
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

void 
CL_SbarCallback (cvar_t *cvar)
{
	vid.recalc_refdef = true;
}

/*
=================
CL_Init_Cvars
=================
*/
void
CL_Init_Cvars (void)
{
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL);

	_cl_name = Cvar_Get ("_cl_name", "player", CVAR_ARCHIVE, NULL);
	_cl_color = Cvar_Get ("_cl_color", "0", CVAR_ARCHIVE, NULL);

	// cl_shownet can be 0, 1, or 2
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL);
	cl_nolerp = Cvar_Get ("cl_nolerp", "0", CVAR_NONE, NULL);

	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, &CL_SbarCallback);
	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL);
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL);

	cl_mapname = Cvar_Get ("cl_mapname", "", CVAR_ROM, NULL);
}

/*
=================
CL_Init
=================
*/
void
CL_Init (void)
{
	SZ_Alloc (&cls.message, 1024);

	CL_Input_Init_Cvars ();
	CL_Input_Init ();
	CL_InitTEnts ();

	//
	// register our commands
	//
	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
}

