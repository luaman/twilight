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

#ifndef __CCLIENT_H
#define __CCLIENT_H

#include "quakedef.h"
#include "qtypes.h"
#include "common.h"
#include "light.h"
#include "zone.h"
#include "gl_info.h"
#include "zone.h"

typedef struct {
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;


typedef enum
{
	ca_dedicated,		// NQ - dedicated server
	ca_disconnected,	// not connected at all
	ca_demostart,		// QW - starting a demo
	ca_connected,		// waiting for server data
	ca_onserver,		// QW - processing data, downloading, etc
	ca_active			// world is active
} ca_state_t;

#define USER_SPECTATOR			BIT(0)
#define TEAM_SPECTATOR			-1
#define TEAM_SHOW				-2
#define TEAM_NOSHOW				-3

typedef struct {
	int			user_id;

	char		team[MAX_SCOREBOARDNAME];
	Sint8		team_num;
	char		name[MAX_SCOREBOARDNAME];

	Uint32		flags;

	float		entertime;
	int			frags;

	colormap_t	color_map;
	Uint8		color_top, color_bottom;

	Uint16		ping;
	Uint8		pl;

	char		skin_name[MAX_SKIN_NAME];
	skin_t		*skin;
	
} user_info_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

typedef struct {
	Sint16	destcolor[3];
	Sint16	percent;	// 0-256
} cshift_t;

// these determine which intermission screen plays
typedef enum { GAME_SINGLE, GAME_COOP, GAME_DEATHMATCH, GAME_TEAMS } game_teams_t;
typedef enum { GAME_STANDARD, GAME_HIPNOTIC, GAME_ROGUE } game_type_t;

#define USER_FLAG_SORTED        BIT(0)
#define USER_FLAG_TEAM_SORTED   BIT(1)
#define USER_FLAG_PL_PING		BIT(2)
#define USER_FLAG_NO_TEAM_NAME	BIT(3)

typedef struct client_common_s {
	game_teams_t	game_teams;
	game_type_t		game_type;

	int				max_users;
	user_info_t		*users;
	Uint32			user_flags;
	int				player_num;

	Sint32			stats[MAX_CL_STATS];
	float			items_gettime[32];
	float			faceanimtime;

	int				intermission;
	int				completed_time;

	double			time, oldtime;

	cshift_t		cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t		prev_cshifts[NUM_CSHIFTS];	// and content types

	colormap_t		*colormap;

	model_t			*worldmodel;
	char			levelname[40];
} client_common_t;

extern client_common_t	 ccl;

typedef struct client_common_static_s {
	ca_state_t		state;
} client_common_static_t;

extern client_common_static_t	 ccls;

extern memzone_t *ccl_zone;
extern void CCL_Init_Cvars (void);
extern void CCL_Init (void);

#endif // __CCLIENT_H

