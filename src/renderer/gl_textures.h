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

extern Uint32 *GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette, qboolean check_empty);
extern void GLT_FloodFillSkin8 (Uint8 * skin, int skinwidth, int skinheight);
//extern qboolean GLT_TriangleCheck8 (Uint32 *tex, span_t *span, int width, int height, astvert_t texcoords[3], Uint32 color);
extern void GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name, int width, int height, int frames, float interval);
extern void GLT_Delete_Sub_Skin (skin_sub_t *sub);
extern void GLT_Delete_Skin (skin_t *skin);
extern void GLT_Init ();

#define TEX_NONE		0
#define TEX_ALPHA		1
#define TEX_MIPMAP		2
#define TEX_FORCE		4

#endif // __gl_textures_h

