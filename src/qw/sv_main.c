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
#include "zone.h"
#include "net.h"
#include "fs/fs.h"
#include "fs/rw_ops.h"
#include "host.h"

#ifdef WIN32
# include "winconsole.h"
#endif

host_t		host;

netadr_t    master_adr[MAX_MASTERS];	// address of group servers

client_t   *host_client;				// current client

cvar_t     *sv_mintic;
cvar_t     *sv_maxtic;
static cvar_t     *timeout;
static cvar_t     *zombietime;
static cvar_t     *rcon_password;
static cvar_t     *password;
static cvar_t     *spectator_password;
cvar_t     *allow_download;
cvar_t     *allow_download_skins;
cvar_t     *allow_download_models;
cvar_t     *allow_download_sounds;
cvar_t     *allow_download_maps;
cvar_t     *sv_highchars;
cvar_t     *sv_phs;
cvar_t     *pausable;
cvar_t     *sv_aim;
static cvar_t     *fraglimit;
static cvar_t     *timelimit;
cvar_t     *teamplay;
static cvar_t     *samelevel;
static cvar_t     *maxclients;
static cvar_t     *maxspectators;
cvar_t     *deathmatch;
static cvar_t     *spawn;
static cvar_t     *watervis;
static cvar_t     *hostname;
static cvar_t     *filterban;
static cvar_t     *temp1;
cvar_t     *coop;
cvar_t     *skill;

cvar_t	   *sv_nailhack;

SDL_RWops	*sv_fraglogfile;

void        SV_AcceptClient (netadr_t adr, int userid, char *userinfo);
void        Master_Shutdown (void);

int			current_skill;

//============================================================================

qboolean
ServerPaused (void)
{
	return sv.paused;
}

/*
================
Quake calls this before calling Sys_Quit or Sys_Error
================
*/
void
SV_Shutdown (void)
{
	Master_Shutdown ();
	if (sv_fraglogfile) {
		SDL_RWclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
	}
	NET_Shutdown ();
}

/*
================
Sends a datagram to all the clients informing them of the server crash,
then exits
================
*/
void
SV_Error (char *error, ...)
{
	va_list     argptr;
	static char string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error ("SV_Error: recursively entered (%s)", string);

	inerror = true;

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Com_Printf ("SV_Error: %s\n", string);

	SV_FinalMessage (va ("server crashed: %s\n", string));

	SV_Shutdown ();

	Sys_Error ("SV_Error: %s\n", string);
}

/*
==================
Used by SV_Error and SV_Quit_f to send a final message to all connected
clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void
SV_FinalMessage (char *message)
{
	int         i;
	client_t   *cl;

	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_print);
	MSG_WriteByte (&net_message, PRINT_HIGH);
	MSG_WriteString (&net_message, message);
	MSG_WriteByte (&net_message, svc_disconnect);

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		if (cl->state >= cs_spawned)
			Netchan_Transmit (&cl->netchan, net_message.cursize,
							  net_message.data);
}



/*
=====================
Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void
SV_DropClient (client_t *drop)
{
	// add the disconnect
	MSG_WriteByte (&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned) {
		if (!drop->spectator) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			pr_global_struct->self = EDICT_TO_PROG (drop->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
		} else if (SpectatorDisconnect) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			pr_global_struct->self = EDICT_TO_PROG (drop->edict);
			PR_ExecuteProgram (SpectatorDisconnect);
		}
	}

	if (drop->spectator)
		Com_Printf ("Spectator %s removed\n", drop->name);
	else
		Com_Printf ("Client %s removed\n", drop->name);

	if (drop->download) {
		SDL_RWclose (drop->download);
		drop->download = NULL;
	}
	if (drop->upload) {
		SDL_RWclose (drop->upload);
		drop->upload = NULL;
	}
	*drop->uploadfn = 0;

	drop->state = cs_zombie;			// become free in a few seconds
	drop->connection_started = svs.realtime;	// for zombie timeout

	drop->old_frags = 0;
	drop->edict->v.frags = 0;
	drop->name[0] = 0;
	memset (drop->userinfo, 0, sizeof (drop->userinfo));

// send notification to all remaining clients
	SV_FullClientUpdate (drop, &sv.reliable_datagram);
}


//====================================================================

int
SV_CalcPing (client_t *cl)
{
	float       ping;
	int         i;
	int         count;
	register client_frame_t *frame;

	ping = 0;
	count = 0;
	for (frame = cl->frames, i = 0; i < UPDATE_BACKUP; i++, frame++) {
		if (frame->ping_time > 0) {
			ping += frame->ping_time;
			count++;
		}
	}
	if (!count)
		return 9999;
	ping /= count;

	return ping * 1000;
}

/*
===================
Writes all update values to a sizebuf
===================
*/
void
SV_FullClientUpdate (client_t *client, sizebuf_t *buf)
{
	int         i;
	char        info[MAX_INFO_STRING];

	i = client - svs.clients;

	MSG_WriteByte (buf, svc_updatefrags);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, client->old_frags);

	MSG_WriteByte (buf, svc_updateping);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, SV_CalcPing (client));

	MSG_WriteByte (buf, svc_updatepl);
	MSG_WriteByte (buf, i);
	MSG_WriteByte (buf, client->lossage);

	MSG_WriteByte (buf, svc_updateentertime);
	MSG_WriteByte (buf, i);
	MSG_WriteFloat (buf, svs.realtime - client->connection_started);

	strlcpy (info, client->userinfo, sizeof (info));
	Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

	MSG_WriteByte (buf, svc_updateuserinfo);
	MSG_WriteByte (buf, i);
	MSG_WriteLong (buf, client->userid);
	MSG_WriteString (buf, info);
}

/*
===================
Writes all update values to a client's reliable stream
===================
*/
void
SV_FullClientUpdateToClient (client_t *client, client_t *cl)
{
	ClientReliableCheckBlock (cl, 24 + strlen (client->userinfo));
	if (cl->num_backbuf) {
		SV_FullClientUpdate (client, &cl->backbuf);
		ClientReliable_FinishWrite (cl);
	} else
		SV_FullClientUpdate (client, &cl->netchan.message);
}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
================
Responds with all the info that qplug or qspy can see
This message can be up to around 5k with worst case string lengths.
================
*/
static void
SVC_Status (void)
{
	int         i;
	client_t   *cl;
	int         ping;
	int         top, bottom;

	Cmd_TokenizeString ("status");
	SV_BeginRedirect (RD_PACKET);
	Com_Printf ("%s\n", svs.info);
	for (i = 0; i < MAX_CLIENTS; i++) {
		cl = &svs.clients[i];
		if ((cl->state == cs_connected || cl->state == cs_spawned)
			&& !cl->spectator) {
			top = Q_atoi (Info_ValueForKey (cl->userinfo, "topcolor"));
			bottom = Q_atoi (Info_ValueForKey (cl->userinfo, "bottomcolor"));
			top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
			bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
			ping = SV_CalcPing (cl);
			Com_Printf ("%i %i %i %i \"%s\" \"%s\" %i %i\n", cl->userid,
						cl->old_frags,
						(int) (svs.realtime - cl->connection_started) / 60, ping,
						cl->name, Info_ValueForKey (cl->userinfo, "skin"), top,
						bottom);
		}
	}
	SV_EndRedirect ();
}

#define	LOG_HIGHWATER	4096
#define	LOG_FLUSH		10*60
static void
SV_CheckLog (void)
{
	sizebuf_t  *sz;

	sz = &svs.log[svs.logsequence & 1];

	// bump sequence if allmost full, or ten minutes have passed and
	// there is something still sitting there
	if (sz->cursize > LOG_HIGHWATER
		|| (svs.realtime - svs.logtime > LOG_FLUSH && sz->cursize)) {
		// swap buffers and bump sequence
		svs.logtime = svs.realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence & 1];
		sz->cursize = 0;
		Com_Printf ("beginning fraglog sequence %i\n", svs.logsequence);
	}

}

/*
================
Responds with all the logged frags for ranking programs.
If a sequence number is passed as a parameter and it is
the same as the current sequence, an A2A_NACK will be returned
instead of the data.
================
*/
static void
SVC_Log (void)
{
	int         seq;
	char        data[MAX_DATAGRAM + 64];

	if (Cmd_Argc () == 2)
		seq = Q_atoi (Cmd_Argv (1));
	else
		seq = -1;

	if (seq == svs.logsequence - 1 || !sv_fraglogfile) {	// they already
		// have this data,
		// or we aren't
		// logging frags
		data[0] = A2A_NACK;
		NET_SendPacket (NS_SERVER, 1, data, net_from);
		return;
	}

	Com_DPrintf ("sending log %i to %s\n", svs.logsequence - 1,
				 NET_AdrToString (net_from));

	snprintf (data, sizeof (data), "stdlog %i\n", svs.logsequence - 1);
	strlcat_s (data, (char *) svs.log_buf[((svs.logsequence - 1) & 1)]);

	NET_SendPacket (NS_SERVER, strlen (data) + 1, data, net_from);
}

/*
================
Just responds with an acknowledgement
================
*/
static void
SVC_Ping (void)
{
	char        data;

	data = A2A_ACK;

	NET_SendPacket (NS_SERVER, 1, &data, net_from);
}

/*
=================
Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
static void
SVC_GetChallenge (void)
{
	int         i;
	int         oldest;
	int         oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = (rand () << 16) ^ rand ();
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = svs.realtime;
		i = oldest;
	}
	// send it back
	Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c%i", S2C_CHALLENGE,
							svs.challenges[i].challenge);
}

/*
==================
A connection request that did not come from the master
==================
*/
static void
SVC_DirectConnect (void)
{
	char        userinfo[1024];
	static int  userid;
	netadr_t    adr;
	int         i;
	client_t   *cl, *newcl;
	client_t    temp;
	edict_t    *ent;
	int         edictnum;
	char       *s;
	int         clients, spectators;
	qboolean    spectator;
	int         qport;
	int         version;
	int         challenge;

	version = Q_atoi (Cmd_Argv (1));
	if (version != PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (NS_SERVER, net_from,
								"%c\nServer is Twilight version %s.\n",
								A2C_PRINT, VERSION);
		Com_Printf ("* rejected connect from version %i\n", version);
		return;
	}

	qport = Q_atoi (Cmd_Argv (2));

	challenge = Q_atoi (Cmd_Argv (3));

	// note an extra byte is needed to replace spectator key
	strlcpy (userinfo, Cmd_Argv (4), sizeof (userinfo) - 1);

	// see if the challenge is valid
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr)) {
			if (challenge == svs.challenges[i].challenge)
				break;					// good
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nBad challenge.\n",
									A2C_PRINT);
			return;
		}
	}
	if (i == MAX_CHALLENGES) {
		Netchan_OutOfBandPrint (NS_SERVER, net_from,
				"%c\nNo challenge for address.\n", A2C_PRINT);
		return;
	}
	// check for password or spectator_password
	s = Info_ValueForKey (userinfo, "spectator");
	if (s[0] && strcmp (s, "0")) {
		if (spectator_password->svalue[0]
				&& strcasecmp (spectator_password->svalue, "none")
				&& strcmp (spectator_password->svalue, s))
		{
			// failed
			Com_Printf ("%s:spectator password failed\n",
						NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from,
									"%c\nrequires a spectator password\n\n",
									A2C_PRINT);
			return;
		}
		Info_RemoveKey (userinfo, "spectator");	// remove passwd
		Info_SetValueForStarKey (userinfo, "*spectator", "1", MAX_INFO_STRING);
		spectator = true;
	} else {
		s = Info_ValueForKey (userinfo, "password");
		if (password->svalue[0] &&
			strcasecmp (password->svalue, "none") &&
			strcmp (password->svalue, s)) {
			Com_Printf ("%s:password failed\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from,
									"%c\nserver requires a password\n\n",
									A2C_PRINT);
			return;
		}
		spectator = false;
		Info_RemoveKey (userinfo, "password");	// remove passwd
	}

	adr = net_from;
	userid++;							// so every client gets a unique id

	newcl = &temp;
	memset (newcl, 0, sizeof (client_t));

	newcl->userid = userid;

	// works properly
	if (!sv_highchars->ivalue) {
		Uint8      *p, *q;

		for (p = (Uint8 *) newcl->userinfo, q = (Uint8 *) userinfo;
			 *q && p < (Uint8 *) newcl->userinfo + sizeof (newcl->userinfo) - 1;
			 q++)
			if (*q > 31 && *q <= 127)
				*p++ = *q;
	} else
		strlcpy (newcl->userinfo, userinfo, sizeof (newcl->userinfo));

	// if there is already a slot for this ip, drop it
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address)
			&& (cl->netchan.qport == qport
				|| adr.port == cl->netchan.remote_address.port)) {
			if (cl->state == cs_connected) {
				Com_Printf ("%s:dup connect\n", NET_AdrToString (adr));
				userid--;
				return;
			}

			Com_Printf ("%s:reconnect\n", NET_AdrToString (adr));
			SV_DropClient (cl);
			break;
		}
	}

	// count up the clients and spectators
	clients = 0;
	spectators = 0;
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_free)
			continue;
		if (cl->spectator)
			spectators++;
		else
			clients++;
	}

	// if at server limits, refuse connection
	if (maxclients->ivalue > MAX_CLIENTS)
		Cvar_Set (maxclients, va ("%i", MAX_CLIENTS));
	if (maxspectators->ivalue > MAX_CLIENTS)
		Cvar_Set (maxspectators, va ("%i", MAX_CLIENTS));
	if (maxspectators->ivalue + maxclients->ivalue > MAX_CLIENTS)
		Cvar_Set (maxspectators, va ("%i",
					   MAX_CLIENTS - maxspectators->ivalue
					   + maxclients->ivalue));
	if ((spectator && spectators >= maxspectators->ivalue)
		|| (!spectator && clients >= maxclients->ivalue)) {
		Com_Printf ("%s:full connect\n", NET_AdrToString (adr));
		Netchan_OutOfBandPrint (NS_SERVER, adr, "%c\nserver is full\n\n", A2C_PRINT);
		return;
	}
	// find a client slot
	newcl = NULL;
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_free) {
			newcl = cl;
			break;
		}
	}
	if (!newcl) {
		Com_Printf ("WARNING: miscounted available clients\n");
		return;
	}
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	*newcl = temp;

	Netchan_OutOfBandPrint (NS_SERVER, adr, "%c", S2C_CONNECTION);

	edictnum = (newcl - svs.clients) + 1;

	Netchan_Setup (NS_SERVER, &newcl->netchan, adr, qport);

	newcl->state = cs_connected;

	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof (newcl->datagram_buf);

	// spectator mode can ONLY be set at join time
	newcl->spectator = spectator;

	ent = EDICT_NUM (edictnum);
	newcl->edict = ent;

	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl);

	// JACK: Init the floodprot stuff.
	for (i = 0; i < 10; i++)
		newcl->whensaid[i] = 0.0;
	newcl->whensaidhead = 0;
	newcl->lockedtill = 0;

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (pr_global_struct->SetNewParms);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		newcl->spawn_parms[i] = (&pr_global_struct->parm1)[i];

	if (newcl->spectator)
		Com_Printf ("Spectator %s connected\n", newcl->name);
	else
		Com_DPrintf ("Client %s connected\n", newcl->name);
	newcl->sendinfo = true;
}

int
Rcon_Validate (void)
{
	if (!strlen (rcon_password->svalue))
		return 0;

	if (strcmp (Cmd_Argv (1), rcon_password->svalue))
		return 0;

	return 1;
}

/*
===============
A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
static void
SVC_RemoteCommand (void)
{
	int         i;
	char        remaining[1024];

	if (!Rcon_Validate ()) {
		Com_Printf ("Bad rcon from %s:\n%s\n", NET_AdrToString (net_from),
					net_message.data + 4);

		SV_BeginRedirect (RD_PACKET);

		Com_Printf ("Bad rcon_password.\n");

	} else {

		Com_Printf ("Rcon from %s:\n%s\n", NET_AdrToString (net_from),
					net_message.data + 4);

		SV_BeginRedirect (RD_PACKET);

		remaining[0] = 0;

		for (i = 2; i < Cmd_Argc (); i++) {
			strlcat_s (remaining, Cmd_Argv (i));
			strlcat_s (remaining, " ");
		}

		Cmd_ExecuteString (remaining, src_client);

	}

	SV_EndRedirect ();
}


void
Cmd_ForwardToServer (void)
{
}

/*
=================
A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void
SV_ConnectionlessPacket (void)
{
	char       *s;
	const char *c;

	MSG_BeginReading ();
	MSG_ReadLong ();					// skip the -1 marker

	s = MSG_ReadStringLine ();

	Cmd_TokenizeString (s);

	c = Cmd_Argv (0);

	if (!strcmp (c, "ping")
		|| (c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n'))) {
		SVC_Ping ();
		return;
	}
	if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n')) {
		Com_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
		return;
	} else if (!strcmp (c, "status")) {
		SVC_Status ();
		return;
	} else if (!strcmp (c, "log")) {
		SVC_Log ();
		return;
	} else if (!strcmp (c, "connect")) {
		SVC_DirectConnect ();
		return;
	} else if (!strcmp (c, "getchallenge")) {
		SVC_GetChallenge ();
		return;
	} else if (!strcmp (c, "rcon"))
		SVC_RemoteCommand ();
	else
		Com_Printf ("bad connectionless packet from %s:\n%s\n",
					NET_AdrToString (net_from), s);
}

/*
==============================================================================

PACKET FILTERING
 

You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/


typedef struct {
	unsigned    mask;
	unsigned    compare;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

static ipfilter_t  ipfilters[MAX_IPFILTERS];
static int         numipfilters;


static qboolean
StringToFilter (const char *s, ipfilter_t * f)
{
	char        num[128];
	int         i, j;
	Uint8       b[4];
	Uint8       m[4];

	for (i = 0; i < 4; i++) {
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0; i < 4; i++) {
		if (*s < '0' || *s > '9') {
			Com_Printf ("Bad filter address: %s\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9') {
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = Q_atoi (num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *) m;
	f->compare = *(unsigned *) b;

	return true;
}

static void
SV_AddIP_f (void)
{
	int         i;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break;						// free spot
	if (i == numipfilters) {
		if (numipfilters == MAX_IPFILTERS) {
			Com_Printf ("IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter (Cmd_Argv (1), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

static void
SV_RemoveIP_f (void)
{
	ipfilter_t  f;
	int         i, j;

	if (!StringToFilter (Cmd_Argv (1), &f))
		return;
	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare) {
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			Com_Printf ("Removed.\n");
			return;
		}
	Com_Printf ("Didn't find %s.\n", Cmd_Argv (1));
}

static void
SV_ListIP_f (void)
{
	int         i;
	Uint8       b[4];

	Com_Printf ("Filter list:\n");
	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		Com_Printf ("%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

static void
SV_WriteIP_f (void)
{
	fs_file_t	*file;
	SDL_RWops	*rw = NULL;
	Uint8       b[4];
	int         i;

	Com_Printf ("Writing listip.cfg.\n");

	if ((file = FS_FindFile ("listip.cfg")))
		rw = file->open(file, FSF_WRITE | FSF_ASCII);

	if (!rw)
		rw = FS_Open_New ("listip.cfg", FSF_ASCII);

	if (!rw) {
		Com_Printf ("Couldn't write listip.cfg.\n");
		return;
	}


	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		RWprintf (rw, "addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	SDL_RWclose (rw);
}

static void
SV_SendBan (void)
{
	Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nbanned.\n", A2C_PRINT);
}

static qboolean
SV_FilterPacket (void)
{
	int         i;
	unsigned    in;

	in = *(unsigned *) net_from.ip;

	for (i = 0; i < numipfilters; i++)
		if ((in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban->ivalue;

	return !filterban->ivalue;
}

//============================================================================

static void
SV_ReadPackets (void)
{
	int         i;
	client_t   *cl;
	qboolean    good;
	int         qport;

	good = false;
	while (NET_GetPacket (NS_SERVER)) {
		if (SV_FilterPacket ()) {
			SV_SendBan ();				// tell them we aren't listening...
			continue;
		}
		// check for connectionless packet (0xffffffff) first
		if (*(int *) net_message.data == -1) {
			SV_ConnectionlessPacket ();
			continue;
		}
		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading ();
		MSG_ReadLong ();				// sequence number
		MSG_ReadLong ();				// sequence number
		qport = MSG_ReadShort () & 0xffff;

		// check for packets from connected clients
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (cl->state == cs_free)
				continue;
			if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
				continue;
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port) {
				Com_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}
			if (Netchan_Process (&cl->netchan)) {	// this is a valid,
				// sequenced packet, so
				// process it
				svs.stats.packets++;
				good = true;
				cl->send_message = true;	// reply at end of frame
				if (cl->state != cs_zombie)
					SV_ExecuteClientMessage (cl);
			}
			break;
		}

		if (i != MAX_CLIENTS)
			continue;
	}
}

/*
==================
If a packet has not been received from a client in timeout.value
seconds, drop the conneciton.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void
SV_CheckTimeouts (void)
{
	int         i;
	client_t   *cl;
	float       droptime;
	int         nclients;

	droptime = svs.realtime - timeout->fvalue;
	nclients = 0;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state == cs_connected || cl->state == cs_spawned) {
			if (!cl->spectator)
				nclients++;
			if (cl->netchan.last_received < droptime) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
				SV_DropClient (cl);
				cl->state = cs_free;	// don't bother with zombie state
			}
		}
		if (cl->state == cs_zombie &&
				svs.realtime - cl->connection_started > zombietime->fvalue) {
			cl->state = cs_free;		// can now be reused
		}
	}
	if (sv.paused && !nclients) {
		// nobody left, unpause the server
		SV_TogglePause ("Pause released since no players are left.\n");
	}

#ifdef WIN32
	{
		// check these and update the console if neccessary
		static int lastclients = 0;
		static int lastmaxclients = 0;
		if(lastclients != nclients)
		{
			WinCon_SetConnectedClients(nclients);
			lastclients = nclients;
		}
		if(lastmaxclients != maxclients->ivalue)
		{
			lastmaxclients = maxclients->ivalue;
			WinCon_SetMaxClients(lastmaxclients);
		}
	}
#endif
}

/*
===================
Add them exactly as if they had been typed at the console
===================
*/
static void
SV_GetConsoleCommands (void)
{
	char       *cmd;

	while (1) {
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}

static void
SV_CheckVars (void)
{
	static char *pw, *spw;
	int         v;

	if (password->svalue == pw && spectator_password->svalue == spw)
		return;
	pw = password->svalue;
	spw = spectator_password->svalue;

	v = 0;
	if (pw && pw[0] && strcmp (pw, "none"))
		v |= 1;
	if (spw && spw[0] && strcmp (spw, "none"))
		v |= 2;

	Com_Printf ("Updated needpass.\n");
	if (!v)
		Info_SetValueForKey (svs.info, "needpass", "", MAX_SERVERINFO_STRING);
	else
		Info_SetValueForKey (svs.info, "needpass", va ("%i", v),
							 MAX_SERVERINFO_STRING);
}

void
SV_Frame (float time)
{
	static double start, end;

	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;

// keep the random time dependent
	rand ();

// decide the simulation time
	if (!sv.paused) {
		svs.realtime += time;
		sv.time += time;
	}
// check timeouts
	SV_CheckTimeouts ();

// toggle the log buffer if full
	SV_CheckLog ();

// move autonomous things around if enough time has passed
	if (!sv.paused)
		SV_Physics ();

// get packets
	SV_ReadPackets ();

// check for commands typed to the host
	SV_GetConsoleCommands ();

// process console commands
	Cbuf_Execute ();

	SV_CheckVars ();

// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

// send a heartbeat to the master if needed
	Master_Heartbeat ();

// collect timing statistics
	end = Sys_DoubleTime ();
	svs.stats.active += end - start;
	if (++svs.stats.count == STATFRAMES) {
		svs.stats.latched_active = svs.stats.active;
		svs.stats.latched_idle = svs.stats.idle;
		svs.stats.latched_packets = svs.stats.packets;
		svs.stats.active = 0;
		svs.stats.idle = 0;
		svs.stats.packets = 0;
		svs.stats.count = 0;
	}
}

static void
SV_InitLocal (void)
{
	int         i;

	// bound the size of the
	sv_mintic = Cvar_Get ("sv_mintic", "0.03", CVAR_NONE, NULL);
	// physics time tic 
	sv_maxtic = Cvar_Get ("sv_maxtic", "0.1", CVAR_NONE, NULL);

	// seconds without any message
	timeout = Cvar_Get ("timeout", "65", CVAR_NONE, NULL);
	// seconds to sink messages after disconnect
	zombietime = Cvar_Get ("zombietime", "2", CVAR_NONE, NULL);

	// password for remote server commands
	rcon_password = Cvar_Get ("rcon_password", "", CVAR_NONE, NULL);

	// password for entering the game
	password = Cvar_Get ("password", "", CVAR_NONE, NULL);
	// password for entering as a sepctator
	spectator_password = Cvar_Get ("spectator_password", "", CVAR_NONE, NULL);

	allow_download = Cvar_Get ("allow_download", "1", CVAR_NONE, NULL);
	allow_download_skins = Cvar_Get ("allow_download_skins", "1",
			CVAR_NONE, NULL);
	allow_download_models = Cvar_Get ("allow_download_models", "1",
			CVAR_NONE, NULL);
	allow_download_sounds = Cvar_Get ("allow_download_sounds", "1",
			CVAR_NONE, NULL);
	allow_download_maps = Cvar_Get ("allow_download_maps", "1",
			CVAR_NONE, NULL);

	sv_highchars = Cvar_Get ("sv_highchars", "1", CVAR_NONE, NULL);

	sv_phs = Cvar_Get ("sv_phs", "1", CVAR_NONE, NULL);

	pausable = Cvar_Get ("pausable", "1", CVAR_NONE, NULL);

	sv_aim = Cvar_Get ("sv_aim", "2", CVAR_NONE, NULL);

	sv_maxvelocity = Cvar_Get ("sv_maxvelocity", "2000", CVAR_NONE, NULL);

	sv_gravity = Cvar_Get ("sv_gravity", "800", CVAR_NONE, NULL);
	sv_stopspeed = Cvar_Get ("sv_stopspeed", "100", CVAR_NONE, NULL);
	sv_maxspeed = Cvar_Get ("sv_maxspeed", "320", CVAR_NONE, NULL);
	sv_spectatormaxspeed = Cvar_Get ("sv_spectatormaxspeed", "500",
			CVAR_NONE, NULL);
	sv_accelerate = Cvar_Get ("sv_accelerate", "10", CVAR_NONE, NULL);
	sv_airaccelerate = Cvar_Get ("sv_airaccelerate", "0.7", CVAR_NONE, NULL);
	sv_wateraccelerate = Cvar_Get ("sv_wateraccelerate", "10", CVAR_NONE,
			NULL);
	sv_friction = Cvar_Get ("sv_friction", "4", CVAR_NONE, NULL);
	sv_waterfriction = Cvar_Get ("sv_waterfriction", "4", CVAR_NONE, NULL);

	sv_nailhack = Cvar_Get ("sv_nailhack", "0", CVAR_NONE, NULL);

	filterban = Cvar_Get ("filterban", "1", CVAR_NONE, NULL);

	// referenced by progs
	temp1 = Cvar_Get ("temp1", "0", CVAR_NONE, NULL);

//
// game rules mirrored in svs.info
//
	fraglimit = Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO, NULL);
	timelimit = Cvar_Get ("timelimit", "0", CVAR_SERVERINFO, NULL);
	teamplay = Cvar_Get ("teamplay", "0", CVAR_SERVERINFO, NULL);
	samelevel = Cvar_Get ("samelevel", "0", CVAR_SERVERINFO, NULL);
	maxclients = Cvar_Get ("maxclients", "8", CVAR_SERVERINFO, NULL);
	maxspectators = Cvar_Get ("maxspectators", "8", CVAR_SERVERINFO, NULL);
	// 0, 1, or 2
	deathmatch = Cvar_Get ("deathmatch", "1", CVAR_SERVERINFO, NULL);
	spawn = Cvar_Get ("spawn", "0", CVAR_SERVERINFO, NULL);
	watervis = Cvar_Get ("watervis", "0", CVAR_SERVERINFO, NULL);
	coop = Cvar_Get ("coop", "0", CVAR_SERVERINFO, NULL);
	skill = Cvar_Get ("skill", "0", CVAR_SERVERINFO, NULL);

	hostname = Cvar_Get ("hostname", "unnamed", CVAR_SERVERINFO, NULL);

	SV_InitOperatorCommands ();
	SV_UserInit ();

	Cmd_AddCommand ("addip", SV_AddIP_f);
	Cmd_AddCommand ("removeip", SV_RemoveIP_f);
	Cmd_AddCommand ("listip", SV_ListIP_f);
	Cmd_AddCommand ("writeip", SV_WriteIP_f);

	for (i = 0; i < MAX_MODELS; i++)
		snprintf (localmodels[i], sizeof (localmodels[i]), "*%i", i);

	Info_SetValueForStarKey (svs.info, "*version", va ("twilight %s", VERSION),
							 MAX_SERVERINFO_STRING);

	svs.realtime = 0;

	// init fraglog stuff
	svs.logsequence = 1;
	svs.logtime = svs.realtime;
	svs.log[0].data = svs.log_buf[0];
	svs.log[0].maxsize = sizeof (svs.log_buf[0]);
	svs.log[0].cursize = 0;
	svs.log[0].allowoverflow = true;
	svs.log[1].data = svs.log_buf[1];
	svs.log[1].maxsize = sizeof (svs.log_buf[1]);
	svs.log[1].cursize = 0;
	svs.log[1].allowoverflow = true;
}


//============================================================================

/*
================
Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
#define	HEARTBEAT_SECONDS	300
void
Master_Heartbeat (void)
{
	char        string[2048];
	int         active;
	int         i;

	if (svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return;							// not time to send yet

	svs.last_heartbeat = svs.realtime;

	// 
	// count active users
	// 
	active = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.clients[i].state == cs_connected ||
			svs.clients[i].state == cs_spawned)
			active++;

	svs.heartbeat_sequence++;
	snprintf (string, sizeof (string), "%c\n%i\n%i\n", S2M_HEARTBEAT,
			  svs.heartbeat_sequence, active);


	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
		if (master_adr[i].port) {
			Com_Printf ("Sending heartbeat to %s\n",
						NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen (string), string, master_adr[i]);
		}
}

/*
=================
Informs all masters that this server is going down
=================
*/
void
Master_Shutdown (void)
{
	char        string[2048];
	int         i;

	snprintf (string, sizeof (string), "%c\n", S2M_SHUTDOWN);

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
		if (master_adr[i].port) {
			Com_Printf ("Sending heartbeat to %s\n",
						NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen (string), string, master_adr[i]);
		}
}

/*
=================
Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void
SV_ExtractFromUserinfo (client_t *cl)
{
	char       *val, *p, *q;
	int         i;
	client_t   *client;
	int         dupc = 1;
	char        newname[80];


	// name for C code
	val = Info_ValueForKey (cl->userinfo, "name");

	// trim user name
	strlcpy (newname, val, sizeof (newname));

	for (p = newname; (*p == ' ' || *p == '\r' || *p == '\n') && *p; p++);

	if (p != newname && !*p) {
		// white space only
		strlcpy_s (newname, "unnamed");
		p = newname;
	}

	if (p != newname && *p) {
		for (q = newname; *p; *q++ = *p++);
		*q = 0;
	}
	for (p = newname + strlen (newname) - 1;
		 p != newname && (*p == ' ' || *p == '\r' || *p == '\n'); p--);
	p[1] = 0;

	if (strcmp (val, newname)) {
		Info_SetValueForKey (cl->userinfo, "name", newname, MAX_INFO_STRING);
		val = Info_ValueForKey (cl->userinfo, "name");
	}

	if (!val[0] || !strcasecmp (val, "console")) {
		Info_SetValueForKey (cl->userinfo, "name", "unnamed", MAX_INFO_STRING);
		val = Info_ValueForKey (cl->userinfo, "name");
	}
	// check to see if another user by the same name exists
	while (1) {
		for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
			if (client->state != cs_spawned || client == cl)
				continue;
			if (!strcasecmp (client->name, val))
				break;
		}
		if (i != MAX_CLIENTS) {			// dup name
			if (strlen (val) > sizeof (cl->name) - 1)
				val[sizeof (cl->name) - 4] = 0;
			p = val;

			if (val[0] == '(') {
				if (val[2] == ')')
					p = val + 3;
				else if (val[3] == ')')
					p = val + 4;
			}

			snprintf (newname, sizeof (newname), "(%d)%-.40s", dupc++, p);
			Info_SetValueForKey (cl->userinfo, "name", newname,
								 MAX_INFO_STRING);
			val = Info_ValueForKey (cl->userinfo, "name");
		} else
			break;
	}

	if (strncmp (val, cl->name, strlen (cl->name))) {
		if (!sv.paused) {
			if (!cl->lastnametime || svs.realtime - cl->lastnametime > 5) {
				cl->lastnamecount = 0;
				cl->lastnametime = svs.realtime;
			} else if (cl->lastnamecount++ > 4) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked for name spam\n",
									cl->name);
				SV_ClientPrintf (cl, PRINT_HIGH,
								 "You were kicked from the game for name spamming\n");
				SV_DropClient (cl);
				return;
			}
		}

		if (cl->state >= cs_spawned && !cl->spectator)
			SV_BroadcastPrintf (PRINT_HIGH, "%s changed name to %s\n", cl->name,
								val);
	}


	strlcpy (cl->name, val, sizeof (cl->name));

	// rate command
	val = Info_ValueForKey (cl->userinfo, "rate");
	if (strlen (val)) {
		i = Q_atoi (val);
		if (i < 500)
			i = 500;
		if (i > 10000)
			i = 10000;
		cl->netchan.rate = 1.0 / i;
	}
	// msg command
	val = Info_ValueForKey (cl->userinfo, "msg");
	if (strlen (val)) {
		cl->messagelevel = Q_atoi (val);
	}

}


//============================================================================

static void
SV_InitNet (void)
{
	Uint	port;
	Uint	p;

	port = PORT_SERVER;
	p = COM_CheckParm ("-port");
	if (p && p < com_argc) {
		port = Q_atoi (com_argv[p + 1]);
		Com_Printf ("Port: %i\n", port);
	}

	NET_Init ();
	NET_OpenSocket (NS_SERVER, port);

	Netchan_Init ();

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
}

static void
SV_CvarServerinfo (cvar_t *var)
{
	if (var->flags & CVAR_SERVERINFO)
	{
		Info_SetValueForKey (svs.info, var->name, var->svalue,
				MAX_SERVERINFO_STRING);
		SV_SendServerInfoChange (var->name, var->svalue);
	}
}

void
SV_Init (void)
{
	Zone_Init ();
	Cvar_Init (&SV_CvarServerinfo);
	Cbuf_Init ();
	Cmd_Init (NULL);
	Sys_Init ();
	Zone_Init_Commands ();

	sv_zone = Zone_AllocZone ("server");

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

	COM_Init_Cvars ();
	Netchan_Init_Cvars ();

	COM_Init ();

	PR_Init ();
	Mod_Init ();

	SV_InitNet ();

	SV_InitLocal ();
	Pmove_Init ();

	Cbuf_InsertText ("exec server.cfg\n");

	host.initialized = true;

	Com_Printf ("Exe: " __TIME__ " " __DATE__ "\n");

	Com_Printf ("\nTwilight Server Version %s (Build %04d)\n\n", VERSION,
				build_number ());

	Com_Printf ("======== QuakeWorld Initialized ========\n");

	// process command line arguments
	Cmd_StuffCmds_f ();
	Cbuf_Execute ();

	// if a map wasn't specified on the command line, spawn start.map
	if (sv.state == ss_dead)
		Cmd_ExecuteString ("map start", src_command);
	if (sv.state == ss_dead)
		SV_Error ("Couldn't spawn a server");
}

