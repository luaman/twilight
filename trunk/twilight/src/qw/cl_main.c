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
#include <stdlib.h>	/* for free() */
#include <setjmp.h>  // FIXME: REMOVE THIS EVIL SHIT!

#include "SDL.h"

#include "quakedef.h"
#include "cdaudio.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "model.h"
#include "host.h"
#include "input.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "pmove.h"
#include "sbar.h"
#include "screen.h"
#include "sound.h"
#include "strlib.h"
#include "view.h"
#include "sys.h"
#include "gl_textures.h"
#include "keys.h"
#include "teamplay.h"
#include "surface.h"
#include "common.h"
#include "cpu.h"

void        Cmd_ForwardToServer (void);

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.


cvar_t		*rcon_password;
cvar_t		*rcon_address;

cvar_t		*cl_timeout;
cvar_t		*cl_shownet;

cvar_t		*cl_hudswap;
cvar_t		*cl_maxfps;
cvar_t		*cl_mapname;

cvar_t		*cl_predict_players;
cvar_t		*cl_solid_players;

cvar_t		*localid;

//
// info mirrors
//
cvar_t		*password;
cvar_t		*spectator;
cvar_t		*name;
cvar_t		*team;
cvar_t		*skin;
cvar_t		*topcolor;
cvar_t		*bottomcolor;
cvar_t		*rate;
cvar_t		*noaim;
cvar_t		*msg;

cvar_t		*host_speeds;
cvar_t		*show_fps;


extern cvar_t *cl_hightrack;
extern cvar_t *qport;


static qboolean allowremotecmd = true;


client_static_t cls;
client_state_t cl;

entity_state_t	cl_baselines[MAX_EDICTS];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

double		connect_time = -1;			// for connection retransmits

qboolean	host_initialized;			// true if into command execution
qboolean	nomaster;

double		host_frametime;
double		oldrealtime;				// last frame run
int			host_framecount;

int			host_hunklevel;

Uint8		*host_basepal;
Uint8		*host_colormap;

netadr_t	master_adr;					// address of the master server

int			fps_count = 0;

jmp_buf     host_abort;

void		Master_Connect_f (void);

float		server_version = 0;			// version of server we connected to

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
	Com_Printf ("Version %s\n", VERSION);
	Com_Printf ("Build: %04d\n", build_number());
	Com_Printf ("Exe: " __TIME__ " " __DATE__ "\n");
}


/*
=======================
CL_SendConnectPacket

called when we get a challenge from the server
======================
*/
void
CL_SendConnectPacket (int challenge)
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
		Com_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	cls.qport = qport->ivalue;

	Info_SetValueForStarKey (cls.userinfo, "*ip", NET_AdrToString (adr),
							 MAX_INFO_STRING);

//  Com_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cconnect %i %i %i \"%s\"\n",
			  255, 255, 255, 255, PROTOCOL_VERSION, cls.qport, challenge,
			  cls.userinfo);
	NET_SendPacket (NS_CLIENT, strlen (data), data, adr);
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
	if (connect_time && cls.realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address\n");
		connect_time = -1;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (27500);
	t2 = Sys_DoubleTime ();

	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);
	snprintf (data, sizeof (data), "%c%c%c%cgetchallenge\n", 255, 255, 255,
			  255);
	NET_SendPacket (NS_CLIENT, strlen (data), data, adr);
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
		Com_Printf ("usage: connect <server>\n");
		return;
	}

	server = Cmd_Argv (1);

	CL_Disconnect ();

	strncpy (cls.servername, server, sizeof (cls.servername) - 1);
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

	if (!rcon_password->svalue) {
		Com_Printf ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = 255;
	message[1] = 255;
	message[2] = 255;
	message[3] = 255;
	message[4] = 0;

	strcat (message, "rcon ");

	strcat (message, rcon_password->svalue);
	strcat (message, " ");

	for (i = 1; i < Cmd_Argc (); i++) {
		strcat (message, Cmd_Argv (i));
		strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!strlen (rcon_address->svalue)) {
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->svalue, &to);
	}

	NET_SendPacket (NS_CLIENT, strlen (message) + 1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void
CL_ClearState (void)
{
	S_StopAllSounds (true);

	Com_DPrintf ("Clearing memory\n");
	Mod_ClearAll ();

	CL_ClearTEnts ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	// We don't get this from the server, that'd take a new protocol
	cl.viewzoom = 1.0f;

	SZ_Clear (&cls.netchan.message);

// clear other arrays   
	memset (cl_dlights, 0, sizeof (cl_dlights));
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	memset (cl_baselines, 0, sizeof(cl_baselines));

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
	Uint8       final[10];

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
		strcpy (final + 1, "drop");
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
		Com_Printf ("Usage: user <username / userid>\n");
		return;
	}

	uid = Q_atoi (Cmd_Argv (1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid
			|| !strcmp (cl.players[i].name, Cmd_Argv (1))) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Com_Printf ("User not in server.\n");
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
	Com_Printf ("userid frags name\n");
	Com_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Com_Printf ("%6i %4i %s\n", cl.players[i].userid,
						cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Com_Printf ("%i total users\n", c);
}

void
CL_Color_f (void)
{
	// just for quake compatability...
	int         top, bottom;
	char        num[16];

	if (Cmd_Argc () == 1) {
		Com_Printf ("\"color\" is \"%s %s\"\n",
					Info_ValueForKey (cls.userinfo, "topcolor"),
					Info_ValueForKey (cls.userinfo, "bottomcolor"));
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
		Com_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	strcpy (cl.serverinfo, Cmd_Argv (1));

	if ((p = Info_ValueForKey (cl.serverinfo, "*vesion")) && *p) {
		v = Q_atof (p);
		if (v) {
			if (!server_version)
				Com_Printf ("Version %1.2f Server\n", v);
			server_version = v;
		}
	}

	CL_ProcessServerInfo ();
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
		Com_Printf ("fullinfo <complete info string>\n");
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
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (!strcasecmp (key, pmodel_name)
			|| !strcasecmp (key, emodel_name))
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
		Com_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (!strcasecmp (Cmd_Argv (1), pmodel_name) ||
		!strcmp (Cmd_Argv (1), emodel_name))
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
		Com_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv (1), &adr)) {
		Com_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv (2);
	out = send + 4;
	send[0] = send[1] = send[2] = send[3] = 0xff;

	l = strlen (in);
	for (i = 0; i < l; i++) {
		if (in[i] == '\\' && in[i + 1] == 'n') {
			*out++ = '\n';
			i++;
		} else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out - send, send, adr);
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
//          Com_Printf ("No demos listed with startdemos\n");
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
	Com_Printf ("\nChanging map...\n");
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
		Com_Printf ("reconnecting...\n");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) {
		Com_Printf ("No server to reconnect to...\n");
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
	char	*s, *cmd, data[6];
	Uint8	c;

	MSG_BeginReading ();
	MSG_ReadLong ();					// skip the -1

	c = MSG_ReadByte ();

	if (!cls.demoplayback)
		Com_Printf ("%s: ", NET_AdrToString (net_from));

	switch (c) {
		case S2C_CONNECTION:
			Com_Printf ("connection\n");
			if (cls.state >= ca_connected) {
				if (!cls.demoplayback)
					Com_Printf ("Dup connect received.  Ignored.\n");
				return;
			}
			Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
			MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, "new");
			cls.state = ca_connected;
			Com_Printf ("Connected.\n");
			allowremotecmd = false;		// localid required now for remote cmds
			return;
		case A2C_CLIENT_COMMAND: 		// remote command from gui front end

			Com_Printf ("client command\n");

			if (!NET_IsLocalAddress (net_from)) {
				Com_Printf ("Command packet from remote host. Ignored.\n");
				return;
			}

			cmd = strdup(MSG_ReadString ());

			s = MSG_ReadString ();

			// Strip off any whitespace.
			while (*s && isspace (*s))
				s++;
			while (*s && isspace (s[strlen (s) - 1]))
				s[strlen (s) - 1] = 0;

			if (!allowremotecmd && !*localid->svalue) {
				Com_Printf ("===========================\n");
				Com_Printf ("Command packet received but no localid has "
						"been set.  You may need to upgrade your server "
						"browser.\n");
				Com_Printf ("===========================\n");
				free (cmd);
				return;
			}

			if (!allowremotecmd && strcmp (localid->svalue, s)) {
				Com_Printf ("===========================\n");
				Com_Printf
					("Invalid localid on command packet: |%s| != |%s|\n",
					 s, localid->svalue);
				Com_Printf ("===========================\n");
				free (cmd);
				return;
			}

			Cbuf_AddText (cmd);
			allowremotecmd = false;
			free (cmd);
			return;
		case A2C_PRINT:				// print command from somewhere
			Com_Printf ("print\n");

			s = MSG_ReadString ();
			Con_Print (s);
			return;
		case A2A_PING:				// ping from somewhere
			Com_Printf ("ping\n");

			data[0] = 0xff;
			data[1] = 0xff;
			data[2] = 0xff;
			data[3] = 0xff;
			data[4] = A2A_ACK;
			data[5] = 0;

			NET_SendPacket (NS_CLIENT, 6, &data, net_from);
			return;
		case S2C_CHALLENGE:
			Com_Printf ("challenge\n");

			CL_SendConnectPacket (Q_atoi (MSG_ReadString ()));
			return;
		case svc_disconnect:
			if (cls.demoplayback)
				Host_EndGame ("End of demo");
			return;
	}

	Com_Printf ("unknown: %c\n", c);
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
			Com_Printf ("%s: Runt packet\n", NET_AdrToString (net_from));
			continue;
		}
		// 
		// packet from server
		// 
		if (!Netchan_Process (&cls.netchan))
			continue;					// wasn't accepted for some reason
		CL_ParseServerMessage ();
	}

	// 
	// check timeout
	// 
	if (cls.state >= ca_connected
		&& cls.realtime - cls.netchan.last_received > cl_timeout->fvalue) {
		Com_Printf ("\nServer connection timed out. (%f %f %f)\n", cls.realtime,
				cls.netchan.last_received, cl_timeout->fvalue);
		CL_Disconnect ();
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
		Com_Printf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: download <datafile>\n");
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

	strcpy (cls.downloadtempname, cls.downloadname);
	cls.download = fopen (cls.downloadname, "wb");
	cls.downloadtype = dl_single;

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va ("download %s\n", Cmd_Argv (1)));
}

void 
CL_WriteConfig_f (void)
{
	if (Cmd_Argc () != 2) {
		Com_Printf ("writeconfig <filename> : dump configuration to file\n");
		return;
	}

	Host_WriteConfiguration (Cmd_Argv(1));
}

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void
Cmd_ForwardToServer (void)
{
	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, Cmd_Argv (0));
	if (Cmd_Argc () > 1) {
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args ());
	}
}

// don't forward the first argument
void
Cmd_ForwardToServer_f (void)
{
	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (strcasecmp (Cmd_Argv (1), "snap") == 0) {
		Cbuf_InsertText ("snap\n");
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected

	if (Cmd_Argc () > 1) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args ());
	}
}

void
Cmd_Say_f (void)
{
	char	*s;

	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv (0));
		return;
	}

	if (cls.demoplayback)
		return;							// not really connected

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, Cmd_Argv (0));
	if (Cmd_Argc () > 1) {
		SZ_Print (&cls.netchan.message, " ");
		s = Team_ParseSay (Cmd_Args ());
		if (*s && *s < 32 && *s != 10) {
			// otherwise the server would eat leading characters
			// less than 32 or greater than 127
			SZ_Print (&cls.netchan.message, "\"");
			SZ_Print (&cls.netchan.message, s);
			SZ_Print (&cls.netchan.message, "\"");
		} else
			SZ_Print (&cls.netchan.message, s);
	}
}

/*
=================
CL_Init_Cvars
=================
*/
void
CL_Init_Cvars (void)
{
	extern cvar_t	*noskins;

	// set for running times
	host_speeds = Cvar_Get ("host_speeds", "0", CVAR_NONE, NULL);
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL);

	rcon_password = Cvar_Get ("rcon_password", "", CVAR_NONE, NULL);
	rcon_address = Cvar_Get ("rcon_address", "", CVAR_NONE, NULL);

	cl_timeout = Cvar_Get ("cl_timeout", "60", CVAR_NONE, NULL);

	// can be 0, 1, or 2
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL);

	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL);
	cl_maxfps = Cvar_Get ("cl_maxfps", "0", CVAR_ARCHIVE, NULL);

	cl_mapname = Cvar_Get ("cl_mapname", "", CVAR_ROM, NULL);

	cl_predict_players = Cvar_Get ("cl_predict_players", "1", CVAR_NONE, NULL);
	cl_solid_players = Cvar_Get ("cl_solid_players", "1", CVAR_NONE, NULL);

	localid = Cvar_Get ("localid", "", CVAR_NONE, NULL);

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

	CL_TEnts_Init ();
	CL_InitPrediction ();
	CL_InitCam ();
	CL_InitSkins ();
	Pmove_Init ();
	Team_Init ();

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

	Cmd_AddCommand ("writeconfig", CL_WriteConfig_f);

	//
	// forward to server commands
	//
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("pause", NULL);
	Cmd_AddCommand ("say", Cmd_Say_f);
	Cmd_AddCommand ("say_team", Cmd_Say_f);
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
	Com_Printf ("Host_EndGame: %s\n", string);

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
	Com_Printf ("Host_Error: %s\n", string);

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

// FIXME
	Sys_Error ("Host_Error: %s\n", string);
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
	if (host_initialized) {
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



//============================================================================


/*
==================
Host_Frame

Runs all active servers
==================
*/
int         nopacketcount;
void
Host_Frame (double time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	static int skipped = 0;
	int         pass1, pass2, pass3;
	float       fps;

	if (setjmp (host_abort))
		return;							// something bad happened, or the
	// server disconnected

	// decide the simulation time
	cls.realtime += time;
	if (oldrealtime > cls.realtime)
		oldrealtime = 0;

	if (cl_maxfps->fvalue)
		fps = cl_maxfps->fvalue;
	else
		fps = rate->fvalue / 80.0f;

	fps = bound (30.0f, fps, 72.0f);

	if (!cls.timedemo && ((cls.realtime - oldrealtime) < (1.0 / fps))) {
		if (skipped < 1)
			SDL_Delay(1);
		skipped++;
		return;							// framerate is too high
	}
	skipped = 0;

	host_frametime = cls.realtime - oldrealtime;
	oldrealtime = cls.realtime;
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

	if (cls.state == ca_active) {

		// Set players solid.
		CL_SetSolidPlayers ();

		// Predict ourself.
		CL_PredictMove ();

		// build a refresh entity list
		CL_EmitEntities ();
	}

	// update video
	if (host_speeds->ivalue)
		time1 = Sys_DoubleTime ();

	SCR_UpdateScreen ();

	if (host_speeds->ivalue)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	} else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

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

static void
simple_decrypt (char *buf, int len)
{
	while (len--)
		*buf++ ^= 0xff;
}

void
Host_FixupModelNames (void)
{
	simple_decrypt (emodel_name, sizeof (emodel_name) - 1);
	simple_decrypt (pmodel_name, sizeof (pmodel_name) - 1);
	simple_decrypt (prespawn_name, sizeof (prespawn_name) - 1);
	simple_decrypt (modellist_name, sizeof (modellist_name) - 1);
	simple_decrypt (soundlist_name, sizeof (soundlist_name) - 1);
}

//============================================================================

static void
Host_CvarUserinfo (cvar_t *var)
{
	if (var->flags & CVAR_USERINFO)
	{
		Info_SetValueForKey (cls.userinfo, var->name, var->svalue,
				MAX_INFO_STRING);
		if (cls.state >= ca_connected)
		{
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message,
					va ("setinfo \"%s\" \"%s\"\n", var->name, var->svalue));
		}
	}
}

/*
====================
Host_Init
====================
*/
void
Host_Init (void)
{
	Zone_Init ();
	Cvar_Init (&Host_CvarUserinfo);		// Cvar system
	Cbuf_Init ();						// Command buffer
	Cmd_Init (&Cmd_ForwardToServer_f);	// Command system
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

	COM_Init_Cvars ();				// basic cvars
	Con_Init_Cvars ();				// all console related cvars
	Key_Init_Cvars ();				// all key related cvars
	Surf_Init_Cvars();				// all model related cvars
	Netchan_Init_Cvars ();			// all netchan related cvars
	SCR_Init_Cvars ();				// all screen(?) related cvars
	VID_Init_Cvars();				// all video related cvars
	V_Init_Cvars();					// all view related cvars
	M_Init_Cvars ();				// all menu related cvars
	R_Init_Cvars ();				// all rendering system related cvars
	Sbar_Init_Cvars ();				// all statusbar related cvars
	CL_Init_Cvars ();				// all cl_* related cvars
	S_Init_Cvars ();				// all sound system related cvars

	COM_Init ();					// setup and initialize filesystem, endianess, add related commands

	Host_FixupModelNames ();		// fix model names (how?)
	Mod_Init ();					// setup models, add related commands

	V_Init ();						// setup view, add related commands

	NET_Init ();

	// setup net sockets and identify host
	NET_OpenSocket (NS_CLIENT, PORT_CLIENT);
//	NET_OpenSocket (NS_CLIENT, PORT_ANY);

	Netchan_Init ();				// setup netchan

	W_LoadWadFile ("gfx.wad");
	Key_Init ();					// setup keysym structures, add related commands
	Con_Init ();					// setup and initialize console, add related commands
	CPU_Init ();
	M_Init ();						// setup menu, add related commands

	Com_Printf ("Exe: "__TIME__" "__DATE__"\n");

	host_basepal = COM_LoadNamedFile ("gfx/palette.lmp", true);
	if (!host_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	host_colormap = COM_LoadNamedFile ("gfx/colormap.lmp", true);
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

	host_initialized = true;

	Com_Printf ("\nClient Version %s (Build %04d)\n\n", VERSION,
				build_number ());

	Com_Printf (" QuakeWorld Initialized \n");
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

	Host_WriteConfiguration ("config");

	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	if (host_basepal)
		VID_Shutdown ();
}

