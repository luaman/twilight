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

#ifndef __KEYS_H
#define __KEYS_H

//
// these are the key numbers that should be passed to Key_Event
//
extern enum {
	K_TAB			= 9,
	K_ENTER			= 13,
	K_ESCAPE		= 27,
	K_SPACE			= 32,

	// normal keys should be passed as lowercased ascii

	K_BACKSPACE		= 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_PAUSE,

	K_NUMLOCK,
	K_CAPSLOCK,
	K_SCROLLOCK,

	K_KP_0,
	K_KP_1,
	K_KP_2,
	K_KP_3,
	K_KP_4,
	K_KP_5,
	K_KP_6,
	K_KP_7,
	K_KP_8,
	K_KP_9,
	K_KP_PERIOD,
	K_KP_DIVIDE,
	K_KP_MULTIPLY,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_ENTER,
	K_KP_EQUALS,

	// mouse buttons generate virtual keys

	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10,
	K_MOUSE11,
	K_MOUSE12,
	K_MOUSE13,
	K_MOUSE14,
	K_MOUSE15,
	K_MOUSE16,

	K_MWHEELUP		= K_MOUSE4,
	K_MWHEELDOWN	= K_MOUSE5,
} keynum_t;

typedef enum { key_game, key_console, key_message, key_menu } keydest_t;

extern keydest_t key_dest;
extern char *keybindings[8][256];
extern int	key_count;				// incremented every key event
extern int	key_lastpress;

extern char chat_buffer[];
extern unsigned chat_bufferlen;
extern qboolean chat_team;

void Key_Event (int key, qboolean down);
void Key_Init_Cvars (void);
void Key_Init (void);
void Key_WriteBindings (FILE * f);
void Key_ClearEditLine (int edit_line);

#endif // __KEYS_H

