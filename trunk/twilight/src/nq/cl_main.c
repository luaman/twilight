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
#include "strlib.h"
#include "client.h"
#include "cmd.h"
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
#include "gl_textures.h"
#include "teamplay.h"
#include "surface.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t		*_cl_name;
cvar_t		*_cl_color;

cvar_t		*cl_shownet;
cvar_t		*cl_nolerp;

cvar_t		*cl_hudswap;
cvar_t		*cl_maxfps;

cvar_t		*cl_mapname;

cvar_t		*show_fps;

client_static_t	 cls;
client_state_t	 cl;
memzone_t		*cl_zone;

// FIXME: put these on hunk?
entity_t	cl_entities[MAX_EDICTS];
entity_t	cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t	cl_dlights[MAX_DLIGHTS];

extern Uint	r_framecount;

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
	Zone_EmptyZone (cl_zone);
	memset (&cl, 0, sizeof (cl));

	// we don't get this from the server, that'd take a new protocol
	cl.viewzoom = 1.0f;

	SZ_Clear (&cls.message);

// clear other arrays   
	memset (cl_entities, 0, sizeof (cl_entities));
	memset (cl_dlights, 0, sizeof (cl_dlights));
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof (cl_temp_entities));
	memset (cl_beams, 0, sizeof (cl_beams));

	SetupLightmapSettings ();
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

		Com_DPrintf ("Sending clc_disconnect\n");
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
	Com_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

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

	Com_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
		case 1:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, "prespawn");
			break;

		case 2:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message,
							 va ("name \"%s\"\n", _cl_name->svalue));

			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message,
							 va ("color %i %i\n", 
								 ((int) _cl_color->ivalue) >> 4,
								 ((int) _cl_color->ivalue) & 15));

			MSG_WriteByte (&cls.message, clc_stringcmd);
			snprintf (str, sizeof (str), "spawn %s", cls.spawnparms);
			MSG_WriteString (&cls.message, str);
			break;

		case 3:
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, "begin");
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
			Com_Printf ("No demos listed with startdemos\n");
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
		Com_Printf ("%3i:", i);
		if (!ent->common.model) {
			Com_Printf ("EMPTY\n");
			continue;
		}
		Com_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
					ent->common.model->name, ent->common.frame,
					ent->common.origin[0], ent->common.origin[1],
					ent->common.origin[2], ent->common.angles[0],
					ent->common.angles[1], ent->common.angles[2]);
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
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
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

	dl->radius = 1.0f;
	dl->die = cl.time + 0.1f;
	VectorCopy (org, dl->origin);

	VectorClear (dl->color);

	if (effects & EF_BRIGHTLIGHT)
	{
		dl->color[0] += 400.0f;
		dl->color[1] += 400.0f;
		dl->color[2] += 400.0f;
	}
	if (effects & EF_DIMLIGHT)
	{
		dl->color[0] += 200.0f;
		dl->color[1] += 200.0f;
		dl->color[2] += 200.0f;
	}

	if (effects & EF_RED)
		dl->color[0] += 200.0f;
	if (effects & EF_BLUE)
		dl->color[2] += 200.0f;
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

	if (!f || cl_nolerp->ivalue || cls.timedemo || sv.active) {
		cl.time = cl.mtime[0];
		r_time = cl.time;
		r_frametime = cl.time - cl.oldtime;
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

	r_time = cl.time;
	r_frametime = cl.time - cl.oldtime;

	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
static void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	int         i;
	float       frac;
	dlight_t   *dl;
	extern		cvar_t *gl_flashblend, *gl_oldlights;

// determine partial update time    
	frac = CL_LerpPoint ();

	V_ClearEntities ();

//
// interpolate player info
//
	Lerp_Vectors (cl.mvelocity[1], frac, cl.mvelocity[0], cl.velocity);

	if (cls.demoplayback) {
		// interpolate the angles
		Lerp_Angles (cl.mviewangles[1], frac, cl.mviewangles[0], cl.viewangles);
	}

// start on the entity after the world
	for (i = 1, ent = cl_entities + 1; i < cl.num_entities; i++, ent++) {
		if (!ent->common.model)				// empty slot
			continue;
// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0]) {
			ent->common.model = NULL;
			VectorClear (ent->common.last_light);
			continue;
		}

		CL_Lerp_OriginAngles (ent);

		if (ent->effects && (cl.time - cl.oldtime))
		{
			if (ent->effects & EF_BRIGHTFIELD)
				R_EntityParticles (&ent->common);

			if (ent->effects & EF_MUZZLEFLASH) {
				// don't draw our own muzzle flash if flashblending
				if (i != cl.viewentity || chase_active->ivalue
						|| !gl_flashblend->ivalue) {
					vec3_t fv, impact, impactnormal;

					dl = CL_AllocDlight (i);
					VectorCopy (ent->common.origin, dl->origin);
					AngleVectors (ent->common.angles, fv, NULL, NULL);
					VectorMA (dl->origin, 18, fv, dl->origin);

					if (!gl_flashblend->ivalue && !gl_oldlights->ivalue)
					{
						TraceLine(cl.worldmodel, ent->common.origin, dl->origin, impact, impactnormal);
						VectorCopy(impact, dl->origin);
					}

					dl->radius = 200 + (rand () & 31);
					dl->minlight = 32;
					dl->color[0] = 0.5f;
					dl->color[1] = 0.5f;
					dl->color[2] = 0.4f;
					dl->die = cl.time + 0.1;
				}
			}

			// spawn light flashes, even ones coming from invisible objects
			if (ent->effects & EF_LIGHTMASK) {
				CL_NewDlight (i, ent->common.origin, ent->effects);
			}
		}

// rotate binary objects locally
		if (ent->common.model->flags && (cl.time - cl.oldtime)) {
			int flags = ent->common.model->flags;

			if (flags & EF_ROTATE) {
				flags &= ~EF_ROTATE;
				ent->common.angles[YAW] = ANGLEMOD (100 * (cl.time + ent->syncbase));
				CL_Update_Matrices (ent);
			}

			if (flags & EF_ROCKET) {
				dl = CL_AllocDlight (i);
				VectorCopy (ent->common.origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;
				VectorSet (dl->color, 1.0f, 0.6f, 0.2f);
			}

			if (flags)
				R_ParticleTrail (&ent->common);
		}

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active->ivalue)
			continue;

		V_AddEntity ( ent );
	}

	for (i = 0; i < cl.num_statics; i++) {
		cl_static_entities[i].visframe = vis_framecount;

		V_AddEntity ( &cl_static_entities[i] );
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

		cl.last_received_message = host_realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet->ivalue)
		Com_Printf ("\n");

	CL_RelinkEntities ();
    CL_ScanForBModels ();
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
		Com_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void
Cmd_ForwardToServer (void)
{
	char *s;

	if (cls.state != ca_connected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected

	MSG_WriteByte (&cls.message, clc_stringcmd);
	if (strcasecmp (Cmd_Argv (0), "cmd") != 0) {
		SZ_Print (&cls.message, Cmd_Argv (0));
		SZ_Print (&cls.message, " ");
	}
	if (Cmd_Argc () > 1) {
		if (strcasecmp (Cmd_Argv (0), "say") &&
				strcasecmp (Cmd_Argv (0), "say_team"))
			SZ_Print (&cls.message, Cmd_Args ());
		else {
			s = Team_ParseSay (Cmd_Args ());
			if (*s && *s < 32 && *s != 10) {
				// otherwise the server would eat leading characters
				// less than 32 or greater than 127
				SZ_Print (&cls.message, "\"");
				SZ_Print (&cls.message, s);
				SZ_Print (&cls.message, "\"");
			} else
				SZ_Print (&cls.message, s);
		}
	} else
		SZ_Print (&cls.message, "\n");
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

	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL);
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL);

	cl_mapname = Cvar_Get ("cl_mapname", "", CVAR_ROM, NULL);

	CL_Input_Init_Cvars ();
	CL_TEnts_Init_Cvars ();
}

/*
=================
CL_Init
=================
*/
void
CL_Init (void)
{
	cl_zone = Zone_AllocZone ("client");

	SZ_Init (&cls.message, cls.msg_buf, sizeof(cls.msg_buf));

	CL_Input_Init ();
	CL_TEnts_Init ();
	Team_Init ();

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

