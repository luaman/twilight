/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.
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

#include "cclient.h"
#include "quakedef.h"
#include "strlib.h"
#include "cmd.h"
#include "cvar.h"
#include "draw.h"
#include "video.h"
#include "wad.h"
#include "mathlib.h"
#include "gl_arrays.h"

extern void CL_UpdatePings (void);

static image_t     *sb_nums[2][11];
static image_t     *sb_colon, *sb_slash;
static image_t     *sb_ibar;
static image_t     *sb_sbar;
static image_t     *sb_scorebar;

static image_t     *sb_weapons[7][8];	// 0 is active, 1 is owned, 2-5 are
										// flashes
static image_t     *sb_ammo[4];
static image_t     *sb_sigil[4];
static image_t     *sb_armor[3];
static image_t     *sb_items[32];

static image_t     *sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are
										// alive 0 is static,
										// 1 is temporary animation
static image_t     *sb_face_invis;
static image_t     *sb_face_quad;
static image_t     *sb_face_invuln;
static image_t     *sb_face_invis_invuln;

// Needed because SB_NONE is already defined in WinGDI.h
#ifdef SB_NONE
#undef SB_NONE
#endif

#define SB_NONE		0
#define SB_PLAYERS	1
#define SB_TEAMS	2
static int			hud_scoreboard;

int         sb_lines;					// scan lines to draw

static image_t     *rsb_invbar[2];
static image_t     *rsb_weapons[5];
static image_t     *rsb_items[2];
static image_t     *rsb_ammo[3];
static image_t     *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
static image_t     *hsb_weapons[7][5];	// 0 is active, 1 is owned, 2-5 are
										// flashes
//MED 01/04/97 added array to simplify weapon parsing
#if 0
static int         hipweapons[4] =
	{ HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT };
#endif
//MED 01/04/97 added hipnotic items array
static image_t     *hsb_items[2];

static memzone_t	*hud_zone;

void        M_DrawImg (int x, int y, image_t *pic);

cvar_t		*cl_sbar;
cvar_t		*cl_hudswap;
cvar_t		*hud_width;
cvar_t		*hud_height;

typedef struct {
	int			width, height;

	int			sbar_x, sbar_y1, sbar_y2;
	int			hud_x1, hud_x2, hud_y1, hud_y2;
	qboolean	headsup;
} hud_t;

static hud_t	hud;

#define	SBAR_HEIGHT1	24
#define	SBAR_HEIGHT2	24
#define SBAR_HEIGHT		(SBAR_HEIGHT1 + SBAR_HEIGHT2)
#define SBAR_WIDTH		320
#define HUD_HEIGHT1		112
#define HUD_HEIGHT2		44
#define HUD_HEIGHT		(HUD_HEIGHT1 + HUD_HEIGHT2)
#define HUD_WIDTH1		24
#define HUD_WIDTH2		42
#define HUD_WIDTH		(HUD_WIDTH1 + HUD_WIDTH2)

typedef struct {
	char	name[MAX_SCOREBOARDNAME];
	int		frags;
	int		players;
	int		phigh, plow, ptotal;
	double	color[3];
} team_t;

static team_t	*teams;
static int		*teamsort;
static int		n_max_teams = 0, n_teams = 0;

static int		*fragsort;
static int		n_max_users = 0, n_users = 0, n_spectators = 0;

void
HUD_Changed (cvar_t *unused)
{
	unused = unused;

	if (!(hud_width && hud_height && cl_sbar && cl_hudswap))
		return;

	/*
	hud.width = hud_width->ivalue;	
	hud.height = hud_height->ivalue;	
	*/
	hud.width = vid.width_2d;
	hud.height = vid.height_2d;

	hud.sbar_x = 0;
	hud.sbar_y1 = hud.height - SBAR_HEIGHT1;
	hud.sbar_y2 = hud.sbar_y1 - SBAR_HEIGHT2;
	if (!cl_hudswap->ivalue) {
		hud.hud_x1 = hud.width - HUD_WIDTH1;
		hud.hud_x2 = hud.width - HUD_WIDTH2;
	} else {
		hud.hud_x1 = 0;
		hud.hud_x2 = 0;
	}
	hud.hud_y1 = hud.height - (SBAR_HEIGHT * 2) - HUD_HEIGHT;
	hud.hud_y2 = hud.hud_y1 + HUD_HEIGHT1;

	if (cl_sbar->ivalue)
		hud.headsup = false;
	else
		hud.headsup = true;
}

/*
===============
Tab key down
===============
*/
static void
HUD_ShowScores (void)
{
	if (hud_scoreboard)
		return;

	hud_scoreboard = SB_PLAYERS;
}

/*
===============
Tab key down
===============
*/
static void
HUD_ShowTeamScores (void)
{
	if (hud_scoreboard)
		return;

	hud_scoreboard = SB_TEAMS;
}

/*
===============
Tab key up
===============
*/
static void
HUD_DontShowScores (void)
{
	hud_scoreboard = SB_NONE;
}

void
HUD_Init_Cvars (void)
{
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, HUD_Changed);
	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, HUD_Changed);
	hud_width = Cvar_Get ("hud_width", "640", CVAR_ARCHIVE, HUD_Changed);
	hud_height = Cvar_Get ("hud_height", "480", CVAR_ARCHIVE, HUD_Changed);
}

static image_t *
HUD_LoadIMG (char *name)
{
	image_t	*img = Image_Load (name, TEX_UPLOAD | TEX_ALPHA);

	/*
	if (!img)
		Com_Printf("ERROR! CAN NOT LOAD -%s-\n", name);
		*/

	return img;
}

void
HUD_Init (void)
{
	int			i;

	hud_zone = Zone_AllocZone("HUD");

	HUD_Changed (cl_sbar);

	for (i = 0; i < 10; i++)
	{
		sb_nums[0][i] = HUD_LoadIMG (va ("gfx/num_%i", i));
		sb_nums[1][i] = HUD_LoadIMG (va ("gfx/anum_%i", i));
	}

	sb_nums[0][10] = HUD_LoadIMG ("gfx/num_minus");
	sb_nums[1][10] = HUD_LoadIMG ("gfx/anum_minus");

	sb_colon = HUD_LoadIMG ("gfx/num_colon");
	sb_slash = HUD_LoadIMG ("gfx/num_slash");

	sb_weapons[0][0] = HUD_LoadIMG ("gfx/inv_shotgun");
	sb_weapons[0][1] = HUD_LoadIMG ("gfx/inv_sshotgun");
	sb_weapons[0][2] = HUD_LoadIMG ("gfx/inv_nailgun");
	sb_weapons[0][3] = HUD_LoadIMG ("gfx/inv_snailgun");
	sb_weapons[0][4] = HUD_LoadIMG ("gfx/inv_rlaunch");
	sb_weapons[0][5] = HUD_LoadIMG ("gfx/inv_srlaunch");
	sb_weapons[0][6] = HUD_LoadIMG ("gfx/inv_lightng");

	sb_weapons[1][0] = HUD_LoadIMG ("gfx/inv2_shotgun");
	sb_weapons[1][1] = HUD_LoadIMG ("gfx/inv2_sshotgun");
	sb_weapons[1][2] = HUD_LoadIMG ("gfx/inv2_nailgun");
	sb_weapons[1][3] = HUD_LoadIMG ("gfx/inv2_snailgun");
	sb_weapons[1][4] = HUD_LoadIMG ("gfx/inv2_rlaunch");
	sb_weapons[1][5] = HUD_LoadIMG ("gfx/inv2_srlaunch");
	sb_weapons[1][6] = HUD_LoadIMG ("gfx/inv2_lightng");

	for (i = 0; i < 5; i++)
	{
		sb_weapons[2 + i][0] = HUD_LoadIMG (va ("gfx/inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] = HUD_LoadIMG (va ("gfx/inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] = HUD_LoadIMG (va ("gfx/inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] = HUD_LoadIMG (va ("gfx/inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] = HUD_LoadIMG (va ("gfx/inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] = HUD_LoadIMG (va ("gfx/inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] = HUD_LoadIMG (va ("gfx/inva%i_lightng", i + 1));
	}

	sb_ammo[0] = HUD_LoadIMG ("gfx/sb_shells");
	sb_ammo[1] = HUD_LoadIMG ("gfx/sb_nails");
	sb_ammo[2] = HUD_LoadIMG ("gfx/sb_rocket");
	sb_ammo[3] = HUD_LoadIMG ("gfx/sb_cells");

	sb_armor[0] = HUD_LoadIMG ("gfx/sb_armor1");
	sb_armor[1] = HUD_LoadIMG ("gfx/sb_armor2");
	sb_armor[2] = HUD_LoadIMG ("gfx/sb_armor3");

	sb_items[0] = HUD_LoadIMG ("gfx/sb_key1");
	sb_items[1] = HUD_LoadIMG ("gfx/sb_key2");
	sb_items[2] = HUD_LoadIMG ("gfx/sb_invis");
	sb_items[3] = HUD_LoadIMG ("gfx/sb_invuln");
	sb_items[4] = HUD_LoadIMG ("gfx/sb_suit");
	sb_items[5] = HUD_LoadIMG ("gfx/sb_quad");

	sb_sigil[0] = HUD_LoadIMG ("gfx/sb_sigil1");
	sb_sigil[1] = HUD_LoadIMG ("gfx/sb_sigil2");
	sb_sigil[2] = HUD_LoadIMG ("gfx/sb_sigil3");
	sb_sigil[3] = HUD_LoadIMG ("gfx/sb_sigil4");

	sb_faces[4][0] = HUD_LoadIMG ("gfx/face1");
	sb_faces[4][1] = HUD_LoadIMG ("gfx/face_p1");
	sb_faces[3][0] = HUD_LoadIMG ("gfx/face2");
	sb_faces[3][1] = HUD_LoadIMG ("gfx/face_p2");
	sb_faces[2][0] = HUD_LoadIMG ("gfx/face3");
	sb_faces[2][1] = HUD_LoadIMG ("gfx/face_p3");
	sb_faces[1][0] = HUD_LoadIMG ("gfx/face4");
	sb_faces[1][1] = HUD_LoadIMG ("gfx/face_p4");
	sb_faces[0][0] = HUD_LoadIMG ("gfx/face5");
	sb_faces[0][1] = HUD_LoadIMG ("gfx/face_p5");

	sb_face_invis = HUD_LoadIMG ("gfx/face_invis");
	sb_face_invuln = HUD_LoadIMG ("gfx/face_invul2");
	sb_face_invis_invuln = HUD_LoadIMG ("gfx/face_inv2");
	sb_face_quad = HUD_LoadIMG ("gfx/face_quad");

	Cmd_AddCommand ("+showscores", HUD_ShowScores);
	Cmd_AddCommand ("-showscores", HUD_DontShowScores);
	Cmd_AddCommand ("+showteamscores", HUD_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", HUD_DontShowScores);

	sb_sbar = HUD_LoadIMG ("gfx/sbar");
	sb_ibar = HUD_LoadIMG ("gfx/ibar");
	sb_scorebar = HUD_LoadIMG ("gfx/scorebar");

	//MED 01/04/97 added new hipnotic weapons
	switch (ccl.game_type) {
		case GAME_HIPNOTIC:
			hsb_weapons[0][0] = HUD_LoadIMG ("gfx/inv_laser");
			hsb_weapons[0][1] = HUD_LoadIMG ("gfx/inv_mjolnir");
			hsb_weapons[0][2] = HUD_LoadIMG ("gfx/inv_gren_prox");
			hsb_weapons[0][3] = HUD_LoadIMG ("gfx/inv_prox_gren");
			hsb_weapons[0][4] = HUD_LoadIMG ("gfx/inv_prox");

			hsb_weapons[1][0] = HUD_LoadIMG ("gfx/inv2_laser");
			hsb_weapons[1][1] = HUD_LoadIMG ("gfx/inv2_mjolnir");
			hsb_weapons[1][2] = HUD_LoadIMG ("gfx/inv2_gren_prox");
			hsb_weapons[1][3] = HUD_LoadIMG ("gfx/inv2_prox_gren");
			hsb_weapons[1][4] = HUD_LoadIMG ("gfx/inv2_prox");

			for (i = 0; i < 5; i++)
			{
				hsb_weapons[2 + i][0] =
					HUD_LoadIMG (va ("gfx/inva%i_laser", i + 1));
				hsb_weapons[2 + i][1] =
					HUD_LoadIMG (va ("gfx/inva%i_mjolnir", i + 1));
				hsb_weapons[2 + i][2] =
					HUD_LoadIMG (va ("gfx/inva%i_gren_prox", i + 1));
				hsb_weapons[2 + i][3] =
					HUD_LoadIMG (va ("gfx/inva%i_prox_gren", i + 1));
				hsb_weapons[2 + i][4] =
					HUD_LoadIMG (va ("gfx/inva%i_prox", i + 1));
			}

			hsb_items[0] = HUD_LoadIMG ("gfx/sb_wsuit");
			hsb_items[1] = HUD_LoadIMG ("gfx/sb_eshld");
			break;
		case GAME_ROGUE:
			rsb_invbar[0] = HUD_LoadIMG ("gfx/r_invbar1");
			rsb_invbar[1] = HUD_LoadIMG ("gfx/r_invbar2");

			rsb_weapons[0] = HUD_LoadIMG ("gfx/r_lava");
			rsb_weapons[1] = HUD_LoadIMG ("gfx/r_superlava");
			rsb_weapons[2] = HUD_LoadIMG ("gfx/r_gren");
			rsb_weapons[3] = HUD_LoadIMG ("gfx/r_multirock");
			rsb_weapons[4] = HUD_LoadIMG ("gfx/r_plasma");

			rsb_items[0] = HUD_LoadIMG ("gfx/r_shield1");
			rsb_items[1] = HUD_LoadIMG ("gfx/r_agrav1");

			// PGM 01/19/97 - team color border
			rsb_teambord = HUD_LoadIMG ("gfx/r_teambord");
			// PGM 01/19/97 - team color border

			rsb_ammo[0] = HUD_LoadIMG ("gfx/r_ammolava");
			rsb_ammo[1] = HUD_LoadIMG ("gfx/r_ammomulti");
			rsb_ammo[2] = HUD_LoadIMG ("gfx/r_ammoplasma");
			break;
		default:
			break;
	}
}


void
HUD_Shutdown (void)
{
	int			i;

	for (i = 0; i < 10; i++)
	{
		Image_Free(sb_nums[0][i], true);
		Image_Free(sb_nums[1][i], true);
	}

	Image_Free(sb_nums[0][10], true);
	Image_Free(sb_nums[1][10], true);

	Image_Free(sb_colon, true);
	Image_Free(sb_slash, true);

	Image_Free(sb_weapons[0][0], true);
	Image_Free(sb_weapons[0][1], true);
	Image_Free(sb_weapons[0][2], true);
	Image_Free(sb_weapons[0][3], true);
	Image_Free(sb_weapons[0][4], true);
	Image_Free(sb_weapons[0][5], true);
	Image_Free(sb_weapons[0][6], true);

	Image_Free(sb_weapons[1][0], true);
	Image_Free(sb_weapons[1][1], true);
	Image_Free(sb_weapons[1][2], true);
	Image_Free(sb_weapons[1][3], true);
	Image_Free(sb_weapons[1][4], true);
	Image_Free(sb_weapons[1][5], true);
	Image_Free(sb_weapons[1][6], true);

	for (i = 0; i < 5; i++)
	{
		Image_Free(sb_weapons[2 + i][0], true);
		Image_Free(sb_weapons[2 + i][1], true);
		Image_Free(sb_weapons[2 + i][2], true);
		Image_Free(sb_weapons[2 + i][3], true);
		Image_Free(sb_weapons[2 + i][4], true);
		Image_Free(sb_weapons[2 + i][5], true);
		Image_Free(sb_weapons[2 + i][6], true);
	}

	Image_Free(sb_ammo[0], true);
	Image_Free(sb_ammo[1], true);
	Image_Free(sb_ammo[2], true);
	Image_Free(sb_ammo[3], true);

	Image_Free(sb_armor[0], true);
	Image_Free(sb_armor[1], true);
	Image_Free(sb_armor[2], true);

	Image_Free(sb_items[0], true);
	Image_Free(sb_items[1], true);
	Image_Free(sb_items[2], true);
	Image_Free(sb_items[3], true);
	Image_Free(sb_items[4], true);
	Image_Free(sb_items[5], true);

	Image_Free(sb_sigil[0], true);
	Image_Free(sb_sigil[1], true);
	Image_Free(sb_sigil[2], true);
	Image_Free(sb_sigil[3], true);

	Image_Free(sb_faces[4][0], true);
	Image_Free(sb_faces[4][1], true);
	Image_Free(sb_faces[3][0], true);
	Image_Free(sb_faces[3][1], true);
	Image_Free(sb_faces[2][0], true);
	Image_Free(sb_faces[2][1], true);
	Image_Free(sb_faces[1][0], true);
	Image_Free(sb_faces[1][1], true);
	Image_Free(sb_faces[0][0], true);
	Image_Free(sb_faces[0][1], true);

	Image_Free(sb_face_invis, true);
	Image_Free(sb_face_invuln, true);
	Image_Free(sb_face_invis_invuln, true);
	Image_Free(sb_face_quad, true);

	Image_Free(sb_sbar, true);
	Image_Free(sb_ibar, true);
	Image_Free(sb_scorebar, true);

	Image_Free(hsb_weapons[0][0], true);
	Image_Free(hsb_weapons[0][1], true);
	Image_Free(hsb_weapons[0][2], true);
	Image_Free(hsb_weapons[0][3], true);
	Image_Free(hsb_weapons[0][4], true);

	Image_Free(hsb_weapons[1][0], true);
	Image_Free(hsb_weapons[1][1], true);
	Image_Free(hsb_weapons[1][2], true);
	Image_Free(hsb_weapons[1][3], true);
	Image_Free(hsb_weapons[1][4], true);

	for (i = 0; i < 5; i++)
	{
		Image_Free(hsb_weapons[2 + i][0], true);
		Image_Free(hsb_weapons[2 + i][1], true);
		Image_Free(hsb_weapons[2 + i][2], true);
		Image_Free(hsb_weapons[2 + i][3], true);
		Image_Free(hsb_weapons[2 + i][4], true);
	}

	Image_Free(hsb_items[0], true);
	Image_Free(hsb_items[1], true);

	Image_Free(rsb_invbar[0], true);
	Image_Free(rsb_invbar[1], true);

	Image_Free (rsb_weapons[0], true);
	Image_Free (rsb_weapons[1], true);
	Image_Free (rsb_weapons[2], true);
	Image_Free (rsb_weapons[3], true);
	Image_Free (rsb_weapons[4], true);

	Image_Free (rsb_items[0], true);
	Image_Free (rsb_items[1], true);

	// PGM 01/19/97 - team color border
	Image_Free (rsb_teambord, true);
	// PGM 01/19/97 - team color border

	Image_Free (rsb_ammo[0], true);
	Image_Free (rsb_ammo[1], true);
	Image_Free (rsb_ammo[2], true);

	/*
	Cmd_RemoveCommand ("+showscores", HUD_ShowScores);
	Cmd_RemoveCommand ("-showscores", HUD_DontShowScores);
	Cmd_RemoveCommand ("+showteamscores", HUD_ShowTeamScores);
	Cmd_RemoveCommand ("-showteamscores", HUD_DontShowScores);
	*/

	teams = NULL;
	teamsort = NULL;
	fragsort = NULL;
	n_max_teams = 0;
	n_teams = 0;
	n_max_users = 0;
	n_users = 0;
	n_spectators = 0;
	
	Zone_FreeZone (&hud_zone);
}


static void
HUD_DrawNum (int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	l = snprintf(str, sizeof(str), "%d", num);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		if (*ptr == '-')
			frame = 10;
		else
			frame = *ptr - '0';

		Draw_Img (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

//=============================================================================

static void
HUD_SortFrags (void)
{
	int			i, j, k, f1, f2;
	user_info_t	*u1, *u2;

	if (ccl.user_flags & USER_FLAG_SORTED)
		return;

	if (n_max_users < ccl.max_users) {
		n_max_users = ccl.max_users;
		if (fragsort)
			Zone_Free(fragsort);
		fragsort = Zone_Alloc(hud_zone, sizeof(*fragsort) * n_max_users);
	}

	// sort by frags
	n_users = 0;
	for (i = 0; i < ccl.max_users; i++)
		if (ccl.users[i].user_id >= 0)
			fragsort[n_users++] = i;

	for (i = 0; i < n_users; i++)
		for (j = 0; j < n_users - 1 - i; j++) {
			u1 = &ccl.users[fragsort[j]];
			if (u1->flags & USER_SPECTATOR) f1 = -9999;
			else if (!u1->name[0]) f1 = -9999;
			else f1 = u1->frags;
			u2 = &ccl.users[fragsort[j + 1]];
			if (u2->flags & USER_SPECTATOR) f2 = -9999;
			else if (!u2->name[0]) f2 = -9999;
			else f2 = u2->frags;
			if (f1 < f2) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}

	ccl.user_flags |= USER_FLAG_SORTED;
}

static void
HUD_SortTeamFrags (void)
{
	int			i, j, k;
	user_info_t	*user;
	team_t		*team;
	qboolean	found;

	if (ccl.user_flags & USER_FLAG_TEAM_SORTED)
		return;

	n_teams = 0;

	if (ccl.game_teams != GAME_TEAMS)
		return;

	n_max_teams = ccl.max_users;
	if (teams)
		Zone_Free(teams);
	if (teamsort)
		Zone_Free(teamsort);
	teams = Zone_Alloc(hud_zone, sizeof(*teams) * n_max_teams);
	teamsort = Zone_Alloc(hud_zone, sizeof(*teamsort) * n_max_teams);

	for (i = 0; i < n_max_teams; i++) {
		teams[i].plow = 9999;
		teams[i].phigh = -9999;
	}

	n_spectators = 0;

	for (i = 0; i < n_max_users; i++) {
		user = &ccl.users[i];
		if (!user->name[0])
			continue;
		if (user->flags & USER_SPECTATOR) {
			n_spectators++;
			user->team_num = TEAM_SPECTATOR;
			continue;
		}

		found = false;

		for (j = 0; j < n_teams; j++) {
			if (!strcmp (teams[j].name, user->team)) {
				team = &teams[j];
				user->team_num = j;
				goto found_team;
			}
		}
		user->team_num = n_teams;
		team = &teams[n_teams++];
		strlcpy(team->name, user->team, MAX_SCOREBOARDNAME);
found_team:
		VectorAdd (user->color_map.bottom, team->color, team->color);
		team->frags += user->frags;
		team->players++;
		if (ccl.user_flags & USER_FLAG_PL_PING) {
			if (team->plow > user->ping)
				team->plow = user->ping;
			if (team->phigh < user->ping)
				team->phigh = user->ping;
			team->ptotal += user->ping;
		}
	}

	for (i = 0; i < n_teams; i++) {
		teamsort[i] = i;
		for (j = 0; j < 3; j++)
			teams[i].color[j] /= teams[i].players;
	}

	for (i = 0; i < n_teams; i++)
		for (j = i + 1; j < n_teams; j++)
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}

	ccl.user_flags |= USER_FLAG_TEAM_SORTED;
}

static void
HUD_Draw_MiniScoreboard (void)
{
	int			i, j, n_lines;
	int			x, y, base_x, base_y;
	user_info_t	*user;
	team_t		*team;
	vec4_t		color;

	HUD_SortFrags ();
	HUD_SortTeamFrags ();

	base_x = 324;
	base_y = hud.height - sb_lines;
	n_lines = sb_lines / 8;

	if (ccl.users[ccl.player_num].flags & USER_SPECTATOR)
		i = 0;
	else {
		for (i = 0; i < n_users; i++)
			if (fragsort[i] == ccl.player_num)
				break;

		i -= n_lines / 2;
		if (i < 0)
			i = 0;
	}

	y = base_y;

	for (j = 0; (i < n_users) && (j < n_lines); i++) {
		user = &ccl.users[fragsort[i]];
		if (!user->name[0])
			continue;
		if (user->flags & USER_SPECTATOR)
			continue;

		j++;

		x = base_x;

		VectorScale(user->color_map.top, 0.5, color);
		Draw_Fill(x, y, 40, 4, color);
		VectorScale(user->color_map.bottom, 0.5, color);
		Draw_Fill(x, y + 4, 40, 4, color);
		if (fragsort[i] == ccl.player_num)
			Draw_String(x, y, va("\020%3d\021", user->frags), 8);
		else
			Draw_String(x + 8, y, va("%3d", user->frags), 8);
		x += (6 * 8);

		if (ccl.game_teams == GAME_TEAMS) {
			Draw_String_Len(x, y, user->team, 4, 8);
			x += (5 * 8);
		}

		Draw_String(x, y, user->name, 8);
		y += 8;
	}

	y = base_y;
	base_x += 208;
	x = base_x;
	for (j = 0; (j < n_lines); j++, y += 8)
		Draw_Character (x, y, 14, 8);
	base_x += 16;

	y = base_y;
	for (i = 0; (i < n_teams) && (i < n_lines); i++) {
		team = &teams[teamsort[i]];
		x = base_x;

		if (teamsort[i] == ccl.users[ccl.player_num].team_num)
			Draw_String(x, y, va("\020%4.4s\020", team->name), 8);
		else
			Draw_String(x, y, va(" %4.4s", team->name), 8);

		x += 6 * 8;
		Draw_String(x, y, va("%5d", team->frags), 8);
		y += 8;
	}
}

static void
HUD_Draw_Scoreboard (int start_x, int start_y, int team)
{
	int			i, min;
	int			x, y;
	user_info_t	*user;
	vec4_t		color;

	if ((team == TEAM_SHOW) && !(ccl.user_flags & USER_FLAG_NO_TEAM_NAME))
		team = TEAM_NOSHOW;

	HUD_SortFrags ();

	x = start_x;
	y = start_y;

	if (ccl.user_flags & USER_FLAG_PL_PING) {
		Draw_String(x, y,       "Ping PL", 8);
		Draw_Conv_String(x, y+8,"(--) ()", 8);
		x += 8 * 8;
	}

	Draw_String(x, y,       "Time Score", 8);
	Draw_Conv_String(x, y+8,"(--) (---)", 8);
	x += 11 * 8;

	if (team == TEAM_SHOW) {
		Draw_String(x, y,       "Team", 8);
		Draw_Conv_String(x, y+8,"(--)", 8);
		x += 5 * 8;
	}

	Draw_String(x, y,       "Name", 8);
	Draw_Conv_String(x, y+8,"(--------------)", 8);
	x += 16 * 8;
	y += 16;

	for (i = 0; i < n_users; i++) {
		user = &ccl.users[fragsort[i]];
		if (!user->name[0])
			continue;
		if ((team >= TEAM_SPECTATOR) && (user->team_num != team))
			continue;

		x = start_x;

		if (ccl.user_flags & USER_FLAG_PL_PING) {
			Draw_String(x, y, va("%4d %2d", user->ping, user->pl), 8);
			x += (8 * 8);
		}

		min = (ccl.time - user->entertime) / 60.0f;
		Draw_String_Len(x, y, va("%4d", min), 4, 8);
		x += (5 * 8);

		if (user->flags & USER_SPECTATOR) {
			if (team == TEAM_SHOW) {
				Draw_Alt_String(x, y, "Spectator", 8);
				x += 11 * 8;
			} else {
				Draw_Alt_String(x, y, "Spec", 8);
				x += 6 * 8;
			}
		} else {
			VectorScale(user->color_map.top, 0.5, color);
			Draw_Fill(x, y, 40, 4, color);
			VectorScale(user->color_map.bottom, 0.5, color);
			Draw_Fill(x, y + 4, 40, 4, color);
			if (fragsort[i] == ccl.player_num)
				Draw_String(x, y, va("\020%3d\021", user->frags), 8);
			else
				Draw_String(x + 8, y, va("%3d", user->frags), 8);
			x += (6 * 8);

			if (team == TEAM_SHOW) {
				Draw_String_Len(x, y, user->team, 4, 8);
				x += (5 * 8);
			}
		}

		Draw_String_Len(x, y, user->name, 16, 8);
		y += 10;
	}
}

static void
HUD_Draw_Scoreboard_Team (int base_x, int base_y, int i)
{
	int		x, y;
	team_t	*team = &teams[i];

	x = base_x;
	y = base_y;

	Draw_Conv_String(x, y, "Team|Frags|Players|", 8);
	x += 19 * 8;
	if (ccl.user_flags & USER_FLAG_PL_PING) {
		Draw_Conv_String(x, y, "low/avg/high", 8);
		x += 12 * 8;
	}
	y += 8;
	x = base_x;
	Draw_String(x, y, va("%4.4s|%5d|%7d|", team->name, team->frags, team->players), 8);
	x += 19 * 8;
	if (ccl.user_flags & USER_FLAG_PL_PING)
		Draw_String(x, y, va("%3d/%3d/%3d", team->plow, team->ptotal / team->players, team->phigh), 8);
	y += 16;
	x = base_x;

	HUD_Draw_Scoreboard (x, y, i);
}

static void
HUD_Draw_Scoreboard_Teams ()
{
	int		x[2], y[2], i;
	int		width, height;
	vec4_t	c1, c2;

	width = 26;
	if (ccl.user_flags & USER_FLAG_PL_PING)
		width += 7;
	width += 4;
	width *= 8;

	HUD_SortFrags ();
	HUD_SortTeamFrags ();

	x[0] = 4;
	x[1] = hud.width - width - 4;
	y[0] = y[1] = 48;
	for (i = 0; i < n_teams; i++) {
		height = 8 + 16 + 16 + (teams[i].players * 10) + 16;
		VectorCopy(teams[i].color, c1); c1[3] = 0.5;
		VectorCopy(teams[i].color, c2); c2[3] = 0.0;
		Draw_Box(x[i % 2] + 4, y[i % 2] + 4, width - 4, height - 4,4,c1,c2);
		HUD_Draw_Scoreboard_Team(x[i % 2] + 8, y[i % 2] + 8, i);
		y[i % 2] += height + (2 * 8);
	}

	if (n_spectators) {
		VectorSet4(c1, 0, 0.5, 0, 0.5);
		VectorSet4(c2, 0, 0.5, 0, 0.0);
		height = (n_spectators * 10) + 32;
		Draw_Box(x[i % 2] + 4, y[i % 2] + 4, width - 4, height - 4,4,c1,c2);
		HUD_Draw_Scoreboard(x[i % 2] + 8, y[i % 2] + 8, TEAM_SPECTATOR);
	}
}

static void
HUD_Draw_Standard_HUD_Inventory (void)
{
	Uint	i, j;
	char	num[4];
	int		x1 = hud.hud_x1, x2 = hud.hud_x2, y1 = hud.hud_y1, y2 = hud.hud_y2;
	int		sbar_x = hud.sbar_x, sbar_y2 = hud.sbar_y2, tmp_y;
	int		flashon;

	// The weapons.
	for (i = 0; i < 7; i++) {
		if (ccl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = (int) ((ccl.time - ccl.items_gettime[i]) * 10);
			if (flashon >= 10) {
				if (ccl.stats[STAT_ACTIVEWEAPON] == (signed) (IT_SHOTGUN << i))
					flashon = 1;
				else flashon = 0;
			} else
				flashon = (flashon % 5) + 2;

			Draw_SubImg (x1, y1 + (i * 16), sb_weapons[flashon][i], 0, 0, 24, 16);
		}
	}

	// The ammo counts.
	for (i = 0; i < 4; i++) {
		snprintf(num, sizeof(num), "%3d", ccl.stats[STAT_SHELLS + i]);
		for (j = 0; j < 3; j++)
			if (num[j] != ' ')
				num[j] = 18 + num[j] - '0';
		tmp_y = y2 + (i * 11);
		Draw_SubImg (x2, tmp_y, sb_ibar, 3 + (i * 48), 0, 42, 11);
		Draw_String (x2 + 3, tmp_y, num, 8);
	}

	// Normal items.
	for (i = 0; i < 6; i++)
		if (ccl.stats[STAT_ITEMS] & (IT_KEY1 << i))
			Draw_Img (sbar_x + (192 + i * 16), sbar_y2, sb_items[i]);

	// Sigils.
	for (i = 0; i < 4; i++)
		if (ccl.stats[STAT_ITEMS] & (IT_SIGIL1 << i))
			Draw_Img (sbar_x + (SBAR_WIDTH - 32 + i * 8), sbar_y2, sb_sigil[i]);
}

static void
HUD_Draw_Standard_Sbar_Inventory (void)
{
	Uint	i, j;
	char	num[4];
	int		x = hud.sbar_x, y = hud.sbar_y2;
	int		flashon;
	
	// The backdrop.
	Draw_Img (x, y, sb_ibar);

	// The ammo counts.
	for (i = 0; i < 4; i++) {
		snprintf(num, sizeof(num), "%3d", ccl.stats[STAT_SHELLS + i]);
		for (j = 0; j < 3; j++)
			if (num[j] != ' ')
				num[j] = 18 + num[j] - '0';
		Draw_String (x + ((6 * i + 1) * 8 - 2), y + 1, num, 8);
	}

	// The weapons.
	for (i = 0; i < 7; i++) {
		if (ccl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = (int) ((ccl.time - ccl.items_gettime[i]) * 10);
			if (flashon >= 10) {
				if (ccl.stats[STAT_ACTIVEWEAPON] == (signed) (IT_SHOTGUN << i))
					flashon = 1;
				else flashon = 0;
			} else
				flashon = (flashon % 5) + 2;

			Draw_Img (x + (i * 24), y + 8, sb_weapons[flashon][i]);
		}
	}

	// Normal items.
	for (i = 0; i < 6; i++)
		if (ccl.stats[STAT_ITEMS] & (IT_KEY1 << i))
			Draw_Img (x + (192 + i * 16), y + 8, sb_items[i]);

	// Sigils.
	for (i = 0; i < 4; i++)
		if (ccl.stats[STAT_ITEMS] & (IT_SIGIL1 << i))
			Draw_Img (x + (SBAR_WIDTH - 32 + i * 8), y + 1, sb_sigil[i]);
}

static void
HUD_Draw_Standard_Sbar (void)
{
	int		x = hud.sbar_x, y = hud.sbar_y1;

	if (!hud.headsup)
		Draw_Img (x, y, sb_sbar);

	if (ccl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		HUD_DrawNum (x + 24, y, 666, 3, 1);
		Draw_Img (x, y, draw_disc);
	} else {
		HUD_DrawNum (x + 24, y, ccl.stats[STAT_ARMOR], 3,
				ccl.stats[STAT_ARMOR] <= 25);
		if (ccl.stats[STAT_ITEMS] & IT_ARMOR3)
			Draw_Img (x, y, sb_armor[2]);
		else if (ccl.stats[STAT_ITEMS] & IT_ARMOR2)
			Draw_Img (x, y, sb_armor[1]);
		else if (ccl.stats[STAT_ITEMS] & IT_ARMOR1)
			Draw_Img (x, y, sb_armor[0]);
	}

	if ((ccl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY)) ==
			(IT_INVISIBILITY | IT_INVULNERABILITY))
		Draw_Img (x + 112, y, sb_face_invis_invuln);
	else if (ccl.stats[STAT_ITEMS] & IT_QUAD)
		Draw_Img (x + 112, y, sb_face_quad);
	else if (ccl.stats[STAT_ITEMS] & IT_INVISIBILITY)
		Draw_Img (x + 112, y, sb_face_invis);
	else if (ccl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Draw_Img (x + 112, y, sb_face_invuln);
	else {
		int f = ccl.stats[STAT_HEALTH] / 20;
		
		if (f > 4)
			f = 4;
		if (f < 0)
			f = 0;
		if (ccl.time <= ccl.faceanimtime)
			Draw_Img (x + 112, y, sb_faces[f][1]);
		else
			Draw_Img (x + 112, y, sb_faces[f][0]);
	}

	HUD_DrawNum (x + 136, y, ccl.stats[STAT_HEALTH], 3,
			ccl.stats[STAT_HEALTH] <= 25);

	if (ccl.stats[STAT_ITEMS] & IT_SHELLS)
		Draw_Img (x + 224, y, sb_ammo[0]);
	else if (ccl.stats[STAT_ITEMS] & IT_NAILS)
		Draw_Img (x + 224, y, sb_ammo[1]);
	else if (ccl.stats[STAT_ITEMS] & IT_ROCKETS)
		Draw_Img (x + 224, y, sb_ammo[2]);
	else if (ccl.stats[STAT_ITEMS] & IT_CELLS)
		Draw_Img (x + 224, y, sb_ammo[3]);

	HUD_DrawNum (x + 248, y, ccl.stats[STAT_AMMO], 3,
			ccl.stats[STAT_AMMO] <= 10);

}

void
HUD_Draw_Single_Stats (qboolean completed)
{
	image_t	*pic;
	double	time;
	int		dig, num, x, y;

	x = (vid.width_2d / 2) - 160;
	if (x < 0)
		x = 0;
	y = (vid.height_2d / 2) - 120;
	if (y < 0)
		y = 0;

	if (completed) {
		pic = Draw_CacheImg ("gfx/complete");
		Draw_Img (x + 64, y + 24, pic);
	}

	pic = Draw_CacheImg ("gfx/inter");
	Draw_Img (x + 0, y + 56, pic);

	// time
	if (ccl.completed_time)
		time = ccl.completed_time;
	else
		time = ccl.time;

	dig = time / 60;
	HUD_DrawNum (x + 160, y + 64, dig, 3, 0);
	num = time - dig * 60;
	Draw_Img (x + 234, y + 64, sb_colon);
	Draw_Img (x + 246, y + 64, sb_nums[0][num / 10]);
	Draw_Img (x + 266, y + 64, sb_nums[0][num % 10]);

	HUD_DrawNum (x + 160, y + 104, ccl.stats[STAT_SECRETS], 3, 0);
	Draw_Img (x + 232, y + 104, sb_slash);
	HUD_DrawNum (x + 240, y + 104, ccl.stats[STAT_TOTALSECRETS], 3, 0);

	HUD_DrawNum (x + 160, y + 144, ccl.stats[STAT_MONSTERS], 3, 0);
	Draw_Img (x + 232, y + 144, sb_slash);
	HUD_DrawNum (x + 240, y + 144, ccl.stats[STAT_TOTALMONSTERS], 3, 0);
}

void
HUD_Draw (void)
{
	int show = hud_scoreboard;

	if (ccls.state != ca_active)
		return;

	HUD_Draw_MiniScoreboard ();

	if (ccl.stats[STAT_HEALTH] <= 0)
		show = SB_TEAMS;
	else {
		HUD_Draw_Standard_Sbar ();
		if (sb_lines > 24) {
			if (hud.headsup)
				HUD_Draw_Standard_HUD_Inventory ();
			else
				HUD_Draw_Standard_Sbar_Inventory ();
		}
	}

	if (show) {
		if (ccl.game_teams == GAME_COOP || ccl.game_teams == GAME_SINGLE) {
			HUD_Draw_Single_Stats (false);
			return;
		}

		CL_UpdatePings ();
		if (ccl.game_teams != GAME_TEAMS)
			show = SB_PLAYERS;

		if (show == SB_TEAMS)
			HUD_Draw_Scoreboard_Teams ();
		else
			HUD_Draw_Scoreboard (0, 0, TEAM_NOSHOW);
	}
}

void
HUD_IntermissionOverlay (void)
{
	if (ccls.state != ca_active)
		return;

	switch (ccl.game_teams) {
		case GAME_TEAMS:
			HUD_Draw_Scoreboard_Teams ();;
			break;
		case GAME_DEATHMATCH:
			HUD_Draw_Scoreboard (0, 0, TEAM_NOSHOW);;
			break;
		case GAME_COOP:
		case GAME_SINGLE:
			HUD_Draw_Single_Stats (true);
			break;
	}
}

void
HUD_FinaleOverlay (void)
{
	image_t     *pic;

	if (ccls.state != ca_active)
		return;

	pic = Draw_CacheImg ("gfx/finale");
	Draw_Img ((hud.width - pic->width) / 2, 16, pic);
}
