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
#include "progs.h"
#include "server.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "fs.h"

static qboolean    sv_allow_cheats;

int         fp_messages = 4, fp_persecond = 4, fp_secondsdead = 10;
char        fp_msg[255] = { 0 };


/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
Make a master server current
====================
*/
static void
SV_SetMaster_f (void)
{
	char        data[2];
	int         i;

	memset (&master_adr, 0, sizeof (master_adr));

	for (i = 1; i < Cmd_Argc (); i++) {
		if (!strcmp (Cmd_Argv (i), "none")
			|| !NET_StringToAdr (Cmd_Argv (i), &master_adr[i - 1])) {
			Com_Printf ("Setting nomaster mode.\n");
			return;
		}
		if (master_adr[i - 1].port == 0)
			master_adr[i - 1].port = BigShort (27000);

		Com_Printf ("Master server at %s\n",
					NET_AdrToString (master_adr[i - 1]));

		Com_Printf ("Sending a ping.\n");

		data[0] = A2A_PING;
		data[1] = 0;
		NET_SendPacket (NS_SERVER, 2, data, master_adr[i - 1]);
	}

	svs.last_heartbeat = -99999;
}


static void
SV_Quit_f (void)
{
	SV_FinalMessage ("server shutdown\n");
	Com_Printf ("Shutting down.\n");
	SV_Shutdown ();
	Sys_Quit (0);
}

static void
SV_Logfile_f (void)
{
	if (sys_logname->svalue)
	{
		Com_Printf ("File logging off.\n");
		Cvar_Set (sys_logname, "");
		return;
	}

	Com_Printf ("Logging text to %s/qconsole.log.\n", com_gamedir);
	Cvar_Set (sys_logname, "qconsole");
}


static void
SV_Fraglogfile_f (void)
{
	char        name[MAX_OSPATH];
	int         i;

	if (sv_fraglogfile) {
		Com_Printf ("Frag file logging off.\n");
		SDL_RWclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
		return;
	}
	// find an unused name
	for (i = 0; i < 1000; i++) {
		snprintf (name, sizeof (name), "frag_%i.log", i);
		if (!FS_FindFile(name)) {			// can't read it, so create this one
			sv_fraglogfile = FS_Open_New (name, FSF_ASCII);
			if (!sv_fraglogfile)
				i = 1000;				// give error
			break;
		}
	}
	if (i == 1000) {
		Com_Printf ("Can't open any logfiles.\n");
		sv_fraglogfile = NULL;
		return;
	}

	Com_Printf ("Logging frags to %s.\n", name);
}


/*
==================
Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
static qboolean
SV_SetPlayer (void)
{
	client_t   *cl;
	int         i;
	int         idnum;

	idnum = Q_atoi (Cmd_Argv (1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == idnum) {
			host_client = cl;
			sv_player = host_client->edict;
			return true;
		}
	}
	Com_Printf ("Userid %i is not on the server\n", idnum);
	return false;
}


/*
==================
Sets client to godmode
==================
*/
static void
SV_God_f (void)
{
	if (!sv_allow_cheats) {
		Com_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	sv_player->v.flags = (int) sv_player->v.flags ^ FL_GODMODE;
	if (!((int) sv_player->v.flags & FL_GODMODE))
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode OFF\n");
	else
		SV_ClientPrintf (host_client, PRINT_HIGH, "godmode ON\n");
}


static void
SV_Noclip_f (void)
{
	if (!sv_allow_cheats) {
		Com_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (sv_player->v.movetype != MOVETYPE_NOCLIP) {
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip ON\n");
	} else {
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf (host_client, PRINT_HIGH, "noclip OFF\n");
	}
}


static void
SV_Give_f (void)
{
	const char	*t;
	int			v;

	if (!sv_allow_cheats) {
		Com_Printf
			("You must run the server with -cheats to enable this command.\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	t = Cmd_Argv (2);
	v = Q_atoi (Cmd_Argv (3));

	switch (t[0]) {
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			sv_player->v.items =
				(int) sv_player->v.items | IT_SHOTGUN << (t[0] - '2');
			break;

		case 's':
			sv_player->v.ammo_shells = v;
			break;
		case 'n':
			sv_player->v.ammo_nails = v;
			break;
		case 'r':
			sv_player->v.ammo_rockets = v;
			break;
		case 'h':
			sv_player->v.health = v;
			break;
		case 'c':
			sv_player->v.ammo_cells = v;
			break;
	}
}


/*
======================
handle a 
map <mapname>
command from the console or progs.
======================
*/
static void
SV_Map_f (void)
{
	char        level[MAX_QPATH];
	char        expanded[MAX_QPATH];

	if (Cmd_Argc () != 2) {
		Com_Printf ("map <levelname> : continue game on a new level\n");
		return;
	}
	strlcpy (level, Cmd_Argv (1), sizeof (level));

	// check to make sure the level exists
	snprintf (expanded, sizeof (expanded), "maps/%s.bsp", level);
	if (!FS_FindFile (expanded)) {
		Com_Printf ("Can't find %s\n", expanded);
		return;
	}

	SV_BroadcastCommand ("changing\n");
	SV_SendMessagesToAll ();

	SV_SpawnServer (level);

	SV_BroadcastCommand ("reconnect\n");
}


/*
==================
Kick a user off of the server
==================
*/
static void
SV_Kick_f (void)
{
	int         i;
	client_t   *cl;
	int         uid;

	uid = Q_atoi (Cmd_Argv (1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == uid) {
			SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", cl->name);
			// print directly, because the dropped client won't get the
			// SV_BroadcastPrintf message
			SV_ClientPrintf (cl, PRINT_HIGH, "You were kicked from the game\n");
			SV_DropClient (cl);
			return;
		}
	}

	Com_Printf ("Couldn't find user number %i\n", uid);
}


static void
SV_Status_f (void)
{
	int         i, j, l;
	client_t   *cl;
	float       cpu, avg, pak;
	const char	*s;

	cpu = (svs.stats.latched_active + svs.stats.latched_idle);
	if (cpu)
		cpu = 100 * svs.stats.latched_active / cpu;
	avg = 1000 * svs.stats.latched_active / STATFRAMES;
	pak = (float) svs.stats.latched_packets / STATFRAMES;

	Com_Printf ("net address      : %s\n", NET_AdrToString (net_local_adr));
	Com_Printf ("cpu utilization  : %3i%%\n", (int) cpu);
	Com_Printf ("avg response time: %i ms\n", (int) avg);
	Com_Printf ("packets/frame    : %5.2f\n", pak);

// min fps lat drp
	if (sv_redirected != RD_NONE) {
		// most remote clients are 40 columns
		// 0123456789012345678901234567890123456789
		Com_Printf ("name               userid frags\n");
		Com_Printf ("  address          rate ping drop\n");
		Com_Printf ("  ---------------- ---- ---- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;

			Com_Printf ("%-16.16s  ", cl->name);

			Com_Printf ("%6i %5i", cl->userid, (int) cl->edict->v.frags);
			if (cl->spectator)
				Com_Printf (" (s)\n");
			else
				Com_Printf ("\n");

			s = NET_BaseAdrToString (cl->netchan.remote_address);
			Com_Printf ("  %-16.16s", s);
			if (cl->state == cs_connected) {
				Com_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				Com_Printf ("ZOMBIE\n");
				continue;
			}
			Com_Printf ("%4i %4i %5.2f\n", (int) (1000 * cl->netchan.frame_rate)
						, (int) SV_CalcPing (cl)
						,
						100.0 * cl->netchan.drop_count /
						cl->netchan.incoming_sequence);
		}
	} else {
		Com_Printf
			("frags userid address         name            rate ping drop  qport\n");
		Com_Printf
			("----- ------ --------------- --------------- ---- ---- ----- -----\n");
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (!cl->state)
				continue;
			Com_Printf ("%5i %6i ", (int) cl->edict->v.frags, cl->userid);

			s = NET_BaseAdrToString (cl->netchan.remote_address);
			Com_Printf ("%s", s);
			l = 16 - strlen (s);
			for (j = 0; j < l; j++)
				Com_Printf (" ");

			Com_Printf ("%s", cl->name);
			l = 16 - strlen (cl->name);
			for (j = 0; j < l; j++)
				Com_Printf (" ");
			if (cl->state == cs_connected) {
				Com_Printf ("CONNECTING\n");
				continue;
			}
			if (cl->state == cs_zombie) {
				Com_Printf ("ZOMBIE\n");
				continue;
			}
			Com_Printf ("%4i %4i %3.1f %4i",
						(int) (1000 * cl->netchan.frame_rate)
						, (int) SV_CalcPing (cl)
						,
						100.0 * cl->netchan.drop_count /
						cl->netchan.incoming_sequence, cl->netchan.qport);
			if (cl->spectator)
				Com_Printf (" (s)\n");
			else
				Com_Printf ("\n");


		}
	}
	Com_Printf ("\n");
}

static void
SV_ConSay_f (void)
{
	client_t   *client;
	int         j;
	const char	*p;
	char        text[1024];

	if (Cmd_Argc () < 2)
		return;

	strlcpy_s (text, "console: ");
	p = Cmd_Args ();

	if (*p == '"') {
		p++;
		strlcat_s (text, p);
		text[strlen (text) - 1] = 0;
	} else
		strlcat_s (text, p);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf (client, PRINT_CHAT, "%s\n", text);
	}
}


static void
SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999;
}

void
SV_SendServerInfoChange (const char *key, const char *value)
{
	if (!sv.state)
		return;

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

/*
===========
  Examine or change the serverinfo string
===========
*/
static void
SV_Serverinfo_f (void)
{
	cvar_t     *var;

	if (Cmd_Argc () == 1) {
		Com_Printf ("Server info settings:\n");
		Info_Print (svs.info);
		return;
	}

	if (Cmd_Argc () != 3) {
		Com_Printf ("usage: serverinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*') {
		Com_Printf ("Star variables cannot be changed.\n");
		return;
	}
	Info_SetValueForKey (svs.info, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_SERVERINFO_STRING);

	// if this is a cvar, change it too 
	var = Cvar_Find (Cmd_Argv (1));
	if (var)
		Cvar_Set (var, Cmd_Argv (2));

	SV_SendServerInfoChange (Cmd_Argv (1), Cmd_Argv (2));
}


/*
===========
  Examine or change the serverinfo string
===========
*/
static void
SV_Localinfo_f (void)
{
	if (Cmd_Argc () == 1) {
		Com_Printf ("Local info settings:\n");
		Info_Print (localinfo);
		return;
	}

	if (Cmd_Argc () != 3) {
		Com_Printf ("usage: localinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*') {
		Com_Printf ("Star variables cannot be changed.\n");
		return;
	}
	Info_SetValueForKey (localinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_LOCALINFO_STRING);
}


/*
===========
Examine a users info strings
===========
*/
static void
SV_User_f (void)
{
	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: user <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Info_Print (host_client->userinfo);
}

/*
================
Sets the fake *gamedir to a different directory.
================
*/
static void
SV_Gamedir (void)
{
	const char *dir;

	if (Cmd_Argc () == 1) {
		Com_Printf ("Current *gamedir: %s\n",
					Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: sv_gamedir <newgamedir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":")) {
		Com_Printf ("*Gamedir should be a single filename, not a path\n");
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
Sets the gamedir and path to a different directory.
================
*/

static void
SV_Floodprot_f (void)
{
	int         arg1, arg2, arg3;

	if (Cmd_Argc () == 1) {
		if (fp_messages) {
			Com_Printf
				("Current floodprot settings: \nAfter %d msgs per %d seconds, silence for %d seconds\n",
				 fp_messages, fp_persecond, fp_secondsdead);
			return;
		} else
			Com_Printf ("No floodprots enabled.\n");
	}

	if (Cmd_Argc () != 4) {
		Com_Printf
			("Usage: floodprot <# of messages> <per # of seconds> <seconds to silence>\n");
		Com_Printf
			("Use floodprotmsg to set a custom message to say to the flooder.\n");
		return;
	}

	arg1 = Q_atoi (Cmd_Argv (1));
	arg2 = Q_atoi (Cmd_Argv (2));
	arg3 = Q_atoi (Cmd_Argv (3));

	if (arg1 <= 0 || arg2 <= 0 || arg3 <= 0) {
		Com_Printf ("All values must be positive numbers\n");
		return;
	}

	if (arg1 > 10) {
		Com_Printf ("Can only track up to 10 messages.\n");
		return;
	}

	fp_messages = arg1;
	fp_persecond = arg2;
	fp_secondsdead = arg3;
}

static void
SV_Floodprotmsg_f (void)
{
	if (Cmd_Argc () == 1) {
		Com_Printf ("Current msg: %s\n", fp_msg);
		return;
	} else if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: floodprotmsg \"<message>\"\n");
		return;
	}
	snprintf (fp_msg, sizeof (fp_msg), "%s", Cmd_Argv (1));
}

/*
================
Sets the gamedir and path to a different directory.
================
*/
static void
SV_Gamedir_f (void)
{
	const char *dir;

	if (Cmd_Argc () == 1) {
		Com_Printf ("Current gamedir: %s\n", com_gamedir);
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: gamedir <newdir>\n");
		return;
	}

	dir = Cmd_Argv (1);

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":")) {
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	COM_Gamedir (dir);
	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

static void
SV_Snap (int uid)
{
	client_t   *cl;
	char        pcxname[80];
	char        checkname[MAX_OSPATH];
	int         i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS) {
		Com_Printf ("userid not found\n");
		return;
	}

	snprintf (pcxname, sizeof (pcxname), "%d-00.pcx", uid);

	snprintf (checkname, sizeof (checkname), "%s/snap", gamedirfile);
	Sys_mkdir (gamedirfile);
	Sys_mkdir (checkname);

	for (i = 0; i <= 99; i++) {
		pcxname[strlen (pcxname) - 6] = i / 10 + '0';
		pcxname[strlen (pcxname) - 5] = i % 10 + '0';
		snprintf (checkname, sizeof (checkname), "%s/snap/%s", gamedirfile,
				  pcxname);
		if (Sys_FileTime (checkname) == -1)
			break;						// file doesn't exist
	}
	if (i == 100) {
		Com_Printf ("Snap: Couldn't create a file, clean some out.\n");
		return;
	}
	strlcpy (cl->uploadfn, checkname, sizeof (cl->uploadfn));

	memcpy (&cl->snap_from, &net_from, sizeof (net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	ClientReliableWrite_Begin (cl, svc_stufftext, 24);
	ClientReliableWrite_String (cl, "cmd snap\n");
	Com_Printf ("Requesting snap from user %d...\n", uid);
}

static void
SV_Snap_f (void)
{
	int         uid;

	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: snap <userid>\n");
		return;
	}

	uid = Q_atoi (Cmd_Argv (1));

	SV_Snap (uid);
}

static void
SV_SnapAll_f (void)
{
	client_t   *cl;
	int         i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_connected || cl->spectator)
			continue;
		SV_Snap (cl->userid);
	}
}

void
SV_InitOperatorCommands (void)
{
	if (COM_CheckParm ("-cheats")) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON",
								 MAX_SERVERINFO_STRING);
	}

	Cmd_AddCommand ("logfile", SV_Logfile_f);
	Cmd_AddCommand ("fraglogfile", SV_Fraglogfile_f);

	Cmd_AddCommand ("snap", SV_Snap_f);
	Cmd_AddCommand ("snapall", SV_SnapAll_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("status", SV_Status_f);

	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	Cmd_AddCommand ("say", SV_ConSay_f);
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("quit", SV_Quit_f);
	Cmd_AddCommand ("god", SV_God_f);
	Cmd_AddCommand ("give", SV_Give_f);
	Cmd_AddCommand ("noclip", SV_Noclip_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("localinfo", SV_Localinfo_f);
	Cmd_AddCommand ("user", SV_User_f);
	Cmd_AddCommand ("gamedir", SV_Gamedir_f);
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir);
	Cmd_AddCommand ("floodprot", SV_Floodprot_f);
	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f);
}

