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

#include "qtypes.h"
#include "common.h"
#include "light.h"
#include "render.h"
#include "zone.h"

typedef struct {
	vec3_t	viewangles;

	// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

typedef struct {
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct {
	char		name[MAX_SCOREBOARDNAME];
	float		entertime;
	int			frags;
	int			colors;						// two 4 bit fields
	colormap_t	colormap;
} scoreboard_t;

typedef struct {
	int		destcolor[3];
	int		percent;	// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#define	NAME_LENGTH	64

//
// client_state_t should hold all pieces of the client state
//

#define	SIGNONS		4					// signon messages to receive before
										// connected
#define	MAX_BEAMS	24
typedef struct {
	int				entity;
	qboolean		lightning;
	model_t			*model;
	float			endtime;
	vec3_t			start, end, diff;
} beam_t;

#define	MAX_EFRAGS		640

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
	ca_dedicated,						// a dedicated server with no ability
										// to start a client
	ca_disconnected,					// full screen console with no
										// connection
	ca_connected						// valid netcon, talking to a server
} cactive_t;

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct {
	cactive_t			state;

	// personalization data sent to server  
	char				mapstring[MAX_QPATH];
	char				spawnparms[MAX_MAPSTRING];	// to restart a level

	// demo loop control
	int					demonum;					// -1 = don't play demos
	char				demos[MAX_DEMOS][MAX_DEMONAME];	// when not playing

	// demo recording info must be here, because record is started before
	// entering a map (and clearing client_state_t)
	qboolean			demorecording;
	qboolean			demoplayback;
	qboolean			timedemo;
	int					forcetrack;				// -1 = use normal cd track
	FILE				*demofile;
	int					td_lastframe;			// to meter out one message a frame
	int					td_startframe;			// host_framecount at start
	float				td_starttime;			// realtime at second frame of timedemo


	// connection information
	int					signon;					// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t			message;				// writing buffer to send to server

	Uint8				msg_buf[1024];

	double				realtime;
} client_static_t;

extern client_static_t cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct {
	int				movemessages;			// since connecting to this server
											// throw out the first couple, so the player
											// doesn't accidentally do something the first frame

	usercmd_t		cmd;					// last command sent to the server

	// information for local display
	int				stats[MAX_CL_STATS];	// health, etc
	int				items;					// inventory bit flags
	float			item_gettime[32];		// cl.time of aquiring item, for blinking
	float			faceanimtime;			// use anim frame if cl.time < this

	colormap_t		*colormap;				// The colormap for the display.
	cshift_t		cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t		prev_cshifts[NUM_CSHIFTS];	// and content types

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  The server sets punchangle when
	// the view is temporarliy offset, and an angle reset commands at the start
	// of each level and after teleporting.
	vec3_t			mviewangles[2];			// during demo playback viewangles is lerped
	// between these
	vec3_t			viewangles;

	vec3_t			mvelocity[2];			// update by server, used for lean+bob (0 is newest)

	vec3_t			velocity;				// lerped between mvelocity[0] and [1]

	vec3_t			punchangle;				// temporary offset

	// pitch drifting vars
	float			idealpitch;
	float			pitchvel;
	qboolean		nodrift;
	float			driftmove;
	double			laststop;

	float			viewheight;
	float			crouch;					// local amount for smoothing stepups

	qboolean		paused;					// send over by server
	qboolean		onground;
	qboolean		inwater;

	int				intermission;			// don't change view angle, full screen, etc
	int				completed_time;			// latched at intermission start

	double			mtime[2];				// the timestamp of last two messages 
	double			time;					// clients view of time, should be between
											// servertime and oldservertime to generate
											// a lerp point for other data
	double			oldtime;				// previous cl.time, time-oldtime is used
											// to decay light values and smooth step ups


	float			last_received_message;	// (realtime) for net trouble icon

	//
	// information that is static for the entire time connected to a server
	//
	model_t			*model_precache[MAX_MODELS];
	struct sfx_s	*sound_precache[MAX_SOUNDS];

	char			levelname[40];			// for display on solo scoreboard
	int				viewentity;				// cl_entitites[cl.viewentity] = player
	Uint8			maxclients;
	int				gametype;

	// refresh related state
	model_t			*worldmodel;				// cl_entitites[0].model
	int				num_entities;			// held in cl_entities array
	int				num_statics;			// held in cl_staticentities array
	entity_t		viewent;				// the gun model

	int				cdtrack, looptrack;		// cd audio
	float			viewzoom;				// scales fov and sensitivity

	// frag scoreboard
	scoreboard_t	*scores;				// [cl.maxclients]
} client_state_t;


//
// cvars
//
extern struct cvar_s *_cl_name;
extern struct cvar_s *_cl_color;

extern struct cvar_s *cl_upspeed;
extern struct cvar_s *cl_forwardspeed;
extern struct cvar_s *cl_backspeed;
extern struct cvar_s *cl_sidespeed;

extern struct cvar_s *cl_movespeedkey;

extern struct cvar_s *cl_yawspeed;
extern struct cvar_s *cl_pitchspeed;

extern struct cvar_s *cl_anglespeedkey;

extern struct cvar_s *cl_shownet;
extern struct cvar_s *cl_nolerp;

extern struct cvar_s *cl_mapname;

extern struct cvar_s *cl_hudswap;
extern struct cvar_s *cl_maxfps;

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

#define	MAX_TEMP_ENTITIES	64			// lightning bolts, etc
#define	MAX_STATIC_ENTITIES	128			// torches, etc

extern memzone_t		*cl_zone;
extern client_state_t	 cl;

// FIXME, allocate dynamically
extern entity_t cl_entities[MAX_EDICTS];
extern entity_t cl_static_entities[MAX_STATIC_ENTITIES];
extern lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
extern dlight_t cl_dlights[MAX_DLIGHTS];
extern entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
extern beam_t cl_beams[MAX_BEAMS];

//=============================================================================

//
// cl_main
//
dlight_t	*CL_AllocDlight (int key);
void		CL_DecayLights (void);

void		CL_Init_Cvars (void);
void		CL_Init (void);

void		CL_EstablishConnection (char *host);
void		CL_Signon1 (void);
void		CL_Signon2 (void);
void		CL_Signon3 (void);
void		CL_Signon4 (void);

void		CL_Disconnect (void);
void		CL_Disconnect_f (void);
void		CL_NextDemo (void);

#define			MAX_VISEDICTS	256

//
// cl_input
//
typedef struct {
	int         down[2];				// key nums holding it down
	int         state;					// low bit is down state
} kbutton_t;

extern kbutton_t in_mlook, in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;

#define freelook (m_freelook->ivalue || (in_mlook.state & 1))

void	CL_Input_Init (void);
void	CL_Input_Init_Cvars (void);
void	CL_SendCmd (void);
void	CL_SendMove (usercmd_t *cmd);

void	CL_ParseTEnt (void);
void	CL_UpdateTEnts (void);

void	CL_ClearState (void);


int		CL_ReadFromServer (void);
void	CL_WriteToServer (usercmd_t *cmd);
void	CL_BaseMove (usercmd_t *cmd);


float	CL_KeyState (kbutton_t *key);
char	*Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void	CL_StopPlayback (void);
int		CL_GetMessage (void);

void	CL_Stop_f (void);
void	CL_Record_f (void);
void	CL_PlayDemo_f (void);
void	CL_TimeDemo_f (void);

//
// cl_parse.c
//
void	CL_ParseServerMessage (void);
void	CL_NewTranslation (int slot);
void	CL_ParseEntityLump (char *entdata);

//
// view
//
void	V_StartPitchDrift (void);
void	V_StopPitchDrift (void);

void	V_RenderView (void);
void	V_UpdatePalette (void);
void	V_Register (void);
void	V_ParseDamage (void);
void	V_SetContentsColor (int contents);
void	V_AddEntity ( entity_t *ent );
void	V_ClearEntities ( void );

//
// cl_ents
//
extern entity_t *traceline_entity[MAX_EDICTS];
extern int traceline_entities;

void CL_ScanForBModels (void);
void CL_Update_Matrices (entity_t *ent);
void CL_Update_Matrices_C (entity_common_t *ent);
void CL_Lerp_OriginAngles (entity_t *ent);
qboolean CL_Update_OriginAngles (entity_t *ent, vec3_t origin, vec3_t angles, float time);
qboolean CL_Update_Frame (entity_t *ent, int frame, float frame_time);
qboolean CL_Update_Frame_C (entity_common_t *ent, int frame, float frame_time);

//
// cl_tent
//
void	CL_TEnts_Init_Cvars (void);
void	CL_TEnts_Init (void);
void	CL_SignonReply (void);

#endif // __CLIENT_H

