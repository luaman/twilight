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
#include <stdarg.h>

#include "quakedef.h"
#include "cmd.h"
#include "common.h"
#include "cvar.h"
#include "mathlib.h"
#include "model.h"
#include "pmove.h"
#include "progs.h"
#include "server.h"
#include "strlib.h"
#include "sys.h"
#include "world.h"
#include "crc.h"
#include "fs/fs.h"
#include "host.h"

edict_t			*sv_player;

usercmd_t		cmd;

cvar_t			*cl_rollspeed;
cvar_t			*cl_rollangle;
static cvar_t			*sv_spectalk;

static cvar_t			*sv_timekick;
static cvar_t			*sv_timekick_allowed;
static cvar_t			*sv_timekick_interval;

static cvar_t			*sv_mapcheck;

/*
============================================================

USER STRINGCMD EXECUTION

host_client and sv_player will be valid.
============================================================
*/

/*
================
Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
static void
SV_New_f (void)
{
	char       *gamedir;
	int         playernum;

	if (host_client->state == cs_spawned)
		return;

	host_client->state = cs_connected;
	host_client->connection_started = svs.realtime;

	// send the info about the new client to all connected clients
//  SV_FullClientUpdate (host_client, &sv.reliable_datagram);
//  host_client->sendinfo = true;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_New] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}
	// send the serverdata
	MSG_WriteByte (&host_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&host_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&host_client->netchan.message, svs.spawncount);
	MSG_WriteString (&host_client->netchan.message, gamedir);

	playernum = NUM_FOR_EDICT (host_client->edict) - 1;
	if (host_client->spectator)
		playernum |= 128;
	MSG_WriteByte (&host_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&host_client->netchan.message,
					 PR_GetString (sv.edicts->v.message));

	// send the movevars
	MSG_WriteFloat (&host_client->netchan.message, movevars.gravity);
	MSG_WriteFloat (&host_client->netchan.message, movevars.stopspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.maxspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.spectatormaxspeed);
	MSG_WriteFloat (&host_client->netchan.message, movevars.accelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.airaccelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.wateraccelerate);
	MSG_WriteFloat (&host_client->netchan.message, movevars.friction);
	MSG_WriteFloat (&host_client->netchan.message, movevars.waterfriction);
	MSG_WriteFloat (&host_client->netchan.message, movevars.entgravity);

	// send music
	MSG_WriteByte (&host_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&host_client->netchan.message, sv.edicts->v.sounds);

	// send server info string
	MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
	MSG_WriteString (&host_client->netchan.message,
					 va ("fullserverinfo \"%s\"\n", svs.info));
}

static void
SV_Soundlist_f (void)
{
	char      **s;
	int         n;

	if (host_client->state != cs_connected) {
		Com_Printf ("soundlist not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (Q_atoi (Cmd_Argv (1)) != svs.spawncount) {
		Com_Printf ("SV_Soundlist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = Q_atoi (Cmd_Argv (2));
	if (n >= MAX_SOUNDS) {
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "SV_Soundlist_f: Invalid soundlist index\n");
		SV_DropClient (host_client);
		return;
	}
//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_Soundlist] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	MSG_WriteByte (&host_client->netchan.message, svc_soundlist);
	MSG_WriteByte (&host_client->netchan.message, n);
	for (s = sv.sound_precache + 1 + n;
		 *s && host_client->netchan.message.cursize < (MAX_MSGLEN / 2);
		 s++, n++)
		MSG_WriteString (&host_client->netchan.message, *s);

	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	if (*s)
		MSG_WriteByte (&host_client->netchan.message, n);
	else
		MSG_WriteByte (&host_client->netchan.message, 0);
}

static void
SV_Modellist_f (void)
{
	char      **s;
	int         n;

	if (host_client->state != cs_connected) {
		Com_Printf ("modellist not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (Q_atoi (Cmd_Argv (1)) != svs.spawncount) {
		Com_Printf ("SV_Modellist_f from different level\n");
		SV_New_f ();
		return;
	}

	n = Q_atoi (Cmd_Argv (2));
	if (n >= MAX_MODELS) {
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "SV_Modellist_f: Invalid modellist index\n");
		SV_DropClient (host_client);
		return;
	}
//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_Modellist] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	MSG_WriteByte (&host_client->netchan.message, svc_modellist);
	MSG_WriteByte (&host_client->netchan.message, n);
	for (s = sv.model_precache + 1 + n;
		 *s && host_client->netchan.message.cursize < (MAX_MSGLEN / 2);
		 s++, n++)
		MSG_WriteString (&host_client->netchan.message, *s);
	MSG_WriteByte (&host_client->netchan.message, 0);

	// next msg
	if (*s)
		MSG_WriteByte (&host_client->netchan.message, n);
	else
		MSG_WriteByte (&host_client->netchan.message, 0);
}

static void
SV_PreSpawn_f (void)
{
	Sint32	buf;
	Uint32	check;

	if (host_client->state != cs_connected) {
		Com_Printf ("prespawn not valid -- already spawned\n");
		return;
	}
	// handle the case of a level changing while a client was connecting
	if (Q_atoi (Cmd_Argv (1)) != svs.spawncount) {
		Com_Printf ("SV_PreSpawn_f from different level\n");
		SV_New_f ();
		return;
	}

	buf = Q_atoi (Cmd_Argv (2));
	if (buf >= sv.num_signon_buffers)
		buf = 0;

	if (!buf) {
		// should be three numbers following containing checksums
		check = Q_atoi (Cmd_Argv (3));

		if (sv_mapcheck->ivalue && check != sv.worldmodel->brush->checksum &&
			check != sv.worldmodel->brush->checksum2) {
			SV_ClientPrintf (host_client, PRINT_HIGH,
							 "Map model file does not match (%s), %i != %i/%i.\n"
							 "You may need a new version of the map, or the proper install files.\n",
							 sv.modelname, check,
							 sv.worldmodel->brush->checksum,
							 sv.worldmodel->brush->checksum2);
			SV_DropClient (host_client);
			return;
		}
		host_client->checksum = check;
	}
	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
	//spawns.  These functions are written to not overflow
	if (host_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_PreSpawn] Back buffered (%d0, clearing",
					host_client->name, host_client->netchan.message.cursize);
		host_client->num_backbuf = 0;
		SZ_Clear (&host_client->netchan.message);
	}

	SZ_Write (&host_client->netchan.message,
			  sv.signon_buffers[buf], sv.signon_buffer_size[buf]);

	buf++;
	if (buf == sv.num_signon_buffers) {	// all done prespawning
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message,
						 va ("cmd spawn %i 0\n", svs.spawncount));
	} else {							// need to prespawn more
		MSG_WriteByte (&host_client->netchan.message, svc_stufftext);
		MSG_WriteString (&host_client->netchan.message,
						 va ("cmd prespawn %i %i\n", svs.spawncount, buf));
	}
}

static void
SV_Spawn_f (void)
{
	int         i;
	client_t   *client;
	edict_t    *ent;
	eval_t     *val;
	int         n;

	if (host_client->state != cs_connected) {
		Com_Printf ("Spawn not valid -- already spawned\n");
		return;
	}
// handle the case of a level changing while a client was connecting
	if (Q_atoi (Cmd_Argv (1)) != svs.spawncount) {
		Com_Printf ("SV_Spawn_f from different level\n");
		SV_New_f ();
		return;
	}

	n = Q_atoi (Cmd_Argv (2));

	// make sure n is valid
	if (n < 0 || n > MAX_CLIENTS) {
		Com_Printf ("SV_Spawn_f invalid client start\n");
		SV_New_f ();
		return;
	}

// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&host_client->netchan.message);

// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i = n, client = svs.clients + n; i < MAX_CLIENTS; i++, client++)
		SV_FullClientUpdateToClient (client, host_client);

// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		ClientReliableWrite_Begin (host_client, svc_lightstyle,
								   3 +
								   (sv.
									lightstyles[i] ? strlen (sv.
															 lightstyles[i]) :
									1));
		ClientReliableWrite_Byte (host_client, (char) i);
		ClientReliableWrite_String (host_client, sv.lightstyles[i]);
	}

	// set up the edict
	ent = host_client->edict;

	memset (&ent->v, 0, progs->entityfields * 4);
	ent->v.colormap = NUM_FOR_EDICT (ent);
	ent->v.team = 0;					// FIXME
	ent->v.netname = PR_SetString (host_client->name);

	host_client->entgravity = 1.0;
	val = GetEdictFieldValue (ent, "gravity");
	if (val)
		val->_float = 1.0;
	host_client->maxspeed = sv_maxspeed->fvalue;
	val = GetEdictFieldValue (ent, "maxspeed");
	if (val)
		val->_float = sv_maxspeed->fvalue;

//
// force stats to be updated
//
	memset (host_client->stats, 0, sizeof (host_client->stats));

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->total_monsters);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_SECRETS);
	ClientReliableWrite_Long (host_client, pr_global_struct->found_secrets);

	ClientReliableWrite_Begin (host_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (host_client, STAT_MONSTERS);
	ClientReliableWrite_Long (host_client, pr_global_struct->killed_monsters);

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
	ClientReliableWrite_String (host_client, "skins\n");

}

static void
SV_SpawnSpectator (void)
{
	Uint		i;
	edict_t		*e;

	VectorClear (sv_player->v.origin);
	VectorClear (sv_player->v.view_ofs);
	sv_player->v.view_ofs[2] = 22;

	// search for an info_playerstart to spawn the spectator at
	for (i = MAX_CLIENTS - 1; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM (i);
		if (!strcmp (PR_GetString (e->v.classname), "info_player_start"))
		{
			VectorCopy (e->v.origin, sv_player->v.origin);
			return;
		}
	}
}

static void
SV_Begin_f (void)
{
	unsigned    pmodel = 0, emodel = 0;
	int         i;

	if (host_client->state == cs_spawned)
		return;							// don't begin again

	host_client->state = cs_spawned;

	// handle the case of a level changing while a client was connecting
	if (Q_atoi (Cmd_Argv (1)) != svs.spawncount) {
		Com_Printf ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	if (host_client->spectator) {
		SV_SpawnSpectator ();

		if (SpectatorConnect) {
			// copy spawn parms out of the client_t
			for (i = 0; i < NUM_SPAWN_PARMS; i++)
				(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

			// call the spawn function
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG (sv_player);
			PR_ExecuteProgram (SpectatorConnect);
		}
	} else {
		// copy spawn parms out of the client_t
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		// actually spawn the player
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (pr_global_struct->PutClientInServer);
	}

	// clear the net statistics, because connecting gives a bogus picture
	host_client->netchan.frame_latency = 0;
	host_client->netchan.frame_rate = 0;
	host_client->netchan.drop_count = 0;
	host_client->netchan.good_count = 0;
	host_client->msec_last_check = -1;
	host_client->msecs = 0;
	host_client->msec_over = 0;

	// check he's not cheating

	pmodel = Q_atoi (Info_ValueForKey (host_client->userinfo, "pmodel"));
	emodel = Q_atoi (Info_ValueForKey (host_client->userinfo, "emodel"));

	if (pmodel != sv.model_player_checksum || emodel != sv.eyes_player_checksum)
		SV_BroadcastPrintf (PRINT_HIGH,
							"%s WARNING: non standard player/eyes model detected\n",
							host_client->name);

	// if we are paused, tell the client
	if (sv.paused) {
		ClientReliableWrite_Begin (host_client, svc_setpause, 2);
		ClientReliableWrite_Byte (host_client, sv.paused);
		SV_ClientPrintf (host_client, PRINT_HIGH, "Server is paused.\n");
	}
}

//=============================================================================

static void
SV_NextDownload_f (void)
{
	Uint8       buffer[1024];
	int         r;
	int         percent;
	int         size;

	if (!host_client->download)
		return;

	r = host_client->downloadsize - host_client->downloadcount;
	if (r > 768)
		r = 768;
	if (SDL_RWread (host_client->download, buffer, r, 1) != 1)
	{
		// read error
		Com_Printf("WARNING: read failed during client download!\n");
		// size = -1 signals a file not found error
		ClientReliableWrite_Begin (host_client, svc_download, 6);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		// close the file
		SDL_RWclose (host_client->download);
		host_client->download = NULL;
		return;
	}
	ClientReliableWrite_Begin (host_client, svc_download, 6 + r);
	ClientReliableWrite_Short (host_client, r);

	host_client->downloadcount += r;
	size = host_client->downloadsize;
	if (!size)
		size = 1;
	percent = host_client->downloadcount * 100 / size;
	ClientReliableWrite_Byte (host_client, percent);
	ClientReliableWrite_SZ (host_client, buffer, r);

	if (host_client->downloadcount != host_client->downloadsize)
		return;

	SDL_RWclose (host_client->download);
	host_client->download = NULL;

}

static void
SV_NextUpload (void)
{
	int         percent;
	int         size;

	if (!*host_client->uploadfn) {
		SV_ClientPrintf (host_client, PRINT_HIGH, "Upload denied\n");
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "stopul");

		// suck out rest of packet
		size = MSG_ReadShort ();
		MSG_ReadByte ();
		msg_readcount += size;
		return;
	}

	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (!host_client->upload) 
	{
		host_client->upload = FS_Open_New (host_client->uploadfn, 0);
		if (!host_client->upload) 
		{
			Sys_Printf ("Can't create %s\n", host_client->uploadfn);
			ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
			ClientReliableWrite_String (host_client, "stopul");
			*host_client->uploadfn = 0;
			return;
		}

		Sys_Printf ("Receiving %s from %d...\n", host_client->uploadfn,
					host_client->userid);
		if (host_client->remote_snap)
			Netchan_OutOfBandPrint (NS_SERVER, host_client->snap_from,
							 "%cServer receiving %s from %d...\n", A2C_PRINT,
							 host_client->uploadfn, host_client->userid);
	}

	SDL_RWwrite(host_client->upload, net_message.data + msg_readcount, size, 1);
	msg_readcount += size;

	Com_DPrintf ("UPLOAD: %d received\n", size);

	if (percent != 100) 
	{
		ClientReliableWrite_Begin (host_client, svc_stufftext, 8);
		ClientReliableWrite_String (host_client, "nextul\n");
	} 
	else 
	{
		SDL_RWclose (host_client->upload);
		host_client->upload = NULL;

		Sys_Printf ("%s upload completed.\n", host_client->uploadfn);

		if (host_client->remote_snap) 
		{
			char       *p;

			if ((p = strchr (host_client->uploadfn, '/')) != NULL)
				p++;
			else
				p = host_client->uploadfn;
			Netchan_OutOfBandPrint (NS_SERVER, host_client->snap_from,
							 "%c%s upload completed.\nTo download, enter:\ndownload %s\n",
							 A2C_PRINT, host_client->uploadfn, p);
		}
	}
}

static void
SV_BeginDownload_f (void)
{
	char		*name, *p;
	fs_file_t	*file;

	name = Zstrdup(tempzone, Cmd_Argv (1));

	// lowercase name
	for (p = name; *p; p++)
		*p = tolower(*p);

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check
	if (strstr (name, "..") || !allow_download->ivalue
		// leading dot is no good
		|| *name == '.'
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp (name, "skins/", 6) == 0 && !allow_download_skins->ivalue)
		// now models
		|| (strncmp (name, "progs/", 6) == 0 && !allow_download_models->ivalue)
		// now sounds
		|| (strncmp (name, "sound/", 6) == 0 && !allow_download_sounds->ivalue)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp (name, "maps/", 6) == 0 && !allow_download_maps->ivalue)
		// MUST be in a subdirectory 
		|| !strstr (name, "/")) {		// don't allow anything with .. path
		ClientReliableWrite_Begin (host_client, svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		Z_Free (name);
		return;
	}

	if (host_client->download) {
		SDL_RWclose (host_client->download);
		host_client->download = NULL;
	}

	file = FS_FindFile (name);
	if (!file)
		goto error;
	if (file->group->flags & FS_NO_UPLOAD)
		goto error;

	host_client->downloadsize = file->len;
	host_client->downloadcount = 0;
	host_client->download = file->open(file, 0);

	if (!file || !host_client->download) {
error:
		Sys_Printf ("Couldn't download %s to %s\n", name, host_client->name);
		ClientReliableWrite_Begin (host_client, svc_download, 4);
		ClientReliableWrite_Short (host_client, -1);
		ClientReliableWrite_Byte (host_client, 0);
		Z_Free (name);
		return;
	}

	SV_NextDownload_f ();
	Sys_Printf ("Downloading %s to %s\n", name, host_client->name);
	Z_Free (name);
}

//=============================================================================

static void
SV_Say (qboolean team)
{
	client_t   *client;
	int         j, tmp;
	const char	*p;
	char        text[2048];
	char        t1[32], *t2;

	if (Cmd_Argc () < 2)
		return;

	if (team) 
		strlcpy (t1, Info_ValueForKey (host_client->userinfo, "team"), sizeof (t1));

	if (host_client->spectator && (!sv_spectalk->ivalue || team))
		snprintf (text, sizeof (text), "[SPEC] %s: ", host_client->name);
	else if (team)
		snprintf (text, sizeof (text), "(%s): ", host_client->name);
	else 
		snprintf (text, sizeof (text), "%s: ", host_client->name);

	if (fp_messages) 
	{
		if (!sv.paused && svs.realtime < host_client->lockedtill) 
		{
			SV_ClientPrintf (host_client, PRINT_CHAT,
							 "You can't talk for %d more seconds\n",
							 (int) (host_client->lockedtill - svs.realtime));
			return;
		}
		tmp = host_client->whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10 + tmp;
		if (!sv.paused && host_client->whensaid[tmp]
			&& (svs.realtime - host_client->whensaid[tmp] < fp_persecond)) {
			host_client->lockedtill = svs.realtime + fp_secondsdead;
			if (fp_msg[0])
				SV_ClientPrintf (host_client, PRINT_CHAT,
								 "FloodProt: %s\n", fp_msg);
			else
				SV_ClientPrintf (host_client, PRINT_CHAT,
								 "FloodProt: You can't talk for %d seconds.\n",
								 fp_secondsdead);
			return;
		}
		host_client->whensaidhead++;
		if (host_client->whensaidhead > 9)
			host_client->whensaidhead = 0;
		host_client->whensaid[host_client->whensaidhead] = svs.realtime;
	}

	p = Cmd_Args ();

	if (*p == '"') 
	{
		p++;
		strlcat_s (text, p);
		text[strlen (text) - 1] = 0;
	} else
		strlcat_s (text, p);

	strlcat_s (text, "\n");

	Sys_Printf ("%s", text);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state < cs_connected)	// Clients connecting can hear.
			continue;
		if (host_client->spectator && !sv_spectalk->ivalue)
			if (!client->spectator)
				continue;

		if (team) 
		{
			// the spectator team
			if (host_client->spectator) 
			{
				if (!client->spectator)
					continue;
			} 
			else 
			{
				t2 = Info_ValueForKey (client->userinfo, "team");
				if (strcmp (t1, t2) || client->spectator)
					continue;			// on different teams
			}
		}
		SV_ClientPrintf (client, PRINT_CHAT, "%s", text);
	}
}


static void
SV_Say_f (void)
{
	SV_Say (false);
}

static void
SV_Say_Team_f (void)
{
	SV_Say (true);
}



//============================================================================

/*
=================
The client is showing the scoreboard, so send new ping times for all
clients
=================
*/
static void
SV_Pings_f (void)
{
	client_t   *client;
	int         j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) 
	{
		if (client->state != cs_spawned)
			continue;

		ClientReliableWrite_Begin (host_client, svc_updateping, 4);
		ClientReliableWrite_Byte (host_client, j);
		ClientReliableWrite_Short (host_client, SV_CalcPing (client));
		ClientReliableWrite_Begin (host_client, svc_updatepl, 4);
		ClientReliableWrite_Byte (host_client, j);
		ClientReliableWrite_Byte (host_client, client->lossage);
	}
}



static void
SV_Kill_f (void)
{
	if (sv_player->v.health <= 0) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG (sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}

void
SV_TogglePause (const char *msg)
{
	int         i;
	client_t   *cl;

	sv.paused ^= 1;

	if (msg)
		SV_BroadcastPrintf (PRINT_HIGH, "%s", msg);

	// send notification to all clients
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) 
	{
		if (!cl->state)
			continue;
		ClientReliableWrite_Begin (cl, svc_setpause, 2);
		ClientReliableWrite_Byte (cl, sv.paused);
	}
}


static void
SV_Pause_f (void)
{
	char        st[sizeof (host_client->name) + 32];

	if (!pausable->ivalue) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Pause not allowed.\n");
		return;
	}

	if (host_client->spectator) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH,
						 "Spectators can not pause.\n");
		return;
	}

	if (!sv.paused)
		snprintf (st, sizeof (st), "%s paused the game\n", host_client->name);
	else
		snprintf (st, sizeof (st), "%s unpaused the game\n", host_client->name);

	SV_TogglePause (st);
}


/*
=================
The client is going to disconnect, so remove the connection immediately
=================
*/
static void
SV_Drop_f (void)
{
	SV_EndRedirect ();
	if (!host_client->spectator)
		SV_BroadcastPrintf (PRINT_HIGH, "%s dropped\n", host_client->name);
	SV_DropClient (host_client);
}

/*
=================
Change the bandwidth estimate for a client
=================
*/
static void
SV_PTrack_f (void)
{
	int         i;
	edict_t    *ent, *tent;

	if (!host_client->spectator)
		return;

	if (Cmd_Argc () != 2) 
	{
		// turn off tracking
		host_client->spec_track = 0;
		ent = EDICT_NUM (host_client - svs.clients + 1);
		tent = EDICT_NUM (0);
		ent->v.goalentity = EDICT_TO_PROG (tent);
		return;
	}

	i = Q_atoi (Cmd_Argv (1));
	if (i < 0 || i >= MAX_CLIENTS || svs.clients[i].state != cs_spawned ||
		svs.clients[i].spectator) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Invalid client to track\n");
		host_client->spec_track = 0;
		ent = EDICT_NUM (host_client - svs.clients + 1);
		tent = EDICT_NUM (0);
		ent->v.goalentity = EDICT_TO_PROG (tent);
		return;
	}
	host_client->spec_track = i + 1;	// now tracking

	ent = EDICT_NUM (host_client - svs.clients + 1);
	tent = EDICT_NUM (i + 1);
	ent->v.goalentity = EDICT_TO_PROG (tent);
}


/*
=================
Change the bandwidth estimate for a client
=================
*/
static void
SV_Rate_f (void)
{
	int         rate;

	if (Cmd_Argc () != 2) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Current rate is %i\n",
						 (int) (1.0 / host_client->netchan.rate + 0.5));
		return;
	}

	rate = Q_atoi (Cmd_Argv (1));
	rate = bound (500, rate, 10000);

	SV_ClientPrintf (host_client, PRINT_HIGH, "Net rate set to %i\n", rate);
	host_client->netchan.rate = 1.0 / rate;
}


/*
=================
Change the message level for a client
=================
*/
static void
SV_Msg_f (void)
{
	if (Cmd_Argc () != 2) 
	{
		SV_ClientPrintf (host_client, PRINT_HIGH, "Current msg level is %i\n",
						 host_client->messagelevel);
		return;
	}

	host_client->messagelevel = Q_atoi (Cmd_Argv (1));

	SV_ClientPrintf (host_client, PRINT_HIGH, "Msg level set to %i\n",
					 host_client->messagelevel);
}

/*
==================
Allow clients to change userinfo
==================
*/
static void
SV_SetInfo_f (void)
{
	int         i;
	char        oldval[MAX_INFO_STRING];


	if (Cmd_Argc () == 1) 
	{
		Com_Printf ("User info settings:\n");
		Info_Print (host_client->userinfo);
		return;
	}

	if (Cmd_Argc () != 3) 
	{
		Com_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv (1)[0] == '*')
		return;							// don't set priveledged values

	strlcpy (oldval, Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)),
				sizeof (oldval));

	Info_SetValueForKey (host_client->userinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_INFO_STRING);
// name is extracted below in SV_ExtractFromUserinfo

	if (!strcmp
		(Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)), oldval))
		return;							// key hasn't changed

	// process any changed values
	SV_ExtractFromUserinfo (host_client);

	i = host_client - svs.clients;
	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteString (&sv.reliable_datagram, Cmd_Argv (1));
	MSG_WriteString (&sv.reliable_datagram,
					 Info_ValueForKey (host_client->userinfo, Cmd_Argv (1)));
}

/*
==================
Dumps the serverinfo info string
==================
*/
static void
SV_ShowServerinfo_f (void)
{
	Info_Print (svs.info);
}

static void
SV_NoSnap_f (void)
{
	if (*host_client->uploadfn) 
	{
		*host_client->uploadfn = 0;
		SV_BroadcastPrintf (PRINT_HIGH, "%s refused remote screenshot\n",
							host_client->name);
	}
}

typedef struct {
	char       *name;
	void        (*func) (void);
} ucmd_t;

static ucmd_t      ucmds[] = {
	{"new", SV_New_f},
	{"modellist", SV_Modellist_f},
	{"soundlist", SV_Soundlist_f},
	{"prespawn", SV_PreSpawn_f},
	{"spawn", SV_Spawn_f},
	{"begin", SV_Begin_f},

	{"drop", SV_Drop_f},
	{"pings", SV_Pings_f},

// issued by hand at client consoles    
	{"rate", SV_Rate_f},
	{"kill", SV_Kill_f},
	{"pause", SV_Pause_f},
	{"msg", SV_Msg_f},

	{"say", SV_Say_f},
	{"say_team", SV_Say_Team_f},

	{"setinfo", SV_SetInfo_f},

	{"serverinfo", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	{"ptrack", SV_PTrack_f},			// ZOID - used with autocam

	{"snap", SV_NoSnap_f},

	{NULL, NULL}
};

static void
SV_ExecuteUserCommand (char *s)
{
	ucmd_t     *u;

	Cmd_TokenizeString (s);
	sv_player = host_client->edict;

	SV_BeginRedirect (RD_CLIENT);

	for (u = ucmds; u->name; u++)
	{
		if (!strcmp (Cmd_Argv (0), u->name)) 
		{
			u->func ();
			break;
		}
	}

	if (!u->name)
		Com_Printf ("Bad user command: %s\n", Cmd_Argv (0));

	SV_EndRedirect ();
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
===============
Used by view and sv_user
===============
*/
float
V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t      forward, right;
	float       sign;
	float       side;
	float       value;

	AngleVectors (angles, forward, right, NULL);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs (side);

	value = cl_rollangle->fvalue;

	if (side < cl_rollspeed->fvalue)
		side = side * value / cl_rollspeed->fvalue;
	else
		side = value;

	return side * sign;
}




//============================================================================

static vec3_t      pmove_mins, pmove_maxs;

static void
AddLinksToPmove (areanode_t *node)
{
	link_t     *l, *next;
	edict_t    *check;
	int         pl;
	int         i;
	physent_t  *pe;

	pl = EDICT_TO_PROG (sv_player);

	// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA (l);

		if (check->v.owner == pl)
			continue;					// player's own missile
		if (check->v.solid == SOLID_BSP
			|| check->v.solid == SOLID_BBOX
			|| check->v.solid == SOLID_SLIDEBOX) {
			if (check == sv_player)
				continue;

			for (i = 0; i < 3; i++)
				if (check->v.absmin[i] > pmove_maxs[i]
					|| check->v.absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			if (pmove.numphysent == MAX_PHYSENTS)
				return;
			pe = &pmove.physents[pmove.numphysent++];

			VectorCopy (check->v.origin, pe->origin);
			pe->info = NUM_FOR_EDICT (check);
			pe->id = -1;
			if (check->v.solid == SOLID_BSP)
				pe->model = sv.models[(int) (check->v.modelindex)];
			else 
			{
				pe->model = NULL;
				VectorCopy (check->v.mins, pe->mins);
				VectorCopy (check->v.maxs, pe->maxs);
			}
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (pmove_maxs[node->axis] > node->dist)
		AddLinksToPmove (node->children[0]);
	if (pmove_mins[node->axis] < node->dist)
		AddLinksToPmove (node->children[1]);
}

/*
===========
===========
Done before running a player command.  Clears the touch array
*/
static Uint8       playertouch[(MAX_EDICTS + 7) / 8];

static void
SV_PreRunCmd (void)
{
	memset (playertouch, 0, sizeof (playertouch));
}

static void
SV_RunCmd (usercmd_t *ucmd, qboolean inside)
{
	edict_t    *ent;
	int         i, n;
	int         oldmsec;

	if (!inside && sv_timekick->ivalue) {
		double	time_since;
		int		time_allowed;

		if (host_client->msec_last_check == -1)
			goto SV_RunCmd__clear;

		host_client->msecs += ucmd->msec;
		time_since = svs.realtime - host_client->msec_last_check;
		if (time_since >= sv_timekick_interval->ivalue) {
			time_allowed = time_since * (1000 + sv_timekick_allowed->ivalue);
			if (host_client->msecs > time_allowed) {
				host_client->msec_over++;
				SV_BroadcastPrintf(PRINT_HIGH, "Temporal anomaly:\n"
						"%f in %f for %s (%d/%d)\n", host_client->msecs,
						time_since, host_client->name, host_client->msec_over,
						sv_timekick->ivalue);
				if (host_client->msec_over >= sv_timekick->ivalue) {
					SV_BroadcastPrintf(PRINT_HIGH,
							"Kicked %s for time sync errors. (%d times)\n",
							host_client->name, host_client->msec_over);
					SV_DropClient (host_client);
					return;
				}
			}
		}
SV_RunCmd__clear:
		host_client->msecs = 0;
		host_client->msec_last_check = svs.realtime;
	}
		
	cmd = *ucmd;

	// chop up very long commands
	if (cmd.msec > 50) {
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		SV_RunCmd (&cmd, true);
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		SV_RunCmd (&cmd, true);
		return;
	}

	if (!sv_player->v.fixangle)
		VectorCopy (ucmd->angles, sv_player->v.v_angle);

	sv_player->v.button0 = ucmd->buttons & 1;
	sv_player->v.button2 = (ucmd->buttons & 2) >> 1;
	sv_player->v.button1 = (ucmd->buttons & 4) >> 2;
	if (ucmd->impulse)
		sv_player->v.impulse = ucmd->impulse;

//
// angles
// show 1/3 the pitch angle and all the roll angle  
	if (sv_player->v.health > 0) {
		if (!sv_player->v.fixangle) {
			sv_player->v.angles[PITCH] = -sv_player->v.v_angle[PITCH] / 3;
			sv_player->v.angles[YAW] = sv_player->v.v_angle[YAW];
		}
		sv_player->v.angles[ROLL] =
			V_CalcRoll (sv_player->v.angles, sv_player->v.velocity) * 4;
	}

	host.frametime = ucmd->msec * 0.001;
	if (host.frametime > 0.1)
		host.frametime = 0.1;

	if (!host_client->spectator) {
		pr_global_struct->frametime = host.frametime;

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (pr_global_struct->PlayerPreThink);

		SV_RunThink (sv_player);
	}

	for (i = 0; i < 3; i++)
		pmove.origin[i] =
			sv_player->v.origin[i] + (sv_player->v.mins[i] - player_mins[i]);
	VectorCopy (sv_player->v.velocity, pmove.velocity);
	VectorCopy (sv_player->v.v_angle, pmove.angles);

	pmove.spectator = host_client->spectator;
	pmove.waterjumptime = sv_player->v.teleport_time;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	pmove.physents[0].id = -1;
	pmove.cmd = *ucmd;
	pmove.dead = sv_player->v.health <= 0;
	pmove.oldbuttons = host_client->oldbuttons;
	pmove.player_id = -1;

	movevars.entgravity = host_client->entgravity;
	movevars.maxspeed = host_client->maxspeed;

	for (i = 0; i < 3; i++) {
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}
	AddLinksToPmove (sv_areanodes);
	PlayerMove ();

	host_client->oldbuttons = pmove.oldbuttons;
	sv_player->v.teleport_time = pmove.waterjumptime;
	sv_player->v.waterlevel = pmove.waterlevel;
	sv_player->v.watertype = pmove.watertype;
	if (pmove.groundent) {
		sv_player->v.flags = (int) sv_player->v.flags | FL_ONGROUND;
		sv_player->v.groundentity =
			EDICT_TO_PROG (EDICT_NUM (pmove.groundent->info));
	} else
		sv_player->v.flags = (int) sv_player->v.flags & ~FL_ONGROUND;
	for (i = 0; i < 3; i++)
		sv_player->v.origin[i] =
			pmove.origin[i] - (sv_player->v.mins[i] - player_mins[i]);

	VectorCopy (pmove.velocity, sv_player->v.velocity);

	VectorCopy (pmove.angles, sv_player->v.v_angle);

	if (!host_client->spectator) {
		// link into place and touch triggers
		SV_LinkEdict (sv_player, true);

		// touch other objects
		for (i = 0; i < pmove.numtouch; i++) {
			n = pmove.touch[i]->info;
			ent = EDICT_NUM (n);
			if (!ent->v.touch || (playertouch[n / 8] & (1 << (n % 8))))
				continue;
			pr_global_struct->self = EDICT_TO_PROG (ent);
			pr_global_struct->other = EDICT_TO_PROG (sv_player);
			PR_ExecuteProgram (ent->v.touch);
			playertouch[n / 8] |= 1 << (n % 8);
		}
	}
}

/*
===========
===========
Done after running a player command.
*/
static void
SV_PostRunCmd (void)
{
	// run post-think

	if (!host_client->spectator) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (pr_global_struct->PlayerPostThink);
		SV_RunNewmis ();
	} else if (SpectatorThink) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG (sv_player);
		PR_ExecuteProgram (SpectatorThink);
	}
}


/*
===================
The current net_message is parsed for the given client
===================
*/
void
SV_ExecuteClientMessage (client_t *cl)
{
	int         c;
	char       *s;
	usercmd_t   oldest, oldcmd, newcmd;
	client_frame_t *frame;
	vec3_t      o;
	qboolean    move_issued = false;	// only allow one move command
	int         checksumIndex;
	Uint8       checksum, calculatedChecksum;
	int         seq_hash;

	// calc ping time
	frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = svs.realtime - frame->senttime;

	// make sure the reply sequence number matches the incoming
	// sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;		// don't reply, sequences have slipped 

	// save time for ping calculations
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = svs.realtime;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	host_client = cl;
	sv_player = host_client->edict;

//  seq_hash = (cl->netchan.incoming_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
	cl->localtime = sv.time;
	cl->delta_sequence = -1;			// no delta unless requested
	while (1) {
		if (msg_badread) {
			Com_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c) {
			default:
				Com_Printf ("SV_ReadClientMessage: unknown command char\n");
				SV_DropClient (cl);
				return;

			case clc_nop:
				break;

			case clc_delta:
				cl->delta_sequence = MSG_ReadByte ();
				break;

			case clc_move:
				if (move_issued)
					return;				// someone is trying to cheat...

				move_issued = true;

				checksumIndex = MSG_GetReadCount ();
				checksum = (Uint8) MSG_ReadByte ();

				// read loss percentage
				cl->lossage = MSG_ReadByte ();

				MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
				MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
				MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);

				if (cl->state != cs_spawned)
					break;

				// if the checksum fails, ignore the rest of the packet
				calculatedChecksum =
					COM_BlockSequenceCRCByte (net_message.data + checksumIndex +
											  1,
											  MSG_GetReadCount () -
											  checksumIndex - 1, seq_hash);

				if (calculatedChecksum != checksum) {
					Com_DPrintf
						("Failed command checksum for %s(%d) (%d != %d)\n",
						 cl->name, cl->netchan.incoming_sequence, checksum,
						 calculatedChecksum);
					return;
				}

				if (!sv.paused) {
					SV_PreRunCmd ();

					if (net_drop < 20) {
						while (net_drop > 2) {
							SV_RunCmd (&cl->lastcmd, false);
							net_drop--;
						}
						if (net_drop > 1)
							SV_RunCmd (&oldest, false);
						if (net_drop > 0)
							SV_RunCmd (&oldcmd, false);
					}
					SV_RunCmd (&newcmd, false);

					SV_PostRunCmd ();
				}

				cl->lastcmd = newcmd;
				cl->lastcmd.buttons = 0;	// avoid multiple fires on lag
				break;


			case clc_stringcmd:
				s = MSG_ReadString ();
				SV_ExecuteUserCommand (s);
				break;

			case clc_tmove:
				o[0] = MSG_ReadCoord ();
				o[1] = MSG_ReadCoord ();
				o[2] = MSG_ReadCoord ();
				// only allowed by spectators
				if (host_client->spectator) {
					VectorCopy (o, sv_player->v.origin);
					SV_LinkEdict (sv_player, false);
				}
				break;

			case clc_upload:
				SV_NextUpload ();
				break;

		}
	}
}

void
SV_UserInit (void)
{
	cl_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL);
	cl_rollangle = Cvar_Get ("cl_rollangle", "2.0", CVAR_NONE, NULL);
	sv_spectalk = Cvar_Get ("sv_spectalk", "1", CVAR_NONE, NULL);

	sv_mapcheck = Cvar_Get ("sv_mapcheck", "1", CVAR_NONE, NULL);

	sv_timekick = Cvar_Get ("sv_timekick", "3", CVAR_NONE, NULL);
	sv_timekick_allowed = Cvar_Get ("sv_timekick_allowed",
									"200", CVAR_NONE, NULL);
	sv_timekick_interval = Cvar_Get ("sv_timekick_interval",
									 "30", CVAR_NONE, NULL);
}

