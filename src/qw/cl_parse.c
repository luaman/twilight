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
#include <stdlib.h>

#include "quakedef.h"
#include "sound/cdaudio.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "collision.h"
#include "cvar.h"
#include "dlight.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "pmove.h"
#include "renderer/screen.h"
#include "sound/sound.h"
#include "strlib.h"
#include "sys.h"
#include "renderer/sky.h"
#include "teamplay.h"
#include "fs/fs.h"

static char       *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",						// [long] server version
	"svc_setview",						// [short] entity number
	"svc_sound",						// <see code>
	"svc_time",							// [float] server time
	"svc_print",						// [string] null terminated string
	"svc_stufftext",					// [string] stuffed into client's
	// console buffer
	// the string should be \n terminated
	"svc_setangle",						// [vec3] set the view angle to this
	// absolute value

	"svc_serverdata",					// [long] version ...
	"svc_lightstyle",					// [byte] [string]
	"svc_updatename",					// [byte] [string]
	"svc_updatefrags",					// [byte] [short]
	"svc_clientdata",					// <shortbits + data>
	"svc_stopsound",					// <see code>
	"svc_updatecolors",					// [byte] [byte]
	"svc_particle",						// [vec3] <variable>
	"svc_damage",						// [byte] impact [byte] blood [vec3]
	// from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",					// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",

	"svc_cdtrack",
	"svc_sellscreen",

	"svc_smallkick",
	"svc_bigkick",

	"svc_updateping",
	"svc_updateentertime",

	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_choke",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",

	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL"
};

static int         oldparsecountmod;
int         parsecountmod;
double      parsecounttime;

int         cl_spikeindex, cl_playerindex, cl_flagindex;

//=============================================================================

int			packet_latency[NET_TIMINGS];

int
CL_CalcNet (void)
{
	int			a, i, lost;
	frame_t		*frame;

	for (i = cls.netchan.outgoing_sequence - UPDATE_BACKUP + 1;
			i <= cls.netchan.outgoing_sequence; i++) {
		frame = &cl.frames[i & UPDATE_MASK];
		if (frame->receivedtime == -1)
			packet_latency[i & NET_TIMINGSMASK] = 9999; // dropped
		else if (frame->receivedtime == -2)
			packet_latency[i & NET_TIMINGSMASK] = 10000;    // choked
		else if (frame->invalid)
			packet_latency[i & NET_TIMINGSMASK] = 9998; // invalid delta
		else
			packet_latency[i & NET_TIMINGSMASK] =
				(frame->receivedtime - frame->senttime) * 20;
	}

	for (lost = 0, a = 0; a < NET_TIMINGS; a++) {
		i = (cls.netchan.outgoing_sequence - a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
	}

	return lost * 100 / NET_TIMINGS;
}

//=============================================================================

/*
===============
Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean
CL_CheckOrDownloadFile (char *filename)
{
	if (strstr (filename, "..")) {
		Com_Printf ("Refusing to download a path with ..\n");
		return true;
	}

	if (FS_FindFile (filename))	// it exists, no need to download
		return true;

	// ZOID - can't download when recording
	if (ccls.demorecording) {
		Com_Printf ("Unable to download %s in record mode.\n",
					cls.downloadname);
		return true;
	}

	// ZOID - can't download when playback
	if (ccls.demoplayback)
		return true;

	strlcpy_s (cls.downloadname, filename);
	Com_Printf ("Downloading %s...\n", cls.downloadname);

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
					 va ("download %s", cls.downloadname));

	cls.downloadnumber++;

	return false;
}

model_t *mdl_torch = NULL;

static void
Model_NextDownload (void)
{
	char       *s;
	int         i;
	char mapname[MAX_QPATH] = { 0 };

	if (cls.downloadnumber == 0) {
		Com_Printf ("Checking models...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_model;

	for (; cl.model_name[cls.downloadnumber][0]; cls.downloadnumber++) {
		s = cl.model_name[cls.downloadnumber];

		if (s[0] == '*')
			continue;					// inline brush model

		if (!CL_CheckOrDownloadFile (s))
			return;						// started a download
	}

	for (i = 1; i < MAX_MODELS; i++) {
		if (!cl.model_name[i][0])
			break;

		if (i == 1)
		{
			ccl.model_precache[i] = Mod_ForName (cl.model_name[i], FLAG_RENDER | FLAG_SUBMODELS);
			strlcpy (mapname, COM_SkipPath (ccl.model_precache[1]->name), MAX_QPATH);
			COM_StripExtension (mapname, mapname);
			Cvar_Set (cl_mapname, mapname);
		} else
			ccl.model_precache[i] = Mod_ForName (cl.model_name[i], FLAG_RENDER);

		if (!ccl.model_precache[i]) {
			Com_Printf ("\nThe required model file '%s' could not be found "
					"or downloaded.\n\n", cl.model_name[i]);
			Com_Printf ("You may need to download or purchase a %s client "
						"pack in order to play on this server.\n\n",
						gamedirfile);
			CL_Disconnect ();
			return;
		}

		if (!strcasecmp (ccl.model_precache[i]->name, "progs/flame.mdl")) {
			if (!mdl_torch)
				mdl_torch = Mod_ForName ("progs/torch.mdl", FLAG_RENDER);
			if (!mdl_torch)
				mdl_torch = Mod_ForName ("progs/fire.mdl", FLAG_RENDER);
		}
	}

	// all done
	ccl.worldmodel = r.worldmodel = ccl.model_precache[1];

	memset (&cl_network_entities, 0, sizeof(cl_network_entities));
	memset (&cl_player_entities, 0, sizeof(cl_player_entities));
	memset (&cl_static_entities, 0, sizeof(cl_static_entities));
	cl_num_static_entities = 0;

	R_NewMap ();
	Team_NewMap ();

	// done with modellist, request first of static signon messages
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
			va ("prespawn %d 0 %d", cl.servercount,
				ccl.worldmodel->brush->checksum2));
}

static void
Sound_NextDownload (void)
{
	char       *s;
	int         i;

	if (cls.downloadnumber == 0) {
		Com_Printf ("Checking sounds...\n");
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_sound;

	for (; cl.sound_name[cls.downloadnumber][0]; cls.downloadnumber++) {
		s = cl.sound_name[cls.downloadnumber];

		if (!CL_CheckOrDownloadFile (va ("sound/%s", s)))
			return;						// started a download
	}

	for (i = 1; i < MAX_SOUNDS; i++) {
		if (!cl.sound_name[i][0])
			break;

		ccl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}

	// done with sounds, request models now
	memset (ccl.model_precache, 0, sizeof (ccl.model_precache));
	cl_playerindex = -1;
	cl_spikeindex = -1;
	cl_flagindex = -1;
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
			va ("modellist %d %d", cl.servercount, 0));
}


static void
CL_RequestNextDownload (void)
{
	switch (cls.downloadtype) {
		case dl_single:
			break;

		case dl_skin:
			Skin_NextDownload ();
			break;

		case dl_model:
			Model_NextDownload ();
			break;

		case dl_sound:
			Sound_NextDownload ();
			break;

		case dl_none:
		default:
			Com_DPrintf ("Unknown download type.\n");
			break;
	}
}

/*
=====================
A download message has been received from the server
=====================
*/
static void
CL_ParseDownload (void)
{
	int         size, percent;


	// read the data
	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (ccls.demoplayback) {
		if (size > 0)
			msg_readcount += size;
		return;							// not in demo playback
	}

	if (size == -1) {
		Com_Printf ("File not found.\n");
		if (cls.download) {
			Com_Printf ("cls.download shouldn't have been set\n");
			SDL_RWclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download) {
		COM_CreatePath (cls.downloadname);

		cls.download = FS_Open_New (cls.downloadname, 0);

		if (!cls.download) {
			msg_readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadname);
			CL_RequestNextDownload ();
			return;
		}
	}

	SDL_RWwrite (cls.download, net_message.data + msg_readcount, size, 1);
	msg_readcount += size;

	if (percent != 100) {
		// request next block
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	} else {
		SDL_RWclose (cls.download);

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}

static Uint8 *upload_data;
static int    upload_pos;
static int    upload_size;

void
CL_NextUpload (void)
{
	Uint8       buffer[768];
	int         r;
	int         percent;
	int         size;

	if (!upload_data)
		return;

	r = upload_size - upload_pos;
	if (r > 768)
		r = 768;
	memcpy (buffer, upload_data + upload_pos, r);
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	upload_pos += r;
	size = upload_size;
	if (!size)
		size = 1;
	percent = upload_pos * 100 / size;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

	Com_DPrintf ("UPLOAD: %6d: %d written\n", upload_pos - r, r);

	if (upload_pos != upload_size)
		return;

	Com_Printf ("Upload completed\n");

	free (upload_data);
	upload_data = 0;
	upload_pos = upload_size = 0;
}

void
CL_StartUpload (Uint8 *data, int size)
{
	if (ccls.state < ca_onserver)
		return;							// gotta be connected

	// override
	if (upload_data)
		free (upload_data);

	Com_DPrintf ("Upload starting of %d...\n", size);

	upload_data = malloc (size);
	memcpy (upload_data, data, size);
	upload_size = size;
	upload_pos = 0;

	CL_NextUpload ();
}

qboolean
CL_IsUploading (void)
{
	return (qboolean)(upload_data);
}

void
CL_StopUpload (void)
{
	if (upload_data)
		free (upload_data);
	upload_data = NULL;
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

static void
CL_ParseServerData (void)
{
	char       *str;
	qboolean    cflag = false;
	int         protover;

	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
// allow 2.2 and 2.29 demos to play
	protover = MSG_ReadLong ();
	if (protover != PROTOCOL_VERSION &&
		!(ccls.demoplayback
		  && (protover == 26 || protover == 27 || protover == 28)))
		Host_EndGame ("Server returned version %i, not %i\n"
				"You probably need to upgrade.\n"
				"Check http://www.quakeworld.net/",
				protover, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong ();

	// game directory
	str = MSG_ReadString ();

	if (strcasecmp (gamedirfile, str)) {
		// save current config
		Host_WriteConfiguration ("config");
		cflag = true;
	}

	COM_Gamedir (str);

	// ZOID--run the autoexec.cfg in the gamedir
	// if it exists
	if (cflag) {
		Cbuf_AddText ("cl_warncmd 0\n");
		Cbuf_AddText ("exec config.cfg\n");
		Cbuf_AddText ("exec frontend.cfg\n");
		Cbuf_AddText ("cl_warncmd 1\n");
	}

	// parse player slot, high bit means spectator
	ccl.player_num = MSG_ReadByte ();
	if (ccl.player_num & 128) {
		cl.spectator = true;
		ccl.player_num &= ~128;
	}

	cl.viewentity = ccl.player_num + 1;

	// get the full level name
	str = MSG_ReadString ();
	strlcpy (ccl.levelname, str, sizeof (ccl.levelname));

	// get the movevars
	movevars.gravity = MSG_ReadFloat ();
	movevars.stopspeed = MSG_ReadFloat ();
	movevars.maxspeed = MSG_ReadFloat ();
	movevars.spectatormaxspeed = MSG_ReadFloat ();
	movevars.accelerate = MSG_ReadFloat ();
	movevars.airaccelerate = MSG_ReadFloat ();
	movevars.wateraccelerate = MSG_ReadFloat ();
	movevars.friction = MSG_ReadFloat ();
	movevars.waterfriction = MSG_ReadFloat ();
	movevars.entgravity = MSG_ReadFloat ();

	// seperate the printfs so the server message can have a color
	Com_Printf
		("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		 "\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

	// ask for the sound list next
	memset (cl.sound_name, 0, sizeof (cl.sound_name));
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
			va ("soundlist %d %d", cl.servercount, 0));

	// now waiting for downloads, etc
	ccls.state = ca_onserver;
}

static void
CL_ParseSoundlist (void)
{
	int         numsounds;
	char       *str;
	int         n;

// precache sounds
//  memset (ccl.sound_precache, 0, sizeof(ccl.sound_precache));

	numsounds = MSG_ReadByte ();

	for (;;) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		numsounds++;
		if (numsounds == MAX_SOUNDS)
			Host_EndGame ("Server sent too many sound_precache");
		strlcpy_s (cl.sound_name[numsounds], str);
	}

	n = MSG_ReadByte ();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
				va ("soundlist %d %d", cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload ();
}

static void
CL_ParseModellist (void)
{
	int         nummodels;
	char       *str;
	int         n;

// precache models and note certain default indexes
	Com_Printf ("Recieving model list...");
	nummodels = MSG_ReadByte ();

	for (;;) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		nummodels++;
		if (nummodels == MAX_MODELS)
			Host_EndGame ("Server sent too many model_precache");
		strlcpy_s (cl.model_name[nummodels], str);

		if (!strcmp (cl.model_name[nummodels], "progs/spike.mdl"))
			cl_spikeindex = nummodels;
		if (!strcmp (cl.model_name[nummodels], "progs/player.mdl"))
			cl_playerindex = nummodels;
		if (!strcmp (cl.model_name[nummodels], "progs/flag.mdl"))
			cl_flagindex = nummodels;
	}

	Com_Printf ("%d", nummodels);
	n = MSG_ReadByte ();

	if (n) {
		Com_Printf (".%d\n", n);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
						 va ("modellist %d %d", cl.servercount, n));
		return;
	}
	Com_Printf ("\n", n);

	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload ();
}

void
CL_ParseBaseline (entity_state_t *es)
{
	int         i;

	es->modelindex = MSG_ReadByte ();
	es->frame = MSG_ReadByte ();
	es->colormap = MSG_ReadByte ();
	es->skinnum = MSG_ReadByte ();
	for (i = 0; i < 3; i++) {
		es->origin[i] = MSG_ReadCoord ();
		es->angles[i] = MSG_ReadAngle ();
	}
}


static void
CL_ParseStaticSound (void)
{
	vec3_t      org;
	int         sound_num, vol, atten;
	int         i;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	S_StaticSound (ccl.sound_precache[sound_num], org, vol, atten);
}



/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

static void
CL_ParseStartSoundPacket (void)
{
	vec3_t      pos;
	int         channel, ent;
	int         sound_num;
	int         volume;
	float       attenuation;
	int         i;

	channel = MSG_ReadShort ();

	if (channel & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (channel & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = MSG_ReadByte ();

	for (i = 0; i < 3; i++)
		pos[i] = MSG_ReadCoord ();

	ent = (channel >> 3) & 1023;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_EndGame ("CL_ParseStartSoundPacket: ent = %i", ent);

	S_StartSound (ent, channel, ccl.sound_precache[sound_num], pos,
				  volume / 255.0, attenuation);
}


/*
==================
Server information pertaining to this client only, sent every frame
==================
*/
static void
CL_ParseClientdata (void)
{
	int         i;
	float       latency;
	frame_t    *frame;

// calculate simulated time of message
	oldparsecountmod = parsecountmod;

	i = cls.netchan.incoming_acknowledged;
	cl.parsecount = i;
	i &= UPDATE_MASK;
	parsecountmod = i;
	frame = &cl.frames[i];
	parsecounttime = cl.frames[i].senttime;

	frame->receivedtime = host.time;

// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (latency < 0 || latency > 1.0) {
//      Com_Printf ("Odd latency: %5.2f\n", latency);
	} else {
		// drift the average latency towards the observed latency
		if (latency < cls.latency)
			cls.latency = latency;
		else
			cls.latency += 0.001;		// drift up, so correction are needed
	}
}

static void
CL_ProcessUserInfo (int slot)
{
	Uint8			color;
	player_info_t	*player = &cl.players[slot];
	user_info_t		*user = &ccl.users[slot];

	slot = slot;

	ccl.user_flags &= ~(USER_FLAG_TEAM_SORTED | USER_FLAG_SORTED);
	strlcpy (user->name, Info_ValueForKey (player->userinfo, "name"),
			   sizeof (user->name));

	strlcpy (user->team, Info_ValueForKey (player->userinfo, "team"),
			   sizeof (user->team));

	color = Q_atoi (Info_ValueForKey (player->userinfo, "topcolor"));
	color = bound(0, color, 13) * 16;
	if (color < 128)
		color += 15;
	user->color_top = color;
	VectorCopy4 (d_8tofloattable[color], user->color_map.top);

	color = Q_atoi (Info_ValueForKey (player->userinfo, "bottomcolor"));
	color = bound(0, color, 13) * 16;
	if (color < 128)
		color += 15;
	user->color_bottom = color;
	VectorCopy4 (d_8tofloattable[color], user->color_map.bottom);

	if (Info_ValueForKey (player->userinfo, "*spectator")[0])
		user->flags |= USER_SPECTATOR;
	else
		user->flags &= ~USER_SPECTATOR;

	strlcpy(user->skin_name, Info_ValueForKey (player->userinfo, "skin"),
			sizeof(user->skin_name));

	if (ccls.state == ca_active)
		user->skin = Skin_Load(user->skin_name);
}

static void
CL_UpdateUserinfo (void)
{
	int         slot;
	player_info_t *player;
	user_info_t	*user;

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame
			("CL_ParseServerMessage: svc_updateuserinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];
	user = &ccl.users[slot];
	user->user_id = MSG_ReadLong ();
	strlcpy (player->userinfo, MSG_ReadString (), sizeof (player->userinfo));

	CL_ProcessUserInfo (slot);
}

static void
CL_SetInfo (void)
{
	int         slot;
	player_info_t *player;
	char        key[MAX_MSGLEN];
	char        value[MAX_MSGLEN];

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame ("CL_ParseServerMessage: svc_setinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];

	strlcpy (key, MSG_ReadString (), sizeof (key));
	strlcpy (value, MSG_ReadString (), sizeof (value));

	Com_DPrintf ("SETINFO %s: %s=%s\n", ccl.users[slot].name, key, value);

	Info_SetValueForKey (player->userinfo, key, value, MAX_INFO_STRING);

	CL_ProcessUserInfo (slot);
}

void
CL_ProcessServerInfo (void)
{
	char *s;

	gl_allow &= ~(GLA_WIREFRAME | GLA_WATERALPHA);

	if (Q_atoi (Info_ValueForKey (cl.serverinfo, "watervis")))
		gl_allow |= GLA_WATERALPHA;

	// Commented in this manner because the rules don't make sense.
	// NOTE: ORDER MATTERS, DO NOT REORDER!

	// First we check to see if there is a coop server var.
	// If it exists and is true then this is a coop game, period.
	if ((s = Info_ValueForKey (cl.serverinfo, "coop")) && *s)
		if (Q_atoi (s)) {
			ccl.game_teams = GAME_COOP;
			return;
		}

	// Next we check to see if there is a deathmatch server var.
	// If it exists and is 0 then it is a single player game, period.
	// HOWEVER: If it does not exist it is true, if it is true then
	// do some more tests.
	if ((s = Info_ValueForKey (cl.serverinfo, "deathmatch")) && *s)
		if (!Q_atoi (s)) {
			ccl.game_teams = GAME_SINGLE;
			return;
		}

	// Now we check to see if teamplay exists and if it is true.
	// If so then it is a teamplay game, and the status bar code cares.
	if ((s = Info_ValueForKey (cl.serverinfo, "teamplay")) && *s)
		if (Q_atoi(s)) {
			ccl.game_teams = GAME_TEAMS;
			return;
		}

	// If we are here, then it is a good old deathmatch game, simple, right?
	ccl.game_teams = GAME_DEATHMATCH;
}

static void
CL_ServerInfo (void)
{
	char        key[MAX_MSGLEN];
	char        value[MAX_MSGLEN];
	
	strlcpy (key, MSG_ReadString (), sizeof (key));
	strlcpy (value, MSG_ReadString (), sizeof (value));

	Com_DPrintf ("SERVERINFO: %s=%s\n", key, value);

	Info_SetValueForKey (cl.serverinfo, key, value, MAX_SERVERINFO_STRING);

	CL_ProcessServerInfo ();
}

static void
CL_SetStat (int stat, int value)
{
	int         j;

	if (stat < 0 || stat >= MAX_CL_STATS)
		Host_EndGame ("CL_SetStat: %i is invalid", stat);

	if (stat == STAT_ITEMS) {			// set flash times
		for (j = 0; j < 32; j++)
			if ((value & (1 << j)) && !(ccl.stats[stat] & (1 << j)))
				ccl.items_gettime[j] = ccl.time;
	}

	ccl.stats[stat] = value;

	if ((stat == STAT_HEALTH) && value <= 0)
		Team_Dead ();
}

static void
CL_MuzzleFlash (void)
{
	vec3_t      fv;
	dlight_t   *dl;
	int         i;
	player_state_t *pl;

	i = MSG_ReadShort ();

	if ((unsigned)(i-1) >= MAX_CLIENTS)
	{
		entity_state_t	*ent;
		int	j, num_ent;

		// a monster firing
		num_ent = cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities.num_entities;
		for (j=0; j<num_ent; j++)
		{
			ent = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities.entities[j];
			if (ent->number == i)
			{
				dl = CCL_AllocDlight (-i);
				AngleVectors (ent->angles, fv, NULL, NULL);
				VectorMA (ent->origin, 18, fv, dl->origin);
				TraceLine (ccl.worldmodel, ent->origin, dl->origin, dl->origin,
						NULL);

				dl->radius = 200 + (rand()&31);
				dl->minlight = 32;
				dl->die = ccl.time + 0.1;
				dl->color[0] = 0.5;
				dl->color[1] = 0.5;
				dl->color[2] = 0.4;
				break;
			}
		}
		return;
	}

	// don't draw our own muzzle flash if flashblending
	if (i - 1 == ccl.player_num && gl_flashblend->ivalue)
		return;

	pl = &cl.frames[parsecountmod].playerstate[i - 1];

	dl = CCL_AllocDlight (-i);
	AngleVectors (pl->viewangles, fv, NULL, NULL);
	VectorMA (pl->origin, 18, fv, dl->origin);
	TraceLine (ccl.worldmodel, pl->origin, dl->origin, dl->origin, NULL);

	dl->radius = 200 + (rand () & 31);
	dl->minlight = 32;
	dl->die = ccl.time + 0.1;
	dl->color[0] = 0.5;
	dl->color[1] = 0.5;
	dl->color[2] = 0.4;
}


#define SHOWNET(x) if(cl_shownet->ivalue==2) \
	Com_Printf ("%3i:%s\n", msg_readcount-1, x);

static int         received_framecount;
void
CL_ParseServerMessage (void)
{
	int         cmd;
	char       *s;
	int         i, j;

	received_framecount = host.framecount;
	cl.last_servermessage = host.time;
	CL_ClearProjectiles ();

//
// if recording demos, copy the message out
//
	if (cl_shownet->ivalue == 1)
		Com_Printf ("%i ", net_message.cursize);
	else if (cl_shownet->ivalue == 2)
		Com_Printf ("------------------\n");


	CL_ParseClientdata ();

//
// parse the message
//
	while (1) {
		if (msg_badread) {
			Host_EndGame ("CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte ();

		if (cmd == -1) {
			msg_readcount++;
			// so the EOM showner has the right value
			SHOWNET ("END OF MESSAGE");
			break;
		}

		SHOWNET (svc_strings[cmd]);

		// other commands
		switch (cmd) {
			default:
				Host_EndGame
					("CL_ParseServerMessage: Illegible server message");
				break;

			case svc_nop:
				break;

			case svc_disconnect:
				if (ccls.state == ca_connected)
					Host_EndGame ("Server disconnected\n"
								  "Server version may not be compatible");
				else
					Host_EndGame ("Server disconnected");
				break;

			case svc_print:
				i = MSG_ReadByte ();
				if (i == PRINT_CHAT) {
					S_LocalSound ("misc/talk.wav");
					con_ormask = 128;
				}
				Com_Printf ("%s", MSG_ReadString ());
				con_ormask = 0;
				break;

			case svc_centerprint:
				SCR_CenterPrint (MSG_ReadString ());
				break;

			case svc_stufftext:
				s = MSG_ReadString ();
				Com_DPrintf ("stufftext: %s\n", s);
				Cbuf_AddText (s);
				break;

			case svc_damage:
				V_ParseDamage ();
				break;

			case svc_serverdata:
				// make sure any stuffed commands are done
				Cbuf_Execute ();
				CL_ParseServerData ();
				break;

			case svc_setangle:
				for (i = 0; i < 3; i++)
					cl.viewangles[i] = MSG_ReadAngle ();
				break;

			case svc_lightstyle:
				i = MSG_ReadByte ();
				if (i >= MAX_LIGHTSTYLES)
					Host_EndGame ("svc_lightstyle > MAX_LIGHTSTYLES");
				strlcpy_s (ccl.lightstyles[i].map, MSG_ReadString ());
				ccl.lightstyles[i].length = strlen (ccl.lightstyles[i].map);
				break;

			case svc_sound:
				CL_ParseStartSoundPacket ();
				break;

			case svc_stopsound:
				i = MSG_ReadShort ();
				S_StopSound (i >> 3, i & 7);
				break;

			case svc_updatefrags:
				i = MSG_ReadByte ();
				if (i >= MAX_CLIENTS)
					Host_EndGame ("CL_ParseServerMessage: svc_updatefrags >"
							" MAX_SCOREBOARD");
				ccl.users[i].frags = MSG_ReadShort ();
				ccl.user_flags &= ~(USER_FLAG_TEAM_SORTED | USER_FLAG_SORTED);
				break;

			case svc_updateping:
				i = MSG_ReadByte ();
				if (i >= MAX_CLIENTS)
					Host_EndGame ("CL_ParseServerMessage: svc_updateping >"
							" MAX_SCOREBOARD");
				ccl.users[i].ping = MSG_ReadShort ();
				ccl.user_flags &= ~USER_FLAG_TEAM_SORTED;
				break;

			case svc_updatepl:
				i = MSG_ReadByte ();
				if (i >= MAX_CLIENTS)
					Host_EndGame ("CL_ParseServerMessage: svc_updatepl >"
							" MAX_SCOREBOARD");
				ccl.users[i].pl = MSG_ReadByte ();
				break;

			case svc_updateentertime:
				// time is sent over as seconds ago
				i = MSG_ReadByte ();
				if (i >= MAX_CLIENTS)
					Host_EndGame ("CL_ParseServerMessage: svc_updateentertime"
							" > MAX_SCOREBOARD");
				ccl.users[i].entertime = ccl.time - MSG_ReadFloat ();
				break;

			case svc_spawnbaseline:
				i = MSG_ReadShort ();
				CL_ParseBaseline (&cl_baselines[i]);
				break;

			case svc_spawnstatic:
				CL_ParseStatic ();
				break;

			case svc_temp_entity:
				CL_ParseTEnt ();
				break;

			case svc_killedmonster:
				ccl.stats[STAT_MONSTERS]++;
				break;

			case svc_foundsecret:
				ccl.stats[STAT_SECRETS]++;
				break;

			case svc_updatestat:
				i = MSG_ReadByte ();
				j = MSG_ReadByte ();
				CL_SetStat (i, j);
				break;

			case svc_updatestatlong:
				i = MSG_ReadByte ();
				j = MSG_ReadLong ();
				CL_SetStat (i, j);
				break;

			case svc_spawnstaticsound:
				CL_ParseStaticSound ();
				break;

			case svc_cdtrack:
				cl.cdtrack = MSG_ReadByte ();
				CDAudio_Play ((Uint8) cl.cdtrack, true);
				break;

			case svc_intermission:
				ccl.intermission = 1;
				ccl.completed_time = ccl.time;
				for (i = 0; i < 3; i++)
					ccl.player_origin[i] = MSG_ReadCoord ();
				for (i = 0; i < 3; i++)
					ccl.player_angles[i] = MSG_ReadAngle ();
				VectorClear (ccl.player_velocity);
				break;

			case svc_finale:
				ccl.intermission = 2;
				ccl.completed_time = ccl.time;
				SCR_CenterPrint (MSG_ReadString ());
				break;

			case svc_sellscreen:
				Cmd_ExecuteString ("help", src_command);
				break;

			case svc_smallkick:
				cl.punchangle = -2;
				break;
			case svc_bigkick:
				cl.punchangle = -4;
				break;

			case svc_muzzleflash:
				CL_MuzzleFlash ();
				break;

			case svc_updateuserinfo:
				CL_UpdateUserinfo ();
				break;

			case svc_setinfo:
				CL_SetInfo ();
				break;

			case svc_serverinfo:
				CL_ServerInfo ();
				break;

			case svc_download:
				CL_ParseDownload ();
				break;

			case svc_playerinfo:
				CL_ParsePlayerinfo ();
				break;

			case svc_nails:
				CL_ParseProjectiles ();
				break;

			case svc_chokecount:
				// some preceding packets were choked
				i = MSG_ReadByte ();
				for (j = 0; j < i; j++)
					cl.frames[(cls.netchan.incoming_acknowledged - 1 -
							   j) & UPDATE_MASK].receivedtime = -2;
				break;

			case svc_modellist:
				CL_ParseModellist ();
				break;

			case svc_soundlist:
				CL_ParseSoundlist ();
				break;

			case svc_packetentities:
				CL_ParsePacketEntities (false);
				break;

			case svc_deltapacketentities:
				CL_ParsePacketEntities (true);
				break;

			case svc_maxspeed:
				movevars.maxspeed = MSG_ReadFloat ();
				break;

			case svc_entgravity:
				movevars.entgravity = MSG_ReadFloat ();
				break;

			case svc_setpause:
				ccl.paused = MSG_ReadByte ();
				if (ccl.paused)
					CDAudio_Pause ();
				else
					CDAudio_Resume ();
				break;

		}
	}

	CL_SetSolidEntities ();
}

void
CL_ParseEntityLump (char *entdata)
{
	const char *data;
	char key[128], value[4096];

	data = entdata;

	if (!data)
		return;
	data = COM_Parse (data);
	if (!data || com_token[0] != '{')
		return;							// error

	while (1)
	{
		data = COM_Parse (data);
		if (!data)
			return;						// error
		if (com_token[0] == '}')
			break;						// end of worldspawn
		if (com_token[0] == '_')
			strlcpy(key, com_token + 1, 128);
		else
			strlcpy(key, com_token, 128);

		while (key[strlen(key)-1] == ' ')
			key[strlen(key)-1] = 0;		// remove trailing spaces

		data = COM_Parse (data);
		if (!data)
			return;						// error
		strlcpy (value, com_token, 4096);

		if (strcmp (key, "sky") == 0 || strcmp (key, "skyname") == 0 ||
				strcmp (key, "qlsky") == 0)
			Cvar_Set (r_skyname, value);

		// more checks here..
	}
}

