/*
	$RCSfile$ -- coordinates spawning and killing of local servers

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
#include <stdarg.h>
#include <stdlib.h>	/* for rand() */

#include "SDL.h"

#include "quakedef.h"
#include "strlib.h"
#include "cdaudio.h"
#include "cmd.h"
#include "console.h"
#include "client.h"
#include "cvar.h"
#include "draw.h"
#include "model.h"
#include "host.h"
#include "input.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "net.h"
#include "sbar.h"
#include "screen.h"
#include "server.h"
#include "sound.h"
#include "sys.h"
#include "view.h"
#include "wad.h"
#include "cpu.h"

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

qboolean    host_initialized;			// true if into command execution

double      host_frametime;
double      host_time;
double      host_realtime;				// without any filtering or bounding
double      oldrealtime;				// last frame run
int         host_framecount;

int         host_hunklevel;

int         minimum_memory;

client_t   *host_client;				// current client

jmp_buf     host_abortserver;

Uint8      *host_basepal;
Uint8      *host_colormap;

cvar_t     *host_framerate;
cvar_t     *host_speeds;

cvar_t     *sys_ticrate;
cvar_t     *serverprofile;

cvar_t     *fraglimit;
cvar_t     *timelimit;
cvar_t     *teamplay;

cvar_t     *samelevel;
cvar_t     *noexit;

cvar_t     *developer;

cvar_t     *skill;
cvar_t     *deathmatch;
cvar_t     *coop;

cvar_t     *pausable;

cvar_t     *temp1;

/*
================
Host_EndGame
================
*/
void
Host_EndGame (char *message, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, message);
	vsnprintf (string, sizeof (string), message, argptr);
	va_end (argptr);
	Com_Printf ("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s\n", string);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void
Host_Error (char *error, ...)
{
	va_list     argptr;
	char        string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque ();			// reenable screen updates

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	Com_Printf ("Host_Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n", string);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void
Host_FindMaxClients (void)
{
	Uint         i;

	svs.maxclients = 1;

	i = COM_CheckParm ("-dedicated");
	if (i) {
		cls.state = ca_dedicated;
		isDedicated = true;
		if (i != (com_argc - 1)) {
			svs.maxclients = Q_atoi (com_argv[i + 1]);
		} else
			svs.maxclients = 16;
	} else {
		cls.state = ca_disconnected;
		r_worldmodel = NULL;
		isDedicated = false;
	}

	i = COM_CheckParm ("-listen");
	if (i) {
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = Q_atoi (com_argv[i + 1]);
		else
			svs.maxclients = MAX_SCOREBOARD;
	}
	svs.maxclients = bound (1, svs.maxclients, MAX_SCOREBOARD);

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients =
		Zone_AllocName ("clients", svs.maxclientslimit * sizeof (client_t));

	if (svs.maxclients > 1)
		Cvar_Set (deathmatch, "1");
	else
		Cvar_Set (deathmatch, "0");
}

/*
=======================
Host_InitLocal_Cvars
======================
*/
void
Host_InitLocal_Cvars (void)
{
	// set for slow motion
	host_framerate = Cvar_Get ("host_framerate", "0", CVAR_NONE, NULL);
	// set for running times
	host_speeds = Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL);

	sys_ticrate = Cvar_Get ("sys_ticrate", "0.05", CVAR_NONE, NULL);
	serverprofile = Cvar_Get ("serverprofile", "0", CVAR_NONE, NULL);

	fraglimit = Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO, NULL);
	timelimit = Cvar_Get ("timelimit", "0", CVAR_SERVERINFO, NULL);
	teamplay = Cvar_Get ("teamplay", "0", CVAR_SERVERINFO, NULL);

	samelevel = Cvar_Get ("samelevel", "0", CVAR_NONE, NULL);
	noexit = Cvar_Get ("noexit", "0", CVAR_SERVERINFO, NULL);

	developer = Cvar_Get ("developer", "0", CVAR_NONE, NULL);

	// 0 - 3
	skill = Cvar_Get ("skill", "1", CVAR_NONE, NULL);
	// 0, 1, or 2
	deathmatch = Cvar_Get ("deathmatch", "0", CVAR_NONE, NULL);
	// 0 or 1
	coop = Cvar_Get ("coop", "0", CVAR_NONE, NULL);

	pausable = Cvar_Get ("pausable", "1", CVAR_NONE, NULL);

	temp1 = Cvar_Get ("temp1", "0", CVAR_NONE, NULL);
}

/*
=======================
Host_InitLocal
======================
*/
void
Host_InitLocal (void)
{
	Host_InitCommands ();
	Host_FindMaxClients ();
	host_time = 1.0;		// so a think at time 0 won't get called
}

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to file
===============
*/
void
Host_WriteConfiguration (char *name)
{
	FILE       *f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized && !isDedicated) {
		char fname[MAX_QPATH] = { 0 };

		snprintf (fname, sizeof (fname), "%s/%s", com_gamedir, name);
		COM_DefaultExtension (fname, ".cfg");

		f = fopen (fname, "wt");
		if (!f) {
			Com_Printf ("Couldn't write %s.\n", fname);
			return;
		}

		Com_Printf ("Writing %s\n", fname);

		Key_WriteBindings (f);
		Cvar_WriteVars (f);

		fclose (f);
	}
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void
SV_ClientPrintf (const char *fmt, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void
SV_BroadcastPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	Uint32		i;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	for (i = 0; i < svs.maxclients; i++)
		if (svs.clients[i].active && svs.clients[i].spawned) {
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void
Host_ClientCommands (char *fmt, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void
SV_DropClient (qboolean crash)
{
	int         saveSelf;
	Uint32		i;
	client_t	*client;

	if (!crash) {
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection)) {
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG (host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect, "QC Function ClientDisconnect is missing.");
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf ("Client %s removed\n", host_client->name);
	}
// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void
Host_ShutdownServer (qboolean crash)
{
	Uint32		i, count;
	sizebuf_t	buf;
	Uint8		message[4];
	double		start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_DoubleTime ();
	do {
		count = 0;
		for (i = 0, host_client = svs.clients; i < svs.maxclients;
			 i++, host_client++) {
			if (host_client->active && host_client->message.cursize) {
				if (NET_CanSendMessage (host_client->netconnection)) {
					NET_SendMessage (host_client->netconnection,
									 &host_client->message);
					SZ_Clear (&host_client->message);
				} else {
					NET_GetMessage (host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime () - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	SZ_Init (&buf, message, sizeof(message));
	SZ_Clear (&buf);

	MSG_WriteByte (&buf, svc_disconnect);
	count = NET_SendToAll (&buf, 5);
	if (count)
		Com_Printf
			("Host_ShutdownServer: NET_SendToAll failed for %u clients\n",
			 count);

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++)
		if (host_client->active)
			SV_DropClient (crash);

	// Clear the structs.
	Host_ClearMemory ();

	// clear structures
	memset (svs.clients, 0, svs.maxclientslimit * sizeof (client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void
Host_ClearMemory (void)
{
	if (!cl.worldmodel && !sv.worldmodel)		// Nothing to clear.
		return;

	Mod_ClearAll ();

	cls.signon = 0;
	if (sv_zone)
		Zone_EmptyZone (sv_zone);
	if (cl_zone)
		Zone_EmptyZone (cl_zone);
	memset (&sv, 0, sizeof (sv));
	memset (&cl, 0, sizeof (cl));
}


/* ============================================================================ */

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean
Host_FilterTime (float time)
{
	float		fps;
	float		newframetime;

	host_realtime += time;
	cls.realtime = host_realtime;
	r_realtime = host_realtime;

	/* if the frame time is below 0.001, don't even bother computing anything */
	newframetime = host_realtime - oldrealtime;
	if (newframetime < 0.001)
		return false;

	fps = cl_maxfps->fvalue;
	if (cl.maxclients > 1) {
		fps = bound (30.0f, fps, 72.0f);
	} else if (fps) {
		fps = bound (30.0f, fps, 999.0f);
	}

	/* eviltypeguy - added && cl.maxclients > 1 to allow uncapped framerate
	   when playing singleplayer quake, possible NetQuake breakage? */
	if ((!cls.timedemo && fps) && (newframetime < (1.0 / fps)))
		return false;					/* framerate is too high */

	host_frametime = newframetime;
	oldrealtime = host_realtime;

	if (host_framerate->fvalue > 0)
		host_frametime = host_framerate->fvalue;
	else {								/* don't allow really long frames */
		if (host_frametime > 0.1)
			host_frametime = 0.1;
	}

	return true;
}

/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void
Host_GetConsoleCommands (void)
{
	char       *cmd;

	while (1) {
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}


/*
==================
Host_ServerFrame

==================
*/
void
Host_ServerFrame (void)
{
// run the world state  
	pr_global_struct->frametime = host_frametime;

// set the time and clear the general datagram
	SV_ClearDatagram ();

// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();

// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
		SV_Physics ();

// send all messages to the clients
	SV_SendClientMessages ();
}

int fps_count = 0;

/*
==================
Host_Frame

Runs all active servers
==================
*/
void
_Host_Frame (double time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int         pass1, pass2, pass3;

	if (setjmp (host_abortserver))
		return;							// something bad happened, or the
	// server disconnected

// keep the random time dependent
	rand ();

// decide the simulation time
	if (!Host_FilterTime (time))
	{
		SDL_Delay (1);
		return;							// don't run too fast, or packets will
	// flood out
	}

// get new key events
	Sys_SendKeyEvents ();

// allow mice or other external controllers to add commands
	IN_Commands ();

// process console commands
	Cbuf_Execute ();

	NET_Poll ();

// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();

//-------------------
//
// server operations
//
//-------------------

// check for commands typed to the host
	Host_GetConsoleCommands ();

	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

// fetch results from server
	if (cls.state == ca_connected) {
		CL_ReadFromServer ();
	}
// update video
	if (host_speeds->ivalue)
		time1 = Sys_DoubleTime ();

	SCR_UpdateScreen ();

	if (host_speeds->ivalue)
		time2 = Sys_DoubleTime ();

// update audio
	if (cls.signon == SIGNONS) {
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	} else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	if (!isDedicated)
		CDAudio_Update ();

	if (host_speeds->ivalue) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Com_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	host_framecount++;
	fps_count++;
}

void
Host_Frame (double time)
{
	double			time1, time2;
	static double	timetotal;
	static int		timecount;
	int				m;
	Uint32			i, c;

	if (!serverprofile->ivalue) {
		_Host_Frame (time);
		return;
	}

	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal * 1000 / timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i = 0; i < svs.maxclients; i++) {
		if (svs.clients[i].active)
			c++;
	}

	Com_Printf ("serverprofile: %2i clients %2i msec\n", c, m);
}

//============================================================================

static void
Host_CvarUserinfo (cvar_t *var)
{
	if (var->flags & CVAR_SERVERINFO)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name,
					var->svalue);
	}
}

/*
====================
Host_Init
====================
*/
void
Host_Init ()
{
	Zone_Init ();
	Cvar_Init (&Host_CvarUserinfo);		// Cvar system
	Cbuf_Init ();						// Command buffer
	Cmd_Init (&Cmd_ForwardToServer);	// Command system
	Sys_Init ();						// System system =)
	Zone_Init_Commands ();

	// These have to be here.
	fs_shareconf = Cvar_Get ("fs_shareconf", SHARECONF, CVAR_ROM, ExpandPath);
	fs_userconf = Cvar_Get ("fs_userconf", USERCONF, CVAR_ROM, ExpandPath);

	// execute +set as early as possible
	// Yes, the repeated Cmd_StuffCmds_f/Cbuf_Execute_Sets are necessary!
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();
	Cbuf_InsertFile (fs_shareconf->svalue);
	Cbuf_Execute_Sets ();
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();
	Cbuf_InsertFile (fs_userconf->svalue);
	Cbuf_Execute_Sets ();
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();

	COM_Init_Cvars ();				// filesystem related variables
	Con_Init_Cvars ();				// console related cvars
	Key_Init_Cvars ();				// key related cvars
	Surf_Init_Cvars();				// model related cvars
	Chase_Init_Cvars ();			// chase camera related cvars
	SCR_Init_Cvars ();				// screen(?) related cvars
	VID_Init_Cvars();				// video related cvars
	V_Init_Cvars();					// view related cvars
	M_Init_Cvars ();				// menu related cvars
	R_Init_Cvars ();				// rendering system related cvars
	Sbar_Init_Cvars ();				// statusbar related cvars
	CL_Init_Cvars ();				// cl_* related cvars
	S_Init_Cvars ();				// sound system related cvars
	NET_Init_Cvars ();				// net related cvars
	Host_InitLocal_Cvars ();		// local host related cvars
	PR_Init_Cvars();				// pr_* related cvars
	SV_Init_Cvars ();				// setup related cvars

	Chase_Init ();					// setup chase camera
	COM_Init ();					// setup filesystem, add related commands

	Host_InitLocal ();				// initialize local host
	W_LoadWadFile ("gfx.wad");
	Key_Init ();					// setup keysyms
	Con_Init ();					// setup console, add related commands
	CPU_Init ();
	M_Init ();						// setup menu, add related commands
	PR_Init ();						// setup pr_* edicts, add related commands
	Mod_Init ();					// setup models, add related commands
	NET_Init ();					// setup networking
	SV_Init ();						// setup server

	V_Init ();						// setup view, add related commands

//	Com_Printf ("Exe: " __TIME__ " " __DATE__ "\n");

	if (cls.state != ca_dedicated) {
		host_basepal = COM_LoadNamedFile ("gfx/palette.lmp", true);
		if (!host_basepal)
			Sys_Error ("Couldn't load gfx/palette.lmp");
		host_colormap = COM_LoadNamedFile ("gfx/colormap.lmp", true);
		if (!host_colormap)
			Sys_Error ("Couldn't load gfx/colormap.lmp");

		VID_Init (host_basepal);
		Draw_Init_Cvars ();
		Draw_Init ();
		SCR_Init ();
		R_Init ();
		S_Init ();
		CDAudio_Init_Cvars ();
		CDAudio_Init ();
		Sbar_Init ();
		CL_Init ();
		IN_Init ();
	}

	Cbuf_InsertText ("exec quake.rc\n");

	host_initialized = true;

	Sys_Printf ("========Quake Initialized=========\n");
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void
Host_Shutdown (void)
{
	static qboolean isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Com_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration ("config");

	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();

	if (cls.state != ca_dedicated) {
		VID_Shutdown ();
	}
}

