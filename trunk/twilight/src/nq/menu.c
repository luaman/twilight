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

#include <stdio.h>

#include "quakedef.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "glquake.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "net.h"
#include "sbar.h"
#include "screen.h"
#include "server.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "wad.h"

// FIXME: do this right
extern cvar_t *_windowed_mouse;
extern cvar_t *gl_texturemode;
extern cvar_t *gl_im_transform;

void        (*vid_menudrawfn) (void);
void        (*vid_menukeyfn) (int key);

enum { m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup,
	m_options, m_video, m_keys, m_help, m_quit, m_lanconfig,
	m_gameoptions, m_search, m_slist, m_gfx
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
	Draw_Alt_String (cx + ((vid.width - 320) >> 1), cy, str);
}

void
M_PrintWhite (int cx, int cy, char *str)
{
	Draw_String (cx + ((vid.width - 320) >> 1), cy, str);
}

void
M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320) >> 1), y, pic);
}


static int
M_ModalMessage (char *text)
{
	extern char *scr_notifystring;
	extern int scr_drawdialog;

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
	
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_ClearBuffer ();			// so dma doesn't loop current sound

	do {
		key_count = -1;			// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n'
			&& key_lastpress != K_ESCAPE);
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
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

	if (top < 128)
		// the artists made some backwards ranges.  sigh.
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
	}
	if (key_dest == key_console && cl.worldmodel) {
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

	f = (int) (host_time * 10) % 6;

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
				&& cls.state != ca_connected)
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
/* SINGLE PLAYER MENU */

int         m_singleplayer_cursor;

#define	SINGLEPLAYER_ITEMS	3


void
M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


void
M_SinglePlayer_Draw (void)
{
	int         f;
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp"));

	f = (int) (host_time * 10) % 6;

	M_DrawPic (54, 32 + m_singleplayer_cursor * 20,
					Draw_CachePic (va ("gfx/menudot%i.lmp", f + 1)));
}


void
M_SinglePlayer_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
			M_Menu_Main_f ();
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
				m_singleplayer_cursor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			if (--m_singleplayer_cursor < 0)
				m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
			break;

		case K_ENTER:
			m_entersound = true;

			switch (m_singleplayer_cursor) {
				case 0:
					if (sv.active)
						if (!M_ModalMessage
							("Are you sure you want to\nstart a new game?\n"))
							break;
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("maxplayers 1\n");
					Cbuf_AddText ("map start\n");
					break;

				case 1:
					M_Menu_Load_f ();
					break;

				case 2:
					M_Menu_Save_f ();
					break;
			}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

int         load_cursor;				// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
char        m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
int         loadable[MAX_SAVEGAMES];

void
M_ScanSaves (void)
{
	int         i, j;
	char        name[MAX_OSPATH];
	FILE       *f;
	int         version;

	for (i = 0; i < MAX_SAVEGAMES; i++) {
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		snprintf (name, sizeof (name), "%s/s%i.sav", com_gamedir, i);
		f = fopen (name, "r");
		if (!f)
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		strlcpy (m_filenames[i], name, sizeof (m_filenames[i]));

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

void
M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();
}


void
M_Menu_Save_f (void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (svs.maxclients != 1)
		return;
	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();
}


void
M_Load_Draw (void)
{
	int         i;
	qpic_t     *p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8 * i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, 12 + ((int) (host_realtime * 4) & 1));
}


void
M_Save_Draw (void)
{
	int         i;
	qpic_t     *p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8 * i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, 12 + ((int) (host_realtime * 4) & 1));
}


void
M_Load_Key (int k)
{
	switch (k) {
		case K_ESCAPE:
			M_Menu_SinglePlayer_f ();
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			if (!loadable[load_cursor])
				return;
			m_state = m_none;
			key_dest = key_game;

			// Host_Loadgame_f can't bring up the loading plaque because too
			// much stack space has been used, so do it now
			SCR_BeginLoadingPlaque ();

			// issue the load command
			Cbuf_AddText (va ("load s%i\n", load_cursor));
			return;

		case K_UPARROW:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES - 1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			if (load_cursor >= MAX_SAVEGAMES)
				load_cursor = 0;
			break;
	}
}


void
M_Save_Key (int k)
{
	switch (k) {
		case K_ESCAPE:
			M_Menu_SinglePlayer_f ();
			break;

		case K_ENTER:
			m_state = m_none;
			key_dest = key_game;
			Cbuf_AddText (va ("save s%i\n", load_cursor));
			return;

		case K_UPARROW:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES - 1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			if (load_cursor >= MAX_SAVEGAMES)
				load_cursor = 0;
			break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int         m_multiplayer_cursor;

#define	MULTIPLAYER_ITEMS	3


void
M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


void
M_MultiPlayer_Draw (void)
{
	int         f;
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	M_DrawPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp"));

	f = (int) (host_time * 10) % 6;

	M_DrawPic (54, 32 + m_multiplayer_cursor * 20,
					Draw_CachePic (va ("gfx/menudot%i.lmp", f + 1)));

	if (tcpipAvailable)
		return;
	M_PrintWhite ((320 / 2) - ((27 * 8) / 2), 148,
				  "No Communications Available");
}


void
M_MultiPlayer_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
			M_Menu_Main_f ();
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
				m_multiplayer_cursor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			if (--m_multiplayer_cursor < 0)
				m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
			break;

		case K_ENTER:
			m_entersound = true;
			switch (m_multiplayer_cursor) {
				case 0:
					if (tcpipAvailable)
						M_Menu_LanConfig_f ();
					break;

				case 1:
					if (tcpipAvailable)
						M_Menu_LanConfig_f ();
					break;

				case 2:
					M_Menu_Setup_f ();
					break;
			}
	}
}

//=============================================================================
/* SETUP MENU */

int         setup_cursor = 4;
int         setup_cursor_table[] = { 40, 56, 80, 104, 140 };

char        setup_hostname[16];
char        setup_myname[16];
int         setup_oldtop;
int         setup_oldbottom;
int         setup_top;
int         setup_bottom;

#define	NUM_SETUP_CMDS	5

void
M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	strcpy (setup_myname, _cl_name->svalue);
	strcpy (setup_hostname, hostname->svalue);
	setup_top = setup_oldtop = _cl_color->ivalue >> 4;
	setup_bottom = setup_oldbottom = _cl_color->ivalue & 15;
}


void
M_Setup_Draw (void)
{
	qpic_t     *p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	M_Print (64, 40, "Hostname");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_DrawTextBox (160, 48, 16, 1);
	M_Print (168, 56, setup_myname);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140 - 8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable (setup_top * 16, setup_bottom * 16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table[setup_cursor],
					 12 + ((int) (host_realtime * 4) & 1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8 * strlen (setup_hostname),
						 setup_cursor_table[setup_cursor],
						 10 + ((int) (host_realtime * 4) & 1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8 * strlen (setup_myname),
						 setup_cursor_table[setup_cursor],
						 10 + ((int) (host_realtime * 4) & 1));
}


void
M_Setup_Key (int k)
{
	int         l;

	switch (k) {
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor--;
			if (setup_cursor < 0)
				setup_cursor = NUM_SETUP_CMDS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor++;
			if (setup_cursor >= NUM_SETUP_CMDS)
				setup_cursor = 0;
			break;

		case K_LEFTARROW:
			if (setup_cursor < 2)
				return;
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top - 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom - 1;
			break;
		case K_RIGHTARROW:
			if (setup_cursor < 2)
				return;
		  forward:
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top + 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom + 1;
			break;

		case K_ENTER:
			if (setup_cursor == 0 || setup_cursor == 1)
				return;

			if (setup_cursor == 2 || setup_cursor == 3)
				goto forward;

			// setup_cursor == 4 (OK)
			if (strcmp (_cl_name->svalue, setup_myname) != 0)
				Cbuf_AddText (va ("name \"%s\"\n", setup_myname));
			if (strcmp (hostname->svalue, setup_hostname) != 0)
				Cvar_Set (hostname, setup_hostname);
			if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
				Cbuf_AddText (va ("color %i %i\n", setup_top, setup_bottom));
			m_entersound = true;
			M_Menu_MultiPlayer_f ();
			break;

		case K_BACKSPACE:
			if (setup_cursor == 0) {
				if (strlen (setup_hostname))
					setup_hostname[strlen (setup_hostname) - 1] = 0;
			}

			if (setup_cursor == 1) {
				if (strlen (setup_myname))
					setup_myname[strlen (setup_myname) - 1] = 0;
			}
			break;

		default:
			if (k < 32 || k > 127)
				break;
			if (setup_cursor == 0) {
				l = strlen (setup_hostname);
				if (l < 15) {
					setup_hostname[l + 1] = 0;
					setup_hostname[l] = k;
				}
			}
			if (setup_cursor == 1) {
				l = strlen (setup_myname);
				if (l < 15) {
					setup_myname[l + 1] = 0;
					setup_myname[l] = k;
				}
			}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
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
			t = scr_viewsize->ivalue + (dir * 10.0f);
			t = bound (30, t, 120);
			Cvar_Set (scr_viewsize, va("%f", t));
			break;
		case 4:						// gamma
			t = v_gamma->fvalue + (dir * 0.05f);
			t = bound (1.0, t, 2.0);
			Cvar_Set (v_gamma, va("%f", t));
			break;
		case 5:						// software brightness
			t = r_brightness->fvalue + (dir * 0.25);
			t = bound (1, t, 5);
			Cvar_Set (r_brightness, va("%f", t));
			break;
		case 6:						// software contrast (base brightness)
			t = r_contrast->fvalue + (dir * 0.025);
			t = bound (.75, t, 1);
			Cvar_Set (r_contrast, va("%f", t));
			break;
		case 7:						// mouse speed
			t = sensitivity->fvalue + (dir * 0.5f);
			t = bound (1, t, 11);
			Cvar_Set (sensitivity, va("%f", t));
			break;
		case 8:						// music volume
			// Slider doesn't work with SDL
			Cvar_Set (bgmvolume, bgmvolume->fvalue ? "0" : "1");
			break;
		case 9:						// sfx volume
			t = volume->fvalue + (dir * 0.1f);
			t = bound (0, t, 1);
			Cvar_Set (volume, va("%f", t));
			break;

		case 10:						// always run
			if (cl_forwardspeed->fvalue > 200) {
				Cvar_Set (cl_forwardspeed, "200");
				Cvar_Set (cl_backspeed, "200");
			} else {
				Cvar_Set (cl_forwardspeed, "400");
				Cvar_Set (cl_backspeed, "400");
			}
			break;

		case 11:						// invert mouse
			t = -m_pitch->fvalue;
			Cvar_Set (m_pitch, va("%f", t));
			break;

		case 12:						// lookspring
			Cvar_Set (lookspring, va("%i", !lookspring->ivalue));
			break;

		case 13:						// lookstrafe
			Cvar_Set (lookstrafe, va("%i", !lookstrafe->ivalue));
			break;

		case 14:
			Cvar_Set (cl_sbar,  va ("%i", !cl_sbar->ivalue));
			break;

		case 15:
			Cvar_Set (cl_hudswap, va ("%i", !cl_hudswap->ivalue));
			break;

		case 16:						// _windowed_mouse
			Cvar_Set (_windowed_mouse, va("%i", !_windowed_mouse->ivalue));
			break;
	}
}


void
M_DrawSlider (int x, int y, float range)
{
	int         i;

	range = bound (0, range, 1);

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
	int 	y;
	qpic_t	*p;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	y = 32;
	M_Print (16, y, "    Customize controls"); y += 8;
	M_Print (16, y, "         Go to console"); y += 8;
	M_Print (16, y, "     Reset to defaults"); y += 8;

	M_Print (16, y, "           Screen size"); M_DrawSlider (220, y, (scr_viewsize->ivalue - 30) / (120 - 30)); y += 8;
	M_Print (16, y, "        Hardware Gamma"); M_DrawSlider (220, y, v_gamma->fvalue - 1.0); y += 8;
	M_Print (16, y, "   Software Brightness"); M_DrawSlider (220, y, (r_brightness->fvalue - 1) / 4); y += 8;
	M_Print (16, y, "     Software Contrast"); M_DrawSlider (220, y, (r_contrast->fvalue - 0.75) * 4); y += 8;
	M_Print (16, y, "           Mouse Speed"); M_DrawSlider (220, y, (sensitivity->fvalue - 1) / 10); y += 8;
	M_Print (16, y, "       CD Music Volume"); M_DrawSlider (220, y, bgmvolume->fvalue); y += 8;
	M_Print (16, y, "          Sound Volume"); M_DrawSlider (220, y, volume->fvalue); y += 8;
	M_Print (16, y, "            Always Run"); M_DrawCheckbox (220, y, cl_forwardspeed->fvalue > 200); y += 8;
	M_Print (16, y, "          Invert Mouse"); M_DrawCheckbox (220, y, m_pitch->fvalue < 0); y += 8;
	M_Print (16, y, "            Lookspring"); M_DrawCheckbox (220, y, lookspring->ivalue); y += 8;
	M_Print (16, y, "            Lookstrafe"); M_DrawCheckbox (220, y, lookstrafe->ivalue); y += 8;
	M_Print (16, y, "    Use old status bar"); M_DrawCheckbox (220, y, cl_sbar->ivalue); y += 8;
	M_Print (16, y, "      HUD on left side"); M_DrawCheckbox (220, y, cl_hudswap->ivalue); y += 8;
	M_Print (16, y, "             Use Mouse"); M_DrawCheckbox (220, y, _windowed_mouse->ivalue); y += 8;
	M_Print (16, y, "      Graphics Options"); y += 8;

	if (vid_menudrawfn) {
		M_Print (16, y, "         Video Options"); y += 8;
	}

	// cursor
	M_DrawCharacter (200, 32 + options_cursor * 8, 12 + ((int) (host_realtime * 4) & 1));
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
					M_Menu_Gfx_f();
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
	Fullbright bmodels		on/off
	Fast dynamic lights		on/off
	Shadows					on/fast/nice
	Frame interpolation	    on/off
	Movement interpolation	on/off
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
	M_Print (16, y, "         Affine models"); M_DrawCheckbox (220, y, gl_affinemodels->ivalue); y += 8;
	M_Print (16, y, "     Fullbright models"); M_DrawCheckbox (220, y, gl_fb_models->ivalue); y += 8;
	M_Print (16, y, "    Fullbright bmodels"); M_DrawCheckbox (220, y, gl_fb_bmodels->ivalue); y += 8;
	M_Print (16, y, "   Fast dynamic lights"); M_DrawCheckbox (220, y, gl_flashblend->ivalue); y += 8;
	M_Print (16, y, "               Shadows"); M_Print (220, y, (r_shadows->ivalue) ? (r_shadows->ivalue == 2 ? "nice" : "fast") : "off"); y += 8;
	M_Print (16, y, "   Frame interpolation"); M_DrawCheckbox (220, y, gl_im_animation->ivalue); y += 8;
	M_Print (16, y, "  Motion interpolation"); M_DrawCheckbox (220, y, gl_im_transform->ivalue); y += 8;
	M_Print (16, y, "          Texture mode"); M_Print (220, y, gl_texturemode->svalue); y += 8;
	M_Print (16, y, "         Light lerping"); M_DrawCheckbox (220, y, r_lightlerp->ivalue); y += 8;
	M_Print (16, y, "      Particle torches"); M_DrawCheckbox (220, y, gl_particletorches->ivalue);

	// cursor
	M_DrawCharacter (200, 32 + gfx_cursor * 8, 12 + ((int) (host_realtime * 4) & 1));
}

void
M_Gfx_Set (void)
{
	int v = 0;

	S_LocalSound ("misc/menu3.wav");

	switch (gfx_cursor)
	{
		case 0:
			v = !gl_affinemodels->ivalue;
			Cvar_Set (gl_affinemodels, va("%i", v));
			break;

		case 1:
			v = !gl_fb_models->ivalue;
			Cvar_Set (gl_fb_models, va("%i", v));
			break;

		case 2:
			v = !gl_fb_bmodels->ivalue;
			Cvar_Set (gl_fb_bmodels, va("%i", v));
			break;

		case 3:
			v = !gl_flashblend->ivalue;
			Cvar_Set (gl_flashblend, va("%i", v));
			break;

		case 4:
			v = r_shadows->ivalue + 1;
			if (v > 2)
				v = 0;
			Cvar_Set (r_shadows, va("%i", v));
			break;

		case 5:
			v = !gl_im_animation->ivalue;
			Cvar_Set (gl_im_animation, va("%i", v));
			break;

		case 6:
			v = !gl_im_transform->ivalue;
			Cvar_Set (gl_im_transform, va("%i", v));
			break;

		case 7:
			for (v = 0; v < 6; v++) {
				if (strcasecmp (texmodes[v].name, gl_texturemode->svalue) == 0)
					break;
			}
			v++;
			if (v > 5)
				v = 0;
			Cvar_Set (gl_texturemode, texmodes[v].name);
			break;

		case 8:
			v = !r_lightlerp->ivalue;
			Cvar_Set (r_lightlerp, va("%i", v));
			break;

		case 9:
			v = !gl_particletorches->ivalue;
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
	int         count;
	int         j;
	char       *b;

	twokeys[0] = twokeys[1] = -1;
	count = 0;

	for (j = 0; j < 256; j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strcmp (b, command)) {
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
	int         j;
	int         l;
	char       *b;

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
	Uint32		i, l, x, y;
	int			keys[2];
	char		*name;
	qpic_t		*p;

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
						 12 + ((int) (host_realtime * 4) & 1));
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

		case K_ENTER:					// go into bind mode
			M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
			S_LocalSound ("misc/menu2.wav");
			if (keys[1] != -1)
				M_UnbindCommand (bindnames[keys_cursor][0]);
			bind_grab = true;
			break;

		case K_BACKSPACE:				// delete bindings
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

#ifndef	_WIN32
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
#endif

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
			Host_Quit_f ();
			break;

		default:
			break;
	}

}


void
M_Quit_Draw (void)
{
	if (wasInMenus) {
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}
#ifdef _WIN32
	M_DrawTextBox (0, 0, 38, 23);
	M_PrintWhite (16, 12, "  Quake version 1.09 by id Software\n\n");
	M_PrintWhite (16, 28, "Programming        Art \n");
	M_Print (16, 36, " John Carmack       Adrian Carmack\n");
	M_Print (16, 44, " Michael Abrash     Kevin Cloud\n");
	M_Print (16, 52, " John Cash          Paul Steed\n");
	M_Print (16, 60, " Dave 'Zoid' Kirsch\n");
	M_PrintWhite (16, 68, "Design             Biz\n");
	M_Print (16, 76, " John Romero        Jay Wilbur\n");
	M_Print (16, 84, " Sandy Petersen     Mike Wilson\n");
	M_Print (16, 92, " American McGee     Donna Jackson\n");
	M_Print (16, 100, " Tim Willits        Todd Hollenshead\n");
	M_PrintWhite (16, 108, "Support            Projects\n");
	M_Print (16, 116, " Barrett Alexander  Shawn Green\n");
	M_PrintWhite (16, 124, "Sound Effects\n");
	M_Print (16, 132, " Trent Reznor and Nine Inch Nails\n\n");
	M_PrintWhite (16, 140, "Quake is a trademark of Id Software,\n");
	M_PrintWhite (16, 148, "inc., (c)1996 Id Software, inc. All\n");
	M_PrintWhite (16, 156, "rights reserved. NIN logo is a\n");
	M_PrintWhite (16, 164, "registered trademark licensed to\n");
	M_PrintWhite (16, 172, "Nothing Interactive, Inc. All rights\n");
	M_PrintWhite (16, 180, "reserved. Press y to exit\n");
#else
	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84, quitMessage[msgNumber * 4 + 0]);
	M_Print (64, 92, quitMessage[msgNumber * 4 + 1]);
	M_Print (64, 100, quitMessage[msgNumber * 4 + 2]);
	M_Print (64, 108, quitMessage[msgNumber * 4 + 3]);
#endif
}


//=============================================================================
/* LAN CONFIG MENU */

int         lanConfig_cursor = -1;
int         lanConfig_cursor_table[] = { 72, 92, 124 };

#define NUM_LANCONFIG_CMDS	3

int         lanConfig_port;
char        lanConfig_portname[6];
char        lanConfig_joinname[22];

void
M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1) {
		if (JoiningGame)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	snprintf (lanConfig_portname, sizeof (lanConfig_portname), "%u",
			  lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void
M_LanConfig_Draw (void)
{
	qpic_t     *p;
	int         basex;
	char       *startJoin;
	char       *protocol;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320 - p->width) / 2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	M_Print (basex + 9 * 8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex + 8 * 8, lanConfig_cursor_table[0] - 8, 6, 1);
	M_Print (basex + 9 * 8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame) {
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print (basex, 108, "Join game at:");
		M_DrawTextBox (basex + 8, lanConfig_cursor_table[2] - 8, 22, 1);
		M_Print (basex + 16, lanConfig_cursor_table[2], lanConfig_joinname);
	} else {
		M_DrawTextBox (basex, lanConfig_cursor_table[1] - 8, 2, 1);
		M_Print (basex + 8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex - 8, lanConfig_cursor_table[lanConfig_cursor],
					 12 + ((int) (host_realtime * 4) & 1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex + 9 * 8 + 8 * strlen (lanConfig_portname),
						 lanConfig_cursor_table[0],
						 10 + ((int) (host_realtime * 4) & 1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex + 16 + 8 * strlen (lanConfig_joinname),
						 lanConfig_cursor_table[2],
						 10 + ((int) (host_realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}


void
M_LanConfig_Key (int key)
{
	int         l;

	switch (key) {
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			lanConfig_cursor--;
			if (lanConfig_cursor < 0)
				lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			lanConfig_cursor++;
			if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
				lanConfig_cursor = 0;
			break;

		case K_ENTER:
			if (lanConfig_cursor == 0)
				break;

			m_entersound = true;

			M_ConfigureNetSubsystem ();

			if (lanConfig_cursor == 1) {
				if (StartingGame) {
					M_Menu_GameOptions_f ();
					break;
				}
				M_Menu_Search_f ();
				break;
			}

			if (lanConfig_cursor == 2) {
				m_return_state = m_state;
				m_return_onerror = true;
				key_dest = key_game;
				m_state = m_none;
				Cbuf_AddText (va ("connect \"%s\"\n", lanConfig_joinname));
				break;
			}

			break;

		case K_BACKSPACE:
			if (lanConfig_cursor == 0) {
				if (strlen (lanConfig_portname))
					lanConfig_portname[strlen (lanConfig_portname) - 1] = 0;
			}

			if (lanConfig_cursor == 2) {
				if (strlen (lanConfig_joinname))
					lanConfig_joinname[strlen (lanConfig_joinname) - 1] = 0;
			}
			break;

		default:
			if (key < 32 || key > 127)
				break;

			if (lanConfig_cursor == 2) {
				l = strlen (lanConfig_joinname);
				if (l < 21) {
					lanConfig_joinname[l + 1] = 0;
					lanConfig_joinname[l] = key;
				}
			}

			if (key < '0' || key > '9')
				break;
			if (lanConfig_cursor == 0) {
				l = strlen (lanConfig_portname);
				if (l < 5) {
					lanConfig_portname[l + 1] = 0;
					lanConfig_portname[l] = key;
				}
			}
	}

	if (StartingGame && lanConfig_cursor == 2) {
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l = Q_atoi (lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	snprintf (lanConfig_portname, sizeof (lanConfig_portname), "%u",
			  lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct {
	char       *name;
	char       *description;
} level_t;

level_t     levels[] = {
	{"start", "Entrance"},				// 0

	{"e1m1", "Slipgate Complex"},		// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},		// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},	// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},		// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},	// 31

	{"dm1", "Place of Two Deaths"},		// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t     hipnoticlevels[] = {
	{"start", "Command HQ"},			// 0

	{"hip1m1", "The Pumping Station"},	// 1
	{"hip1m2", "Storage Facility"},
	{"hip1m3", "The Lost Mine"},
	{"hip1m4", "Research Facility"},
	{"hip1m5", "Military Complex"},

	{"hip2m1", "Ancient Realms"},		// 6
	{"hip2m2", "The Black Cathedral"},
	{"hip2m3", "The Catacombs"},
	{"hip2m4", "The Crypt"},
	{"hip2m5", "Mortum's Keep"},
	{"hip2m6", "The Gremlin's Domain"},

	{"hip3m1", "Tur Torment"},			// 12
	{"hip3m2", "Pandemonium"},
	{"hip3m3", "Limbo"},
	{"hip3m4", "The Gauntlet"},

	{"hipend", "Armagon's Lair"},		// 16

	{"hipdm1", "The Edge of Oblivion"}	// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t     roguelevels[] = {
	{"start", "Split Decision"},
	{"r1m1", "Deviant's Domain"},
	{"r1m2", "Dread Portal"},
	{"r1m3", "Judgement Call"},
	{"r1m4", "Cave of Death"},
	{"r1m5", "Towers of Wrath"},
	{"r1m6", "Temple of Pain"},
	{"r1m7", "Tomb of the Overlord"},
	{"r2m1", "Tempus Fugit"},
	{"r2m2", "Elemental Fury I"},
	{"r2m3", "Elemental Fury II"},
	{"r2m4", "Curse of Osiris"},
	{"r2m5", "Wizard's Keep"},
	{"r2m6", "Blood Sacrifice"},
	{"r2m7", "Last Bastion"},
	{"r2m8", "Source of Evil"},
	{"ctf1", "Division of Change"}
};

typedef struct {
	char       *description;
	int         firstLevel;
	int         levels;
} episode_t;

episode_t   episodes[] = {
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t   hipnoticepisodes[] = {
	{"Scourge of Armagon", 0, 1},
	{"Fortress of the Dead", 1, 5},
	{"Dominion of Darkness", 6, 6},
	{"The Rift", 12, 4},
	{"Final Level", 16, 1},
	{"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t   rogueepisodes[] = {
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int         startepisode;
int         startlevel;
int         maxplayers;
qboolean    m_serverInfoMessage = false;
double      m_serverInfoMessageTime;

void
M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}


int         gameoptions_cursor_table[] =
	{ 40, 56, 64, 72, 80, 88, 96, 112, 120 };
#define	NUM_GAMEOPTIONS	9
int         gameoptions_cursor;

void
M_GameOptions_Draw (void)
{
	qpic_t     *p;
	int         x;

	M_DrawPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va ("%i", maxplayers));

	M_Print (0, 64, "        Game Type");
	if (coop->ivalue)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	if (rogue) {
		char       *msg;

		switch (teamplay->ivalue) {
			case 1:
				msg = "No Friendly Fire";
				break;
			case 2:
				msg = "Friendly Fire";
				break;
			case 3:
				msg = "Tag";
				break;
			case 4:
				msg = "Capture the Flag";
				break;
			case 5:
				msg = "One Flag CTF";
				break;
			case 6:
				msg = "Three Team CTF";
				break;
			default:
				msg = "Off";
				break;
		}
		M_Print (160, 72, msg);
	} else {
		char       *msg;

		switch (teamplay->ivalue) {
			case 1:
				msg = "No Friendly Fire";
				break;
			case 2:
				msg = "Friendly Fire";
				break;
			default:
				msg = "Off";
				break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill->ivalue == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill->ivalue == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill->ivalue == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit->ivalue == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va ("%i frags", (int) fraglimit->ivalue));

	M_Print (0, 96, "       Time Limit");
	if (timelimit->ivalue == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va ("%i minutes", (int) timelimit->ivalue));

	M_Print (0, 112, "         Episode");
	// MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print (160, 112, hipnoticepisodes[startepisode].description);
	// PGM 01/07/97 added rogue episodes
	else if (rogue)
		M_Print (160, 112, rogueepisodes[startepisode].description);
	else
		M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");
	// MED 01/06/97 added hipnotic episodes
	if (hipnotic) {
		M_Print (160, 120,
				 hipnoticlevels[hipnoticepisodes[startepisode].firstLevel +
								startlevel].description);
		M_Print (160, 128,
				 hipnoticlevels[hipnoticepisodes[startepisode].firstLevel +
								startlevel].name);
	}
	// PGM 01/07/97 added rogue episodes
	else if (rogue) {
		M_Print (160, 120,
				 roguelevels[rogueepisodes[startepisode].firstLevel +
							 startlevel].description);
		M_Print (160, 128,
				 roguelevels[rogueepisodes[startepisode].firstLevel +
							 startlevel].name);
	} else {
		M_Print (160, 120,
				 levels[episodes[startepisode].firstLevel +
						startlevel].description);
		M_Print (160, 128,
				 levels[episodes[startepisode].firstLevel + startlevel].name);
	}

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor],
					 12 + ((int) (host_realtime * 4) & 1));

	if (m_serverInfoMessage) {
		if ((host_realtime - m_serverInfoMessageTime) < 5.0) {
			x = (320 - 26 * 8) / 2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		} else {
			m_serverInfoMessage = false;
		}
	}
}


void
M_NetStart_Change (int dir)
{
	int         count;
	int			t;

	switch (gameoptions_cursor) {
		case 1:
			maxplayers += dir;
			if (maxplayers > svs.maxclientslimit) {
				maxplayers = svs.maxclientslimit;
				m_serverInfoMessage = true;
				m_serverInfoMessageTime = host_realtime;
			}
			if (maxplayers < 2)
				maxplayers = 2;
			break;

		case 2:
			Cvar_Set (coop, coop->ivalue ? "0" : "1");
			break;

		case 3:
			if (rogue)
				count = 6;
			else
				count = 2;

			t = teamplay->ivalue + dir;
			t = bound (0, t, count);
			Cvar_Set (teamplay, va("%i", t));
			break;

		case 4:
			t = skill->ivalue + dir;
			t = bound (0, t, 3);
			Cvar_Set (skill, va("%i", t));
			break;

		case 5:
			t = fraglimit->ivalue + dir * 10;
			t = bound (0, t, 100);
			Cvar_Set (fraglimit, va("%i", t));
			break;

		case 6:
			t = timelimit->ivalue + dir * 5;
			t = bound (0, t, 60);
			Cvar_Set (timelimit, va("%i", t));
			break;

		case 7:
			// MED 01/06/97 added hipnotic count
			if (hipnotic)
				count = 6;
			// PGM 01/07/97 added rogue count
			// PGM 03/02/97 added 1 for dmatch episode
			else if (rogue)
				count = 4;
			else if (registered->ivalue)
				count = 7;
			else
				count = 2;

			startepisode = bound (0, startepisode + dir, count - 1);
			startlevel = 0;
			break;

		case 8:
			// MED 01/06/97 added hipnotic episodes
			if (hipnotic)
				count = hipnoticepisodes[startepisode].levels;
			// PGM 01/06/97 added hipnotic episodes
			else if (rogue)
				count = rogueepisodes[startepisode].levels;
			else
				count = episodes[startepisode].levels;

			startlevel = bound (0, startlevel + dir, count - 1);
			break;
	}
}

void
M_GameOptions_Key (int key)
{
	switch (key) {
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor--;
			if (gameoptions_cursor < 0)
				gameoptions_cursor = NUM_GAMEOPTIONS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor++;
			if (gameoptions_cursor >= NUM_GAMEOPTIONS)
				gameoptions_cursor = 0;
			break;

		case K_LEFTARROW:
			if (gameoptions_cursor == 0)
				break;
			S_LocalSound ("misc/menu3.wav");
			M_NetStart_Change (-1);
			break;

		case K_RIGHTARROW:
			if (gameoptions_cursor == 0)
				break;
			S_LocalSound ("misc/menu3.wav");
			M_NetStart_Change (1);
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			if (gameoptions_cursor == 0) {
				if (sv.active)
					Cbuf_AddText ("disconnect\n");
				Cbuf_AddText ("listen 0\n");	// so host_netport will be
				// re-examined
				Cbuf_AddText (va ("maxplayers %u\n", maxplayers));
				SCR_BeginLoadingPlaque ();

				if (hipnotic)
					Cbuf_AddText (va
								  ("map %s\n",
								   hipnoticlevels[hipnoticepisodes
												  [startepisode].firstLevel +
												  startlevel].name));
				else if (rogue)
					Cbuf_AddText (va
								  ("map %s\n",
								   roguelevels[rogueepisodes[startepisode].
											   firstLevel + startlevel].name));
				else
					Cbuf_AddText (va
								  ("map %s\n",
								   levels[episodes[startepisode].firstLevel +
										  startlevel].name));

				return;
			}

			M_NetStart_Change (1);
			break;
	}
}

//=============================================================================
/* SEARCH MENU */

qboolean    searchComplete = false;
double      searchCompleteTime;

void
M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f ();

}


void
M_Search_Draw (void)
{
	qpic_t     *p;
	int         x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	x = (320 / 2) - ((12 * 8) / 2) + 4;
	M_DrawTextBox (x - 8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if (slistInProgress) {
		NET_Poll ();
		return;
	}

	if (!searchComplete) {
		searchComplete = true;
		searchCompleteTime = host_realtime;
	}

	if (hostCacheCount) {
		M_Menu_ServerList_f ();
		return;
	}

	M_PrintWhite ((320 / 2) - ((22 * 8) / 2), 64, "No Quake servers found");
	if ((host_realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}


void
M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int         slist_cursor;
qboolean    slist_sorted;

void
M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}


void
M_ServerList_Draw (void)
{
	int         n;
	char        string[64];
	qpic_t     *p;

	if (!slist_sorted) {
		if (hostCacheCount > 1) {
			int         i, j;
			hostcache_t temp;

			for (i = 0; i < hostCacheCount; i++)
				for (j = i + 1; j < hostCacheCount; j++)
					if (strcmp (hostcache[j].name, hostcache[i].name) < 0) {
						memcpy (&temp, &hostcache[j], sizeof (hostcache_t));
						memcpy (&hostcache[j], &hostcache[i],
								  sizeof (hostcache_t));
						memcpy (&hostcache[i], &temp, sizeof (hostcache_t));
					}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);
	for (n = 0; n < hostCacheCount; n++) {
		if (hostcache[n].maxusers)
			snprintf (string, sizeof (string), "%-15.15s %-15.15s %2u/%2u\n",
					  hostcache[n].name, hostcache[n].map, hostcache[n].users,
					  hostcache[n].maxusers);
		else
			snprintf (string, sizeof (string), "%-15.15s %-15.15s\n",
					  hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8 * n, string);
	}
	M_DrawCharacter (0, 32 + slist_cursor * 8, 12 + ((int) (host_realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
}


void
M_ServerList_Key (int k)
{
	switch (k) {
		case K_ESCAPE:
			M_Menu_LanConfig_f ();
			break;

		case K_SPACE:
			M_Menu_Search_f ();
			break;

		case K_UPARROW:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor--;
			if (slist_cursor < 0)
				slist_cursor = hostCacheCount - 1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor++;
			if (slist_cursor >= hostCacheCount)
				slist_cursor = 0;
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			m_return_state = m_state;
			m_return_onerror = true;
			slist_sorted = false;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText (va
						  ("connect \"%s\"\n", hostcache[slist_cursor].cname));
			break;

		default:
			break;
	}

}

//=============================================================================
/* Menu Subsystem */

void
M_Init_Cvars(void)
{
}

void
M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
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
			M_Load_Draw ();
			break;

		case m_save:
			M_Save_Draw ();
			break;

		case m_multiplayer:
			M_MultiPlayer_Draw ();
			break;

		case m_setup:
			M_Setup_Draw ();
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

		case m_lanconfig:
			M_LanConfig_Draw ();
			break;

		case m_gameoptions:
			M_GameOptions_Draw ();
			break;

		case m_search:
			M_Search_Draw ();
			break;

		case m_slist:
			M_ServerList_Draw ();
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
			M_Load_Key (key);
			return;

		case m_save:
			M_Save_Key (key);
			return;

		case m_multiplayer:
			M_MultiPlayer_Key (key);
			return;

		case m_setup:
			M_Setup_Key (key);
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

		case m_lanconfig:
			M_LanConfig_Key (key);
			return;

		case m_gameoptions:
			M_GameOptions_Key (key);
			return;

		case m_search:
			M_Search_Key (key);
			break;

		case m_slist:
			M_ServerList_Key (key);
			return;

		case m_gfx:
			M_Gfx_Key (key);
			return;
	}
}


void
M_ConfigureNetSubsystem (void)
{
// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");

	net_hostport = lanConfig_port;
}

