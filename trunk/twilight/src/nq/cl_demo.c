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
#include "mathlib.h"
#include "net.h"
#include "strlib.h"
#include "sys.h"

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
	ccls.demoplayback = false;
	ccls.demofile = NULL;
	ccls.state = ca_disconnected;
	ccl.worldmodel = r_worldmodel = NULL;

	if (ccls.timedemo)
		CL_FinishTimeDemo ();

	memset (ccl.cshifts, 0, sizeof(ccl.cshifts));
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
static void
CL_WriteDemoMessage (void)
{
	int		len;
	float	ang[3];
	
	len = LittleLong (net_message.cursize);
	fwrite (&len, 4, 1, ccls.demofile);

	ang[0] = LittleFloat (cl.viewangles[0]);
	ang[1] = LittleFloat (cl.viewangles[1]);
	ang[2] = LittleFloat (cl.viewangles[2]);

	fwrite (ang, 12, 1, ccls.demofile);

	fwrite (net_message.data, net_message.cursize, 1, ccls.demofile);
	fflush (ccls.demofile);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int
CL_GetMessage (void)
{
	int	r, i;

	if (ccls.demoplayback) {
		// decide if it is time to grab the next message 
		if (cls.signon == SIGNONS)		// always grab until fully connected
		{
			if (ccls.timedemo) {
				if (host_framecount == ccls.td_lastframe)
					return 0;			// already read this frame's message
				ccls.td_lastframe = host_framecount;
				// if this is the second frame, grab the real td_starttime
				// so the bogus time on the first frame doesn't count
				if (host_framecount == ccls.td_startframe + 1)
					ccls.td_starttime = host_realtime;
			} else if ( /* ccl.time > 0 && */ ccl.time <= cl.mtime[0]) {
				return 0;				// don't need another message yet
			}
		}
		// get the next message
		fread (&net_message.cursize, 4, 1, ccls.demofile);
		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);

		fread (cl.mviewangles[0], 12, 1, ccls.demofile);

		for (i = 0; i < 3; i++) {
			cl.mviewangles[0][i] = LittleFloat (cl.mviewangles[0][i]);
		}

		net_message.cursize = LittleLong (net_message.cursize);
		if (net_message.cursize > MAX_MSGLEN)
			Sys_Error ("Demo message > MAX_MSGLEN");
		r = fread (net_message.data, net_message.cursize, 1, ccls.demofile);
		if (r != 1) {
			CL_StopPlayback ();
			return 0;
		}

		return 1;
	}

	while (1) {
		r = NET_GetMessage (cls.netcon);

		if (r != 1 && r != 2)
			return r;

		// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Com_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (ccls.demorecording)
		CL_WriteDemoMessage ();

	return r;
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
	if (cmd_source != src_command)
		return;

	if (!ccls.demorecording) {
		Com_Printf ("Not recording a demo.\n");
		return;
	}
	// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

	// finish up
	fclose (ccls.demofile);
	ccls.demofile = NULL;
	ccls.demorecording = false;
	Com_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void
CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int		track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc ();
	if (c != 2 && c != 3 && c != 4) {
		Com_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr (Cmd_Argv (1), "..")) {
		Com_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && ccls.state >= ca_connected) {
		Com_Printf
			("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
	}
	// write the forced cd track number, or -1
	if (c == 4) {
		track = Q_atoi (Cmd_Argv (3));
		Com_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	} else
		track = -1;

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, Cmd_Argv (1));

	// start the map up
	if (c > 2)
		Cmd_ExecuteString (va ("map %s", Cmd_Argv (2)), src_command);

	// open the demo file
	COM_DefaultExtension (name, ".dem");

	Com_Printf ("recording to %s.\n", name);
	ccls.demofile = fopen (name, "wb");
	if (!ccls.demofile) {
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	fprintf (ccls.demofile, "%i\n", cls.forcetrack);

	ccls.demorecording = true;
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
	char		name[256];
	int			c;
	qboolean	neg = false;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () != 2) {
		Com_Printf ("play <demoname> : plays a demo\n");
		return;
	}

	// disconnect from server
	CL_Disconnect ();

	// open the demo file
	strcpy (name, Cmd_Argv (1));
	COM_DefaultExtension (name, ".dem");

	Com_Printf ("Playing demo from %s.\n", name);
	COM_FOpenFile (name, &ccls.demofile, true);
	if (!ccls.demofile) {
		Com_Printf ("ERROR: couldn't open.\n");
		ccls.demonum = -1;	// stop demo loop
		return;
	}

	ccls.demoplayback = true;
	ccls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = getc (ccls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;
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
	time = host_realtime - ccls.td_starttime;
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
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc () != 2) {
		Com_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();

	// ccls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	ccls.timedemo = true;
	ccls.td_startframe = host_framecount;
	ccls.td_lastframe = -1;				// get a new message this frame
}
