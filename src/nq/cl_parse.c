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
#include "cdaudio.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "model.h"
#include "host.h"
#include "mathlib.h"
#include "net.h"
#include "screen.h"
#include "server.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "teamplay.h"

char       *svc_strings[] = {
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

	"svc_serverinfo",					// [long] version
	// [string] signon string
	// [string]..[0]model cache [string]...[0]sounds cache
	// [string]..[0]item cache
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
	"svc_finale",						// [string] music [string] text
	"svc_cdtrack",						// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene"
};

//=============================================================================

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t   *
CL_EntityNum (int num)
{
	if (num >= cl.num_entities) {
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %i is an invalid number", num);
		cl.num_entities = num + 1;
	}

	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void
CL_ParseStartSoundPacket (void)
{
	vec3_t      pos;
	int         channel, ent;
	int         sound_num;
	int         volume;
	int         field_mask;
	float       attenuation;
	int         i;

	field_mask = MSG_ReadByte ();

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	for (i = 0; i < 3; i++)
		pos[i] = MSG_ReadCoord ();

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos,
				  volume / 255.0, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void
CL_KeepaliveMessage (void)
{
	float       time;
	static float lastmsg;
	int         ret;
	sizebuf_t   old;
	Uint8       olddata[8192];

	if (sv.active)
		return;							// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);

	do {
		ret = CL_GetMessage ();
		switch (ret) {
			default:
				Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");
			case 0:
				break;					// nothing waiting
			case 1:
				Host_Error ("CL_KeepaliveMessage: received a message");
				break;
			case 2:
				if (MSG_ReadByte () != svc_nop)
					Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
				break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Com_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

model_t		*mdl_fire = NULL;

/*
==================
CL_ParseServerInfo
==================
*/
void
CL_ParseServerInfo (void)
{
	char       *str;
	int         i;
	int         nummodels, numsounds;
	char        model_precache[MAX_MODELS][MAX_QPATH];
	char        sound_precache[MAX_SOUNDS][MAX_QPATH];

	Com_DPrintf ("Serverinfo packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();

	if (i != PROTOCOL_VERSION)
		Host_Error ("Server returned version %i, not %i", i, PROTOCOL_VERSION);

// parse maxclients
	cl.maxclients = MSG_ReadByte ();

	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
		Host_Error ("Bad maxclients (%u) from server\n", cl.maxclients);

	cl.scores = Zone_Alloc (cl_zone, cl.maxclients * sizeof (*cl.scores));

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof (cl.levelname));

// seperate the printfs so the server message can have a color
	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	memset (cl.model_precache, 0, sizeof (cl.model_precache));
	for (nummodels = 1;; nummodels++) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS)
			Host_Error ("Server sent too many model precaches\n");
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

// precache sounds
	memset (cl.sound_precache, 0, sizeof (cl.sound_precache));
	for (numsounds = 1;; numsounds++) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS)
			Host_Error ("Server sent too many sound precaches\n");
		strcpy (sound_precache[numsounds], str);
	}

//
// now we try to load everything else until a cache allocation fails
//

	for (i = 1; i < nummodels; i++) {
		if (i == 1)
		{
			char mapname[MAX_QPATH] = { 0 };

			cl.model_precache[i] = Mod_ForName (model_precache[i],
					FLAG_CRASH | FLAG_RENDER | FLAG_SUBMODELS);

			strlcpy (mapname, COM_SkipPath (model_precache[i]), sizeof (mapname));
			COM_StripExtension (mapname, mapname);
			Cvar_Set (cl_mapname, mapname);
		} else
			cl.model_precache[i] = Mod_ForName (model_precache[i],
					FLAG_CRASH | FLAG_RENDER);


		if (!strcasecmp (model_precache[i], "progs/flame.mdl"))
			if (!mdl_fire)
				mdl_fire = Mod_ForName ("progs/fire.mdl", FLAG_RENDER);

		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();

	for (i = 1; i < numsounds; i++) {
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();


// local state
	cl_entities[0].common.model = cl.worldmodel = r_worldmodel = cl.model_precache[1];

	R_NewMap ();
	Team_NewMap ();

	Zone_CheckSentinelsGlobal ();		// Make sure nothing is hurt

	noclip_anglehack = false;			// noclip is turned off at start 
}


/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
void
CL_ParseUpdate (int bits)
{
	int         i;
	model_t    *model;
	int         modnum;
	qboolean    forcelink;
	entity_t   *ent;
	int         num;
	int         skin;
	vec3_t		origin, angles;

	if (cls.signon == SIGNONS - 1) {
		// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS) {
		i = MSG_ReadByte ();
		bits |= (i << 8);
	}

	num = (bits & U_LONGENTITY) ? 
		MSG_ReadShort() : MSG_ReadByte();

	ent = CL_EntityNum (num);

	forcelink = false;

	if (bits & U_MODEL) {
		modnum = MSG_ReadByte ();
		if (modnum >= MAX_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	} else
		modnum = ent->baseline.modelindex;

	model = cl.model_precache[modnum];
	if (model != ent->common.model) {
		entity_state_t	baseline;

		baseline = ent->baseline;
		memset(ent, 0, sizeof(*ent));
		ent->common.real_ent = ent;
		ent->baseline = baseline;
		ent->common.model = model;
		// automatic animation (torches, etc) can be either all together
		// or randomized
		if (model) {
			if (model->synctype == ST_RAND)
				ent->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		} else
			forcelink = true;			// hack to make null model players work
	}

	// no previous frame to lerp from
	if (ent->msgtime != cl.mtime[1])
	{
		forcelink = true;
		ent->common.time_left = 0;
	}

	ent->msgtime = cl.mtime[0];

	if (bits & U_FRAME)
		CL_Update_Frame(ent, MSG_ReadByte(), cl.time);
	else
		CL_Update_Frame(ent, ent->baseline.frame, cl.time);

	i = (bits & U_COLORMAP) ? 
		MSG_ReadByte() : ent->baseline.colormap;

	if (!i)
		ent->common.colormap = NULL;
	else {
		if (i > cl.maxclients)
			Sys_Error ("i >= cl.maxclients");
		ent->common.colormap = &cl.scores[i - 1].colormap;
	}

	skin = (bits & U_SKIN) ? 
		MSG_ReadByte() : ent->baseline.skin;

	if (skin != ent->common.skinnum)
		ent->common.skinnum = skin;

	ent->effects = (bits & U_EFFECTS) ? 
		MSG_ReadByte() : ent->baseline.effects;

	// shift the known values for interpolation
	origin[0] = (bits & U_ORIGIN1) ? 
		MSG_ReadCoord() : ent->baseline.origin[0];
	angles[0] = (bits & U_ANGLE1) ? 
		MSG_ReadAngle() : ent->baseline.angles[0];
	origin[1] = (bits & U_ORIGIN2) ? 
		MSG_ReadCoord() : ent->baseline.origin[1];
	angles[1] = (bits & U_ANGLE2) ? 
		MSG_ReadAngle() : ent->baseline.angles[1];
	origin[2] = (bits & U_ORIGIN3) ? 
		MSG_ReadCoord() : ent->baseline.origin[2];
	angles[2] = (bits & U_ANGLE3) ? 
		MSG_ReadAngle() : ent->baseline.angles[2];

	if (bits & U_NOLERP)
		ent->forcelink = true;

	CL_Update_OriginAngles(ent, origin, angles, cl.mtime[1]);

	if (forcelink) {
		// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->common.origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->common.angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void
CL_ParseBaseline (entity_t *ent)
{
	int         i;

	ent->baseline.modelindex = MSG_ReadByte ();
	ent->baseline.frame = MSG_ReadByte ();
	ent->baseline.colormap = MSG_ReadByte ();
	ent->baseline.skin = MSG_ReadByte ();
	for (i = 0; i < 3; i++) {
		ent->baseline.origin[i] = MSG_ReadCoord ();
		ent->baseline.angles[i] = MSG_ReadAngle ();
	}
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void
CL_ParseClientdata (int bits)
{
	int			i, j;

	cl.viewheight = (bits & SU_VIEWHEIGHT) ? 
		MSG_ReadChar() : DEFAULT_VIEWHEIGHT;

	cl.idealpitch = (bits & SU_IDEALPITCH) ?
		MSG_ReadChar() : 0;

	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);

	for (i = 0; i < 3; i++)
	{
		cl.punchangle[i] = (bits & (SU_PUNCH1 << i)) ?
			MSG_ReadChar() : 0;
		cl.mvelocity[0][i] = (bits & (SU_VELOCITY1 << i)) ?
			MSG_ReadChar() * 16 : 0;
	}

// [always sent]    if (bits & SU_ITEMS)
	i = MSG_ReadLong ();

	if (cl.items != i)
	{
		// set flash times
		for (j = 0; j < 32; j++)
			if ((i & (1 << j)) && !(cl.items & (1 << j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ?
		MSG_ReadByte() : 0;

	i = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;

	if (cl.stats[STAT_ARMOR] != i)
		cl.stats[STAT_ARMOR] = i;

	i = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;

	if (cl.stats[STAT_WEAPON] != i)
		cl.stats[STAT_WEAPON] = i;

	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
		cl.stats[STAT_HEALTH] = i;

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
		cl.stats[STAT_AMMO] = i;

	for (i = 0; i < 4; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS + i] != j)
			cl.stats[STAT_SHELLS + i] = j;
	}

	i = MSG_ReadByte ();

	if (game_mission->ivalue)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1 << i))
			cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
			cl.stats[STAT_ACTIVEWEAPON] = i;
	}
}

/*
=====================
CL_NewTranslation
=====================
*/
void
CL_NewTranslation (int slot)
{
	Uint8	color;

	color = cl.scores[slot].colors & 0xF0;
	if (color < 128)
		color += 15;
	VectorCopy4 (d_8tofloattable[color], cl.scores[slot].colormap.top);

	color = (cl.scores[slot].colors & 0x0F) << 4;
	if (color < 128)
		color += 15;
	VectorCopy4 (d_8tofloattable[color], cl.scores[slot].colormap.bottom);
}

/*
=====================
CL_ParseStatic
=====================
*/
void
CL_ParseStatic (void)
{
	entity_t   *ent;

	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics++];
	CL_ParseBaseline (ent);
	if (!ent->baseline.modelindex) {
		cl.num_statics--;
		return;
	}

// copy it to the current state
	ent->common.model = cl.model_precache[ent->baseline.modelindex];
	CL_Update_Frame(ent, ent->baseline.frame, cl.mtime[1]);
	ent->common.colormap = NULL;
	ent->common.skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->common.time_left = 0;
	VectorClear (ent->common.last_light);

	CL_Update_OriginAngles(ent, ent->baseline.origin, ent->baseline.angles, cl.mtime[1]);
}

/*
===================
CL_ParseStaticSound
===================
*/
void
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

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x)											\
	if (cl_shownet->ivalue==2)								\
		Com_Printf ("%3i:%s\n", msg_readcount-1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void
CL_ParseServerMessage (void)
{
	int         cmd;
	int         i;

//
// if recording demos, copy the message out
//
	if (cl_shownet->ivalue == 1)
		Com_Printf ("%i ", net_message.cursize);
	else if (cl_shownet->ivalue == 2)
		Com_Printf ("------------------\n");

	// unless the server says otherwise
	cl.onground = false;

//
// parse the message
//
	MSG_BeginReading ();

	while (1) {
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1) {
			SHOWNET ("END OF MESSAGE");
			return;						// end of message
		}
		// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128) {
			SHOWNET ("fast update");
			CL_ParseUpdate (cmd & 127);
			continue;
		}

		SHOWNET (svc_strings[cmd]);

		// other commands
		switch (cmd) {
			default:
				Host_Error
					("CL_ParseServerMessage: Illegible server message\n");
				break;

			case svc_nop:
//				Com_Printf ("svc_nop\n");
				break;

			case svc_time:
				cl.mtime[1] = cl.mtime[0];
				cl.mtime[0] = MSG_ReadFloat ();
				break;

			case svc_clientdata:
				i = MSG_ReadShort ();
				CL_ParseClientdata (i);
				break;

			case svc_version:
				i = MSG_ReadLong ();
				if (i != PROTOCOL_VERSION)
					Host_Error ("CL_ParseServerMessage: Server is protocol"
							" %i instead of %i\n", i, PROTOCOL_VERSION);
				break;

			case svc_disconnect:
				Host_EndGame ("Server disconnected\n");

			case svc_print:
				Com_Printf ("%s", MSG_ReadString ());
				break;

			case svc_centerprint:
				SCR_CenterPrint (MSG_ReadString ());
				break;

			case svc_stufftext:
				Cbuf_AddText (MSG_ReadString ());
				break;

			case svc_damage:
				V_ParseDamage ();
				break;

			case svc_serverinfo:
				CL_ParseServerInfo ();
				// leave intermission full screen
				break;

			case svc_setangle:
				for (i = 0; i < 3; i++)
					cl.viewangles[i] = MSG_ReadAngle ();
				break;

			case svc_setview:
				cl.viewentity = MSG_ReadShort ();
				break;

			case svc_lightstyle:
				i = MSG_ReadByte ();
				if (i >= MAX_LIGHTSTYLES)
					Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
				strcpy (cl_lightstyle[i].map, MSG_ReadString ());
				cl_lightstyle[i].length = strlen (cl_lightstyle[i].map);
				break;

			case svc_sound:
				CL_ParseStartSoundPacket ();
				break;

			case svc_stopsound:
				i = MSG_ReadShort ();
				S_StopSound (i >> 3, i & 7);
				break;

			case svc_updatename:
				i = MSG_ReadByte ();
				if (i >= cl.maxclients)
					Host_Error ("CL_ParseServerMessage: svc_updatename >"
							" MAX_SCOREBOARD");
				strcpy (cl.scores[i].name, MSG_ReadString ());
				break;

			case svc_updatefrags:
				i = MSG_ReadByte ();
				if (i >= cl.maxclients)
					Host_Error ("CL_ParseServerMessage: svc_updatefrags >"
							" MAX_SCOREBOARD");
				cl.scores[i].frags = MSG_ReadShort ();
				break;

			case svc_updatecolors:
				i = MSG_ReadByte ();
				if (i >= cl.maxclients)
					Host_Error ("CL_ParseServerMessage: svc_updatecolors >"
							" MAX_SCOREBOARD");
				cl.scores[i].colors = MSG_ReadByte ();
				CL_NewTranslation (i);
				break;

			case svc_particle:
				R_ParseParticleEffect ();
				break;

			case svc_spawnbaseline:
				i = MSG_ReadShort ();
				// must use CL_EntityNum() to force cl.num_entities up
				CL_ParseBaseline (CL_EntityNum (i));
				break;
			case svc_spawnstatic:
				CL_ParseStatic ();
				break;
			case svc_temp_entity:
				CL_ParseTEnt ();
				break;

			case svc_setpause:
				{
					cl.paused = MSG_ReadByte ();

					if (cl.paused)
						CDAudio_Pause ();
					else
						CDAudio_Resume ();
				}
				break;

			case svc_signonnum:
				i = MSG_ReadByte ();
				if (i <= cls.signon)
					Host_Error ("Received signon %i when at %i", i,
							cls.signon);
				cls.signon = i;
				CL_SignonReply ();
				break;

			case svc_killedmonster:
				cl.stats[STAT_MONSTERS]++;
				break;

			case svc_foundsecret:
				cl.stats[STAT_SECRETS]++;
				break;

			case svc_updatestat:
				i = MSG_ReadByte ();
				if (i < 0 || i >= MAX_CL_STATS)
					Sys_Error ("svc_updatestat: %i is invalid", i);
				cl.stats[i] = MSG_ReadLong ();
				break;

			case svc_spawnstaticsound:
				CL_ParseStaticSound ();
				break;

			case svc_cdtrack:
				cl.cdtrack = MSG_ReadByte ();
				cl.looptrack = MSG_ReadByte ();
				if ((cls.demoplayback || cls.demorecording)
					&& (cls.forcetrack != -1))
					CDAudio_Play ((Uint8) cls.forcetrack, true);
				else
					CDAudio_Play ((Uint8) cl.cdtrack, true);
				break;

			case svc_intermission:
				cl.intermission = 1;
				cl.completed_time = cl.time;
				break;

			case svc_finale:
				cl.intermission = 2;
				cl.completed_time = cl.time;
				SCR_CenterPrint (MSG_ReadString ());
				break;

			case svc_cutscene:
				cl.intermission = 3;
				cl.completed_time = cl.time;
				SCR_CenterPrint (MSG_ReadString ());
				break;

			case svc_sellscreen:
				Cmd_ExecuteString ("help", src_command);
				break;
		}
	}
}

void
CL_ParseEntityLump (char *entdata)
{
	char *data;
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

