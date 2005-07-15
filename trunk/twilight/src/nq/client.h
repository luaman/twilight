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

#include "cclient.h"
#include "qtypes.h"
#include "common.h"
#include "render.h"
#include "zone.h"

typedef struct {
	vec3_t	viewangles;

	// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

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

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct {
	// personalization data sent to server  
	char				mapstring[MAX_QPATH];
	char				spawnparms[MAX_MAPSTRING];	// to restart a level

	int					forcetrack;

	// connection information
	int					signon;					// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t			message;				// writing buffer to send to server

	Uint8				msg_buf[1024];
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

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  The server sets punchangle when
	// the view is temporarliy offset, and an angle reset commands at the start
	// of each level and after teleporting.
	vec3_t			mviewangles[2];			// during demo playback viewangles is lerped
	// between these
	vec3_t			viewangles;
	vec3_t			punchangle;

	vec3_t			mvelocity[2];			// update by server, used for lean+bob (0 is newest)

	// pitch drifting vars
	float			idealpitch;

	float			viewheight;

	qboolean		onground;
	qboolean		inwater;

	double			mtime[2];				// the timestamp of last two messages 
	float			last_received_message;	// (realtime) for net trouble icon

	//
	// information that is static for the entire time connected to a server
	//
	int				viewentity;				// cl_entitites[cl.viewentity] = player

	// refresh related state
	int				num_entities;			// held in cl_entities array
	int				num_statics;			// held in cl_staticentities array
	entity_t		viewent;				// the gun model

	int				cdtrack, looptrack;		// cd audio
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


#define	MAX_TEMP_ENTITIES	64			// lightning bolts, etc
#define	MAX_STATIC_ENTITIES	128			// torches, etc

extern memzone_t		*cl_zone;
extern client_state_t	 cl;

// FIXME, allocate dynamically
extern entity_t cl_entities[MAX_EDICTS];
extern entity_t cl_static_entities[MAX_STATIC_ENTITIES];
extern entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
extern beam_t cl_beams[MAX_BEAMS];

//=============================================================================

//
// cl_main
//

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

extern kbutton_t in_strafe;

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
void	CL_ParseEntityLump (const char *entdata);

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

//
// cl_ents
//
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

