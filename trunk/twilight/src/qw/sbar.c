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
#include "cvar.h"
#include "draw.h"
#include "mathlib.h"
#include "sbar.h"
#include "screen.h"
#include "strlib.h"
#include "vid.h"
#include "wad.h"


// num frame for '-' stats digit
#define STAT_MINUS		10

static qpic_t     *sb_nums[2][11];
static qpic_t     *sb_colon, *sb_slash;
static qpic_t     *sb_ibar;
static qpic_t     *sb_sbar;
static qpic_t     *sb_scorebar;

// 0 is active, 1 is owned, 2-5 are flashes
static qpic_t     *sb_weapons[7][8];

static qpic_t     *sb_ammo[4];
static qpic_t     *sb_sigil[4];
static qpic_t     *sb_armor[3];
static qpic_t     *sb_items[32];

// 0 is gibbed, 1 is dead, 2-6 are alive
static qpic_t     *sb_faces[7][2];

static qpic_t     *sb_face_invis;
static qpic_t     *sb_face_quad;
static qpic_t     *sb_face_invuln;
static qpic_t     *sb_face_invis_invuln;

static qboolean    sb_showscores;
static qboolean    sb_showteamscores;

static void        Sbar_DeathmatchOverlay (int start);
static void        Sbar_TeamOverlay (void);
static void        Sbar_MiniDeathmatchOverlay (void);

static qboolean largegame = false;

cvar_t		*cl_sbar;

/*
===============
Tab key down
===============
*/
static void
Sbar_ShowTeamScores (void)
{
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
}

/*
===============
Tab key up
===============
*/
static void
Sbar_DontShowTeamScores (void)
{
	sb_showteamscores = false;
}

/*
===============
Tab key down
===============
*/
static void
Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
}

/*
===============
Tab key up
===============
*/
static void
Sbar_DontShowScores (void)
{
	sb_showscores = false;
}

void
Sbar_Init_Cvars (void)
{
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, NULL);
}

void
Sbar_Init (void)
{
	int         i;

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = Draw_PicFromWad (va ("num_%i", i));
		sb_nums[1][i] = Draw_PicFromWad (va ("anum_%i", i));
	}

	sb_nums[0][10] = Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = Draw_PicFromWad ("anum_minus");

	sb_colon = Draw_PicFromWad ("num_colon");
	sb_slash = Draw_PicFromWad ("num_slash");

	sb_weapons[0][0] = Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = Draw_PicFromWad ("inv2_lightng");

	for (i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] = Draw_PicFromWad (va ("inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] = Draw_PicFromWad (va ("inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] = Draw_PicFromWad (va ("inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] = Draw_PicFromWad (va ("inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] = Draw_PicFromWad (va ("inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] = Draw_PicFromWad (va ("inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] = Draw_PicFromWad (va ("inva%i_lightng", i + 1));
	}

	sb_ammo[0] = Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = Draw_PicFromWad ("sb_cells");

	sb_armor[0] = Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = Draw_PicFromWad ("sb_armor3");

	sb_items[0] = Draw_PicFromWad ("sb_key1");
	sb_items[1] = Draw_PicFromWad ("sb_key2");
	sb_items[2] = Draw_PicFromWad ("sb_invis");
	sb_items[3] = Draw_PicFromWad ("sb_invuln");
	sb_items[4] = Draw_PicFromWad ("sb_suit");
	sb_items[5] = Draw_PicFromWad ("sb_quad");

	sb_sigil[0] = Draw_PicFromWad ("sb_sigil1");
	sb_sigil[1] = Draw_PicFromWad ("sb_sigil2");
	sb_sigil[2] = Draw_PicFromWad ("sb_sigil3");
	sb_sigil[3] = Draw_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = Draw_PicFromWad ("face1");
	sb_faces[4][1] = Draw_PicFromWad ("face_p1");
	sb_faces[3][0] = Draw_PicFromWad ("face2");
	sb_faces[3][1] = Draw_PicFromWad ("face_p2");
	sb_faces[2][0] = Draw_PicFromWad ("face3");
	sb_faces[2][1] = Draw_PicFromWad ("face_p3");
	sb_faces[1][0] = Draw_PicFromWad ("face4");
	sb_faces[1][1] = Draw_PicFromWad ("face_p4");
	sb_faces[0][0] = Draw_PicFromWad ("face5");
	sb_faces[0][1] = Draw_PicFromWad ("face_p5");

	sb_face_invis = Draw_PicFromWad ("face_invis");
	sb_face_invuln = Draw_PicFromWad ("face_invul2");
	sb_face_invis_invuln = Draw_PicFromWad ("face_inv2");
	sb_face_quad = Draw_PicFromWad ("face_quad");

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores);

	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");
}


//=============================================================================

// drawing routines are reletive to the status bar location

static void
Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x, y + (vid.height_2d - SBAR_HEIGHT), pic);
}

/*
=============
JACK: Draws a portion of the picture in the status bar.
=============
*/

static void
Sbar_DrawSubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
				 int height)
{
	Draw_SubPic (x, y + (vid.height_2d - SBAR_HEIGHT), pic, srcx, srcy, width,
				 height);
}


/*
================
Draws one solid graphics character
================
*/
static void
Sbar_DrawCharacter (int x, int y, int num)
{
	Draw_Character (x + 4, y + vid.height_2d - SBAR_HEIGHT, num, 8);
}

static void
Sbar_DrawString (int x, int y, char *str)
{
	Draw_String (x, y + vid.height_2d - SBAR_HEIGHT, str, 8);
}

static int
Sbar_itoa (int num, char *buf)
{
	char       *str;
	int         pow10;
	int         dig;

	str = buf;

	if (num < 0) {
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10; num >= pow10; pow10 *= 10);

	do {
		pow10 /= 10;
		dig = num / pow10;
		*str++ = '0' + dig;
		num -= dig * pow10;
	} while (pow10 != 1);

	*str = 0;

	return str - buf;
}


static void
Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		Sbar_DrawPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

//=============================================================================

static int         fragsort[MAX_CLIENTS];
static int         scoreboardlines;
typedef struct {
	char        team[16 + 1];
	int         frags;
	int         players;
	int         plow, phigh, ptotal;
} team_t;
static team_t      teams[MAX_CLIENTS];
static int         teamsort[MAX_CLIENTS];
static int         scoreboardteams;

static void
Sbar_SortFrags ()
{
	int         i, j, k;

	if (cl.frags_updated & FRAGS_SORTED)
		return;

// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;

			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i = 0; i < scoreboardlines; i++)
		for (j = 0; j < scoreboardlines - 1 - i; j++)
			if (cl.players[fragsort[j]].frags <
				cl.players[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}

	cl.frags_updated |= FRAGS_SORTED;
}

static void
Sbar_SortTeams (void)
{
	int         i, j, k;
	player_info_t *s;
	char        t[16 + 1];

	if (cl.frags_updated & FRAGS_TEAM_SORTED)
		return;

// request new ping times every two second
	scoreboardteams = 0;

	if (cl.gametype != GAME_TEAMS)
		return;

// sort the teams
	memset (teams, 0, sizeof (teams));
	for (i = 0; i < MAX_CLIENTS; i++)
		teams[i].plow = 999;

	for (i = 0; i < MAX_CLIENTS; i++) {
		s = &cl.players[i];
		if (!s->name[0])
			continue;
		if (s->spectator)
			continue;

		// find his team in the list
		t[16] = 0;
		strncpy (t, s->team, 16);
		if (!t || !t[0])
			continue;					// not on team
		for (j = 0; j < scoreboardteams; j++)
			if (!strcmp (teams[j].team, t)) {
				teams[j].frags += s->frags;
				teams[j].players++;
				goto addpinginfo;
			}
		if (j == scoreboardteams) {		// must add him
			j = scoreboardteams++;
			strcpy (teams[j].team, t);
			teams[j].frags = s->frags;
			teams[j].players = 1;
		  addpinginfo:
			if (teams[j].plow > s->ping)
				teams[j].plow = s->ping;
			if (teams[j].phigh < s->ping)
				teams[j].phigh = s->ping;
			teams[j].ptotal += s->ping;
		}
	}

	// sort
	for (i = 0; i < scoreboardteams; i++)
		teamsort[i] = i;

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++)
		for (j = i + 1; j < scoreboardteams; j++)
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}

	cl.frags_updated |= FRAGS_TEAM_SORTED;
}


static void
Sbar_SoloScoreboard (void)
{
	int         minutes, seconds, tens, units;
	int         l;
	double		time;

	if (cl.gametype < GAME_DEATHMATCH) {
		Sbar_DrawString (8, 4, va("Monsters:%3i /%3i", cl.stats[STAT_MONSTERS],
					cl.stats[STAT_TOTALMONSTERS]));

		Sbar_DrawString (8, 12, va("Secrets :%3i /%3i", cl.stats[STAT_SECRETS],
					cl.stats[STAT_TOTALSECRETS]));
	}

// time
	time = cls.realtime;
	if (cl.gametype < GAME_DEATHMATCH)
		time -= cl.players[cl.playernum].entertime;
	minutes = time / 60;
	seconds = time - 60 * minutes;
	tens = seconds / 10;
	units = seconds - 10 * tens;
	Sbar_DrawString (184, 4, va("Time :%3i:%i%i", minutes, tens, units));

// draw level name
	if (cl.gametype < GAME_DEATHMATCH) {
		l = strlen (cl.levelname);
		Sbar_DrawString (232 - l * 4, 12, cl.levelname);
	}
}

//=============================================================================

static void
Sbar_DrawInventory (void)
{
	int         i, j, y;
	char        num[6];
	float       time;
	int         flashon;
	qboolean    headsup;
	qboolean    hudswap;

	headsup = !cl_sbar->ivalue;
	hudswap = cl_hudswap->ivalue;

	if (!headsup)
		Sbar_DrawPic (0, -24, sb_ibar);
// weapons
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			time = cl.item_gettime[i];
			flashon = (int) ((cl.time - time) * 10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10) {
				if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
					flashon = 1;
				else
					flashon = 0;
			} else
				flashon = (flashon % 5) + 2;

			if (headsup) {
				if (i || vid.height_2d > 200)
					Sbar_DrawSubPic ((hudswap) ? 0 : (vid.width_2d - 24),
							-68 - (7 - i) * 16, sb_weapons[flashon][i],
							0, 0, 24, 16);

			} else
				Sbar_DrawPic (i * 24, -16, sb_weapons[flashon][i]);
		}
	}

// ammo counts
	for (i = 0; i < 4; i++) {
		snprintf (num, sizeof (num), "%3i", cl.stats[STAT_SHELLS + i]);
		for (j = 0; j < 3; j++)
			num[j] = (num[j] == 32 ? ' ' : 18 + num[j] - '0');
		if (headsup) {
			y = -24 - (4 - i) * 11;
			if (hudswap) {
				Sbar_DrawSubPic (0, y, sb_ibar, 3 + (i * 48), 0, 42, 11);
				Sbar_DrawString (3, y, num);
			} else {
				Sbar_DrawSubPic (vid.width_2d - 42, y, sb_ibar, 3 + (i * 48),
						0, 42, 11);
				Sbar_DrawString (vid.width_2d - 39, y, num);
			}
		} else
			Sbar_DrawString ((6 * i + 1) * 8 - 2, -24, num);
	}

	flashon = 0;
// items
	for (i = 0; i < 6; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			Sbar_DrawPic (192 + i * 16, -16, sb_items[i]);
		}
// sigils
	for (i = 0; i < 4; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
			time = cl.item_gettime[28 + i];
			Sbar_DrawPic (320 - 32 + i * 8, -16, sb_sigil[i]);
		}
}

//=============================================================================

static void
Sbar_DrawFrags (void)
{
	int         i, k, l;
	int         x, y, f;
	player_info_t *s;
	vec4_t		color;

	if (cl.gametype < GAME_DEATHMATCH)
		return;

	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
	y = vid.height_2d - SBAR_HEIGHT - 23;

	color[3] = 1.0f;
	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;
		if (s->spectator)
			continue;

		// draw background
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (x * 8 + 10, y, 28, 4, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (x * 8 + 10, y + 4, 28, 3, color);

		// draw number
		f = s->frags;
		Sbar_DrawString ((x + 1) * 8, -24, va ("%3i", f));

		if (k == cl.playernum) {
			Sbar_DrawCharacter (x * 8 + 2, -24, 16);
			Sbar_DrawCharacter ((x + 4) * 8 - 4, -24, 17);
		}
		x += 4;
	}
}

//=============================================================================


static void
Sbar_DrawFace (void)
{
	int         f, anim;

	if ((cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY))
		== (IT_INVISIBILITY | IT_INVULNERABILITY)) {
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		Sbar_DrawPic (112, 0, sb_face_quad);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		Sbar_DrawPic (112, 0, sb_face_invis);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

	if (cl.stats[STAT_HEALTH] >= 100)
		f = 4;
	else
		f = cl.stats[STAT_HEALTH] / 20;

	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;
	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}

static void
Sbar_DrawNormal (void)
{
	if (cl_sbar->ivalue)
		Sbar_DrawPic (0, 0, sb_sbar);

	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawNum (24, 0, 666, 3, 1);
		Sbar_DrawPic (0, 0, draw_disc);
	} else {
		Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
					  cl.stats[STAT_ARMOR] <= 25);
		if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
			Sbar_DrawPic (0, 0, sb_armor[2]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
			Sbar_DrawPic (0, 0, sb_armor[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
			Sbar_DrawPic (0, 0, sb_armor[0]);
	}

	// face
	Sbar_DrawFace ();

	// health
	Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3,
				  cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
	if (cl.stats[STAT_ITEMS] & IT_SHELLS)
		Sbar_DrawPic (224, 0, sb_ammo[0]);
	else if (cl.stats[STAT_ITEMS] & IT_NAILS)
		Sbar_DrawPic (224, 0, sb_ammo[1]);
	else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
		Sbar_DrawPic (224, 0, sb_ammo[2]);
	else if (cl.stats[STAT_ITEMS] & IT_CELLS)
		Sbar_DrawPic (224, 0, sb_ammo[3]);

	Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}

void
Sbar_Draw (void)
{
	qboolean    headsup;

	headsup = !cl_sbar->ivalue;

	if (scr_con_current == vid.height_2d)
		return;							// console is full screen

// top line
	if (sb_lines > 24) {
		if (!cl.spectator || autocam == CAM_TRACK)
			Sbar_DrawInventory ();
		if (!headsup || vid.width_2d < 512)
			Sbar_DrawFrags ();
	}
// main area
	if (sb_lines > 0) {
		if (cl.spectator) {
			if (autocam != CAM_TRACK) {
				Sbar_DrawPic (0, 0, sb_scorebar);
				Sbar_DrawString (160 - 7 * 8, 4, "SPECTATOR MODE");
				Sbar_DrawString (160 - 14 * 8 + 4, 12,
								 "Press [ATTACK] for AutoCamera");
			} else {
				if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
					Sbar_DrawPic (0, 0, sb_scorebar);
					Sbar_SoloScoreboard ();
				}
				else
					Sbar_DrawNormal ();

				Sbar_DrawString (0, -8, va("Tracking %-.13s, [JUMP] for next",
							cl.players[spec_track].name));
			}
		} else if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
			Sbar_DrawPic (0, 0, sb_scorebar);
			Sbar_SoloScoreboard ();
		} else
			Sbar_DrawNormal ();
	}
// main screen deathmatch rankings
	// if we're dead show team scores in team games
	if (cl.stats[STAT_HEALTH] <= 0 && !cl.spectator) {
		if (cl.gametype == GAME_TEAMS && !sb_showscores) {
			Sbar_TeamOverlay ();
		} else {
			Sbar_DeathmatchOverlay (0);
		}
	} else if (sb_showscores)
		Sbar_DeathmatchOverlay (0);
	else if (sb_showteamscores)
		Sbar_TeamOverlay ();

	if (sb_lines > 0)
		if (cl.gametype >= GAME_DEATHMATCH)
			Sbar_MiniDeathmatchOverlay ();
}

//=============================================================================

static void
Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		Draw_Pic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
==================
team frags
added by Zoid
==================
*/
static void
Sbar_TeamOverlay (void)
{
	qpic_t		*pic;
	Sint32		i, k, l;
	Uint32		x, y;
	team_t		*tm;
	Sint32		plow, phigh, pavg;

	// request new ping times every two second
	if (cl.gametype != GAME_TEAMS) {
		Sbar_DeathmatchOverlay (0);
		return;
	}

	pic = Draw_CachePic ("gfx/ranking.lmp");
	Draw_Pic (160 - pic->width / 2, 0, pic);

	y = 24;
	x = 36;
	Draw_String (x, y, "low/avg/high team total players", 8);
	y += 8;
//  Draw_String(x, y, "------------ ---- ----- -------");
	Draw_String (x, y, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f "
			"\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
			"\x1d\x1e\x1e\x1e\x1e\x1e\x1f", 8);
	y += 8;

	// sort the teams
	Sbar_SortTeams ();

	// draw the text
	l = scoreboardlines;

	for (i = 0; i < scoreboardteams && y <= vid.height_2d - 10; i++) {
		k = teamsort[i];
		tm = teams + k;

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;
		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;
		if (!tm->players)
			pavg = 999;
		else
			pavg = tm->ptotal / tm->players;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		Draw_String (x, y, va("%3i/%3i/%3i", plow, pavg, phigh), 8);

		// draw team
		Draw_String (x + 104, y, tm->team, 8);

		// draw total
		Draw_String (x + 104 + 40, y, va("%5i", tm->frags), 8);

		// draw players
		Draw_String (x + 104 + 88, y, va("%5i", tm->players), 8);

		if (!strncmp (cl.players[cl.playernum].team, tm->team, 16)) {
			Draw_Character (x + 104 - 8, y, 16, 8);
			Draw_Character (x + 104 + 32, y, 17, 8);
		}

		y += 8;
	}
	y += 8;
	Sbar_DeathmatchOverlay (y);
}

/*
==================
ping time frags name
==================
*/
static void
Sbar_DeathmatchOverlay (int start)
{
	qpic_t			*pic;
	Sint32			i, k, l;
	Uint32			x, y;
	player_info_t	*s;
	Sint32			total;
	Sint32			minutes;
	Sint32			p;
	char			team[5];
	Sint32			skip = 10;
	vec4_t			color;

	if (largegame)
		skip = 8;

	// request new ping times every two second
	if (curtime - cl.last_ping_request > 2) {
		cl.last_ping_request = curtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}

	if (!start) {
		pic = Draw_CachePic ("gfx/ranking.lmp");
		Draw_Pic (160 - pic->width / 2, 0, pic);
	}

	// draw the text
	l = scoreboardlines;

	if (start)
		y = start;
	else
		y = 24;
	if (cl.gametype == GAME_TEAMS) {
		x = 4;
//                            0    40 64   104   152  192 
		Draw_String (x, y, "ping pl time frags team name", 8);
		y += 8;
//      Draw_String ( x , y, "---- -- ---- ----- ---- ----------------");
		Draw_String (x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f "
				"\x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e"
				"\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f", 8);
		y += 8;
	} else {
		x = 16;
//                            0    40 64   104   152
		Draw_String (x, y, "ping pl time frags name", 8);
		y += 8;
//      Draw_String ( x , y, "---- -- ---- ----- ----------------");
		Draw_String (x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f "
				"\x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e"
				"\x1e\x1e\x1e\x1e\x1e\x1f", 8);
		y += 8;
	}

	color[3] = 1.0f;
	for (i = 0; i < l && y <= vid.height_2d - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;
		Draw_String (x, y, va("%4d", p), 8);

		// draw pl
		p = s->pl;
		if (p > 25)
			Draw_Alt_String (x + 32, y, va("%3d", p), 8);
		else
			Draw_String (x + 32, y, va("%3d", p), 8);

		if (s->spectator) {
			Draw_String (x + 40, y, "(spectator)", 8);
			// draw name
			if (cl.gametype == GAME_TEAMS)
				Draw_String (x + 152 + 40, y, s->name, 8);
			else
				Draw_String (x + 152, y, s->name, 8);
			y += skip;
			continue;
		}
		// draw time
		if (cl.intermission)
			total = cl.completed_time - s->entertime;
		else
			total = curtime - s->entertime;
		minutes = (int) total / 60;
		Draw_String (x + 64, y, va("%4d", minutes), 8);

		// draw background
		VectorScale (s->colormap.top, 0.5, color);
		if (largegame)
			Draw_Fill (x + 104, y + 1, 40, 3, color);
		else
			Draw_Fill (x + 104, y, 40, 4, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (x + 104, y + 4, 40, 4, color);

		// draw number
		Draw_String_Len (x + 112, y, va("%3d", s->frags), 3, 8);

		if (k == cl.playernum) {
			Draw_Character (x + 104, y, 16, 8);
			Draw_Character (x + 136, y, 17, 8);
		}
		// team
		if (cl.gametype == GAME_TEAMS) {
			strlcpy (team, s->team, sizeof (team));
			Draw_String (x + 152, y, team, 8);
		}
		// draw name
		if (cl.gametype == GAME_TEAMS)
			Draw_String (x + 152 + 40, y, s->name, 8);
		else
			Draw_String (x + 152, y, s->name, 8);

		y += skip;
	}

	if (y >= vid.height_2d - 10)		// we ran over the screen size, squish
		largegame = true;
}

/*
==================
frags name
frags team name
displayed to right of status bar if there's room
==================
*/
static void
Sbar_MiniDeathmatchOverlay (void)
{
	Sint32			i, k;
	Uint32			x, y, f;
	player_info_t	*s;
	char        	team[5];
	Sint32			numlines;
	char			name[16 + 1];
	team_t			*tm;
	vec4_t			color;

	if (vid.width_2d < 512 || !sb_lines)
		return;							// not enuff room

	// scores   
	Sbar_SortFrags ();
	if (vid.width_2d >= 640)
		Sbar_SortTeams ();

	if (!scoreboardlines)
		return;							// no one there?

	// draw the text
	y = vid.height_2d - sb_lines - 1;
	numlines = sb_lines / 8;
	if (numlines < 3)
		return;							// not enough room

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.playernum)
			break;

	// we're not there, we are probably a spectator, just display top
	if (i == scoreboardlines)
		i = 0;
	else								// figure out start
		i = i - numlines / 2;

	i = bound (0, i, scoreboardlines - numlines);

	x = 324;

	color[3] = 1.0f;
	for (; i < scoreboardlines && y < vid.height_2d - 8 + 1; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0] || s->spectator)
			continue;

		// draw ping
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (x, y + 1, 40, 3, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (x, y + 4, 40, 4, color);

		// draw number
		f = s->frags;

		Draw_String_Len (x + 8, y, va("%3d", f), 3, 8);

		if (k == cl.playernum) {
			Draw_Character (x, y, 16, 8);
			Draw_Character (x + 32, y, 17, 8);
		}
		// team
		if (cl.gametype == GAME_TEAMS) {
			team[4] = 0;
			strncpy (team, s->team, 4);
			Draw_String (x + 48, y, team, 8);
		}
		// draw name
		name[16] = 0;
		strncpy (name, s->name, 16);
		if (cl.gametype == GAME_TEAMS)
			Draw_String (x + 48 + 40, y, name, 8);
		else
			Draw_String (x + 48, y, name, 8);
		y += 8;
	}

	// draw teams if room
	if (vid.width_2d < 640 || !(cl.gametype == GAME_TEAMS))
		return;

	// draw seperator
	x += 208;
	for (y = vid.height_2d - sb_lines; y < vid.height_2d - 6; y += 2)
		Draw_Character (x, y, 14, 8);

	x += 16;

	y = vid.height_2d - sb_lines;
	for (i = 0; i < scoreboardteams && y <= vid.height_2d; i++) {
		k = teamsort[i];
		tm = teams + k;

		// draw pings
		team[4] = 0;
		strncpy (team, tm->team, 4);
		Draw_String (x, y, team, 8);

		// draw total
		Draw_String (x + 40, y, va("%5d", tm->frags), 8);

		if (!strncmp (cl.players[cl.playernum].team, tm->team, 16)) {
			Draw_Character (x - 8, y, 16, 8);
			Draw_Character (x + 32, y, 17, 8);
		}

		y += 8;
	}

}

void
Sbar_IntermissionOverlay (void)
{
	qpic_t     *pic;
	int         dig;
	int         num;

	switch (cl.gametype) {
		case GAME_TEAMS:
			Sbar_TeamOverlay ();
			break;
		case GAME_DEATHMATCH:
			Sbar_DeathmatchOverlay (0);
			break;
		case GAME_SINGLE:
		case GAME_COOP:
			pic = Draw_CachePic ("gfx/complete.lmp");
			Draw_Pic (64, 24, pic);

			pic = Draw_CachePic ("gfx/inter.lmp");
			Draw_Pic (0, 56, pic);

			// time
			dig = cl.completed_time / 60;
			Sbar_IntermissionNumber (160, 64, dig, 3, 0);
			num = cl.completed_time - dig * 60;
			Draw_Pic (234, 64, sb_colon);
			Draw_Pic (246, 64, sb_nums[0][num / 10]);
			Draw_Pic (266, 64, sb_nums[0][num % 10]);

			Sbar_IntermissionNumber (160, 104, cl.stats[STAT_SECRETS], 3, 0);
			Draw_Pic (232, 104, sb_slash);
			Sbar_IntermissionNumber (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

			Sbar_IntermissionNumber (160, 144, cl.stats[STAT_MONSTERS], 3, 0);
			Draw_Pic (232, 144, sb_slash);
			Sbar_IntermissionNumber (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);
			break;
	}
}

void
Sbar_FinaleOverlay (void)
{
	qpic_t     *pic;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_Pic ((vid.width_2d - pic->width) / 2, 16, pic);
}
