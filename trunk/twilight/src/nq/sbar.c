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

#include "quakedef.h"
#include "strlib.h"
#include "cmd.h"
#include "cvar.h"
#include "draw.h"
#include "sbar.h"
#include "screen.h"
#include "server.h"
#include "vid.h"
#include "wad.h"
#include "mathlib.h"


#define STAT_MINUS		10				// num frame for '-' stats digit
qpic_t     *sb_nums[2][11];
qpic_t     *sb_colon, *sb_slash;
qpic_t     *sb_ibar;
qpic_t     *sb_sbar;
qpic_t     *sb_scorebar;

qpic_t     *sb_weapons[7][8];			// 0 is active, 1 is owned, 2-5 are
										// flashes
qpic_t     *sb_ammo[4];
qpic_t     *sb_sigil[4];
qpic_t     *sb_armor[3];
qpic_t     *sb_items[32];

qpic_t     *sb_faces[7][2];				// 0 is gibbed, 1 is dead, 2-6 are
										// alive
							// 0 is static, 1 is temporary animation
qpic_t     *sb_face_invis;
qpic_t     *sb_face_quad;
qpic_t     *sb_face_invuln;
qpic_t     *sb_face_invis_invuln;

qboolean    sb_showscores;

int         sb_lines;					// scan lines to draw

qpic_t     *rsb_invbar[2];
qpic_t     *rsb_weapons[5];
qpic_t     *rsb_items[2];
qpic_t     *rsb_ammo[3];
qpic_t     *rsb_teambord;				// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t     *hsb_weapons[7][5];			// 0 is active, 1 is owned, 2-5 are
										// flashes
//MED 01/04/97 added array to simplify weapon parsing
int         hipweapons[4] =
	{ HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT };
//MED 01/04/97 added hipnotic items array
qpic_t     *hsb_items[2];

static void        Sbar_MiniDeathmatchOverlay (void);
static void        Sbar_DeathmatchOverlay (void);
void        M_DrawPic (int x, int y, qpic_t *pic);

cvar_t		*cl_sbar;

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
	int			i;

	for (i = 0; i < 10; i++)
	{
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

	for (i = 0; i < 5; i++)
	{
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

	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");

	//MED 01/04/97 added new hipnotic weapons
	if (game_hipnotic->ivalue)
	{
		hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
		hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
		hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
		hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
		hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");

		hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
		hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
		hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
		hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
		hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");

		for (i = 0; i < 5; i++)
		{
			hsb_weapons[2 + i][0] =
				Draw_PicFromWad (va ("inva%i_laser", i + 1));
			hsb_weapons[2 + i][1] =
				Draw_PicFromWad (va ("inva%i_mjolnir", i + 1));
			hsb_weapons[2 + i][2] =
				Draw_PicFromWad (va ("inva%i_gren_prox", i + 1));
			hsb_weapons[2 + i][3] =
				Draw_PicFromWad (va ("inva%i_prox_gren", i + 1));
			hsb_weapons[2 + i][4] = Draw_PicFromWad (va ("inva%i_prox", i + 1));
		}

		hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
		hsb_items[1] = Draw_PicFromWad ("sb_eshld");
	}

	if (game_rogue->ivalue)
	{
		rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
		rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");

		rsb_weapons[0] = Draw_PicFromWad ("r_lava");
		rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
		rsb_weapons[2] = Draw_PicFromWad ("r_gren");
		rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
		rsb_weapons[4] = Draw_PicFromWad ("r_plasma");

		rsb_items[0] = Draw_PicFromWad ("r_shield1");
		rsb_items[1] = Draw_PicFromWad ("r_agrav1");

		// PGM 01/19/97 - team color border
		rsb_teambord = Draw_PicFromWad ("r_teambord");
		// PGM 01/19/97 - team color border

		rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
		rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
		rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
	}
}


//=============================================================================

// drawing routines are relative to the status bar location

static void
Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x, y + (vid.height_2d - SBAR_HEIGHT), pic);
}

/*
JACK: Draws a portion of the picture in the status bar.
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

int		fragsort[MAX_SCOREBOARD];
int		scoreboardlines;

static void
Sbar_SortFrags (void)
{
	int	i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < cl.maxclients; i++) {
		if (cl.scores[i].name[0]) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i = 0; i < scoreboardlines; i++)
		for (j = 0; j < scoreboardlines - 1 - i; j++)
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
}


static void
Sbar_SoloScoreboard (void)
{
	char        str[80];
	int         minutes, seconds, tens, units;
	int         l;

	if (cl.gametype != GAME_DEATHMATCH)
	{
		snprintf (str, sizeof (str), "Monsters:%3i /%3i", cl.stats[STAT_MONSTERS],
				  cl.stats[STAT_TOTALMONSTERS]);
		Sbar_DrawString (8, 4, str);

		snprintf (str, sizeof (str), "Secrets :%3i /%3i", cl.stats[STAT_SECRETS],
				  cl.stats[STAT_TOTALSECRETS]);
		Sbar_DrawString (8, 12, str);
	}

// time
	minutes = cl.time / 60;
	seconds = cl.time - 60 * minutes;
	tens = seconds / 10;
	units = seconds - 10 * tens;
	snprintf (str, sizeof (str), "Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

// draw level name
	if (cl.gametype != GAME_DEATHMATCH)
	{
		l = strlen (cl.levelname);
		Sbar_DrawString (232 - l * 4, 12, cl.levelname);
	}
}

static void
Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();

	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay ();
}

//=============================================================================

static void
Sbar_DrawInventory (void)
{
	int			i, j, y;
	char		num[6];
	float		time;
	int			flashon;
	qboolean	headsup;
	qboolean	hudswap;

	headsup = !cl_sbar->ivalue && !game_rogue->ivalue && !game_hipnotic->ivalue;
	hudswap = cl_hudswap->ivalue;

	if (game_rogue->ivalue)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
			Sbar_DrawPic (0, -24, rsb_invbar[0]);
		else
			Sbar_DrawPic (0, -24, rsb_invbar[1]);
	}
	else if (!headsup)
		Sbar_DrawPic (0, -24, sb_ibar);

	// weapons
	for (i = 0; i < 7; i++)
	{
		if (cl.items & (IT_SHOTGUN << i))
		{
			time = cl.item_gettime[i];
			flashon = (int) ((cl.time - time) * 10);
			if (flashon >= 10)
			{
				if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
					flashon = 1;
				else
					flashon = 0;
			}
			else
				flashon = (flashon % 5) + 2;

			if (headsup)
			{
				if (i || vid.height_2d > 200)
					Sbar_DrawSubPic ((hudswap) ? 0 : (vid.width_2d - 24),
							-68 - (7 - i) * 16, sb_weapons[flashon][i],
							0, 0, 24, 16);

			}
			else
				Sbar_DrawPic (i * 24, -16, sb_weapons[flashon][i]);
		}
	}

	// MED 01/04/97 - hipnotic weapons
	if (game_hipnotic->ivalue)
	{
		int			grenadeflashing = 0;

		for (i = 0; i < 4; i++)
		{
			if (cl.items & (1 << hipweapons[i]))
			{
				time = cl.item_gettime[hipweapons[i]];
				flashon = (int) ((cl.time - time) * 10);
				if (flashon >= 10)
				{
					if (cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[i]))
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon % 5) + 2;

				// check grenade launcher
				if (i == 2)
				{
					if (cl.items & HIT_PROXIMITY_GUN)
					{
						if (flashon)
						{
							grenadeflashing = 1;
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
						}
					}
				}
				else if (i == 3)
				{
					if (cl.items & (IT_SHOTGUN << 4))
					{
						if (flashon && !grenadeflashing)
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
						else if (!grenadeflashing)
							Sbar_DrawPic (96, -16, hsb_weapons[0][3]);
					}
					else
						Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
				}
				else
					Sbar_DrawPic (176 + (i * 24), -16, hsb_weapons[flashon][i]);
			}
		}
	}

	if (game_rogue->ivalue)
	{
		// check for powered up weapon.
		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
		{
			for (i = 0; i < 5; i++)
			{
				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
					Sbar_DrawPic ((i + 2) * 24, -16, rsb_weapons[i]);
			}
		}
	}

	// ammo counts
	for (i = 0; i < 4; i++)
	{
		snprintf (num, sizeof (num), "%3i", cl.stats[STAT_SHELLS + i]);
		for (j = 0; j < 3; j++)
			num[j] = (num[j] == 32 ? ' ' : 18 + num[j] - '0');
		if (headsup)
		{
			y = -24 - (4 - i) * 11;
			if (hudswap)
			{
				Sbar_DrawSubPic (0, y, sb_ibar, 3 + (i * 48), 0, 42, 11);
				Sbar_DrawString (3, y, num);
			}
			else
			{
				Sbar_DrawSubPic (vid.width_2d - 42, y, sb_ibar, 3 + (i * 48),
						0, 42, 11);
				Sbar_DrawString (vid.width_2d - 39, y, num);
			}
		}
		else
			Sbar_DrawString ((6 * i + 1) * 8 - 2, -24, num);
	}

	flashon = 0;

	// items
	for (i = 0; i < 6; i++)
		if (cl.items & (1 << (17 + i)))
		{
			time = cl.item_gettime[17 + i];
			// MED 01/04/97 changed keys
			if (!game_hipnotic->ivalue || (i > 1))
				Sbar_DrawPic (192 + i * 16, -16, sb_items[i]);
		}
	// MED 01/04/97 added hipnotic items
	// hipnotic items
	if (game_hipnotic->ivalue)
	{
		for (i = 0; i < 2; i++)
			if (cl.items & (1 << (24 + i)))
			{
				time = cl.item_gettime[24 + i];
				Sbar_DrawPic (288 + i * 16, -16, hsb_items[i]);
			}
	}

	if (game_rogue->ivalue)
	{
		// new rogue items
		for (i = 0; i < 2; i++)
		{
			if (cl.items & (1 << (29 + i)))
			{
				time = cl.item_gettime[29 + i];

				Sbar_DrawPic (288 + i * 16, -16, rsb_items[i]);
			}
		}
	}
	else
	{
		// sigils
		for (i = 0; i < 4; i++)
		{
			if (cl.items & (1 << (28 + i)))
			{
				time = cl.item_gettime[28 + i];
				Sbar_DrawPic (320 - 32 + i * 8, -16, sb_sigil[i]);
			}
		}
	}
}

//=============================================================================

static void
Sbar_DrawFrags (void)
{
	Sint32			k;
	Uint32			i, l, x, y, xofs;
	scoreboard_t	*s;
	vec4_t			color;

	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
	if (cl.gametype == GAME_DEATHMATCH)
		xofs = 0;
	else
		xofs = (vid.width_2d - 320) >> 1;

	y = vid.height_2d - SBAR_HEIGHT - 23;

	color[3] = 1.0f;
	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (xofs + x * 8 + 10, y, 28, 4, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (xofs + x * 8 + 10, y + 4, 28, 3, color);

		// draw number
		Sbar_DrawString (((x + 1) * 8) + 4, -24, va("%3i", s->frags));

		if (k == cl.viewentity - 1) {
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
	int			f, anim;

	// PGM 01/19/97 - team color drawing
	// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (game_rogue->ivalue && (cl.maxclients != 1) && (teamplay->ivalue > 3)
			&& (teamplay->ivalue < 7))
	{
		int				xofs;
		scoreboard_t	*s;
		vec4_t			color;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		if (cl.gametype == GAME_DEATHMATCH)
			xofs = 113;
		else
			xofs = ((vid.width_2d - 320) >> 1) + 113;

		color[3] = 1.0f;
		Sbar_DrawPic (112, 0, rsb_teambord);
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (xofs, vid.height_2d - SBAR_HEIGHT + 3, 22, 9, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (xofs, vid.height_2d - SBAR_HEIGHT + 12, 22, 9, color);

		// draw number
		Sbar_DrawString (113, 3, va("%3i", s->frags));
		return;
	}
	// PGM 01/19/97 - team color drawing

	if ((cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY))
			== (IT_INVISIBILITY | IT_INVULNERABILITY))
	{
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.items & IT_QUAD)
	{
		Sbar_DrawPic (112, 0, sb_face_quad);
		return;
	}
	if (cl.items & IT_INVISIBILITY)
	{
		Sbar_DrawPic (112, 0, sb_face_invis);
		return;
	}
	if (cl.items & IT_INVULNERABILITY)
	{
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

void
Sbar_Draw (void)
{
	qboolean	headsup;

	if (scr_con_current == vid.height_2d)
		// console is full screen
		return;

	headsup = !cl_sbar->ivalue && !game_rogue->ivalue && !game_hipnotic->ivalue;

	if (sb_lines > 24)
	{
		Sbar_DrawInventory ();
		if ((cl.maxclients != 1) && (!headsup || (vid.width_2d < 512)))
			Sbar_DrawFrags ();
	}

	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
	{
		Sbar_DrawPic (0, 0, sb_scorebar);
		Sbar_DrawScoreboard ();
	}
	else if (sb_lines)
	{
		if (!headsup)
			Sbar_DrawPic (0, 0, sb_sbar);

		// keys (hipnotic only)
		// MED 01/04/97 moved keys here so they would not be overwritten
		if (game_hipnotic->ivalue)
		{
			if (cl.items & IT_KEY1)
				Sbar_DrawPic (209, 3, sb_items[0]);
			if (cl.items & IT_KEY2)
				Sbar_DrawPic (209, 12, sb_items[1]);
		}
		// armor
		if (cl.items & IT_INVULNERABILITY)
		{
			Sbar_DrawNum (24, 0, 666, 3, 1);
			Sbar_DrawPic (0, 0, draw_disc);
		}
		else
		{
			if (game_rogue->ivalue)
			{
				Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
						cl.stats[STAT_ARMOR] <= 25);
				if (cl.items & RIT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & RIT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & RIT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
			else
			{
				Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
						cl.stats[STAT_ARMOR] <= 25);
				if (cl.items & IT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & IT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & IT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
		}

		// face
		Sbar_DrawFace ();

		// health
		Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3,
				cl.stats[STAT_HEALTH] <= 25);

		// ammo icon
		if (game_rogue->ivalue)
		{
			if (cl.items & RIT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & RIT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & RIT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & RIT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
			else if (cl.items & RIT_LAVA_NAILS)
				Sbar_DrawPic (224, 0, rsb_ammo[0]);
			else if (cl.items & RIT_PLASMA_AMMO)
				Sbar_DrawPic (224, 0, rsb_ammo[1]);
			else if (cl.items & RIT_MULTI_ROCKETS)
				Sbar_DrawPic (224, 0, rsb_ammo[2]);
		}
		else
		{
			if (cl.items & IT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & IT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & IT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & IT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
		}

		Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3,
				cl.stats[STAT_AMMO] <= 10);
	}

	if (sb_lines > 0)
	{
		if (cl.gametype == GAME_DEATHMATCH)
			Sbar_MiniDeathmatchOverlay ();
	}
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

static void
Sbar_DeathmatchOverlay (void)
{
	qpic_t     *pic;
	int         i, k, l;
	int         x, y;
	vec4_t		color;
	scoreboard_t *s;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	M_DrawPic ((320 - pic->width) / 2, 8, pic);

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;

	x = 80 + ((vid.width_2d - 320) >> 1);
	y = 40;
	
	color[3] = 1.0f;
	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (x, y, 40, 4, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (x, y + 4, 40, 4, color);

		// draw number
		Draw_String_Len (x + 8, y, va("%3d", s->frags), 3, 8);

		if (k == cl.viewentity - 1)
			Draw_Character (x - 8, y, 12, 8);

		// draw name
		Draw_String (x + 64, y, s->name, 8);

		y += 10;
	}
}

static void
Sbar_MiniDeathmatchOverlay (void)
{
	Sint32			k, i, numlines;
	Uint32			l, x, y;
	scoreboard_t	*s;
	vec4_t			color;

	if (vid.width_2d < 512 || !sb_lines)
		return;

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;
	y = vid.height_2d - sb_lines;
	numlines = sb_lines / 8;
	if (numlines < 3)
		return;

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;

	if (i == scoreboardlines)			// we're not there
		i = 0;
	else								// figure out start
		i = i - numlines / 2;

	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	if (i < 0)
		i = 0;

	x = 324;

	color[3] = 1.0f;
	for (; i < scoreboardlines && y < vid.height_2d - 8; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		VectorScale (s->colormap.top, 0.5, color);
		Draw_Fill (x, y + 1, 40, 3, color);
		VectorScale (s->colormap.bottom, 0.5, color);
		Draw_Fill (x, y + 4, 40, 4, color);

		// draw number
		Draw_String_Len (x + 8, y, va("%3d", s->frags), 3, 8);

		if (k == cl.viewentity - 1) {
			Draw_Character (x, y, 16, 8);
			Draw_Character (x + 32, y, 17, 8);
		}

		// draw name
		Draw_String (x + 48, y, s->name, 8);

		y += 8;
	}
}

void
Sbar_IntermissionOverlay (void)
{
	qpic_t     *pic;
	int         dig;
	int         num;

	if (cl.gametype == GAME_DEATHMATCH) {
		Sbar_DeathmatchOverlay ();
		return;
	}

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

}


void
Sbar_FinaleOverlay (void)
{
	qpic_t     *pic;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_Pic ((vid.width_2d - pic->width) / 2, 16, pic);
}

