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

//
// these are the key numbers that should be passed to Key_Event
//

#ifndef _KEYS_H
#define _KEYS_H

#include <SDL/SDL_keysym.h>

typedef enum {
	KM_BUTTON1 = SDLK_LAST + 1,
	KM_BUTTON2,
	KM_BUTTON3,
	KM_WHEEL_UP,
	KM_WHEEL_DOWN,

	KSYM_LAST,
} knum_t;

typedef enum {
	KGT_CONSOLE,
	KGT_DEFAULT,
	KGT_1,
	KGT_2,

	KGT_MAX,
} kgt_t;



typedef enum { key_game, key_console, key_message, key_menu } keydest_t;

extern keydest_t key_dest;
extern kgt_t game_target;

extern int	keydown[KSYM_LAST];		// if > 1, it is autorepeating
extern char *keybindings[KGT_MAX][KSYM_LAST];
extern char chat_buffer[];
extern int  chat_bufferlen;
extern qboolean chat_team;

void        Key_Event (knum_t key, short unicode, qboolean down);
void        Key_Init (void);
void        Key_WriteBindings (FILE * f);
void        Key_SetBinding (kgt_t target, knum_t keynum, char *binding);
void        Key_ClearStates (void);

#endif
