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

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "host.h"
#include "pmove.h"
#include "sys.h"
#include "strlib.h"

static void CL_FinishTimeDemo (void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void
CL_StopPlayback (void)
{
	if (!ccls.demoplayback)
		return;

	fclose (ccls.demofile);
	ccls.demofile = NULL;
	ccls.state = ca_disconnected;
	ccls.demoplayback = 0;

	if (ccls.timedemo)
		CL_FinishTimeDemo ();

	memset (ccl.cshifts, 0, sizeof(ccl.cshifts));
	ccl.stats[STAT_ITEMS] = 0;
}

#define dem_cmd		0
#define dem_read	1
#define dem_set		2

/*
====================
CL_WriteDemoCmd

Writes the current user cmd
====================
*/
void
CL_WriteDemoCmd (usercmd_t *pcmd)
{
	float		fl, ang[3];
	Uint8		c;
	usercmd_t	cmd;

	fl = LittleFloat ((float) ccls.realtime);
	fwrite (&fl, sizeof (fl), 1, ccls.demofile);

	c = dem_cmd;
	fwrite (&c, sizeof (c), 1, ccls.demofile);

	// correct for byte order, bytes don't matter
	cmd = *pcmd;

	cmd.angles[0] = LittleFloat (cmd.angles[0]);
	cmd.angles[1] = LittleFloat (cmd.angles[1]);
	cmd.angles[2] = LittleFloat (cmd.angles[2]);
	cmd.forwardmove = LittleShort (cmd.forwardmove);
	cmd.sidemove = LittleShort (cmd.sidemove);
	cmd.upmove = LittleShort (cmd.upmove);

	fwrite (&cmd, sizeof (cmd), 1, ccls.demofile);

	ang[0] = LittleFloat (cl.viewangles[0]);
	ang[1] = LittleFloat (cl.viewangles[1]);
	ang[2] = LittleFloat (cl.viewangles[2]);

	fwrite (ang, 12, 1, ccls.demofile);

	fflush (ccls.demofile);
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
static void
CL_WriteDemoMessage (sizebuf_t *msg)
{
	int		len;
	float	fl;
	Uint8	c;

	if (!ccls.demorecording)
		return;

	fl = LittleFloat ((float) ccls.realtime);
	fwrite (&fl, sizeof (fl), 1, ccls.demofile);

	c = dem_read;
	fwrite (&c, sizeof (c), 1, ccls.demofile);

	len = LittleLong (msg->cursize);
	fwrite (&len, 4, 1, ccls.demofile);
	fwrite (msg->data, msg->cursize, 1, ccls.demofile);

	fflush (ccls.demofile);
}

/*
====================
CL_GetDemoMessage

  FIXME...
====================
*/
static qboolean
CL_GetDemoMessage (void)
{
	int			r, i, j;
	float		demotime;
	Uint8		c;
	usercmd_t	*pcmd;

	// read the time from the packet
	fread (&demotime, sizeof (demotime), 1, ccls.demofile);
	demotime = LittleFloat (demotime);

	// decide if it is time to grab the next message        
	if (ccls.timedemo) {
		if (ccls.td_lastframe < 0)
			ccls.td_lastframe = demotime;
		else if (demotime > ccls.td_lastframe) {
			ccls.td_lastframe = demotime;
			// rewind back to time
			fseek (ccls.demofile, ftell (ccls.demofile) - sizeof (demotime),
				   SEEK_SET);
			return 0;					// already read this frame's message
		}
		if (!ccls.td_starttime && ccls.state == ca_active) {
			ccls.td_starttime = Sys_DoubleTime ();
			ccls.td_startframe = host_framecount;
		}
		ccls.realtime = demotime;			// warp
	} else if (!cl.paused && ccls.state >= ca_onserver) {
		// always grab until fully connected
		if (ccls.realtime + 1.0 < demotime) {
			// too far back
			ccls.realtime = demotime - 1.0;
			// rewind back to time
			fseek (ccls.demofile, ftell (ccls.demofile) - sizeof (demotime),
				   SEEK_SET);
			return 0;
		} else if (ccls.realtime < demotime) {
			// rewind back to time
			fseek (ccls.demofile, ftell (ccls.demofile) - sizeof (demotime),
				   SEEK_SET);
			return 0;	// don't need another message yet
		}
	} else
		ccls.realtime = demotime;	// we're warping

	if (ccls.state < ca_demostart)
		Host_Error ("CL_GetDemoMessage: ccls.state != ca_active");

	// get the msg type
	fread (&c, sizeof (c), 1, ccls.demofile);

	switch (c) {
		case dem_cmd:
			// user sent input
			i = cls.netchan.outgoing_sequence & UPDATE_MASK;
			pcmd = &cl.frames[i].cmd;
			r = fread (pcmd, sizeof (*pcmd), 1, ccls.demofile);
			if (r != 1) {
				CL_StopPlayback ();
				return 0;
			}
			// byte order stuff
			for (j = 0; j < 3; j++)
				pcmd->angles[j] = LittleFloat (pcmd->angles[j]);
			pcmd->forwardmove = LittleShort (pcmd->forwardmove);
			pcmd->sidemove = LittleShort (pcmd->sidemove);
			pcmd->upmove = LittleShort (pcmd->upmove);
			cl.frames[i].senttime = demotime;
			cl.frames[i].receivedtime = -1;	// we haven't gotten a reply yet
			cls.netchan.outgoing_sequence++;

			r = fread (cl.viewangles, 12, 1, ccls.demofile);

			for (i = 0; i < 3; i++) {
				cl.viewangles[i] = LittleFloat (cl.viewangles[i]);
			}
			break;

		case dem_read:
			// get the next message
			fread (&net_message.cursize, 4, 1, ccls.demofile);
			net_message.cursize = LittleLong (net_message.cursize);
			// Com_Printf("read: %ld bytes\n", net_message.cursize);
			if (net_message.cursize > MAX_MSGLEN)
				Host_EndGame ("Demo message > MAX_MSGLEN");
			r = fread (net_message.data, net_message.cursize, 1, ccls.demofile);
			if (r != 1) {
				CL_StopPlayback ();
				return 0;
			}
			break;

		case dem_set:
			fread (&i, 4, 1, ccls.demofile);
			cls.netchan.outgoing_sequence = LittleLong (i);
			fread (&i, 4, 1, ccls.demofile);
			cls.netchan.incoming_sequence = LittleLong (i);
			break;

		default:
			Com_Printf ("Corrupted demo.\n");
			CL_StopPlayback ();
			return 0;
	}

	return 1;
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
qboolean
CL_GetMessage (void)
{
	if (ccls.demoplayback)
		return CL_GetDemoMessage ();

	if (!NET_GetPacket (NS_CLIENT))
		return false;

	CL_WriteDemoMessage (&net_message);

	return true;
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void
CL_Stop_f (void)
{
	if (!ccls.demorecording) {
		Com_Printf ("Not recording a demo.\n");
		return;
	}
	// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);	// -1 sequence means out of band
	MSG_WriteByte (&net_message, svc_disconnect);
	MSG_WriteString (&net_message, "EndOfDemo");
	CL_WriteDemoMessage (&net_message);

	// finish up
	fclose (ccls.demofile);
	ccls.demofile = NULL;
	ccls.demorecording = false;
	Com_Printf ("Completed demo\n");
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
static void
CL_WriteRecordDemoMessage (sizebuf_t *msg, int seq)
{
	int		len, i;
	float	fl;
	Uint8	c;

	if (!ccls.demorecording)
		return;

	fl = LittleFloat ((float) ccls.realtime);
	fwrite (&fl, sizeof (fl), 1, ccls.demofile);

	c = dem_read;
	fwrite (&c, sizeof (c), 1, ccls.demofile);

	len = LittleLong (msg->cursize + 8);
	fwrite (&len, 4, 1, ccls.demofile);

	i = LittleLong (seq);
	fwrite (&i, 4, 1, ccls.demofile);
	fwrite (&i, 4, 1, ccls.demofile);

	fwrite (msg->data, msg->cursize, 1, ccls.demofile);

	fflush (ccls.demofile);
}

static void
CL_WriteSetDemoMessage (void)
{
	int		len;
	float	fl;
	Uint8	c;

	if (!ccls.demorecording)
		return;

	fl = LittleFloat ((float) ccls.realtime);
	fwrite (&fl, sizeof (fl), 1, ccls.demofile);

	c = dem_set;
	fwrite (&c, sizeof (c), 1, ccls.demofile);

	len = LittleLong (cls.netchan.outgoing_sequence);
	fwrite (&len, 4, 1, ccls.demofile);
	len = LittleLong (cls.netchan.incoming_sequence);
	fwrite (&len, 4, 1, ccls.demofile);

	fflush (ccls.demofile);
}

/*
====================
CL_Record_f

record <demoname> <server>
====================
*/
void
CL_Record_f (void)
{
	int				c, n, i, j, seq = 1;
	char			name[MAX_OSPATH], buf_data[MAX_MSGLEN], *s;
	entity_t		*ent;
	sizebuf_t		buf;
	player_info_t	*player;
	user_info_t		*user;
	entity_state_t	*es, blankes;
	extern char		gamedirfile[];

	c = Cmd_Argc ();
	if (c != 2) {
		Com_Printf ("record <demoname>\n");
		return;
	}

	if (ccls.state != ca_active) {
		Com_Printf ("You must be connected to record.\n");
		return;
	}

	if (ccls.demorecording)
		CL_Stop_f ();

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, Cmd_Argv (1));

	// open the demo file
	COM_DefaultExtension (name, ".qwd");

	ccls.demofile = fopen (name, "wb");
	if (!ccls.demofile) {
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Com_Printf ("recording to %s.\n", name);
	ccls.demorecording = true;

	/*-------------------------------------------------*/
	// serverdata
	// send the info about the new client to all connected clients
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, cl.servercount);
	MSG_WriteString (&buf, gamedirfile);

	if (cl.spectator)
		MSG_WriteByte (&buf, ccl.player_num | 128);
	else
		MSG_WriteByte (&buf, ccl.player_num);

	// send full levelname
	MSG_WriteString (&buf, ccl.levelname);

	// send the movevars
	MSG_WriteFloat (&buf, movevars.gravity);
	MSG_WriteFloat (&buf, movevars.stopspeed);
	MSG_WriteFloat (&buf, movevars.maxspeed);
	MSG_WriteFloat (&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat (&buf, movevars.accelerate);
	MSG_WriteFloat (&buf, movevars.airaccelerate);
	MSG_WriteFloat (&buf, movevars.wateraccelerate);
	MSG_WriteFloat (&buf, movevars.friction);
	MSG_WriteFloat (&buf, movevars.waterfriction);
	MSG_WriteFloat (&buf, movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0);	// none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("fullserverinfo \"%s\"\n", cl.serverinfo));

	// flush packet
	CL_WriteRecordDemoMessage (&buf, seq++);
	SZ_Clear (&buf);

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.sound_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.sound_name[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}
	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = cl.model_name[n + 1];
	while (*s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = cl.model_name[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}

	// spawnstatic
	for (i = 0; i < cl_num_static_entities; i++) {
		ent = &cl_static_entities[i];

		MSG_WriteByte (&buf, svc_spawnstatic);

		for (j = 1; j < MAX_MODELS; j++)
			if (ent->common.model == cl.model_precache[j])
				break;
		if (j == MAX_MODELS)
			MSG_WriteByte (&buf, 0);
		else
			MSG_WriteByte (&buf, j);

		MSG_WriteByte (&buf, ent->common.frame[0]);
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, ent->common.skinnum);
		for (j = 0; j < 3; j++) {
			MSG_WriteCoord (&buf, ent->msg_origins[0][j]);
			MSG_WriteAngle (&buf, ent->msg_angles[0][j]);
		}

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// spawnstaticsound
	// static sounds are skipped in demos, life is hard

	// baselines
	memset (&blankes, 0, sizeof (blankes));
	for (i = 0; i < MAX_EDICTS; i++) {
		es = cl_baselines + i;

		if (memcmp (es, &blankes, sizeof (blankes))) {
			MSG_WriteByte (&buf, svc_spawnbaseline);
			MSG_WriteShort (&buf, i);

			MSG_WriteByte (&buf, es->modelindex);
			MSG_WriteByte (&buf, es->frame);
			MSG_WriteByte (&buf, es->colormap);
			MSG_WriteByte (&buf, es->skinnum);
			for (j = 0; j < 3; j++) {
				MSG_WriteCoord (&buf, es->origin[j]);
				MSG_WriteAngle (&buf, es->angles[j]);
			}

			if (buf.cursize > MAX_MSGLEN / 2) {
				CL_WriteRecordDemoMessage (&buf, seq++);
				SZ_Clear (&buf);
			}
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("cmd spawn %i 0\n", cl.servercount));

	if (buf.cursize) {
		CL_WriteRecordDemoMessage (&buf, seq++);
		SZ_Clear (&buf);
	}

	// send current status of all other players
	for (i = 0; i < MAX_CLIENTS; i++) {
		player = cl.players + i;
		user = ccl.users + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, user->frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, user->ping);

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, user->pl);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, ccls.realtime - user->entertime);

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, user->user_id);
		MSG_WriteString (&buf, player->userinfo);

		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char) i);
		MSG_WriteString (&buf, cl_lightstyle[i].map);
	}

	for (i = 0; i < MAX_CL_STATS; i++) {
		MSG_WriteByte (&buf, svc_updatestatlong);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, ccl.stats[i]);
		if (buf.cursize > MAX_MSGLEN / 2) {
			CL_WriteRecordDemoMessage (&buf, seq++);
			SZ_Clear (&buf);
		}
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("skins\n"));

	CL_WriteRecordDemoMessage (&buf, seq++);

	CL_WriteSetDemoMessage ();

	// done
}

/*
====================
CL_ReRecord_f

rerecord <demoname>
====================
*/
void
CL_ReRecord_f (void)
{
	int		c;
	char	name[MAX_OSPATH];

	c = Cmd_Argc ();
	if (c != 2) {
		Com_Printf ("rerecord <demoname>\n");
		return;
	}

	if (!*cls.servername) {
		Com_Printf ("No server to reconnect to...\n");
		return;
	}

	if (ccls.demorecording)
		CL_Stop_f ();

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, Cmd_Argv (1));

	// open the demo file
	COM_DefaultExtension (name, ".qwd");

	ccls.demofile = fopen (name, "wb");
	if (!ccls.demofile) {
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	Com_Printf ("recording to %s.\n", name);
	ccls.demorecording = true;

	CL_Disconnect ();
	CL_BeginServerConnect ();
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void
CL_PlayDemo_f (void)
{
	char	name[256];

	if (Cmd_Argc () != 2) {
		Com_Printf ("play <demoname> : plays a demo\n");
		return;
	}

	// disconnect from server
	CL_Disconnect ();

	// open the demo file
	strcpy (name, Cmd_Argv (1));
	COM_DefaultExtension (name, ".qwd");

	Com_Printf ("Playing demo from %s.\n", name);
	COM_FOpenFile (name, &ccls.demofile, true);
	if (!ccls.demofile) {
		Com_Printf ("ERROR: couldn't open.\n");
		ccls.demonum = -1;				// stop demo loop
		return;
	}

	ccls.demoplayback = true;
	ccls.state = ca_demostart;
	Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, 0);
	ccls.realtime = 0;
}

/*
====================
CL_FinishTimeDemo

====================
*/
static void
CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;

	ccls.timedemo = false;
	// the first frame didn't count
	frames = (host_framecount - ccls.td_startframe) - 1;
	time = Sys_DoubleTime () - ccls.td_starttime;
	if (!time)
		time = 1;
	Com_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time,
				frames / time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void
CL_TimeDemo_f (void)
{
	if (Cmd_Argc () != 2) {
		Com_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();

	if (ccls.state != ca_demostart)
		return;

	// ccls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	ccls.timedemo = true;
	ccls.td_starttime = 0;
	ccls.td_startframe = host_framecount;
	ccls.td_lastframe = -1;	// get a new message this frame
}

