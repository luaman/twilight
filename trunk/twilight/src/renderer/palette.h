/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull
	Copyright (C) 2003  Forest Hale

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

#ifndef __PALETTE_H
#define __PALETTE_H

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define d_palette_empty	0x000000FF
#else
#define d_palette_empty	0xFF000000
#endif

extern Uint8	*host_basepal;
extern Uint32 d_palette_raw[256];
extern Uint32 d_palette_base[256];
extern Uint32 d_palette_fb[256];
extern Uint32 d_palette_base_team[256];
extern Uint32 d_palette_top[256];
extern Uint32 d_palette_bottom[256];
extern float d_8tofloattable[256][4];

extern cvar_t *gl_fb;

void PAL_Init_Cvars(void);
void PAL_Init(void);

#endif // __PALETTE_H
