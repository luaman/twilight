/*
	Dynamic texture generation.

	Copyright (C) 2000-2001   Zephaniah E. Hull.
	Copyright (C) 2000-2001   Ragnvald "Despair" Maartmann-Moe IV

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

#ifndef __GEN_TEXTURES_H
#define __GEN_TEXTURES_H

#include "qtypes.h"

extern const int GTF_smoke[8];
extern const int GTF_rainsplash[16];
extern const int GTF_dot;
extern const int GTF_raindrop;
extern const int GTF_bubble;
extern const int GTF_lightning_beam;
extern const int GTF_blooddecal[8];

#define MAX_GT_FONT_TEXTURES 64
// GTF_texture_t is a rectangle in the particlefonttexture
typedef struct {
	int		texture;
	float	s1, t1, s2, t2;
} GTF_texture_t;

int GTF_texnum;
GTF_texture_t GTF_texture[MAX_GT_FONT_TEXTURES];

#endif // __GEN_TEXTURES_H
