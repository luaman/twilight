/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"
#include "keys.h"
#ifdef _WINDOWS
#include <windows.h>
#endif
/*

key up events are sent even if in console mode

*/


#define		MAXCMDLINE	256
char        key_lines[32][MAXCMDLINE];
int         key_linepos;
int         key_lastpress;

int         edit_line = 0;
int         history_line = 0;

keydest_t   key_dest;
kgt_t       game_target = KGT_DEFAULT;

int         key_count;					// incremented every key event

char       *keybindings[KGT_MAX][KSYM_LAST];
int         keydown[KSYM_LAST];			// if > 1, it is autorepeating

typedef struct {
	char       *name;
	int         kgtnum;
} kgtname_t;

kgtname_t   kgtnames[] = {
	{"KGT_CONSOLE", KGT_CONSOLE},
	{"KGT_DEFAULT", KGT_DEFAULT},
	{"KGT_0", KGT_DEFAULT},
	{"KGT_1", KGT_1},
	{"KGT_2", KGT_2},
	{NULL, 0}
};

typedef struct {
	char       *name;
	int         keynum;
} keyname_t;

keyname_t   keynames[] = {
	{"K_UNKNOWN", SDLK_UNKNOWN},
	{"K_FIRST", SDLK_FIRST},
	{"K_BACKSPACE", SDLK_BACKSPACE},
	{"K_TAB", SDLK_TAB},
	{"K_CLEAR", SDLK_CLEAR},
	{"K_RETURN", SDLK_RETURN},
	{"K_PAUSE", SDLK_PAUSE},
	{"K_ESCAPE", SDLK_ESCAPE},
	{"K_SPACE", SDLK_SPACE},
	{"K_EXCLAIM", SDLK_EXCLAIM},
	{"K_QUOTEDBL", SDLK_QUOTEDBL},
	{"K_HASH", SDLK_HASH},
	{"K_DOLLAR", SDLK_DOLLAR},
	{"K_AMPERSAND", SDLK_AMPERSAND},
	{"K_QUOTE", SDLK_QUOTE},
	{"K_LEFTPAREN", SDLK_LEFTPAREN},
	{"K_RIGHTPAREN", SDLK_RIGHTPAREN},
	{"K_ASTERISK", SDLK_ASTERISK},
	{"K_PLUS", SDLK_PLUS},
	{"K_COMMA", SDLK_COMMA},
	{"K_MINUS", SDLK_MINUS},
	{"K_PERIOD", SDLK_PERIOD},
	{"K_SLASH", SDLK_SLASH},
	{"K_0", SDLK_0},
	{"K_1", SDLK_1},
	{"K_2", SDLK_2},
	{"K_3", SDLK_3},
	{"K_4", SDLK_4},
	{"K_5", SDLK_5},
	{"K_6", SDLK_6},
	{"K_7", SDLK_7},
	{"K_8", SDLK_8},
	{"K_9", SDLK_9},
	{"K_COLON", SDLK_COLON},
	{"K_SEMICOLON", SDLK_SEMICOLON},
	{"K_LESS", SDLK_LESS},
	{"K_EQUALS", SDLK_EQUALS},
	{"K_GREATER", SDLK_GREATER},
	{"K_QUESTION", SDLK_QUESTION},
	{"K_AT", SDLK_AT},
	{"K_LEFTBRACKET", SDLK_LEFTBRACKET},
	{"K_BACKSLASH", SDLK_BACKSLASH},
	{"K_RIGHTBRACKET", SDLK_RIGHTBRACKET},
	{"K_CARET", SDLK_CARET},
	{"K_UNDERSCORE", SDLK_UNDERSCORE},
	{"K_BACKQUOTE", SDLK_BACKQUOTE},
	{"K_a", SDLK_a},
	{"K_b", SDLK_b},
	{"K_c", SDLK_c},
	{"K_d", SDLK_d},
	{"K_e", SDLK_e},
	{"K_f", SDLK_f},
	{"K_g", SDLK_g},
	{"K_h", SDLK_h},
	{"K_i", SDLK_i},
	{"K_j", SDLK_j},
	{"K_k", SDLK_k},
	{"K_l", SDLK_l},
	{"K_m", SDLK_m},
	{"K_n", SDLK_n},
	{"K_o", SDLK_o},
	{"K_p", SDLK_p},
	{"K_q", SDLK_q},
	{"K_r", SDLK_r},
	{"K_s", SDLK_s},
	{"K_t", SDLK_t},
	{"K_u", SDLK_u},
	{"K_v", SDLK_v},
	{"K_w", SDLK_w},
	{"K_x", SDLK_x},
	{"K_y", SDLK_y},
	{"K_z", SDLK_z},
	{"K_DELETE", SDLK_DELETE},
	{"K_WORLD_0", SDLK_WORLD_0},
	{"K_WORLD_1", SDLK_WORLD_1},
	{"K_WORLD_2", SDLK_WORLD_2},
	{"K_WORLD_3", SDLK_WORLD_3},
	{"K_WORLD_4", SDLK_WORLD_4},
	{"K_WORLD_5", SDLK_WORLD_5},
	{"K_WORLD_6", SDLK_WORLD_6},
	{"K_WORLD_7", SDLK_WORLD_7},
	{"K_WORLD_8", SDLK_WORLD_8},
	{"K_WORLD_9", SDLK_WORLD_9},
	{"K_WORLD_10", SDLK_WORLD_10},
	{"K_WORLD_11", SDLK_WORLD_11},
	{"K_WORLD_12", SDLK_WORLD_12},
	{"K_WORLD_13", SDLK_WORLD_13},
	{"K_WORLD_14", SDLK_WORLD_14},
	{"K_WORLD_15", SDLK_WORLD_15},
	{"K_WORLD_16", SDLK_WORLD_16},
	{"K_WORLD_17", SDLK_WORLD_17},
	{"K_WORLD_18", SDLK_WORLD_18},
	{"K_WORLD_19", SDLK_WORLD_19},
	{"K_WORLD_20", SDLK_WORLD_20},
	{"K_WORLD_21", SDLK_WORLD_21},
	{"K_WORLD_22", SDLK_WORLD_22},
	{"K_WORLD_23", SDLK_WORLD_23},
	{"K_WORLD_24", SDLK_WORLD_24},
	{"K_WORLD_25", SDLK_WORLD_25},
	{"K_WORLD_26", SDLK_WORLD_26},
	{"K_WORLD_27", SDLK_WORLD_27},
	{"K_WORLD_28", SDLK_WORLD_28},
	{"K_WORLD_29", SDLK_WORLD_29},
	{"K_WORLD_30", SDLK_WORLD_30},
	{"K_WORLD_31", SDLK_WORLD_31},
	{"K_WORLD_32", SDLK_WORLD_32},
	{"K_WORLD_33", SDLK_WORLD_33},
	{"K_WORLD_34", SDLK_WORLD_34},
	{"K_WORLD_35", SDLK_WORLD_35},
	{"K_WORLD_36", SDLK_WORLD_36},
	{"K_WORLD_37", SDLK_WORLD_37},
	{"K_WORLD_38", SDLK_WORLD_38},
	{"K_WORLD_39", SDLK_WORLD_39},
	{"K_WORLD_40", SDLK_WORLD_40},
	{"K_WORLD_41", SDLK_WORLD_41},
	{"K_WORLD_42", SDLK_WORLD_42},
	{"K_WORLD_43", SDLK_WORLD_43},
	{"K_WORLD_44", SDLK_WORLD_44},
	{"K_WORLD_45", SDLK_WORLD_45},
	{"K_WORLD_46", SDLK_WORLD_46},
	{"K_WORLD_47", SDLK_WORLD_47},
	{"K_WORLD_48", SDLK_WORLD_48},
	{"K_WORLD_49", SDLK_WORLD_49},
	{"K_WORLD_50", SDLK_WORLD_50},
	{"K_WORLD_51", SDLK_WORLD_51},
	{"K_WORLD_52", SDLK_WORLD_52},
	{"K_WORLD_53", SDLK_WORLD_53},
	{"K_WORLD_54", SDLK_WORLD_54},
	{"K_WORLD_55", SDLK_WORLD_55},
	{"K_WORLD_56", SDLK_WORLD_56},
	{"K_WORLD_57", SDLK_WORLD_57},
	{"K_WORLD_58", SDLK_WORLD_58},
	{"K_WORLD_59", SDLK_WORLD_59},
	{"K_WORLD_60", SDLK_WORLD_60},
	{"K_WORLD_61", SDLK_WORLD_61},
	{"K_WORLD_62", SDLK_WORLD_62},
	{"K_WORLD_63", SDLK_WORLD_63},
	{"K_WORLD_64", SDLK_WORLD_64},
	{"K_WORLD_65", SDLK_WORLD_65},
	{"K_WORLD_66", SDLK_WORLD_66},
	{"K_WORLD_67", SDLK_WORLD_67},
	{"K_WORLD_68", SDLK_WORLD_68},
	{"K_WORLD_69", SDLK_WORLD_69},
	{"K_WORLD_70", SDLK_WORLD_70},
	{"K_WORLD_71", SDLK_WORLD_71},
	{"K_WORLD_72", SDLK_WORLD_72},
	{"K_WORLD_73", SDLK_WORLD_73},
	{"K_WORLD_74", SDLK_WORLD_74},
	{"K_WORLD_75", SDLK_WORLD_75},
	{"K_WORLD_76", SDLK_WORLD_76},
	{"K_WORLD_77", SDLK_WORLD_77},
	{"K_WORLD_78", SDLK_WORLD_78},
	{"K_WORLD_79", SDLK_WORLD_79},
	{"K_WORLD_80", SDLK_WORLD_80},
	{"K_WORLD_81", SDLK_WORLD_81},
	{"K_WORLD_82", SDLK_WORLD_82},
	{"K_WORLD_83", SDLK_WORLD_83},
	{"K_WORLD_84", SDLK_WORLD_84},
	{"K_WORLD_85", SDLK_WORLD_85},
	{"K_WORLD_86", SDLK_WORLD_86},
	{"K_WORLD_87", SDLK_WORLD_87},
	{"K_WORLD_88", SDLK_WORLD_88},
	{"K_WORLD_89", SDLK_WORLD_89},
	{"K_WORLD_90", SDLK_WORLD_90},
	{"K_WORLD_91", SDLK_WORLD_91},
	{"K_WORLD_92", SDLK_WORLD_92},
	{"K_WORLD_93", SDLK_WORLD_93},
	{"K_WORLD_94", SDLK_WORLD_94},
	{"K_WORLD_95", SDLK_WORLD_95},
	{"K_KP0", SDLK_KP0},
	{"K_KP1", SDLK_KP1},
	{"K_KP2", SDLK_KP2},
	{"K_KP3", SDLK_KP3},
	{"K_KP4", SDLK_KP4},
	{"K_KP5", SDLK_KP5},
	{"K_KP6", SDLK_KP6},
	{"K_KP7", SDLK_KP7},
	{"K_KP8", SDLK_KP8},
	{"K_KP9", SDLK_KP9},
	{"K_KP_PERIOD", SDLK_KP_PERIOD},
	{"K_KP_DIVIDE", SDLK_KP_DIVIDE},
	{"K_KP_MULTIPLY", SDLK_KP_MULTIPLY},
	{"K_KP_MINUS", SDLK_KP_MINUS},
	{"K_KP_PLUS", SDLK_KP_PLUS},
	{"K_KP_ENTER", SDLK_KP_ENTER},
	{"K_KP_EQUALS", SDLK_KP_EQUALS},
	{"K_UP", SDLK_UP},
	{"K_DOWN", SDLK_DOWN},
	{"K_RIGHT", SDLK_RIGHT},
	{"K_LEFT", SDLK_LEFT},
	{"K_INSERT", SDLK_INSERT},
	{"K_HOME", SDLK_HOME},
	{"K_END", SDLK_END},
	{"K_PAGEUP", SDLK_PAGEUP},
	{"K_PAGEDOWN", SDLK_PAGEDOWN},
	{"K_F1", SDLK_F1},
	{"K_F2", SDLK_F2},
	{"K_F3", SDLK_F3},
	{"K_F4", SDLK_F4},
	{"K_F5", SDLK_F5},
	{"K_F6", SDLK_F6},
	{"K_F7", SDLK_F7},
	{"K_F8", SDLK_F8},
	{"K_F9", SDLK_F9},
	{"K_F10", SDLK_F10},
	{"K_F11", SDLK_F11},
	{"K_F12", SDLK_F12},
	{"K_F13", SDLK_F13},
	{"K_F14", SDLK_F14},
	{"K_F15", SDLK_F15},
	{"K_NUMLOCK", SDLK_NUMLOCK},
	{"K_CAPSLOCK", SDLK_CAPSLOCK},
	{"K_SCROLLOCK", SDLK_SCROLLOCK},
	{"K_RSHIFT", SDLK_RSHIFT},
	{"K_LSHIFT", SDLK_LSHIFT},
	{"K_RCTRL", SDLK_RCTRL},
	{"K_LCTRL", SDLK_LCTRL},
	{"K_RALT", SDLK_RALT},
	{"K_LALT", SDLK_LALT},
	{"K_RMETA", SDLK_RMETA},
	{"K_LMETA", SDLK_LMETA},
	{"K_LSUPER", SDLK_LSUPER},
	{"K_RSUPER", SDLK_RSUPER},
	{"K_MODE", SDLK_MODE},
	{"K_COMPOSE", SDLK_COMPOSE},
	{"K_HELP", SDLK_HELP},
	{"K_PRINT", SDLK_PRINT},
	{"K_SYSREQ", SDLK_SYSREQ},
	{"K_BREAK", SDLK_BREAK},
	{"K_MENU", SDLK_MENU},
	{"K_POWER", SDLK_POWER},
	{"K_EURO", SDLK_EURO},

	// End of SDL stuff.
	{"M_BUTTON1", KM_BUTTON1},
	{"M_BUTTON2", KM_BUTTON2},
	{"M_BUTTON3", KM_BUTTON3},
	{"M_WHEEL_UP", KM_WHEEL_UP},
	{"M_WHEEL_DOWN", KM_WHEEL_DOWN},
	{NULL, 0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

qboolean
CheckForCommand (void)
{
	char        command[128];
	char       *cmd, *s;
	int         i;

	s = key_lines[edit_line] + 1;

	for (i = 0; i < 127; i++)
		if (s[i] <= ' ')
			break;
		else
			command[i] = s[i];
	command[i] = 0;

	cmd = Cmd_CompleteCommand (command);
	if (!cmd || Q_strcmp (cmd, command))
		cmd = Cvar_CompleteVariable (command);
	if (!cmd || Q_strcmp (cmd, command))
		return false;					// just a chat message
	return true;
}

void
CompleteCommand (void)
{
	char       *cmd, *s;

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;

	cmd = Cmd_CompleteCommand (s);
	if (!cmd)
		cmd = Cvar_CompleteVariable (s);
	if (cmd) {
		key_lines[edit_line][1] = '/';
		Q_strcpy (key_lines[edit_line] + 2, cmd);
		key_linepos = Q_strlen (cmd) + 2;
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}
}

/*
====================
Key_Game

Game key handling.
====================
*/
qboolean
Key_Game (knum_t key, short unicode)
{
	char       *kb;
	char        cmd[1024];

	kb = keybindings[game_target][key];
	if (!kb && game_target > KGT_DEFAULT)
		kb = keybindings[KGT_DEFAULT][key];

	/* 
	   Con_Printf("kb %p, game_target %d, key_dest %d, key %d\n", kb,
	   game_target, key_dest, key); */
	if (!kb)
		return false;

	if (!keydown[key]) {
		if (kb[0] == '+') {
			snprintf (cmd, sizeof (cmd), "-%s %d\n", kb + 1, key);
			Cbuf_AddText (cmd);
		}
	} else if (keydown[key] == 1) {
		if (kb[0] == '+') {
			snprintf (cmd, sizeof (cmd), "%s %d\n", kb, key);
			Cbuf_AddText (cmd);
		} else {
			snprintf (cmd, sizeof (cmd), "%s\n", kb);
			Cbuf_AddText (cmd);
		}
	}
	return true;
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void
Key_Console (knum_t key, short unicode)
{
	if (keydown[key] != 1)
		return;

	if (Key_Game (key, unicode))
		return;

	if (unicode == '^M' || key == SDLK_RETURN) {				// backslash text are commands, else
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText (key_lines[edit_line] + 2);	// skip the >
		else if (CheckForCommand ())
			Cbuf_AddText (key_lines[edit_line] + 1);	// valid command
		else {							// convert to a chat message
			if (cls.state >= ca_connected)
				Cbuf_AddText ("say ");
			Cbuf_AddText (key_lines[edit_line] + 1);	// skip the >
		}

		Cbuf_AddText ("\n");
		Con_Printf ("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen ();		// force an update, because the command
		// may take some time
		return;
	}

	if (unicode == '\t') {				// command completion
		CompleteCommand ();
		return;
	}

	if (unicode == '') {
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == SDLK_UP) {
		do {
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1) & 31;
		Q_strcpy (key_lines[edit_line], key_lines[history_line]);
		key_linepos = Q_strlen (key_lines[edit_line]);
		return;
	}

	if (key == SDLK_DOWN) {
		if (history_line == edit_line)
			return;
		do {
			history_line = (history_line + 1) & 31;
		}
		while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line) {
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		} else {
			Q_strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = Q_strlen (key_lines[edit_line]);
		}
		return;
	}

	if (key == SDLK_PAGEUP || key == KM_WHEEL_UP) {
		con->display -= 2;
		return;
	}

	if (key == SDLK_PAGEDOWN || key == KM_WHEEL_DOWN) {
		con->display += 2;
		if (con->display > con->current)
			con->display = con->current;
		return;
	}

	if (key == SDLK_HOME) {
		con->display = con->current - con_totallines + 10;
		return;
	}

	if (key == SDLK_END) {
		con->display = con->current;
		return;
	}

	if (unicode < 32 || unicode > 127)
		return;							// non printable

	if (key_linepos < MAXCMDLINE - 1) {
		key_lines[edit_line][key_linepos] = unicode;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}

}

//============================================================================

qboolean    chat_team;
char        chat_buffer[MAXCMDLINE];
int         chat_bufferlen = 0;

void
Key_Message (knum_t key, short unicode)
{
	if (keydown[key] != 1)
		return;

	if (unicode == '\n') {
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText (chat_buffer);
		Cbuf_AddText ("\"\n");

		key_dest = key_game;
		game_target = KGT_DEFAULT;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (unicode == '\x1b' || key == SDLK_ESCAPE) {
		key_dest = key_game;
		game_target = KGT_DEFAULT;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (unicode == '\b') {
		if (chat_bufferlen) {
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof (chat_buffer) - 1)
		return;							// all full

	if (key < 32 || key > 127)
		return;							// non printable

	chat_buffer[chat_bufferlen++] = unicode;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKgtnum

Returns a kgt number to be used to index kgtbindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int
Key_StringToKgtnum (char *str)
{
	kgtname_t  *kn;

	if (!str || !str[0])
		return -1;

	for (kn = kgtnames; kn->name; kn++) {
		if (!Q_strcasecmp (str, kn->name))
			return kn->kgtnum;
	}
	return -1;
}

/*
===================
Key_KgtnumToString

Returns a string (a K_* name) for the
given kgtnum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char       *
Key_KgtnumToString (int kgtnum)
{
	kgtname_t  *kn;

	if (kgtnum == -1)
		return "<KGT NOT FOUND>";

	for (kn = kgtnames; kn->name; kn++)
		if (kgtnum == kn->kgtnum)
			return kn->name;

	return "<UNKNOWN KGTNUM>";
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int
Key_StringToKeynum (char *str)
{
	keyname_t  *kn;

	if (!str || !str[0])
		return -1;

	for (kn = keynames; kn->name; kn++) {
		if (!Q_strcasecmp (str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char       *
Key_KeynumToString (int keynum)
{
	keyname_t  *kn;

	if (keynum == -1)
		return "<KEY NOT FOUND>";

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
===================
Key_SetBinding
===================
*/
void
Key_SetBinding (kgt_t target, knum_t keynum, char *binding)
{
	char       *new;
	int         l;

	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[target][keynum]) {
		Z_Free (keybindings[target][keynum]);
		keybindings[target][keynum] = NULL;
	}
// allocate memory for new binding
	l = Q_strlen (binding);
	new = Z_Malloc (l + 1);
	Q_strcpy (new, binding);
	new[l] = 0;
	keybindings[target][keynum] = new;
}

/*
===================
Key_Unbind_f
===================
*/
void
Key_Unbind_f (void)
{
	int         b, t;

	if (Cmd_Argc () != 3) {
		Con_Printf ("unbind <kgt> <key> : remove commands from a key\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	Key_SetBinding (t, b, "");
}

void
Key_Unbindall_f (void)
{
	int         i, j;

	for (j = 0; j < KGT_MAX; j++)
		for (i = 0; i < KSYM_LAST; i++)
			if (keybindings[j][i])
				Key_SetBinding (j, i, "");
}


/*
===================
Key_Bind_f
===================
*/
void
Key_Bind_f (void)
{
	int         i, c, t, b;
	char        cmd[1024];

	c = Cmd_Argc ();

	if (c != 3 && c != 4) {
		Con_Printf ("bind <kgt> <key> [command] : attach a command to a key\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	if (c == 3) {
		if (keybindings[t][b])
			Con_Printf ("\"%s\"[\"%s\"] = \"%s\"\n", Cmd_Argv (2), Cmd_Argv (1),
						keybindings[t][b]);
		else
			Con_Printf ("\"%s\"[\"%s\"] is not bound\n", Cmd_Argv (2),
						Cmd_Argv (1));
		return;
	}
// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	for (i = 3; i < c; i++) {
		Q_strcat (cmd, Cmd_Argv (i));
		if (i != (c - 1))
			Q_strcat (cmd, " ");
	}

	Key_SetBinding (t, b, cmd);
}

/*
===================
Key_GameTarget_f
===================
*/
void
Key_GameTarget_f (void)
{
	int         c, t;

	c = Cmd_Argc ();

	if (c != 2) {
		Con_Printf ("kgt <kgt> : set to a specific key game target\n");
		return;
	}

	t = Key_StringToKgtnum (Cmd_Argv (1));
	if (t == -1) {
		Con_Printf ("\"%s\" isn't a valid kgt\n", Cmd_Argv (1));
		return;
	}

	game_target = t;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void
Key_WriteBindings (FILE * f)
{
	int         i, j;

	for (j = 0; j < KGT_MAX; j++)
		for (i = 0; i < KSYM_LAST; i++)
			if (keybindings[j][i])
				fprintf (f, "bind %s %s \"%s\"\n", Key_KgtnumToString (j),
						 Key_KeynumToString (i), keybindings[j][i]);
}


/*
===================
Key_Init
===================
*/
void
Key_Init (void)
{
	int         i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

//
// register our functions
//
	Cmd_AddCommand ("bind", Key_Bind_f);
	Cmd_AddCommand ("unbind", Key_Unbind_f);
	Cmd_AddCommand ("unbindall", Key_Unbindall_f);
	Cmd_AddCommand ("kgt", Key_GameTarget_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void
Key_Event (knum_t key, short unicode, qboolean down)
{
//  Con_Printf ("%d %d %d : %d\n", game_target, key_dest, key, down); //@@@

	if (down)
		keydown[key]++;
	else
		keydown[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0) {
		return;							// just catching keys for Con_NotifyBox
	}
//
// handle escape specialy, so the user can never unbind it
//
	if (unicode == '\x1b' || key == SDLK_ESCAPE) {
		if (!down || (keydown[key] > 1))
			return;
		switch (key_dest) {
			case key_message:
				Key_Message (key, unicode);
				break;
			case key_menu:
				M_Keydown (key);
				break;
			case key_game:
			case key_console:
				M_ToggleMenu_f ();
				break;
			default:
				Sys_Error ("Bad key_dest");
		}
		return;
	}
//
// during demo playback, most keys bring up the main menu
//
	if (cls.demoplayback && down && key_dest == key_game) {
		M_ToggleMenu_f ();
		return;
	}

	if (!down && Key_Game (key, unicode))
		return;

//
// if not a consolekey, send to the interpreter no matter what mode is
//
	switch (key_dest) {
		case key_message:
			Key_Message (key, unicode);
			break;
		case key_menu:
			M_Keydown (key);
			break;

		case key_game:
			Key_Game (key, unicode);
			break;
		case key_console:
			Key_Console (key, unicode);
			break;
		default:
			Sys_Error ("Bad key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void
Key_ClearStates (void)
{
	int         i;

	for (i = 0; i < 256; i++) {
		keydown[i] = false;
	}
}
