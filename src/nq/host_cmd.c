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
#include "model.h"
#include "host.h"
#include "keys.h"
#include "net.h"
#include "renderer/screen.h"
#include "server.h"
#include "strlib.h"
#include "sys.h"
#include "world.h"
#include "fs/fs.h"
#include "fs/rw_ops.h"

int         current_skill;

static void
Host_Quit_f (void)
{
	CL_Disconnect ();
	Host_ShutdownServer (false);

	Sys_Quit (0);
}


static void
Host_Status_f (void)
{
	client_t	*client;
	int         seconds, minutes, hours = 0;
	Uint32		j;
	void		(*print) (const char *fmt, ...);

	if (cmd_source == src_command) {
		if (!sv.active) {
			Cmd_ForwardToServer ();
			return;
		}
		print = Com_Printf;
	} else
		print = SV_ClientPrintf;

	print ("host:    %s\n", hostname->svalue);
	print ("version: %s\n", VERSION);
	print ("map:     %s\n", sv.name);
	print ("players: %i active (%i max)\n\n", net_activeconnections,
		   svs.maxclients);
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client->active)
			continue;
		seconds = (int) (net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes) {
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		} else
			hours = 0;
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j + 1, client->name,
			   (int) client->edict->v.frags, hours, minutes, seconds);
		print ("   %s\n", client->netconnection->address);
	}
}


/*
==================
Sets client to godmode
==================
*/
static void
Host_God_f (void)
{
	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	sv_player->v.flags = (int) sv_player->v.flags ^ FL_GODMODE;
	if (!((int) sv_player->v.flags & FL_GODMODE))
		SV_ClientPrintf ("godmode OFF\n");
	else
		SV_ClientPrintf ("godmode ON\n");
}

static void
Host_Notarget_f (void)
{
	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	sv_player->v.flags = (int) sv_player->v.flags ^ FL_NOTARGET;
	if (!((int) sv_player->v.flags & FL_NOTARGET))
		SV_ClientPrintf ("notarget OFF\n");
	else
		SV_ClientPrintf ("notarget ON\n");
}

qboolean    noclip_anglehack;

static void
Host_Noclip_f (void)
{
	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv_player->v.movetype != MOVETYPE_NOCLIP) {
		noclip_anglehack = true;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf ("noclip ON\n");
	} else {
		noclip_anglehack = false;
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("noclip OFF\n");
	}
}

/*
==================
Sets client to flymode
==================
*/
static void
Host_Fly_f (void)
{
	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	if (sv_player->v.movetype != MOVETYPE_FLY) {
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf ("flymode ON\n");
	} else {
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf ("flymode OFF\n");
	}
}


static void
Host_Ping_f (void)
{
	Uint32		i, j;
	float		total;
	client_t	*client;

	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		if (!client->active)
			continue;
		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total += client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int) (total * 1000), client->name);
	}
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/


/*
======================
handle a 
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
static void
Host_Map_f (void)
{
	int         i;
	char        name[MAX_QPATH];

	if (cmd_source != src_command)
		return;
	if (Cmd_Argc () != 2) {
		Com_Printf ("map <levelname> : continue game on a new level\n");
		return;
	}

	ccls.demonum = -1;					// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer (false);

	key_dest = key_game;				// remove console or menu
	SCR_BeginLoadingPlaque ();

	cls.mapstring[0] = 0;
	for (i = 0; i < Cmd_Argc (); i++) {
		strlcat_s (cls.mapstring, Cmd_Argv (i));
		strlcat_s (cls.mapstring, " ");
	}
	strlcat_s (cls.mapstring, "\n");

	svs.serverflags = 0;				// haven't completed an episode yet
	strlcpy_s (name, Cmd_Argv (1));
	SV_SpawnServer (name);

	if (!sv.active)
		return;

	if (ccls.state != ca_dedicated) {
		cls.spawnparms[0] = '\0';

		for (i = 2; i < Cmd_Argc (); i++) {
			strlcat_s (cls.spawnparms, Cmd_Argv (i));
			strlcat_s (cls.spawnparms, " ");
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
==================
Goes to a new map, taking all clients along
==================
*/
static void
Host_Changelevel_f (void)
{
	char        level[MAX_QPATH];

	if (Cmd_Argc () != 2) {
		Com_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || ccls.demoplayback) {
		Com_Printf ("Only the server may changelevel\n");
		return;
	}
	SV_SaveSpawnparms ();
	strlcpy_s (level, Cmd_Argv (1));
	SV_SpawnServer (level);
}

/*
==================
Restarts the current server for a dead player
==================
*/
static void
Host_Restart_f (void)
{
	char        mapname[MAX_QPATH];

	if (ccls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;
	// Must copy out, because it gets cleared in sv_spawnserver.
	strlcpy_s (mapname, sv.name);
	SV_SpawnServer (mapname);
}

/*
==================
This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
static void
Host_Reconnect_f (void)
{
	SCR_BeginLoadingPlaque ();
	cls.signon = 0;						// need new connection messages
}

/*
=====================
User command to connect to server
=====================
*/
static void
Host_Connect_f (void)
{
	char        name[MAX_QPATH];

	ccls.demonum = -1;					// stop demo loop in case this fails
	if (ccls.demoplayback) {
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	strlcpy_s (name, Cmd_Argv (1));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current 
===============
*/
static void
Host_SavegameComment (char *text)
{
	int         i;
	char        kills[20];

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		text[i] = ' ';
	memcpy (text, ccl.levelname, strlen (ccl.levelname));
	snprintf (kills, sizeof (kills), "kills:%3i/%3i", ccl.stats[STAT_MONSTERS],
			  ccl.stats[STAT_TOTALMONSTERS]);
	memcpy (text + 22, kills, strlen (kills));
// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		if (text[i] == ' ')
			text[i] = '_';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}


static void
Host_Savegame_f (void)
{
	fs_file_t	*file = NULL;
	SDL_RWops	*rw = NULL;
	Uint		i;
	char		name[256], comment[SAVEGAME_COMMENT_LENGTH + 1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Com_Printf ("Not playing a local game.\n");
		return;
	}

	if (ccl.intermission)
	{
		Com_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Com_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr (Cmd_Argv (1), ".."))
	{
		Com_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i = 0; i < svs.maxclients; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0))
		{
			Com_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	strlcpy_s (name, Cmd_Argv (1));
	COM_DefaultExtension (name, ".sav", sizeof (name));

	if ((file = FS_FindFile (name)))
		rw = file->open(file, FSF_WRITE);

	if (!rw)
		rw = FS_Open_New (name, FSF_WRITE);

	if (!rw)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	RWprintf (rw, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	RWprintf (rw, "%s\n", comment);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		RWprintf (rw, "%f\n", svs.clients->spawn_parms[i]);
	RWprintf (rw, "%d\n", current_skill);
	RWprintf (rw, "%s\n", sv.name);
	RWprintf (rw, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			RWprintf (rw, "%s\n", sv.lightstyles[i]);
		else
			RWprintf (rw, "m\n");
	}

	ED_WriteGlobals (rw);
	for (i = 0; i < sv.num_edicts; i++)
	{
		ED_Write (rw, EDICT_NUM (i));
	}
	SDL_RWclose (rw);
	Com_Printf ("done.\n");
}


static void
Host_Loadgame_f (void)
{
	char		name[MAX_OSPATH];
	char		*mapname;
	const char	*start;
	char		*saved, *cur, *tmp, c;
	float       time, tfloat, spawn_parms[NUM_SPAWN_PARMS];
	Sint		entnum, version;
	Uint		i;
	edict_t		*ent;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("load <savename> : load a game\n");
		return;
	}

	ccls.demonum = -1;					// stop demo loop in case this fails

	strlcpy (name, Cmd_Argv (1), sizeof (name));
	COM_DefaultExtension (name, ".sav", sizeof (name));

	// we can't call SCR_BeginLoadingPlaque, because too much stack space has
	// been used.  The menu calls it before stuffing loadgame command
	// SCR_BeginLoadingPlaque ();

	Com_Printf ("Loading game from %s...\n", name);
	cur = saved = (char *) COM_LoadTempFile (name, true);
	if (!saved) {
		Com_Printf ("ERROR: couldn't load.\n");
		return;
	}

	tmp = strchr (cur, '\n'); *tmp = '\0';
	sscanf (cur, "%d", &version);
	cur = tmp + 1;
	if (version != SAVEGAME_VERSION)
	{
		Zone_Free (saved);
		Com_Printf ("Savegame is version %i, not %i\n", version,
				SAVEGAME_VERSION);
		return;
	}
	for (i = 0; i < NUM_SPAWN_PARMS; i++) {
		tmp = strchr (cur, '\n'); *tmp = '\0';
		sscanf (cur, "%f", &spawn_parms[i]);
		cur = tmp + 1;
	}
	// Quake 1.06 used float skill...
	tmp = strchr (cur, '\n'); *tmp = '\0';
	sscanf (cur, "%f", &tfloat);
	cur = tmp + 1;
	current_skill = (int) (tfloat + 0.1);
	Cvar_Set (skill, va("%i", current_skill));

	tmp = strchr (cur, '\n'); *tmp = '\0';
	mapname = cur;
	cur = tmp + 1;

	tmp = strchr (cur, '\n'); *tmp = '\0';
	sscanf (cur, "%f", &time);
	cur = tmp + 1;

	CL_Disconnect_f ();

	SV_SpawnServer (mapname);

	if (!sv.active)
	{
		Com_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;					// pause until all clients connect
	sv.loadgame = true;

	// load the light styles

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		tmp = strchr (cur, '\n'); *tmp = '\0';
		sv.lightstyles[i] = Zone_Alloc (sv_zone, strlen (cur) + 1);
		strlcpy_s (sv.lightstyles[i], cur);
		cur = tmp + 1;
	}

	// load the edicts out of the savegame file
	entnum = -1;						// -1 is the globals
	while ((cur - saved) < com_filesize)
	{
		tmp = strchr (cur, '}') + 1; c = *tmp; *tmp = '\0';
		start = COM_Parse (cur);
		*tmp = c; cur = tmp;
		if (!com_token[0])
			// end of file
			break;
		if (strcmp (com_token, "{"))
			Sys_Error ("First token isn't a brace");

		if (entnum < 0)
		{
			// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{
			// parse an edict
			ent = EDICT_NUM ((Uint)entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	Zone_Free (saved);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (ccls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}

//============================================================================

static void
Host_Name_f (void)
{
	char newName[16];

	if (Cmd_Argc () == 1) {
		Com_Printf ("\"name\" is \"%s\"\n", _cl_name->svalue);
		return;
	}
	if (Cmd_Argc () == 2)
		strlcpy_s(newName, Cmd_Argv (1));
	else
		strlcpy_s(newName, Cmd_Args ());

	if (cmd_source == src_command) {
		if (strcmp (_cl_name->svalue, newName) == 0)
			return;
		Cvar_Set (_cl_name, newName);
		if (ccls.state >= ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp (host_client->name, "unconnected"))
		if (strcmp (host_client->name, newName) != 0)
			Com_Printf ("%s renamed to %s\n", host_client->name, newName);
	strlcpy_s (host_client->name, newName);
	host_client->edict->v.netname = host_client->name - pr_strings;

// send notification to all clients

	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

static void
Host_Version_f (void)
{
	Com_Printf ("Version %s\n", VERSION);
	Com_Printf ("Build: %04d\n", build_number());
	Com_Printf ("Exe: " __TIME__ " " __DATE__ "\n");
}

static void
Host_Say (qboolean teamonly)
{
	client_t	*client;
	client_t	*save;
	Uint32		j;
	char		*p, *p_r;
	char		text[64];
	qboolean    fromServer = false;

	if (cmd_source == src_command) {
		if (ccls.state == ca_dedicated) {
			fromServer = true;
			teamonly = false;
		} else {
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = p_r = Zstrdup(tempzone, Cmd_Args ());
// remove quotes if present
	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}
// turn on color set 1
	if (!fromServer)
		snprintf (text, sizeof (text), "%c%s: ", 1, save->name);
	else
		snprintf (text, sizeof (text), "%c<%s> ", 1, hostname->svalue);

	j = sizeof (text) - 2 - strlen (text);	// -2 for /n and null
	// terminator
	if (strlen (p) > j)
		p[j] = 0;

	strlcat_s (text, p);
	strlcat_s (text, "\n");

	Z_Free(p_r);

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay->ivalue && teamonly
			&& client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
	}
	host_client = save;

	Sys_Printf ("%s", &text[1]);
}


static void
Host_Say_f (void)
{
	Host_Say (false);
}


static void
Host_Say_Team_f (void)
{
	Host_Say (true);
}


static void
Host_Tell_f (void)
{
	client_t	*client, *save;
	Uint32		j;
	char		*p, text[64];

	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	strlcpy_s (text, host_client->name);
	strlcat_s (text, ": ");

	p = Zstrdup(tempzone, Cmd_Args ());

	// remove quotes if present
	if (*p == '"') {
		p++;
		p[strlen (p) - 1] = 0;
	}
	// check length & truncate if necessary
	j = sizeof (text) - 2 - strlen (text);	// -2 for /n and null
	// terminator
	if (strlen (p) > j)
		p[j] = 0;

	strlcat_s (text, p);
	strlcat_s (text, "\n");

	Z_Free (p);

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++) {
		if (!client->active || !client->spawned)
			continue;
		if (strcasecmp (client->name, Cmd_Argv (1)))
			continue;
		host_client = client;
		SV_ClientPrintf ("%s", text);
		break;
	}
	host_client = save;
}


static void
Host_Color_f (void)
{
	int         top, bottom;
	int         playercolor;

	if (Cmd_Argc () == 1) {
		Com_Printf ("\"color\" is \"%i %i\"\n",
				_cl_color->ivalue >> 4,
				_cl_color->ivalue & 0x0f);
		Com_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		top = bottom = Q_atoi (Cmd_Argv (1));
	else {
		top = Q_atoi (Cmd_Argv (1));
		bottom = Q_atoi (Cmd_Argv (2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top * 16 + bottom;

	if (cmd_source == src_command) {
		Cvar_Set (_cl_color, va("%i", playercolor));
		if (ccls.state >= ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

static void
Host_Kill_f (void)
{
	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}

	if (sv_player->v.health <= 0) {
		SV_ClientPrintf ("Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG (sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill, "QC function ClientKill is missing.");
}

static void
Host_WriteConfig_f (void)
{
	if (Cmd_Argc () != 2) {
		Com_Printf ("writeconfig <filename> : dump configuration to file\n");
		return;
	}

	Host_WriteConfiguration (Cmd_Argv(1));
}

static void
Host_Pause_f (void)
{

	if (cmd_source == src_command) {
		Cmd_ForwardToServer ();
		return;
	}
	if (!pausable->ivalue)
		SV_ClientPrintf ("Pause not allowed.\n");
	else {
		sv.paused ^= 1;

		if (sv.paused) {
			SV_BroadcastPrintf ("%s paused the game\n",
								pr_strings + sv_player->v.netname);
		} else {
			SV_BroadcastPrintf ("%s unpaused the game\n",
								pr_strings + sv_player->v.netname);
		}

		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================


static void
Host_PreSpawn_f (void)
{
	if (cmd_source == src_command) {
		Com_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned) {
		Com_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;
}

static void
Host_Spawn_f (void)
{
	Uint32		i;
	client_t	*client;
	edict_t		*ent;

	if (cmd_source == src_command) {
		Com_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned) {
		Com_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

	// LordHavoc: moved this above the QC calls at FrikaC's request
	// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

	// run the entrance script
	if (sv.loadgame) {	// loaded games are fully inited
		// already
		// if this is the last client to be connected, unpause
		sv.paused = false;
	} else {
		// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT (ent, __FILE__, __LINE__);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = host_client->name - pr_strings;

		// copy spawn parms out of the client_t

		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect, "QC function ClientConnect is missing.");

		if ((Sys_DoubleTime () - host_client->netconnection->connecttime) <=
			sv.time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer, "QC function PutClientInServer is missing.");
	}

	// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++) {
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char) i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

	// send some stats
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);


	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	ent = EDICT_NUM (1 + (Uint)(host_client - svs.clients));
	MSG_WriteByte (&host_client->message, svc_setangle);
	for (i = 0; i < 2; i++)
		MSG_WriteAngle (&host_client->message, ent->v.angles[i]);

	MSG_WriteAngle (&host_client->message, 0);

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

static void
Host_Begin_f (void)
{
	if (cmd_source == src_command) {
		Com_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================


/*
==================
Kicks a user off of the server
==================
*/
static void
Host_Kick_f (void)
{
	char       *who, *args;
	const char	*message = NULL;
	client_t   *save;
	Uint32		i;
	qboolean    byNumber = false;

	if (cmd_source == src_command) {
		if (!sv.active) {
			Cmd_ForwardToServer ();
			return;
		}
	} else if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	save = host_client;

	if (Cmd_Argc () > 2 && strcmp (Cmd_Argv (1), "#") == 0) {
		i = Q_atof (Cmd_Argv (2)) - 1;
		if (i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	} else {
		for (i = 0, host_client = svs.clients; i < svs.maxclients;
			 i++, host_client++) {
			if (!host_client->active)
				continue;
			if (strcasecmp (host_client->name, Cmd_Argv (1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients) {
		if (cmd_source == src_command)
			if (ccls.state == ca_dedicated)
				who = "Console";
			else
				who = _cl_name->svalue;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc () > 2) {
			args = Zstrdup(tempzone, Cmd_Args ());
			message = COM_Parse (args);
			Z_Free (args);
			if (byNumber) {
				message++;				// skip the #
				while (*message == ' ')	// skip white space
					message++;
				message += strlen (Cmd_Argv (2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

static void
Host_Give_f (void)
{
	const char	*t;
	int			v;
	eval_t		*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch && !host_client->privileged)
		return;

	t = Cmd_Argv (1);
	v = Q_atoi (Cmd_Argv (2));

	switch (t[0])
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			// MED 01/04/97 added hipnotic give stuff
			if (game_hipnotic->ivalue)
			{
				switch (t[0])
				{
					case '0':
						sv_player->v.items =
							(int) sv_player->v.items | HIT_MJOLNIR;
						break;

					case '6':
						if (t[1] == 'a')
							sv_player->v.items =
								(int) sv_player->v.items | HIT_PROXIMITY_GUN;
						else
							sv_player->v.items =
								(int) sv_player->v.items | IT_GRENADE_LAUNCHER;
						break;

					case '9':
						sv_player->v.items =
							(int) sv_player->v.items | HIT_LASER_CANNON;
						break;

					default:
						if (t[0] >= '2')
							sv_player->v.items =
								(int) sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
						break;
				}
			}
			else
			{
				if (t[0] >= '2')
					sv_player->v.items =
						(int) sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
			}
			break;

		case 's':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_shells1);
				if (val)
					val->_float = v;
			}

			sv_player->v.ammo_shells = v;
			break;

		case 'n':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_nails1);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon <= IT_LIGHTNING)
						sv_player->v.ammo_nails = v;
				}
			}
			else
				sv_player->v.ammo_nails = v;
			break;

		case 'l':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_lava_nails);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon > IT_LIGHTNING)
						sv_player->v.ammo_nails = v;
				}
			}
			break;

		case 'r':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_rockets1);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon <= IT_LIGHTNING)
						sv_player->v.ammo_rockets = v;
				}
			}
			else
				sv_player->v.ammo_rockets = v;
			break;

		case 'm':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_multi_rockets);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon > IT_LIGHTNING)
						sv_player->v.ammo_rockets = v;
				}
			}
			break;

		case 'h':
			sv_player->v.health = v;
			break;

		case 'c':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_cells1);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon <= IT_LIGHTNING)
						sv_player->v.ammo_cells = v;
				}
			}
			else
				sv_player->v.ammo_cells = v;
			break;

		case 'p':
			if (game_rogue->ivalue)
			{
				val = GETEDICTFIELDVALUE (sv_player, eval_ammo_plasma);
				if (val)
				{
					val->_float = v;
					if (sv_player->v.weapon > IT_LIGHTNING)
						sv_player->v.ammo_cells = v;
				}
			}
			break;
	}
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


static void
Host_Startdemos_f (void)
{
	int         i, c;

	if (ccls.state == ca_dedicated) {
		if (!sv.active)
			Cbuf_AddText ("map start\n");
		return;
	}

	c = Cmd_Argc () - 1;
	if (c > MAX_DEMOS) {
		Com_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Com_Printf ("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		strlcpy_s (ccls.demos[i - 1], Cmd_Argv (i));

	if (!sv.active && ccls.demonum != -1 && !ccls.demoplayback) {
		ccls.demonum = 0;
		CL_NextDemo ();
	} else
		ccls.demonum = -1;
}


/*
==================
Return to looping demos
==================
*/
static void
Host_Demos_f (void)
{
	if (ccls.state == ca_dedicated)
		return;
	if (ccls.demonum == -1)
		ccls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Return to looping demos
==================
*/
static void
Host_Stopdemo_f (void)
{
	if (ccls.state == ca_dedicated)
		return;
	if (!ccls.demoplayback)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}

//=============================================================================

void
Host_InitCommands (void)
{
	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand ("god", Host_God_f);
	Cmd_AddCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("name", Host_Name_f);
	Cmd_AddCommand ("noclip", Host_Noclip_f);
	Cmd_AddCommand ("version", Host_Version_f);
	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("color", Host_Color_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand ("give", Host_Give_f);
	Cmd_AddCommand ("writeconfig", Host_WriteConfig_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);
}
