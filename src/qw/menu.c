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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "keys.h"
#include "glquake.h"
#include "mathlib.h"
#include "menu.h"
#include "screen.h"
#include "sound.h"
#include "strlib.h"
#include "view.h"
#include "sys.h"

extern cvar_t *gl_texturemode;

void        (*vid_menudrawfn) (void);
void        (*vid_menukeyfn) (int key);

enum { m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup,
	m_options, m_video, m_keys, m_help, m_quit, m_serialconfig,
	m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist, m_gfx
} m_state;

void        M_Menu_Main_f (void);
void        M_Menu_SinglePlayer_f (void);
void        M_Menu_Load_f (void);
void        M_Menu_Save_f (void);
void        M_Menu_MultiPlayer_f (void);
void        M_Menu_Setup_f (void);
void        M_Menu_Options_f (void);
void        M_Menu_Keys_f (void);
void        M_Menu_Video_f (void);
void		M_Menu_Gfx_f (void);
void        M_Menu_Help_f (void);
void        M_Menu_Quit_f (void);
void        M_Menu_SerialConfig_f (void);
void        M_Menu_ModemConfig_f (void);
void        M_Menu_LanConfig_f (void);
void        M_Menu_GameOptions_f (void);
void        M_Menu_Search_f (void);
void        M_Menu_ServerList_f (void);

void        M_Main_Draw (void);
void        M_SinglePlayer_Draw (void);
void        M_Load_Draw (void);
void        M_Save_Draw (void);
void        M_MultiPlayer_Draw (void);
void        M_Setup_Draw (void);
void        M_Options_Draw (void);
void        M_Keys_Draw (void);
void        M_Video_Draw (void);
void        M_Help_Draw (void);
void        M_Quit_Draw (void);
void        M_SerialConfig_Draw (void);
void        M_ModemConfig_Draw (void);
void        M_LanConfig_Draw (void);
void        M_GameOptions_Draw (void);
void        M_Search_Draw (void);
void        M_ServerList_Draw (void);

void        M_Main_Key (int key);
void        M_SinglePlayer_Key (int key);
void        M_Load_Key (int key);
void        M_Save_Key (int key);
void        M_MultiPlayer_Key (int key);
void        M_Setup_Key (int key);
void        M_Options_Key (int key);
void        M_Keys_Key (int key);
void        M_Video_Key (int key);
void        M_Help_Key (int key);
void        M_Quit_Key (int key);
void        M_SerialConfig_Key (int key);
void        M_ModemConfig_Key (int key);
void        M_LanConfig_Key (int key);
void        M_GameOptions_Key (int key);
void        M_Search_Key (int key);
void        M_ServerList_Key (int key);

qboolean    m_entersound;				// play after drawing a frame, so

										// caching
								// won't disrupt the sound
qboolean    m_recursiveDraw;

int         m_return_state;
qboolean    m_return_onerror;
char        m_return_reason[32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)

void        M_ConfigureNetSubsystem (void);

//=============================================================================
/* Support Routines */

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void
M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character (cx + ((vid.width - 320) >> 1), line, num);
}

void
M_Print (int cx, int cy, char *str)
{
/*
	while (*str) {
		M_DrawCharacter (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
*/
	Draw_Alt_String (cx + ((vid.width - 320) >> 1), cy, str);
}

void
M_PrintWhite (int cx, int cy, char *str)
{
/*
	while (*str) {
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
*/
	Draw_String (cx + ((vid.width - 320) >> 1), cy, str);
}

void
M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320) >> 1), y, pic);
}

Uint8       identityTable[256];
Uint8       translationTable[256];

void
M_BuildTranslationTable (int top, int bottom)
{
	int         j;
	Uint8      *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)						// the artists made some backwards
		// ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}


void
M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320) >> 1), y, pic,
							translationTable);
}


void
M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t     *p;
	int         cx, cy;
	int         n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		M_DrawPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawPic (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawPic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		M_DrawPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawPic (cx, cy + 8, p);
}

//=============================================================================

int         m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void
M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu) {
		if (m_state != m_main) {
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	} else if (key_dest == key_console && cl.worldmodel) {
		Con_ToggleConsole_f ();
	} else {
		M_Menu_Main_f ();
	}
}


//=============================================================================
/* MAIN MENU */

int         m_main_cursor;

#define	MAIN_ITEMS	5


void
M_Menu_Main_f (void)
{
	if (key_dest != key_menu) {
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}


void
M_Main_Draw (void)
{
	int         f;
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp"));

	f = (int) (cls.realtime * 10) % 6;

	M_DrawPic (54, 32 + m_main_cursor * 20,
					Draw_CachePic (va ("gfx/menudot%i.lmp", f + 1)));
}


void
M_Main_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
			key_dest = key_game;
			m_state = m_none;
			cls.demonum = m_save_demonum;
			if (cls.demonum != -1 && !cls.demoplayback
				&& cls.state == ca_disconnected)
				CL_NextDemo ();
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			if (++m_main_cursor >= MAIN_ITEMS)
				m_main_cursor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			if (--m_main_cursor < 0)
				m_main_cursor = MAIN_ITEMS - 1;
			break;

		case K_ENTER:
			m_entersound = true;

			switch (m_main_cursor) {
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					M_Menu_Help_f ();
					break;

				case 4:
					M_Menu_Quit_f ();
					break;
			}
	}
}


//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	19

#define	SLIDER_RANGE	10

int         options_cursor;

void
M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}


void
M_AdjustSliders (int dir)
{
	float		t;

	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor) {
		case 3:						// screen size
			t = scr_viewsize->value + (dir * 10);
			t = bound (30, t, 120);
			Cvar_Set (scr_viewsize,  va ("%f", t));
			break;
		case 4:						// gamma
			t = v_gamma->value + (dir * 0.05);
			t = bound (1.0, t, 2.0);
			Cvar_Set (v_gamma, va ("%f", t));
			break;
		case 5:						// software brightness
			t = r_brightness->value + (dir * 0.25);
			t = bound (1, t, 5);
			Cvar_Set (r_brightness, va("%f", t));
			break;
		case 6:						// software contrast (base brightness)
			t = r_contrast->value + (dir * 0.025);
			t = bound (0.75, t, 1.0);
			Cvar_Set (r_contrast, va("%f", t));
			break;
		case 7:						// mouse speed
			t = sensitivity->value + (dir * 0.5);
			t = bound (1, t, 11);
			Cvar_Set (sensitivity, va ("%f", t));
			break;
		case 8:						// music volume
			Cvar_Set (bgmvolume,  va ("%i", !(int)bgmvolume->value));
			break;
		case 9:						// sfx volume
			t = volume->value + dir * 0.1;
			t = bound (0, t, 1);
			Cvar_Set (volume, va ("%f", t));
			break;

		case 10:					// always run
			if (cl_forwardspeed->value > 200) {
				Cvar_Set (cl_forwardspeed, "200");
				Cvar_Set (cl_backspeed, "200");
			} else {
				Cvar_Set (cl_forwardspeed, "400");
				Cvar_Set (cl_backspeed, "400");
			}
			break;

		case 11:						// invert mouse
			Cvar_Set (m_pitch,  va ("%f", -m_pitch->value));
			break;

		case 12:						// lookspring
			Cvar_Set (lookspring,  va ("%i", !(int)lookspring->value));
			break;

		case 13:						// lookstrafe
			Cvar_Set (lookstrafe,  va ("%i", !(int)lookstrafe->value));
			break;

		case 14:
			Cvar_Set (cl_sbar,  va ("%i", !(int)cl_sbar->value));
			break;

		case 15:
			Cvar_Set (cl_hudswap, va ("%i", !(int)cl_hudswap->value));
			break;

		case 16:						// _windowed_mouse
			Cvar_Set (_windowed_mouse, va ("%i", !(int)_windowed_mouse->value));
			break;
	}
}


void
M_DrawSlider (int x, int y, float range)
{
	int         i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x - 8, y, 128);
	for (i = 0; i < SLIDER_RANGE; i++)
		M_DrawCharacter (x + i * 8, y, 129);
	M_DrawCharacter (x + i * 8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE - 1) * 8 * range, y, 131);
}

void
M_DrawCheckbox (int x, int y, int on)
{
	M_Print (x, y, (on) ? "on" : "off");
}

void
M_Options_Draw (void)
{
	int		y;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	y = 32;
	M_Print (16, y, "    Customize controls"); y += 8;
	M_Print (16, y, "         Go to console"); y += 8;
	M_Print (16, y, "     Reset to defaults"); y += 8;

	M_Print (16, y, "           Screen size"); M_DrawSlider (220, 56, (scr_viewsize->value - 30) / (120 - 30)); y += 8;
	M_Print (16, y, "        Hardware Gamma"); M_DrawSlider (220, y, v_gamma->value - 1.0); y += 8;
	M_Print (16, y, "   Software Brightness"); M_DrawSlider(220, y, (r_brightness->value - 1) / 4); y += 8;
	M_Print (16, y, "     Software Contrast"); M_DrawSlider(220, y, (r_contrast->value - 0.75) * 4); y += 8;
	M_Print (16, y, "           Mouse Speed"); M_DrawSlider (220, y, (sensitivity->value - 1) / 10); y += 8;
	M_Print (16, y, "       CD Music Volume"); M_DrawSlider (220, y, bgmvolume->value); y += 8;
	M_Print (16, y, "          Sound Volume"); M_DrawSlider (220, y, volume->value); y += 8;
	M_Print (16, y, "            Always Run"); M_DrawCheckbox (220, y, cl_forwardspeed->value > 200); y += 8;
	M_Print (16, y, "          Invert Mouse"); M_DrawCheckbox (220, y, m_pitch->value < 0); y += 8;
	M_Print (16, y, "            Lookspring"); M_DrawCheckbox (220, y, lookspring->value); y += 8;
	M_Print (16, y, "            Lookstrafe"); M_DrawCheckbox (220, y, lookstrafe->value); y += 8;
	M_Print (16, y, "    Use old status bar"); M_DrawCheckbox (220, y, cl_sbar->value); y += 8;
	M_Print (16, y, "      HUD on left side"); M_DrawCheckbox (220, y, cl_hudswap->value); y += 8;
	M_Print (16, y, "             Use Mouse"); M_DrawCheckbox (220, y, _windowed_mouse->value); y += 8;
	M_Print (16, y, "      Graphics Options"); y += 8;

	if (vid_menudrawfn)
		M_Print (16, y, "         Video Options"); y += 8;


	// cursor
	M_DrawCharacter (200, 32 + options_cursor * 8, 12 + ((int) (cls.realtime * 4) & 1));
}


void
M_Options_Key (int k)
{
	switch (k) {
		case K_ESCAPE:
			M_Menu_Main_f ();
			break;

		case K_ENTER:
			m_entersound = true;
			switch (options_cursor) {
				case 0:
					M_Menu_Keys_f ();
					break;
				case 1:
					m_state = m_none;
					Con_ToggleConsole_f ();
					break;
				case 2:
					Cbuf_AddText ("exec default.cfg\n");
					break;
				case 17:
					M_Menu_Gfx_f ();
					break;
				case 18:
					M_Menu_Video_f ();
					break;
				default:
					M_AdjustSliders (1);
					break;
			}
			return;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			options_cursor--;
			if (options_cursor < 0)
				options_cursor = OPTIONS_ITEMS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			options_cursor++;
			if (options_cursor >= OPTIONS_ITEMS)
				options_cursor = 0;
			break;

		case K_LEFTARROW:
			M_AdjustSliders (-1);
			break;

		case K_RIGHTARROW:
			M_AdjustSliders (1);
			break;
	}

	if (options_cursor == 18) {
		if (k == K_UPARROW)
			options_cursor = 17;
		else
			options_cursor = 0;
	}
}

//=============================================================================
/* GFX */

/*
	Smooth models			on/off
	Affine models			on/off
	Fullbright models		on/off
	Fast dynamic lights		on/off
	Shadows					on/fast/nice
	Frame interpolation	    on/off
	Motion interpolation	on/off
	Texture Mode			see glmode_t modes[]
	Light lerping			on/off
	Particle torches		on/off
*/

#define GFX_ITEMS	10

int gfx_cursor = 0;

typedef struct {
	char       *name;
	int         minimize, maximize;
} glmode_t;

glmode_t    texmodes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

void
M_Menu_Gfx_f (void)
{
	key_dest = key_menu;
	m_state = m_gfx;
	m_entersound = true;
}

void
M_Gfx_Draw (void)
{
	int				y;
	static qpic_t	*p = NULL;

	if (!p) p = Draw_CachePic ("gfx/p_option.lmp");

	M_DrawPic ((320 - p->width) / 2, 4, p);

	y = 32;
	M_Print (16, y, "         Affine models"); M_DrawCheckbox (220, y, gl_affinemodels->value); y += 8;
	M_Print (16, y, "     Fullbright models"); M_DrawCheckbox (220, y, gl_fb_models->value); y += 8;
	M_Print (16, y, "    Fullbright bmodels"); M_DrawCheckbox (220, y, gl_fb_bmodels->value); y += 8;
	M_Print (16, y, "   Fast dynamic lights"); M_DrawCheckbox (220, y, gl_flashblend->value); y += 8;
	M_Print (16, y, "               Shadows"); M_Print (220, y, (r_shadows->value) ? (r_shadows->value == 2 ? "nice" : "fast") : "off"); y += 8;
	M_Print (16, y, "   Frame interpolation"); M_DrawCheckbox (220, y, gl_im_animation->value); y += 8;
	M_Print (16, y, "  Motion interpolation"); M_DrawCheckbox (220, y, gl_im_transform->value); y += 8;
	M_Print (16, y, "          Texture mode"); M_Print (220, y, gl_texturemode->string); y += 8;
	M_Print (16, y, "         Light lerping"); M_DrawCheckbox (220, y, r_lightlerp->value); y += 8;
	M_Print (16, y, "      Particle torches"); M_DrawCheckbox (220, y, gl_particletorches->value);

	// cursor
	M_DrawCharacter (200, 32 + gfx_cursor * 8, 12 + ((int) (cls.realtime * 4) & 1));
}

void
M_Gfx_Set (void)
{
	int v = 0;

	S_LocalSound ("misc/menu3.wav");

	switch (gfx_cursor)
	{
		case 0:
			v = !(int)gl_affinemodels->value;
			Cvar_Set (gl_affinemodels, va("%i", v));
			break;

		case 1:
			v = !(int)gl_fb_models->value;
			Cvar_Set (gl_fb_models, va("%i", v));
			break;

		case 2:
			v = !(int)gl_fb_bmodels->value;
			Cvar_Set (gl_fb_bmodels, va("%i", v));
			break;

		case 3:
			v = !(int)gl_flashblend->value;
			Cvar_Set (gl_flashblend, va("%i", v));
			break;

		case 4:
			v = (int)r_shadows->value + 1;
			if (v > 2) v = 0;
			Cvar_Set (r_shadows, va("%i", v));
			break;

		case 5:
			v = !(int)gl_im_animation->value;
			Cvar_Set (gl_im_animation, va("%i", v));
			break;

		case 6:
			v = !(int)gl_im_transform->value;
			Cvar_Set (gl_im_transform, va("%i", v));
			break;

		case 7:
			for (v = 0; v < 6; v++) {
				if (strcasecmp (texmodes[v].name, gl_texturemode->string) == 0)
					break;
			}
			v++;
			if (v > 5)
				v = 0;
			Cvar_Set (gl_texturemode, texmodes[v].name);
			break;

		case 8:
			v = !(int)r_lightlerp->value;
			Cvar_Set (r_lightlerp, va("%i", v));
			break;

		case 9:
			v = !(int)gl_particletorches->value;
			Cvar_Set (gl_particletorches, va("%i", v));
			break;

		default:
			break;
	}
}

void
M_Gfx_Key (int k)
{
	switch (k) {
		case K_ESCAPE:
			M_Menu_Options_f ();
			return;
			
		case K_ENTER:
			m_entersound = true;
			return;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			gfx_cursor--;
			if (gfx_cursor < 0)
				gfx_cursor = GFX_ITEMS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			gfx_cursor++;
			if (gfx_cursor >= GFX_ITEMS)
				gfx_cursor = 0;
			break;

		case K_LEFTARROW:
			M_Gfx_Set ();
			break;

		case K_RIGHTARROW:
			M_Gfx_Set ();
			break;

		default:
			break;
	}
}

//=============================================================================
/* KEYS MENU */

char       *bindnames[][2] = {
	{"+attack", "attack"},
	{"impulse 10", "change weapon"},
	{"+jump", "jump / swim up"},
	{"+forward", "walk forward"},
	{"+back", "backpedal"},
	{"+left", "turn left"},
	{"+right", "turn right"},
	{"+speed", "run"},
	{"+moveleft", "step left"},
	{"+moveright", "step right"},
	{"+strafe", "sidestep"},
	{"+lookup", "look up"},
	{"+lookdown", "look down"},
	{"centerview", "center view"},
	{"+mlook", "mouse look"},
	{"+klook", "keyboard look"},
	{"+moveup", "swim up"},
	{"+movedown", "swim down"}
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

Uint32		keys_cursor;
int         bind_grab;

void
M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}


void
M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen (command);
	count = 0;

	for (j = 0; j < 256; j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l)) {
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void
M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen (command);

	for (j = 0; j < 256; j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l))
			Key_SetBinding (j, "");
	}
}


void
M_Keys_Draw (void)
{
	Uint32	i, l;
	int		keys[2];
	char	*name;
	Uint32	x, y;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

	// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++) {
		y = 48 + 8 * i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1) {
			M_Print (140, y, "???");
		} else {
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen (name) * 8;
			if (keys[1] != -1) {
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor * 8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor * 8,
						 12 + ((int) (cls.realtime * 4) & 1));
}


void
M_Keys_Key (int k)
{
	int         keys[2];

	if (bind_grab) {					// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE) {
			bind_grab = false;
		} else if (k != '`') {
			Key_SetBinding (k, bindnames[keys_cursor][0]);
		}

		bind_grab = false;
		return;
	}

	switch (k) {
		case K_ESCAPE:
			M_Menu_Options_f ();
			break;

		case K_LEFTARROW:
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor--;
			if (keys_cursor < 0)
				keys_cursor = NUMCOMMANDS - 1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor++;
			if (keys_cursor >= NUMCOMMANDS)
				keys_cursor = 0;
			break;

		case K_ENTER:				// go into bind mode
			M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
			S_LocalSound ("misc/menu2.wav");
			if (keys[1] != -1)
				M_UnbindCommand (bindnames[keys_cursor][0]);
			bind_grab = true;
			break;

		case K_BACKSPACE:			// delete bindings
		case K_DEL:					// delete bindings
			S_LocalSound ("misc/menu2.wav");
			M_UnbindCommand (bindnames[keys_cursor][0]);
			break;
	}
}

//=============================================================================
/* VIDEO MENU */

void
M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}


void
M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void
M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* HELP MENU */

int         help_page;

#define	NUM_HELP_PAGES	6


void
M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void
M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic (va ("gfx/help%i.lmp", help_page)));
}


void
M_Help_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
			M_Menu_Main_f ();
			break;

		case K_UPARROW:
		case K_RIGHTARROW:
			m_entersound = true;
			if (++help_page >= NUM_HELP_PAGES)
				help_page = 0;
			break;

		case K_DOWNARROW:
		case K_LEFTARROW:
			m_entersound = true;
			if (--help_page < 0)
				help_page = NUM_HELP_PAGES - 1;
			break;
	}

}

//=============================================================================
/* QUIT MENU */

int         msgNumber;
int         m_quit_prevstate;
qboolean    wasInMenus;

char       *quitMessage[] = {
/* .........1.........2.... */
	"  Are you gonna quit    ",
	"  this game just like   ",
	"   everything else?     ",
	"                        ",

	" Milord, methinks that  ",
	"   thou art a lowly     ",
	" quitter. Is this true? ",
	"                        ",

	" Do I need to bust your ",
	"  face open for trying  ",
	"        to quit?        ",
	"                        ",

	" Man, I oughta smack you",
	"   for trying to quit!  ",
	"     Press Y to get     ",
	"      smacked out.      ",

	" Press Y to quit like a ",
	"   big loser in life.   ",
	"  Press N to stay proud ",
	"    and successful!     ",

	"   If you press Y to    ",
	"  quit, I will summon   ",
	"  Satan all over your   ",
	"      hard drive!       ",

	"  Um, Asmodeus dislikes ",
	" his children trying to ",
	" quit. Press Y to return",
	"   to your Tinkertoys.  ",

	"  If you quit now, I'll ",
	"  throw a blanket-party ",
	"   for you next time!   ",
	"                        "
};

void
M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = Q_rand () & 7;
}


void
M_Quit_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
		case 'n':
		case 'N':
			if (wasInMenus) {
				m_state = m_quit_prevstate;
				m_entersound = true;
			} else {
				key_dest = key_game;
				m_state = m_none;
			}
			break;

		case 'Y':
		case 'y':
			key_dest = key_console;
			CL_Disconnect ();
			Sys_Quit ();
			break;

		default:
			break;
	}

}

void
M_Menu_SinglePlayer_f (void)
{
	m_state = m_singleplayer;
}

void
M_SinglePlayer_Draw (void)
{
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
//  M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
//  M_DrawPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	M_DrawTextBox (60, 10 * 8, 23, 4);
	M_PrintWhite (92, 12 * 8, "QuakeWorld is for");
	M_PrintWhite (88, 13 * 8, "Internet play only");

}

void
M_SinglePlayer_Key (key)
{
	if (key == K_ESCAPE || key == K_ENTER)
		m_state = m_main;
}

void
M_Menu_MultiPlayer_f (void)
{
	m_state = m_multiplayer;
}

void
M_MultiPlayer_Draw (void)
{
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
//  M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
//  M_DrawPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	M_DrawTextBox (46, 8 * 8, 27, 9);
	M_PrintWhite (72, 10 * 8, "If you want to find QW  ");
	M_PrintWhite (72, 11 * 8, "games, head on over to: ");
	M_Print (72, 12 * 8, "   www.quakeworld.net   ");
	M_PrintWhite (72, 13 * 8, "          or            ");
	M_Print (72, 14 * 8, "   www.quakespy.com     ");
	M_PrintWhite (72, 15 * 8, "For pointers on getting ");
	M_PrintWhite (72, 16 * 8, "        started!        ");
}

void
M_MultiPlayer_Key (key)
{
	if (key == K_ESCAPE || key == K_ENTER)
		m_state = m_main;
}

void
M_Quit_Draw (void)
{
#define VSTR(x) #x
#define VSTR2(x) VSTR(x)
	char       *cmsg[] = {
//    0123456789012345678901234567890123456789
		"0            QuakeWorld",
		"1    version " VSTR2 (PROTOCOL_VERSION) " by id Software",
		"0Programming",
		"1 John Carmack    Michael Abrash",
		"1 John Cash       Christian Antkow",
		"0Additional Programming",
		"1 Dave 'Zoid' Kirsch",
		"1 Jack 'morbid' Mathews",
		"0Id Software is not responsible for",
		"0providing technical support for",
		"0QUAKEWORLD(tm). (c)1996 Id Software,",
		"0Inc.  All Rights Reserved.",
		"0QUAKEWORLD(tm) is a trademark of Id",
		"0Software, Inc.",
		"1NOTICE: THE COPYRIGHT AND TRADEMARK",
		"1NOTICES APPEARING  IN YOUR COPY OF",
		"1QUAKE(r) ARE NOT MODIFIED BY THE USE",
		"1OF QUAKEWORLD(tm) AND REMAIN IN FULL",
		"1FORCE.",
		"0NIN(r) is a registered trademark",
		"0licensed to Nothing Interactive, Inc.",
		"0All rights reserved. Press y to exit",
		NULL
	};
	char      **p;
	int         y;

	if (wasInMenus) {
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}
#if 1
	M_DrawTextBox (0, 0, 38, 23);
	y = 12;
	for (p = cmsg; *p; p++, y += 8) {
		if (**p == '0')
			M_PrintWhite (16, y, *p + 1);
		else
			M_Print (16, y, *p + 1);
	}
#else
	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84, quitMessage[msgNumber * 4 + 0]);
	M_Print (64, 92, quitMessage[msgNumber * 4 + 1]);
	M_Print (64, 100, quitMessage[msgNumber * 4 + 2]);
	M_Print (64, 108, quitMessage[msgNumber * 4 + 3]);
#endif
}



//=============================================================================
/* Menu Subsystem */

void
M_Init_Cvars (void)
{
}

void
M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void
M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw) {
		if (scr_con_current) {
			Draw_ConsoleBackground (vid.height);
			S_ExtraUpdate ();
		} else
			Draw_FadeScreen ();
	} else {
		m_recursiveDraw = false;
	}

	switch (m_state) {
		case m_none:
			break;

		case m_main:
			M_Main_Draw ();
			break;

		case m_singleplayer:
			M_SinglePlayer_Draw ();
			break;

		case m_load:
//      M_Load_Draw ();
			break;

		case m_save:
//      M_Save_Draw ();
			break;

		case m_multiplayer:
			M_MultiPlayer_Draw ();
			break;

		case m_setup:
//      M_Setup_Draw ();
			break;

		case m_options:
			M_Options_Draw ();
			break;

		case m_keys:
			M_Keys_Draw ();
			break;

		case m_video:
			M_Video_Draw ();
			break;

		case m_help:
			M_Help_Draw ();
			break;

		case m_quit:
			M_Quit_Draw ();
			break;

		case m_serialconfig:
//      M_SerialConfig_Draw ();
			break;

		case m_modemconfig:
//      M_ModemConfig_Draw ();
			break;

		case m_lanconfig:
//      M_LanConfig_Draw ();
			break;

		case m_gameoptions:
//      M_GameOptions_Draw ();
			break;

		case m_search:
//      M_Search_Draw ();
			break;

		case m_slist:
//      M_ServerList_Draw ();
			break;

		case m_gfx:
			M_Gfx_Draw ();
			break;
	}

	if (m_entersound) {
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}


void
M_Keydown (int key)
{
	switch (m_state) {
		case m_none:
			return;

		case m_main:
			M_Main_Key (key);
			return;

		case m_singleplayer:
			M_SinglePlayer_Key (key);
			return;

		case m_load:
//      M_Load_Key (key);
			return;

		case m_save:
//      M_Save_Key (key);
			return;

		case m_multiplayer:
			M_MultiPlayer_Key (key);
			return;

		case m_setup:
//      M_Setup_Key (key);
			return;

		case m_options:
			M_Options_Key (key);
			return;

		case m_keys:
			M_Keys_Key (key);
			return;

		case m_video:
			M_Video_Key (key);
			return;

		case m_help:
			M_Help_Key (key);
			return;

		case m_quit:
			M_Quit_Key (key);
			return;

		case m_serialconfig:
//      M_SerialConfig_Key (key);
			return;

		case m_modemconfig:
//      M_ModemConfig_Key (key);
			return;

		case m_lanconfig:
//      M_LanConfig_Key (key);
			return;

		case m_gameoptions:
//      M_GameOptions_Key (key);
			return;

		case m_search:
//      M_Search_Key (key);
			break;

		case m_slist:
//      M_ServerList_Key (key);
			return;

		case m_gfx:
			M_Gfx_Key (key);
			return;
	}
}

