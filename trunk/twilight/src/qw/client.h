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

#ifndef __CLIENT_H
#define __CLIENT_H

#include "quakedef.h"
#include "common.h"
#include "info.h"
#include "light.h"
#include "net.h"
#include "protocol.h"
#include "render.h"
#include "zone.h"
#include "pmove.h"

/*
 * player_state_t is the information needed by a player entity
 * to do move prediction and to generate a drawable entity
 */
typedef struct {
	// all player's won't be updated each frame
	int			messagenum;

	// not the same as  packet time because player commands are asyncronous
	double		state_time;

	usercmd_t	command;				// last command for prediction

	vec3_t		origin;
	vec3_t		viewangles;				// only for demos, not from server
	vec3_t		velocity;
	int			weaponframe;

	int			modelindex;
	int			frame;
	int         skinnum;
	int         effects;

	int			flags;					// dead, gib, etc

	float		waterjumptime;
	physent_t	*groundent;				// NULL in air, else pmove ent num
	int			number;
	int			oldbuttons;
} player_state_t;


typedef struct player_info_s {
	int			userid;
	char		userinfo[MAX_INFO_STRING];

	// scoreboard information
	char		team[MAX_SCOREBOARDNAME];
	char		name[MAX_SCOREBOARDNAME];
	float		entertime;
	int			frags;
	int			ping;
	Uint8		pl;
	int			spectator;

	// skin information
	colormap_t	colormap;
	char		skin_name[MAX_SKIN_NAME];
	skin_t		*skin;
} player_info_t;


typedef struct {
	/*
	 * generated clientside
	 */

	// cmd that generated the frame
	usercmd_t			cmd;
	// time cmd was sent off
	double				senttime;
	// the seqence to delta from, -1 = full update
	int					delta_sequence;

	/*
	 * received from server
	 */
	
	// time message was received, or -1
	double				receivedtime;
	// message received that reflects performing the usercmd
	player_state_t		playerstate[MAX_CLIENTS];
	packet_entities_t	packet_entities;
	// true if the packet_entities delta was invalid
	qboolean			invalid;
} frame_t;


typedef struct {
	int	destcolor[3];
	int	percent;		// 0-256 (yeah, someone's on drugs)
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4


/*
 * client_state_t should hold all pieces of the client state
 */
typedef struct {
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;



#define	MAX_EFRAGS		512

#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
	// full screen console with no connection
	ca_disconnected,
	// starting up a demo
	ca_demostart,
	// netchan_t established, waiting for svc_serverdata
	ca_connected,
	// processing data lists, donwloading, etc
	ca_onserver,
	// everything is in, so frames can be rendered
	ca_active
} cactive_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;								// download type


/*
 * the client_static_t structure is persistant through an arbitrary number
 * of server connections
 */
typedef struct {
	// connection information
	cactive_t	state;

	// network stuff
	netchan_t	netchan;

	// private userinfo for sending to masterless servers
	char		userinfo[MAX_INFO_STRING];

	char		servername[MAX_OSPATH];	// name of server from original connect

	int			qport;

	FILE		*download;				// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

	// demo loop control
	int			demonum;				// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];	// when not playing

	
	/*
	 * demo recording info must be here, because record is started before
	 * entering a map (and clearing client_state_t)
	 */
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	FILE		*demofile;
	float		td_lastframe;			// to meter out one message a frame
	int			td_startframe;			// host_framecount at start
	float		td_starttime;			// realtime at second frame of timedemo

	float		latency;				// rolling average

	double		realtime;
} client_static_t;

extern client_static_t cls;

// these determine which intermission screen plays
typedef enum { GAME_SINGLE, GAME_COOP, GAME_DEATHMATCH, GAME_TEAMS } gametype_t;

#define FRAGS_SORTED		1
#define FRAGS_TEAM_SORTED	2

/*
 * the client_state_t structure is wiped completely at every server signon
 */
typedef struct {
	int				servercount;			// server identification for prespawns

	char			serverinfo[MAX_SERVERINFO_STRING];

	int				parsecount;				// server message counter
	
	/*
	 * this is the sequence number of the last good  packetentity_t we got.
	 * If this is 0, we can't render a frame yet
	 */
	int				validsequence;

	/*
	 * since connecting to this server throw out the first couple, so the
	 * player doesn't accidentally do something the first frame
	 */
	int				movemessages;

	int				spectator;

	gametype_t		gametype;

	double			last_ping_request;		// while showing scoreboard
	double			last_servermessage;

	// sentcmds[cl.netchan.outgoing_sequence & UPDATE_MASK] = cmd
	frame_t			frames[UPDATE_BACKUP];

	// information for local display
	int				stats[MAX_CL_STATS];	// health, etc
	float			item_gettime[32];		// cl.time of aquiring, for blinking
	float			faceanimtime;			// use anim frame if cl.time < this

	colormap_t		*colormap;				// The colormap for local display.
	cshift_t		cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t		prev_cshifts[NUM_CSHIFTS];	// and content types

	/*
	 * the client maintains its own idea of view angles, which are sent to
	 * the server each frame.  And only reset at level change and teleport
	 * times
	 */
	vec3_t			viewangles;

	// the time that the client is rendering at.  always <= realtime
	double			time, oldtime;

	// the client simulates or interpolates movement to get these values
	vec3_t			simorg;
	vec3_t			simvel;
	vec3_t			simangles;

	// pitch drifting vars
	float			pitchvel;
	qboolean		nodrift;
	float			driftmove;
	double			laststop;


	float			crouch;					// local amount for smoothing stepups

	qboolean		paused;					// send over by server

	float			punchangle;				// yview kick from weapon firing

	int				intermission;			// full screen, fixed view, etc
	int         	completed_time;			// from time at intermission start

	/*
	 * information that is static for the entire time connected to a server
	 */
	char			model_name[MAX_MODELS][MAX_QPATH];
	char			sound_name[MAX_SOUNDS][MAX_QPATH];

	struct model_s	*model_precache[MAX_MODELS];
	struct sfx_s	*sound_precache[MAX_SOUNDS];

	char			levelname[40];			// for display on solo scoreboard
	int				playernum;
	int				viewentity;

	// refresh related state
	struct model_s	*worldmodel;		// cl_entitites[0].model
	int				num_entities;			// bottom up in cl_entities array

	int				cdtrack;				// cd audio

	entity_t		viewent;
	vec3_t			viewent_origin;
	vec3_t			viewent_angles;
	int				viewent_frame;
	float			viewzoom;			// scales fov and sensitivity

	// all player information
	player_info_t	players[MAX_CLIENTS];
	int				frags_updated;
} client_state_t;


/*
 * cvars
 */
extern struct cvar_s *cl_upspeed;
extern struct cvar_s *cl_forwardspeed;
extern struct cvar_s *cl_backspeed;
extern struct cvar_s *cl_sidespeed;

extern struct cvar_s *cl_movespeedkey;

extern struct cvar_s *cl_yawspeed;
extern struct cvar_s *cl_pitchspeed;

extern struct cvar_s *cl_anglespeedkey;

extern struct cvar_s *cl_shownet;
extern struct cvar_s *cl_hudswap;
extern struct cvar_s *cl_mapname;

extern struct cvar_s *cl_pitchdriftspeed;
extern struct cvar_s *lookspring;
extern struct cvar_s *lookstrafe;
extern struct cvar_s *sensitivity;

extern struct cvar_s *m_pitch;
extern struct cvar_s *m_yaw;
extern struct cvar_s *m_forward;
extern struct cvar_s *m_side;

extern struct cvar_s *m_freelook;
extern struct cvar_s *m_filter;

extern struct cvar_s *_windowed_mouse;

extern struct cvar_s *name;



extern client_state_t cl;

// FIXME, allocate dynamically
extern entity_state_t cl_baselines[MAX_EDICTS];
extern lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
extern dlight_t cl_dlights[MAX_DLIGHTS];

extern qboolean nomaster;
extern float server_version;	// version of server we connected to

//============================================================================

/*
 * cl_main
 */
dlight_t *CL_AllocDlight (int key);
void CL_DecayLights (void);

void CL_Init_Cvars (void);
void CL_Init (void);

void CL_EstablishConnection (char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);
qboolean CL_DemoBehind (void);

void CL_BeginServerConnect (void);

#define MAX_STATIC_ENTITIES	128
#define MAX_TMP_ENTITIES	1024

extern int cl_num_static_entities;
extern entity_t	cl_static_entities[MAX_STATIC_ENTITIES];

extern int cl_num_tmp_entities;
extern entity_t	cl_tmp_entities[MAX_TMP_ENTITIES];

extern entity_t	cl_network_entities[MAX_EDICTS];
extern entity_t	cl_player_entities[MAX_CLIENTS];

extern char	emodel_name[], pmodel_name[], prespawn_name[], modellist_name[],
			soundlist_name[];

/*
 * cl_input
 */
typedef struct {
	int	down[2];	// key nums holding it down
	int	state;		// low bit is down state
} kbutton_t;

extern kbutton_t in_mlook, in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;

#define freelook (m_freelook->ivalue || (in_mlook.state & 1))

void CL_InputSetRepeatDelay (struct cvar_s *var);
void CL_InputSetRepeatInterval (struct cvar_s *var);
void CL_Input_Init_Cvars(void);
void CL_Input_Init (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);

void CL_ClearState (void);
void CL_ReadPackets (void);

int CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

/*
 * cl_demo.c
 */
void CL_StopPlayback (void);
qboolean CL_GetMessage (void);
void CL_WriteDemoCmd (usercmd_t *pcmd);

void CL_Stop_f (void);
void CL_Record_f (void);
void CL_ReRecord_f (void);
void CL_PlayDemo_f (void);
void CL_TimeDemo_f (void);

/*
 * cl_parse.c
 */
#define NET_TIMINGS 256
#define NET_TIMINGSMASK 255
extern int packet_latency[NET_TIMINGS];
int CL_CalcNet (void);
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot);
qboolean CL_CheckOrDownloadFile (char *filename);
qboolean CL_IsUploading (void);
void CL_NextUpload (void);
void CL_StartUpload (Uint8 *data, int size);
void CL_StopUpload (void);
void CL_ParseEntityLump (char *entdata);
void CL_ProcessServerInfo (void);

/*
 * view.c
 */
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_RenderView (void);
void V_UpdatePalette (void);
void V_Register (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);
void V_AddEntity ( entity_t *ent );
void V_ClearEntities ( void );

/*
 * cl_tent
 */
void CL_TEnts_Init_Cvars (void);
void CL_TEnts_Init (void);
void CL_ClearTEnts (void);

/*
 * cl_ents.c
 */
void CL_SetSolidPlayers (void);
void CL_SetUpPlayerPrediction (qboolean dopred);
void CL_EmitEntities (void);
void CL_ClearProjectiles (void);
void CL_ParseProjectiles (void);
void CL_ParsePacketEntities (qboolean delta);
void CL_SetSolidEntities (void);
void CL_ParsePlayerinfo (void);
void CL_Update_Matrices (entity_t *ent);
void CL_Update_Matrices_C (entity_common_t *ent);
qboolean CL_Update_OriginAngles (entity_t *ent, vec3_t origin, vec3_t angles, float time);
void CL_Lerp_OriginAngles (entity_t *ent);
qboolean CL_Update_Frame (entity_t *e, int frame, float frame_time);
qboolean CL_Update_Frame_C (entity_common_t *e, int frame, float frame_time);

/*
 * cl_pred.c
 */
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_PredictUsercmd (int id, player_state_t * from, player_state_t * to,
		usercmd_t *u, qboolean spectator);

/*
 * cl_cam.c
 */
#define	CAM_NONE	0
#define	CAM_TRACK	1

extern int autocam;
extern int spec_track;	// player# of who we are tracking

qboolean Cam_DrawViewModel (void);
qboolean Cam_DrawPlayer (int playernum);
void Cam_Track (usercmd_t *cmd);
void Cam_FinishMove (usercmd_t *cmd);
void Cam_Reset (void);
void CL_InitCam (void);

/*
 * skin.c
 */
void CL_InitSkins (void);
skin_t *Skin_Load (char *skin_name);
void Skin_Skins_f (void);
void Skin_AllSkins_f (void);
void Skin_NextDownload (void);

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

#endif // __CLIENT_H

