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
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "screen.h"
#include "strlib.h"
#include "sys.h"


/*

key up events are sent even if in console mode

*/

char        key_lines[32][MAX_INPUTLINE];
int         key_linepos;
int         shift_down = false;
int			ctrl_down = false;
int         key_lastpress;

int         edit_line = 0;
int         history_line = 0;

cvar_t	   *cl_chatmode;

keydest_t   key_dest;

int         key_count;					// incremented every key event

char       *keybindings[256];
qboolean    consolekeys[256];			// if true, can't be rebound while in
										// console
qboolean    menubound[256];				// if true, can't be rebound while in
										// menu
int         keyshift[256];				// key to map to if shift held down in
										// console
int         key_repeats[256];			// if > 1, it is autorepeating
qboolean    keydown[256];

typedef struct {
	char       *name;
	int         keynum;
} keyname_t;

keyname_t   keynames[] = {
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},
	{"MOUSE9", K_MOUSE9},
	{"MOUSE10", K_MOUSE10},
	{"MOUSE11", K_MOUSE11},
	{"MOUSE12", K_MOUSE12},
	{"MOUSE13", K_MOUSE13},
	{"MOUSE14", K_MOUSE14},
	{"MOUSE15", K_MOUSE15},
	{"MOUSE16", K_MOUSE16},

	{"SEMICOLON", ';'},					// because a raw semicolon seperates
										// commands
	{"NUMLOCK", K_NUMLOCK},
	{"CAPSLOCK", K_CAPSLOCK},
	{"SCROLLOCK", K_SCROLLOCK},

	{"KP_0", K_KP_0},
	{"KP_1", K_KP_1},
	{"KP_2", K_KP_2},
	{"KP_3", K_KP_3},
	{"KP_4", K_KP_4},
	{"KP_5", K_KP_5},
	{"KP_6", K_KP_6},
	{"KP_7", K_KP_7},
	{"KP_8", K_KP_8},
	{"KP_9", K_KP_9},
	{"KP_PERIOD", K_KP_PERIOD},
	{"KP_DIVIDE", K_KP_DIVIDE},
	{"KP_MULTIPLY", K_KP_MULTIPLY},
	{"KP_MINUS", K_KP_MINUS},
	{"KP_PLUS", K_KP_PLUS},
	{"KP_ENTER", K_KP_ENTER},
	{"KP_EQUALS", K_KP_EQUALS},

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
	if (!cmd || strcmp (cmd, command))
		cmd = Cvar_TabComplete (command);
	if (!cmd || strcmp (cmd, command))
		return false;					// just a chat message
	return true;
}

static void
Key_ClearEditLine (int edit_line)
{
	memset (key_lines[edit_line], '\0', MAX_INPUTLINE);
	key_lines[edit_line][0] = ']';
	key_linepos = 1;
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void
Key_Console (int key)
{
	if (key == K_ENTER)
	{
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText (key_lines[edit_line] + 2);	// skip the >
		else if (cl_chatmode->value && cls.state >= ca_connected)
		{
			if (CheckForCommand ())
				Cbuf_AddText (key_lines[edit_line] + 1);
			else {
				Cbuf_AddText ("say ");
				Cbuf_AddText (key_lines[edit_line] + 1);
			}
		}
		else
			Cbuf_AddText (key_lines[edit_line] + 1);

		Cbuf_AddText ("\n");
		Con_Printf ("%s\n", key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		Key_ClearEditLine (edit_line);
		if (cls.state == ca_disconnected)
			// force an update, because the command may take some time
			SCR_UpdateScreen ();
		return;
	}

	// Command Line Completion
	if (key == K_TAB)
		Con_CompleteCommandLine();

	if (key == K_LEFTARROW)
	{
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		if (key_lines[edit_line][key_linepos])
			key_linepos++;
	}

	if (key == K_UPARROW)
	{
		do {
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line + 1) & 31;
		strcpy (key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen (key_lines[edit_line]);
		return;
	}

	if (key == K_DOWNARROW)
	{
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
			strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen (key_lines[edit_line]);
		}
		return;
	}

	if (key == K_BACKSPACE)
	{
		if (key_linepos > 1)
		{
			strcpy (key_lines[edit_line] + key_linepos - 1,
					key_lines[edit_line] + key_linepos);
			key_linepos--;
		}
		return;
	}

	if (key == K_DEL)
	{
		if ((unsigned)key_linepos < strlen (key_lines[edit_line]))
			strcpy (key_lines[edit_line] + key_linepos,
					key_lines[edit_line] + key_linepos + 1);
	}

	if (key == K_PGUP || key == K_MWHEELUP)
	{
		con->display -= 2;
		return;
	}

	if (key == K_PGDN || key == K_MWHEELDOWN)
	{
		con->display += 2;
		if (con->display > con->current)
			con->display = con->current;
		return;
	}

	if (key == K_HOME)
	{
		if (key_linepos == 1 && key_lines[edit_line][1] == '\0')
			con->display = con->current - con_totallines + 10;
		else
			key_linepos = 1;
		return;
	}

	if (key == K_END)
	{
		if (key_linepos == 1 && key_lines[edit_line][1] == '\0')
			con->display = con->current;
		else
			key_linepos = strlen (key_lines[edit_line]);
		return;
	}

	if (ctrl_down)
	{
		if (key == 'u' || key == 'U')
		{
			Key_ClearEditLine (edit_line);
			return;
		}

		if (key == 'a' || key == 'A')
		{
			key_linepos = 1;
			return;
		}

		if (key == 'e' || key == 'E')
		{
			key_linepos = strlen (key_lines[edit_line]);
			return;
		}
	}

	// There was some clipboard stuff here, but it was not portable

	if (key < 32 || key > 127)
		return;							// non printable

	if (key_linepos < MAX_INPUTLINE - 1)
	{
		int			i;

		i = strlen (key_lines[edit_line]) - 1;
		i = min (i, MAX_INPUTLINE - 2);

		// insert a character here
		for (; i >= key_linepos; i--)
			key_lines[edit_line][i + 1] = key_lines[edit_line][i];

		key_lines[edit_line][key_linepos++] = key;
	}

}

//============================================================================

qboolean    chat_team;
char        chat_buffer[MAX_INPUTLINE];
unsigned	chat_bufferlen = 0;

void
Key_Message (int key)
{

	if (key == K_ENTER) {
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText (chat_buffer);
		Cbuf_AddText ("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE) {
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;							// non printable

	if (key == K_BACKSPACE) {
		if (chat_bufferlen) {
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof (chat_buffer) - 1)
		return;							// all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


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
	if (!str[1])
		return str[0];

	for (kn = keynames; kn->name; kn++) {
		if (!strcasecmp (str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char       *
Key_KeynumToString (int keynum)
{
	keyname_t  *kn;
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127) {	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

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
Key_SetBinding (int keynum, char *binding)
{
	char       *new;
	int         l;

	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[keynum]) {
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
// allocate memory for new binding
	l = strlen (binding);
	new = Z_Malloc (l + 1);
	strcpy (new, binding);
	new[l] = 0;
	keybindings[keynum] = new;
}

/*
===================
Key_Unbind_f
===================
*/
void
Key_Unbind_f (void)
{
	int         b;

	if (Cmd_Argc () != 2) {
		Con_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (1));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	Key_SetBinding (b, "");
}

void
Key_Unbindall_f (void)
{
	int         i;

	for (i = 0; i < 256; i++)
		if (keybindings[i])
			Key_SetBinding (i, "");
}


/*
===================
Key_Bind_f
===================
*/
void
Key_Bind_f (void)
{
	int         i, c, b;
	char        cmd[1024];

	c = Cmd_Argc ();

	if (c != 2 && c != 3) {
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv (1));
	if (b == -1) {
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	if (c == 2) {
		if (keybindings[b])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv (1), keybindings[b]);
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv (1));
		return;
	}
// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	for (i = 2; i < c; i++) {
		strcat (cmd, Cmd_Argv (i));
		if (i != (c - 1))
			strcat (cmd, " ");
	}

	Key_SetBinding (b, cmd);
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
	int         i;

	for (i = 0; i < 256; i++)
		if (keybindings[i])
			fprintf (f, "bind %s \"%s\"\n", Key_KeynumToString (i),
					 keybindings[i]);
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
// init ascii characters in console mode
//
	for (i = 32; i < 128; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i = 0; i < 256; i++)
		keyshift[i] = i;
	for (i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

//
// register our functions
//
	Cmd_AddCommand ("bind", Key_Bind_f);
	Cmd_AddCommand ("unbind", Key_Unbind_f);
	Cmd_AddCommand ("unbindall", Key_Unbindall_f);
}

void
Key_Init_Cvars (void)
{
	cl_chatmode = Cvar_Get ("cl_chatmode", "0", CVAR_ARCHIVE, NULL);
}


/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void
Key_Event (int key, qboolean down)
{
	char       *kb;
	char        cmd[1024];

//  Con_Printf ("%i : %i\n", key, down); //@@@

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0) {
		return;							// just catching keys for Con_NotifyBox
	}

	// update auto-repeat status
	if (down) {
		key_repeats[key]++;
		if (key != K_BACKSPACE && key_repeats[key] > 1) {
			return;						// ignore most autorepeats
		}
	}

	if (key == K_SHIFT)
		shift_down = down;

	if (key == K_CTRL)
		ctrl_down = down;

	//
	// handle escape specially, so the user can never unbind it
	//
	if (key == K_ESCAPE) {
		if (!down)
			return;
		switch (key_dest) {
			case key_message:
				Key_Message (key);
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
	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	//
	if (!down) {
		kb = keybindings[key];
		if (kb && kb[0] == '+') {
			snprintf (cmd, sizeof(cmd), "-%s %i\n", kb + 1, key);
			Cbuf_AddText (cmd);
		}
		if (keyshift[key] != key) {
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+') {
				snprintf (cmd, sizeof(cmd), "-%s %i\n", kb + 1, key);
				Cbuf_AddText (cmd);
			}
		}
		return;
	}

	//
	// during demo playback, most keys bring up the main menu
	//
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game) {
		M_ToggleMenu_f ();
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if ((key_dest == key_menu && menubound[key])
		|| (key_dest == key_console && !consolekeys[key])
		|| (key_dest == key_game
			&& (cls.state == ca_active || !consolekeys[key]))) {
		kb = keybindings[key];
		if (kb) {
			if (kb[0] == '+') {			// button commands add keynum as a parm
				snprintf (cmd, sizeof(cmd), "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
			} else {
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	if (!down)
		return;							// other systems only care about key
										// down events

	if (shift_down)
		key = keyshift[key];

	switch (key_dest) {
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
			M_Keydown (key);
			break;

		case key_game:
		case key_console:
			Key_Console (key);
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
		key_repeats[i] = false;
	}
}
