/*
	$RCSfile$
	OpenGL Texture management.

	Copyright (C) 2002  Zephaniah E. Hull.

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

    "$Id$";
*/

#ifndef __gl_textures_h
#define __gl_textures_h

#include "qtypes.h"
#include "model.h"

extern void GLT_FloodFillSkin8 (Uint8 * skin, int skinwidth, int skinheight);
extern qboolean GLT_TriangleCheck8 (Uint8 *tex, int width, int height, astvert_t texcoords[3], Uint8 color);
extern int GLT_Mangle8 (Uint8 *in, Uint8 *out, int width, int height, short mask, Uint8 to, qboolean bleach);
extern void GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name, int width, int height, int frames, float interval);
extern void GLT_Init ();

#define TEX_ALPHA		1
#define TEX_MIPMAP		2
#define TEX_FBMASK		4

#endif // __gl_textures_h

