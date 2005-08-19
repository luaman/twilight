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

	$Id$
*/
// server.h

#ifndef __SERVER_H
#define __SERVER_H

#include <setjmp.h>
#include "client.h"
#include "collision.h"
#include "progs.h"
#include "net.h"

typedef struct {
	Uint32			maxclients;
	int				maxclientslimit;
	struct client_s	*clients;			// [maxclients]
	int				serverflags;			// episode completion information
	qboolean		changelevel_issued;		// cleared when at SV_SpawnServer
} server_static_t;

//=============================================================================

typedef enum { ss_loading, ss_active } server_state_t;

typedef struct {
	qboolean		active;			// false if only a net client

	qboolean		paused;
	qboolean		loadgame;		// handle connections specially

	double			time;

	Uint			lastcheck;		// used by PF_checkclient
	double			lastchecktime;

	char			name[64];		// map name
	char			modelname[64];	// maps/<name>.bsp, for model_precache[0]
	struct model_s	*worldmodel;
	char			model_precache[MAX_MODELS][MAX_QPATH];	// NULL terminated
	struct model_s	*models[MAX_MODELS];
	char			sound_precache[MAX_SOUNDS][MAX_QPATH];	// NULL terminated
	char			lightstyles[MAX_LIGHTSTYLES][64];
	Uint			num_edicts;
	Uint			max_edicts;
	/* Can NOT be array indexed, because edict_t is variable sized,
	 * but can be used to reference the world ent. */
	edict_t			*edicts;

	server_state_t state;			// some actions are only valid during load

	sizebuf_t		datagram;
	Uint8			datagram_buf[MAX_DATAGRAM];

	sizebuf_t		reliable_datagram;			// copied to all clients at end of frame
	Uint8			reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t		signon;
	Uint8			signon_buf[8192];
} server_t;


#define	NUM_PING_TIMES	16
#define	NUM_SPAWN_PARMS	16

typedef struct client_s {
	qboolean	active;					// false = client is free
	qboolean	spawned;				// false = don't send datagrams
	qboolean	dropasap;				// has been told to go to another level
	qboolean	privileged;				// can execute any host command
	qboolean	sendsignon;				// only valid before spawned

	double      last_message;			// reliable messages must be sent
	// periodically

	struct qsocket_s *netconnection;	// communications handle

	usercmd_t   cmd;					// movement
	vec3_t      wishdir;				// intended motion calced from cmd

	sizebuf_t   message;				// can be added to at any time,
	// copied and clear once per frame
	Uint8       msgbuf[MAX_MSGLEN];
	edict_t		*edict;					// EDICT_NUM(clientnum+1)
	char		name[32];				// for printing to other people
	int			colors;

	float		ping_times[NUM_PING_TIMES];
	int			num_pings;				// ping_times[num_pings%NUM_PING_TIMES]
	float		ping;				// LordHavoc: can be used for prediction or whatever...
	float		latency;			// LordHavoc: specifically used for prediction, accounts for sys_ticrate too

	// spawn parms are carried from level to level
	float		spawn_parms[NUM_SPAWN_PARMS];

	// client known data for deltas 
	int			old_frags;
} client_t;


//=============================================================================

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
#define	MOVETYPE_ANGLENOCLIP	1
#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_STEP			4		// gravity, special edge handling
#define	MOVETYPE_FLY			5
#define	MOVETYPE_TOSS			6		// gravity
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment

// edict->solid values
#define	SOLID_NOT				0		// no interaction with other objects
#define	SOLID_TRIGGER			1		// touch on edge, but not blocking
#define	SOLID_BBOX				2		// touch on edge, block
#define	SOLID_SLIDEBOX			3		// touch on edge, but not an onground
#define	SOLID_BSP				4		// bsp clip, touch on edge, block
// LordHavoc: corpse code
#define	SOLID_CORPSE			5		// same as SOLID_BBOX, except it behaves as SOLID_NOT against SOLID_SLIDEBOX objects (players/monsters)

// edict->deadflag values
#define	DEAD_NO					0
#define	DEAD_DYING				1
#define	DEAD_DEAD				2

#define	DAMAGE_NO				0
#define	DAMAGE_YES				1
#define	DAMAGE_AIM				2

// edict->flags
#define	FL_FLY					1
#define	FL_SWIM					2
//#define   FL_GLIMPSE              4
#define	FL_CONVEYOR				4
#define	FL_CLIENT				8
#define	FL_INWATER				16
#define	FL_MONSTER				32
#define	FL_GODMODE				64
#define	FL_NOTARGET				128
#define	FL_ITEM					256
#define	FL_ONGROUND				512
#define	FL_PARTIALGROUND		1024	// not all corners are valid
#define	FL_WATERJUMP			2048	// player jumping out of water
#define	FL_JUMPRELEASED			4096	// for jump debouncing

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8

#define	SPAWNFLAG_NOT_EASY			256
#define	SPAWNFLAG_NOT_MEDIUM		512
#define	SPAWNFLAG_NOT_HARD			1024
#define	SPAWNFLAG_NOT_DEATHMATCH	2048

//============================================================================

extern client_t *host_client;
extern cvar_t *teamplay;
extern cvar_t *skill;
extern cvar_t *deathmatch;
extern cvar_t *coop;
extern cvar_t *pausable;
extern int current_skill;
extern qboolean noclip_anglehack;

extern server_t sv;
extern server_static_t svs;
extern memzone_t *sv_zone;
extern char localmodels[256][5];
extern Uint8 fatpvs[32767 / 8];
extern int fatbytes;

extern cvar_t *sv_aim;
extern cvar_t *sv_friction;
extern cvar_t *sv_stopspeed;
extern cvar_t *sv_gravity;
extern cvar_t *sv_maxvelocity;
extern cvar_t *sv_nostep;
extern cvar_t *sv_jumpstep;
extern cvar_t *sv_stepheight;
extern edict_t *sv_player;
extern cvar_t *sv_edgefriction;
extern cvar_t *sv_idealpitchscale;
extern cvar_t *sv_maxspeed;
extern cvar_t *sv_accelerate;


//===========================================================

void SV_ClientPrintf(const char *fmt, ...);
void SV_BroadcastPrintf(const char *fmt, ...);
void SV_DropClient(qboolean crash);
void SV_Init_Cvars(void);
void SV_Init(void);
void SV_StartParticle(vec3_t org, vec3_t dir, int color, int count);
void SV_StartSound(edict_t *entity, int channel, char *sample, int volume, float attenuation);
void SV_CheckForNewClients(void);
void SV_ClearDatagram(void);
void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg);
void SV_SendClientMessages(void);
int SV_ModelIndex(const char *name);
void SV_SaveSpawnparms(void);
void SV_SpawnServer(const char *server);
qboolean SV_CheckBottom(edict_t *ent);
qboolean SV_movestep(edict_t *ent, vec3_t move, qboolean relink);
void SV_MoveToGoal(void);
void SV_Physics(void);
trace_t SV_Trace_Toss(edict_t *tossent, edict_t *ignore);
void SV_SetIdealPitch(void);
void SV_RunClients(void);

#endif // __SERVER_H

