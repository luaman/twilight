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

#include <ctype.h>
#include <SDL.h>

#include "quakedef.h"
#include "cdaudio.h"
#include "cvar.h"
#include "input.h"
#include "keys.h"
#include "menu.h"
#include "pmove.h"
#include "sbar.h"
#include "screen.h"
#include "sound.h"
#include "view.h"

#ifdef _WIN32
#include "winquake.h"
#include "winsock.h"
#else
#include <netinet/in.h>
#endif

#include "keys.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

qboolean    noclip_anglehack;			// remnant from old quake


cvar_t     *rcon_password;
cvar_t     *rcon_address;

cvar_t     *cl_timeout;
cvar_t     *cl_shownet;

cvar_t     *cl_sbar;
cvar_t     *cl_hudswap;
cvar_t     *cl_maxfps;

cvar_t     *lookspring;
cvar_t     *lookstrafe;
cvar_t     *sensitivity;

cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

cvar_t     *entlatency;
cvar_t     *cl_predict_players;
cvar_t     *cl_predict_players2;
cvar_t     *cl_solid_players;

cvar_t     *localid;

//
// info mirrors
//
cvar_t     *password;
cvar_t     *spectator;
cvar_t     *name;
cvar_t     *team;
cvar_t     *skin;
cvar_t     *topcolor;
cvar_t     *bottomcolor;
cvar_t     *rate;
cvar_t     *noaim;
cvar_t     *msg;

cvar_t     *host_speeds;
cvar_t     *show_fps;


extern cvar_t *cl_hightrack;
extern cvar_t *qport;


static qboolean allowremotecmd = true;


client_static_t cls;
client_state_t cl;

entity_state_t cl_baselines[MAX_EDICTS];
efrag_t     cl_efrags[MAX_EFRAGS];
entity_t    cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t    cl_dlights[MAX_DLIGHTS];

// refresh list
// this is double buffered so the last frame
// can be scanned for oldorigins of trailing objects
int         cl_numvisedicts, cl_oldnumvisedicts;
entity_t   *cl_visedicts, *cl_oldvisedicts;
entity_t    cl_visedicts_list[2][MAX_VISEDICTS];

double      connect_time = -1;			// for connection retransmits

qboolean    host_initialized;			// true if into command execution
qboolean    nomaster;

double      host_frametime;
double      realtime;					// without any filtering or bounding
double      oldrealtime;				// last frame run
int         host_framecount;

int         host_hunklevel;

byte       *host_basepal;
byte       *host_colormap;

netadr_t    master_adr;					// address of the master server

int         fps_count;

jmp_buf     host_abort;

void        Master_Connect_f (void);

float       server_version = 0;			// version of server we connected to

char        emodel_name[] =
	{ 'e' ^ 0xff, 'm' ^ 0xff, 'o' ^ 0xff, 'd' ^ 0xff, 'e' ^ 0xff, 'l' ^ 0xff,
	0
};
char        pmodel_name[] =
	{ 'p' ^ 0xff, 'm' ^ 0xff, 'o' ^ 0xff, 'd' ^ 0xff, 'e' ^ 0xff, 'l' ^ 0xff,
	0
};
char        prespawn_name[] =
	{ 'p' ^ 0xff, 'r' ^ 0xff, 'e' ^ 0xff, 's' ^ 0xff, 'p' ^ 0xff, 'a' ^ 0xff,
	'w' ^ 0xff, 'n' ^ 0xff,
	' ' ^ 0xff, '%' ^ 0xff, 'i' ^ 0xff, ' ' ^ 0xff, '0' ^ 0xff, ' ' ^ 0xff,
	'%' ^ 0xff, 'i' ^ 0xff, 0
};
char        modellist_name[] =
	{ 'm' ^ 0xff, 'o' ^ 0xff, 'd' ^ 0xff, 'e' ^ 0xff, 'l' ^ 0xff, 'l' ^ 0xff,
	'i' ^ 0xff, 's' ^ 0xff, 't' ^ 0xff,
	' ' ^ 0xff, '%' ^ 0xff, 'i' ^ 0xff, ' ' ^ 0xff, '%' ^ 0xff, 'i' ^ 0xff, 0
};
char        soundlist_name[] =
	{ 's' ^ 0xff, 'o' ^ 0xff, 'u' ^ 0xff, 'n' ^ 0xff, 'd' ^ 0xff, 'l' ^ 0xff,
	'i' ^ 0xff, 's' ^ 0xff, 't' ^ 0xff,
	' ' ^ 0xff, '%' ^ 0xff, 'i' ^ 0xff, ' ' ^ 0xff, '%' ^ 0xff, 'i' ^ 0xff, 0
};

/*
==================
CL_Quit_f
==================
*/
void
CL_Quit_f (void)
{
	CL_Disconnect ();
	Sys_Quit ();
}

/*
=======================
CL_Version_f
======================
*/
void
CL_Version_f (void)
{
	Con_Printf ("Version %s\n", VERSION);
	Con_Printf ("Exe: " __TIME__ " " __DATE__ "\n");
}


/*
=======================
CL_SendConnectPacket

called by CL_Connect_f and CL_CheckResend
======================
*/
void
CL_SendConnectPacket (void)
{
	netadr_t    adr;
	char        data[2048];
	double      t1, t2;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
//       Now, adds lookup time to the connect time.
//       Should I add it to realtime instead?!?!

	if (cls.state != ca_disconnected)
		return;

	t1 = Sys_DoubleTime ();

	if (!NET_StringToAdr (cls.servername, &adr)) {
		Con_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (!NET_IsClientLegal (&adr)) {
		Con_Printf ("Illegal server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	cls.qport = qport->value;

	Info_SetValueForStarKey (cls.userinfo, "*ip", NET_AdrToString (adr),
							 MAX_INFO_STRING);

//  Con_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cconnect %i %i %i \"%s\"\n",
			  255, 255, 255, 255, PROTOCOL_VERSION, cls.qport, cls.challenge,
			  cls.userinfo);
	NET_SendPacket (Q_strlen (data), data, adr);
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out

=================
*/
void
CL_CheckForResend (void)
{
	netadr_t    adr;
	char        data[2048];
	double      t1, t2;

	if (connect_time == -1)
		return;
	if (cls.state != ca_disconnected)
		return;
	if (connect_time && realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Con_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}
	if (!NET_IsClientLegal (&adr)) {
		Con_Printf ("Illegal server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = realtime + t2 - t1;	// for retransmit requests

	Con_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cgetchallenge\n", 255, 255, 255,
			  255);
	NET_SendPacket (Q_strlen (data), data, adr);
}

void
CL_BeginServerConnect (void)
{
	connect_time = 0;
	CL_CheckForResend ();
}

/*
================
CL_Connect_f

================
*/
void
CL_Connect_f (void)
{
	char       *server;

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect ();

	Q_strncpy (cls.servername, server, sizeof (cls.servername) - 1);
	CL_BeginServerConnect ();
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void
CL_Rcon_f (void)
{
	char        message[1024];
	int         i;
	netadr_t    to;

	if (!rcon_password->string) {
		Con_Printf ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = 255;
	message[1] = 255;
	message[2] = 255;
	message[3] = 255;
	message[4] = 0;

	Q_strcat (message, "rcon ");

	Q_strcat (message, rcon_password->string);
	Q_strcat (message, " ");

	for (i = 1; i < Cmd_Argc (); i++) {
		Q_strcat (message, Cmd_Argv (i));
		Q_strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!Q_strlen (rcon_address->string)) {
			Con_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
	}

	NET_SendPacket (Q_strlen (message) + 1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void
CL_ClearState (void)
{
	int         i;

	S_StopAllSounds (true);

	Con_DPrintf ("Clearing memory\n");
	D_FlushCaches ();
	Mod_ClearAll ();
	if (host_hunklevel)					// FIXME: check this...
		Hunk_FreeToLowMark (host_hunklevel);

	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	SZ_Clear (&cls.netchan.message);

// clear other arrays   
	memset (cl_efrags, 0, sizeof (cl_efrags));
	memset (cl_dlights, 0, sizeof (cl_dlights));
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));

//
// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	cl.free_efrags[i].entnext = NULL;
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
	byte        final[10];

	connect_time = -1;

	SDL_WM_SetCaption ("Twilight QWCL: disconnected", "Twilight");

// stop sounds (especially looping!)
	S_StopAllSounds (true);

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state != ca_disconnected) {
		if (cls.demorecording)
			CL_Stop_f ();

		final[0] = clc_stringcmd;
		Q_strcpy (final + 1, "drop");
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);

		cls.state = ca_disconnected;

		cls.demoplayback = cls.demorecording = cls.timedemo = false;
	}
	Cam_Reset ();

	if (cls.download) {
		fclose (cls.download);
		cls.download = NULL;
	}

	CL_StopUpload ();

}

void
CL_Disconnect_f (void)
{
	CL_Disconnect ();
}

/*
====================
CL_User_f

user <name or userid>

Dump userdata / masterdata for a user
====================
*/
void
CL_User_f (void)
{
	int         uid;
	int         i;

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: user <username / userid>\n");
		return;
	}

	uid = atoi (Cmd_Argv (1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid
			|| !Q_strcmp (cl.players[i].name, Cmd_Argv (1))) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Con_Printf ("User not in server.\n");
}

/*
====================
CL_Users_f

Dump userids for all current players
====================
*/
void
CL_Users_f (void)
{
	int         i;
	int         c;

	c = 0;
	Con_Printf ("userid frags name\n");
	Con_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Con_Printf ("%6i %4i %s\n", cl.players[i].userid,
						cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Con_Printf ("%i total users\n", c);
}

void
CL_Color_f (void)
{
	// just for quake compatability...
	int         top, bottom;
	char        num[16];

	if (Cmd_Argc () == 1) {
		Con_Printf ("\"color\" is \"%s %s\"\n",
					Info_ValueForKey (cls.userinfo, "topcolor"),
					Info_ValueForKey (cls.userinfo, "bottomcolor"));
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		top = bottom = atoi (Cmd_Argv (1));
	else {
		top = atoi (Cmd_Argv (1));
		bottom = atoi (Cmd_Argv (2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	snprintf (num, sizeof (num), "%i", top);
	Cvar_Set (topcolor, num);
	snprintf (num, sizeof (num), "%i", bottom);
	Cvar_Set (bottomcolor, num);
}

/*
==================
CL_FullServerinfo_f

Sent by server when serverinfo changes
==================
*/
void
CL_FullServerinfo_f (void)
{
	char       *p;
	float       v;

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	Q_strcpy (cl.serverinfo, Cmd_Argv (1));

	if ((p = Info_ValueForKey (cl.serverinfo, "*vesion")) && *p) {
		v = Q_atof (p);
		if (v) {
			if (!server_version)
				Con_Printf ("Version %1.2f Server\n", v);
			server_version = v;
		}
	}
}

/*
==================
CL_FullInfo_f

Allow clients to change userinfo
==================
Casey was here :)
*/
void
CL_FullInfo_f (void)
{
	char        key[512];
	char        value[512];
	char       *o;
	char       *s;

	if (Cmd_Argc () != 2) {
		Con_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv (1);
	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s) {
			Con_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (!Q_strcasecmp (key, pmodel_name)
			|| !Q_strcasecmp (key, emodel_name))
			continue;

		Info_SetValueForKey (cls.userinfo, key, value, MAX_INFO_STRING);
	}
}

/*
==================
CL_SetInfo_f

Allow clients to change userinfo
==================
*/
void
CL_SetInfo_f (void)
{
	if (Cmd_Argc () == 1) {
		Info_Print (cls.userinfo);
		return;
	}
	if (Cmd_Argc () != 3) {
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (!Q_strcasecmp (Cmd_Argv (1), pmodel_name) ||
		!Q_strcmp (Cmd_Argv (1), emodel_name))
		return;

	Info_SetValueForKey (cls.userinfo, Cmd_Argv (1), Cmd_Argv (2),
						 MAX_INFO_STRING);
	if (cls.state >= ca_connected)
		Cmd_ForwardToServer ();
}

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void
CL_Packet_f (void)
{
	char        send[2048];
	int         i, l;
	char       *in, *out;
	netadr_t    adr;

	if (Cmd_Argc () != 3) {
		Con_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv (1), &adr)) {
		Con_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv (2);
	out = send + 4;
	send[0] = send[1] = send[2] = send[3] = 0xff;

	l = Q_strlen (in);
	for (i = 0; i < l; i++) {
		if (in[i] == '\\' && in[i + 1] == 'n') {
			*out++ = '\n';
			i++;
		} else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (out - send, send, adr);
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

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
//          Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	snprintf (str, sizeof (str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}


/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void
CL_Changing_f (void)
{
	if (cls.download)					// don't change when downloading
		return;

	S_StopAllSounds (true);
	cl.intermission = 0;
	cls.state = ca_connected;			// not active anymore, but not
	// disconnected
	Con_Printf ("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void
CL_Reconnect_f (void)
{
	if (cls.download)					// don't change when downloading
		return;

	S_StopAllSounds (true);

	if (cls.state == ca_connected) {
		Con_Printf ("reconnecting...\n");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) {
		Con_Printf ("No server to reconnect to...\n");
		return;
	}

	CL_Disconnect ();
	CL_BeginServerConnect ();
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void
CL_ConnectionlessPacket (void)
{
	char       *s;
	int         c;

	MSG_BeginReading ();
	MSG_ReadLong ();					// skip the -1

	c = MSG_ReadByte ();
	if (!cls.demoplayback)
		Con_Printf ("%s: ", NET_AdrToString (net_from));
//  Con_DPrintf ("%s", net_message.data + 5);
	if (c == S2C_CONNECTION) {
		Con_Printf ("connection\n");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Con_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (&cls.netchan, net_from, cls.qport);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		Con_Printf ("Connected.\n");
		allowremotecmd = false;			// localid required now for remote cmds
		return;
	}
	// remote command from gui front end
	if (c == A2C_CLIENT_COMMAND) {
		char        cmdtext[2048];

		Con_Printf ("client command\n");

		if ((*(unsigned *) net_from.ip != *(unsigned *) net_local_adr.ip
			 && *(unsigned *) net_from.ip != htonl (INADDR_LOOPBACK))) {
			Con_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		s = MSG_ReadString ();

		Q_strncpy (cmdtext, s, sizeof (cmdtext) - 1);
		cmdtext[sizeof (cmdtext) - 1] = 0;

		s = MSG_ReadString ();

		while (*s && isspace (*s))
			s++;
		while (*s && isspace (s[Q_strlen (s) - 1]))
			s[Q_strlen (s) - 1] = 0;

		if (!allowremotecmd
			&& (!*localid->string || Q_strcmp (localid->string, s))) {
			if (!*localid->string) {
				Con_Printf ("===========================\n");
				Con_Printf ("Command packet received from local host, but no "
							"localid has been set.  You may need to upgrade your server "
							"browser.\n");
				Con_Printf ("===========================\n");
				return;
			}
			Con_Printf ("===========================\n");
			Con_Printf
				("Invalid localid on command packet received from local host. "
				 "\n|%s| != |%s|\n"
				 "You may need to reload your server browser and QuakeWorld.\n",
				 s, localid->string);
			Con_Printf ("===========================\n");
			Cvar_Set (localid, "");
			return;
		}

		Cbuf_AddText (cmdtext);
		allowremotecmd = false;
		return;
	}
	// print command from somewhere
	if (c == A2C_PRINT) {
		Con_Printf ("print\n");

		s = MSG_ReadString ();
		Con_Print (s);
		return;
	}
	// ping from somewhere
	if (c == A2A_PING) {
		char        data[6];

		Con_Printf ("ping\n");

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		NET_SendPacket (6, &data, net_from);
		return;
	}

	if (c == S2C_CHALLENGE) {
		Con_Printf ("challenge\n");

		s = MSG_ReadString ();
		cls.challenge = atoi (s);
		CL_SendConnectPacket ();
		return;
	}
#if 0
	if (c == svc_disconnect) {
		Con_Printf ("disconnect\n");

		Host_EndGame ("Server disconnected");
		return;
	}
#endif

	Con_Printf ("unknown:  %c\n", c);
}


/*
=================
CL_ReadPackets
=================
*/
void
CL_ReadPackets (void)
{
//  while (NET_GetPacket ())
	while (CL_GetMessage ()) {
		// 
		// remote command packet
		// 
		if (*(int *) net_message.data == -1) {
			CL_ConnectionlessPacket ();
			continue;
		}

		if (net_message.cursize < 8) {
			Con_Printf ("%s: Runt packet\n", NET_AdrToString (net_from));
			continue;
		}
		// 
		// packet from server
		// 
		if (!cls.demoplayback &&
			!NET_CompareAdr (net_from, cls.netchan.remote_address)) {
			Con_DPrintf ("%s:sequenced packet without connection\n",
						 NET_AdrToString (net_from));
			continue;
		}
		if (!Netchan_Process (&cls.netchan))
			continue;					// wasn't accepted for some reason
		CL_ParseServerMessage ();

//      if (cls.demoplayback && cls.state >= ca_active && !CL_DemoBehind())
//          return;
	}

	// 
	// check timeout
	// 
	if (cls.state >= ca_connected
		&& realtime - cls.netchan.last_received > cl_timeout->value) {
		Con_Printf ("\nServer connection timed out.\n");
		CL_Disconnect ();
		return;
	}

}

//=============================================================================

/*
=====================
CL_Download_f
=====================
*/
void
CL_Download_f (void)
{
	char       *p, *q;

	if (cls.state == ca_disconnected) {
		Con_Printf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: download <datafile>\n");
		return;
	}

	snprintf (cls.downloadname, sizeof (cls.downloadname), "%s/%s", com_gamedir,
			  Cmd_Argv (1));

	p = cls.downloadname;
	for (;;) {
		if ((q = strchr (p, '/')) != NULL) {
			*q = 0;
			Sys_mkdir (cls.downloadname);
			*q = '/';
			p = q + 1;
		} else
			break;
	}

	Q_strcpy (cls.downloadtempname, cls.downloadname);
	cls.download = fopen (cls.downloadname, "wb");
	cls.downloadtype = dl_single;

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va ("download %s\n", Cmd_Argv (1)));
}

/*
=================
CL_Init_Cvars
=================
*/
void
CL_Init_Cvars (void)
{
	extern cvar_t	*baseskin;
	extern cvar_t	*noskins;

	// set for running times
	host_speeds = Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL);
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL);

	cl_warncmd = Cvar_Get ("cl_warncmd", "0", CVAR_NONE, NULL);

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", CVAR_NONE, NULL);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, NULL);
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, NULL);

	cl_movespeedkey = Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, NULL);

	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, NULL);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, NULL);

	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL);

	rcon_password = Cvar_Get ("rcon_password", "", CVAR_NONE, NULL);
	rcon_address = Cvar_Get ("rcon_address", "", CVAR_NONE, NULL);

	cl_timeout = Cvar_Get ("cl_timeout", "60", CVAR_NONE, NULL);

	// can be 0, 1, or 2
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL);

	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, NULL);
	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL);
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL);

	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, NULL);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE, NULL);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL);
	m_yaw = Cvar_Get ("m_yaw", "0.022", CVAR_NONE, NULL);
	m_forward = Cvar_Get ("m_forward", "1", CVAR_NONE, NULL);
	m_side = Cvar_Get ("m_side", "0.8", CVAR_NONE, NULL);

	entlatency = Cvar_Get ("entlatency", "20", CVAR_NONE, NULL);
	cl_predict_players = Cvar_Get ("cl_predict_players", "1", CVAR_NONE, NULL);
	cl_predict_players2 = Cvar_Get ("cl_predict_players2", "1", CVAR_NONE, NULL);
	cl_solid_players = Cvar_Get ("cl_solid_players", "1", CVAR_NONE, NULL);

	localid = Cvar_Get ("localid", "", CVAR_NONE, NULL);

	baseskin = Cvar_Get ("baseskin", "base", CVAR_NONE, NULL);
	noskins = Cvar_Get ("noskins", "0", CVAR_NONE, NULL);

	// 
	// info mirrors
	// 
	password = Cvar_Get ("password", "", CVAR_USERINFO, NULL);
	spectator = Cvar_Get ("spectator", "", CVAR_USERINFO, NULL);
	name = Cvar_Get ("name", "unnamed", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	team = Cvar_Get ("team", "", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	skin = Cvar_Get ("skin", "", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	topcolor = Cvar_Get ("topcolor", "0", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	bottomcolor = Cvar_Get ("bottomcolor", "0", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	rate = Cvar_Get ("rate", "2500", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	noaim = Cvar_Get ("noaim", "0", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
	msg = Cvar_Get ("msg", "1", CVAR_ARCHIVE|CVAR_USERINFO, NULL);
}

/*
=================
CL_Init
=================
*/
void
CL_Init (void)
{
	char			st[80];

	cls.state = ca_disconnected;

	Info_SetValueForKey (cls.userinfo, "name", "unnamed", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "topcolor", "0", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "bottomcolor", "0", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "rate", "2500", MAX_INFO_STRING);
	Info_SetValueForKey (cls.userinfo, "msg", "1", MAX_INFO_STRING);
	snprintf (st, sizeof (st), "%s-%04d", VERSION, build_number ());
	Info_SetValueForStarKey (cls.userinfo, "*ver", st, MAX_INFO_STRING);

	CL_Input_Init_Cvars ();			// initialize all cl_input related cvars
	CL_Input_Init ();				// setup input system, add related commands

	CL_InitTEnts ();
	CL_InitPrediction ();
	CL_InitCam ();
	Pmove_Init ();

	//
	// register our commands
	//
	Cmd_AddCommand ("version", CL_Version_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("rerecord", CL_ReRecord_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("user", CL_User_f);
	Cmd_AddCommand ("users", CL_Users_f);

	Cmd_AddCommand ("setinfo", CL_SetInfo_f);
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f);
	Cmd_AddCommand ("fullserverinfo", CL_FullServerinfo_f);

	Cmd_AddCommand ("color", CL_Color_f);
	Cmd_AddCommand ("download", CL_Download_f);

	Cmd_AddCommand ("nextul", CL_NextUpload);
	Cmd_AddCommand ("stopul", CL_StopUpload);

	//
	// forward to server commands
	//
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("pause", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("serverinfo", NULL);
}


/*
================
Host_EndGame

Call this to drop to a console without exiting the qwcl
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
	Con_Printf ("\n===========================\n");
	Con_Printf ("Host_EndGame: %s\n", string);
	Con_Printf ("===========================\n\n");

	CL_Disconnect ();

	longjmp (host_abort, 1);
}

/*
================
Host_Error

This shuts down the client and exits qwcl
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

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n", string);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

// FIXME
	Sys_Error ("Host_Error: %s\n", string);
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void
Host_WriteConfiguration (void)
{
	FILE       *f;

	if (host_initialized) {
		f = fopen (va ("%s/config.cfg", com_gamedir), "w");
		if (!f) {
			Con_Printf ("Couldn't write config.cfg.\n");
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVars (f);

		fclose (f);
	}
}


//============================================================================

#if 0
/*
==================
Host_SimulationTime

This determines if enough time has passed to run a simulation frame
==================
*/
qboolean
Host_SimulationTime (float time)
{
	float       fps;

	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cl_maxfps.value)
		fps = max (30.0, min (cl_maxfps.value, 72.0));
	else
		fps = max (30.0, min (rate.value / 80.0, 72.0));

	if (!cls.timedemo && (realtime + time) - oldrealtime < 1.0 / fps)
		return false;					// framerate is too high
	return true;
}
#endif


/*
==================
Host_Frame

Runs all active servers
==================
*/
int         nopacketcount;
void
Host_Frame (float time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int         pass1, pass2, pass3;
	float       fps;

	if (setjmp (host_abort))
		return;							// something bad happened, or the
	// server disconnected

	// decide the simulation time
	realtime += time;
	if (oldrealtime > realtime)
		oldrealtime = 0;

	if (cl_maxfps->value)
		fps = max (30.0, min (cl_maxfps->value, 72.0));
	else
		fps = max (30.0, min (rate->value / 80.0, 72.0));

	if (!cls.timedemo && realtime - oldrealtime < 1.0 / fps)
		return;							// framerate is too high

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
	if (host_frametime > 0.2)
		host_frametime = 0.2;

	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// fetch results from server
	CL_ReadPackets ();

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected) {
		CL_CheckForResend ();
	} else
		CL_SendCmd ();

	// Set up prediction for other players
	CL_SetUpPlayerPrediction (false);

	// do client side motion prediction
	CL_PredictMove ();

	// Set up prediction for other players
	CL_SetUpPlayerPrediction (true);

	// build a refresh entity list
	CL_EmitEntities ();

	// update video
	if (host_speeds->value)
		time1 = Sys_DoubleTime ();

	SCR_UpdateScreen ();

	if (host_speeds->value)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	} else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update ();

	if (host_speeds->value) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	host_framecount++;
	fps_count++;
}

static void
simple_crypt (char *buf, int len)
{
	while (len--)
		*buf++ ^= 0xff;
}

void
Host_FixupModelNames (void)
{
	simple_crypt (emodel_name, sizeof (emodel_name) - 1);
	simple_crypt (pmodel_name, sizeof (pmodel_name) - 1);
	simple_crypt (prespawn_name, sizeof (prespawn_name) - 1);
	simple_crypt (modellist_name, sizeof (modellist_name) - 1);
	simple_crypt (soundlist_name, sizeof (soundlist_name) - 1);
}

//============================================================================

/*
====================
Host_Init
====================
*/
void
Host_Init (void)
{
	Memory_Init ();
	Cvar_Init ();		// add all cvar related manipulation commands and set developer cvar
	Cbuf_Init ();		// initialize cmd_text buffer
	Cmd_Init ();		// setup the basic commands we need for the system

	// execute +set as early as possible
	Cmd_StuffCmds_f ();
	Cbuf_Execute_Sets ();

	Con_Init_Cvars ();				// initialize all console related cvars
	Key_Init_Cvars ();				// initialize all key related cvars
	Mod_Init_Cvars();				// initialize all model related cvars
	Netchan_Init_Cvars ();			// initialize all netchan related cvars
	SCR_Init_Cvars ();				// initialize all screen(?) related cvars
	VID_Init_Cvars();				// initialize all video related cvars
	V_Init_Cvars();					// initialize all view related cvars
	M_Init_Cvars ();				// initialize all menu related cvars
	R_Init_Cvars ();				// initialize all rendering system related cvars
	Sbar_Init_Cvars ();				// initialize all statusbar related cvars
	CL_Init_Cvars ();				// initialize all cl_* related cvars
	S_Init_Cvars ();				// initialize all sound system related cvars
	IN_Init_Cvars ();				// initialize all input related cvars

	COM_Init ();					// setup and initialize filesystem, endianess, add related commands
	COM_Init_Cvars ();				// initialize basic cvars

	Host_FixupModelNames ();		// fix model names (how?)
	Mod_Init ();					// setup models, add related commands

	V_Init ();						// setup view, add related commands

	NET_Init (PORT_CLIENT);			// setup net sockets and identify host
	Netchan_Init ();				// setup netchan

	W_LoadWadFile ("gfx.wad");
	Key_Init ();					// setup keysym structures, add related commands
	Con_Init ();					// setup and initialize console, add related commands
	M_Init ();						// setup menu, add related commands

	R_InitTextures ();				// setup texture system defaults

//  Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf ("%4.1f megs RAM used.\n", sys_memsize / (1024 * 1024.0));

	host_basepal = (byte *) COM_LoadHunkFile ("gfx/palette.lmp");
	if (!host_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	host_colormap = (byte *) COM_LoadHunkFile ("gfx/colormap.lmp");
	if (!host_colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");

	VID_Init (host_basepal);
	Draw_Init_Cvars ();				// initialize all draw system related cvars
	Draw_Init ();					// setup draw system, add related commands

	SCR_Init ();					// setup and initialize screen(?), add related commands

	R_Init ();						// setup rendering system, add related commands

	S_Init ();						// setup sound system, add related commands

	cls.state = ca_disconnected;
	CDAudio_Init_Cvars ();			// initialize all cdaudio related cvars
	CDAudio_Init ();				// setup cdaudio system, add related commands

	Sbar_Init ();					// setup statusbar, add related commands

	CL_Init ();						// setup client, add related commands

	IN_Init ();						// setup input

	Cbuf_InsertText ("exec quake.rc\n");
	Cbuf_AddText
		("echo Type connect <internet address> or use GameSpy to connect to a game.\n");
	Cbuf_AddText ("cl_warncmd 1\n");

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	Con_Printf ("\nClient Version %4.2f (Build %04d)\n\n", VERSION,
				build_number ());

	Con_Printf ("������� QuakeWorld Initialized �������\n");
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

	Host_WriteConfiguration ();

	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	if (host_basepal)
		VID_Shutdown ();
}

