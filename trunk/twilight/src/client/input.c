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
#include "cclient.h"
#include "cvar.h"
#include "keys.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "video.h"

#include <stdlib.h>

static cvar_t *i_keypadmode;
static cvar_t *_windowed_mouse;
static qboolean keypadmode = false;

float mouse_x, mouse_y;

static enum { M_NONE, M_FREE, M_GRAB, M_FULL } mouse = M_NONE;

static void IN_KeypadMode (cvar_t *cvar);
static void IN_WindowedMouse (cvar_t *cvar);

void
IN_Init_Cvars (void)
{
	i_keypadmode = Cvar_Get ("i_keypadmode", "0", CVAR_ARCHIVE, &IN_KeypadMode);
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "1", CVAR_ARCHIVE,
			&IN_WindowedMouse);
}

void
IN_Init (void)
{
	IN_WindowedMouse (_windowed_mouse);
	mouse_x = mouse_y = 0.0f;
	SDL_EnableUNICODE (1);
}

void
IN_Shutdown (void)
{
	if (mouse > M_NONE) {
		SDL_ShowCursor (1);
		SDL_WM_GrabInput (SDL_GRAB_OFF);
	}

	SDL_EnableUNICODE (0);
}

void
IN_SendKeyEvents (void)
{
	SDL_Event	event;
	int			sym, state, but;
	SDLMod		modstate;
	char		ascii;
	Uint16		unicode;

	while (SDL_PollEvent (&event) > 0) {
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				modstate = SDL_GetModState ();
				
				unicode = event.key.keysym.unicode;
				if (unicode >= ' ' && unicode <= '~')
					ascii = unicode;
				else
					ascii = 0;

				switch (sym) {
					case SDLK_DELETE: sym = K_DEL; break;
					case SDLK_BACKSPACE: sym = K_BACKSPACE; break;
					case SDLK_F1: sym = K_F1; break;
					case SDLK_F2: sym = K_F2; break;
					case SDLK_F3: sym = K_F3; break;
					case SDLK_F4: sym = K_F4; break;
					case SDLK_F5: sym = K_F5; break;
					case SDLK_F6: sym = K_F6; break;
					case SDLK_F7: sym = K_F7; break;
					case SDLK_F8: sym = K_F8; break;
					case SDLK_F9: sym = K_F9; break;
					case SDLK_F10: sym = K_F10; break;
					case SDLK_F11: sym = K_F11; break;
					case SDLK_F12: sym = K_F12; break;
					case SDLK_BREAK:
					case SDLK_PAUSE:
						sym = K_PAUSE;
						break;
					case SDLK_UP: sym = K_UPARROW; break;
					case SDLK_DOWN: sym = K_DOWNARROW; break;
					case SDLK_RIGHT: sym = K_RIGHTARROW; break;
					case SDLK_LEFT: sym = K_LEFTARROW; break;
					case SDLK_INSERT: sym = K_INS; break;
					case SDLK_HOME: sym = K_HOME; break;
					case SDLK_END: sym = K_END; break;
					case SDLK_PAGEUP: sym = K_PGUP; break;
					case SDLK_PAGEDOWN: sym = K_PGDN; break;
					case SDLK_RSHIFT:
					case SDLK_LSHIFT:
						sym = K_SHIFT;
						break;
					case SDLK_RCTRL:
					case SDLK_LCTRL:
						sym = K_CTRL;
						break;
					case SDLK_RALT:
					case SDLK_LALT:
						sym = K_ALT;
						break;
					case SDLK_NUMLOCK: sym = K_NUMLOCK; break;
					case SDLK_CAPSLOCK: sym = K_CAPSLOCK; break;
					case SDLK_SCROLLOCK: sym = K_SCROLLOCK; break;
					case SDLK_KP0: case SDLK_KP1: case SDLK_KP2: case SDLK_KP3:
					case SDLK_KP4: case SDLK_KP5: case SDLK_KP6: case SDLK_KP7:
					case SDLK_KP8: case SDLK_KP9: case SDLK_KP_PERIOD:
					case SDLK_KP_DIVIDE: case SDLK_KP_MULTIPLY:
					case SDLK_KP_MINUS: case SDLK_KP_PLUS:
					case SDLK_KP_ENTER: case SDLK_KP_EQUALS:
					if (keypadmode) {
						switch (sym) {
							case SDLK_KP0: sym = K_KP_0; break;
							case SDLK_KP1: sym = K_KP_1; break;
							case SDLK_KP2: sym = K_KP_2; break;
							case SDLK_KP3: sym = K_KP_3; break;
							case SDLK_KP4: sym = K_KP_4; break;
							case SDLK_KP5: sym = K_KP_5; break;
							case SDLK_KP6: sym = K_KP_6; break;
							case SDLK_KP7: sym = K_KP_7; break;
							case SDLK_KP8: sym = K_KP_8; break;
							case SDLK_KP9: sym = K_KP_9; break;
							case SDLK_KP_PERIOD: sym = K_KP_PERIOD; break;
							case SDLK_KP_DIVIDE: sym = K_KP_DIVIDE; break;
							case SDLK_KP_MULTIPLY:sym = K_KP_MULTIPLY;break;                                case SDLK_KP_MINUS: sym = K_KP_MINUS; break;
							case SDLK_KP_PLUS: sym = K_KP_PLUS; break;
							case SDLK_KP_ENTER: sym = K_KP_ENTER; break;
							case SDLK_KP_EQUALS: sym = K_KP_EQUALS; break;
						}
					} else if (!(modstate & KMOD_NUM)) {
						switch (sym) {
							case SDLK_KP0: sym = K_INS; break;
							case SDLK_KP1: sym = K_END; break;
							case SDLK_KP2: sym = K_DOWNARROW; break;
							case SDLK_KP3: sym = K_PGDN; break;
							case SDLK_KP4: sym = K_LEFTARROW; break;
							case SDLK_KP5: sym = SDLK_5; break;
							case SDLK_KP6: sym = K_RIGHTARROW; break;
							case SDLK_KP7: sym = K_HOME; break;
							case SDLK_KP8: sym = K_UPARROW; break;
							case SDLK_KP9: sym = K_PGUP; break;
							case SDLK_KP_PERIOD: sym = K_DEL; break;
							case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
							case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK ;break;
							case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
							case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
							case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
							case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
						}
					} else {
						switch (sym) {
							case SDLK_KP0: sym = SDLK_0; break;
							case SDLK_KP1: sym = SDLK_1; break;
							case SDLK_KP2: sym = SDLK_2; break;
							case SDLK_KP3: sym = SDLK_3; break;
							case SDLK_KP4: sym = SDLK_4; break;
							case SDLK_KP5: sym = SDLK_5; break;
							case SDLK_KP6: sym = SDLK_6; break;
							case SDLK_KP7: sym = SDLK_7; break;
							case SDLK_KP8: sym = SDLK_8; break;
							case SDLK_KP9: sym = SDLK_9; break;
							case SDLK_KP_PERIOD: sym = SDLK_PERIOD; break;
							case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
							case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK; break;
							case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
							case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
							case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
							case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
						}
					}
					break;
				}

				/* Anything else above 255 we just don't handle */
				if (sym > 255)
					sym = 0;
				Key_Event (sym, ascii, state);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (mouse <= M_FREE)
					break;

				but = event.button.button;
				if ((but < 1) || (but > 16))
					break;

				switch (but) {
					case 2: but = 3; break;
					case 3: but = 2; break;
				}

				Key_Event (K_MOUSE1 + (but - 1), 0,
						event.type == SDL_MOUSEBUTTONDOWN);
				break;

			case SDL_MOUSEMOTION:
				if ((mouse <= M_FREE) || !(ccls.state >= ca_connected))
					break;

				mouse_x += event.motion.xrel;
				mouse_y += event.motion.yrel;
				break;

			case SDL_QUIT:
				Sys_Quit (0);
				break;

			default:
				break;
		}
	}
}

static void
IN_WindowedMouse (cvar_t *cvar)
{
	cvar = cvar;

	if (!VID_Inited)
		return;

	if (COM_CheckParm ("-nomouse"))
		mouse = M_NONE;
	else if ((sdl_flags & SDL_FULLSCREEN))
		mouse = M_FULL;
	else if (_windowed_mouse->ivalue)
		mouse = M_GRAB;
	else
		mouse = M_FREE;

	if (mouse >= M_GRAB)
	{
		SDL_ShowCursor (0);
		SDL_WM_GrabInput (SDL_GRAB_ON);
	} else {
		SDL_ShowCursor (1);
		SDL_WM_GrabInput (SDL_GRAB_OFF);
	}
}

static void
IN_KeypadMode (cvar_t *cvar)
{
	keypadmode = !!cvar->ivalue;
}
