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
#include "image.h"
#include "wad.h"

extern Uint32 *GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette, qboolean check_empty);
extern void GLT_FloodFill8 (Uint8 * skin, int skinwidth, int skinheight);
extern void GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name, int width, int height, int frames, float interval);
extern void GLT_Delete_Sub_Skin (skin_sub_t *sub);
extern void GLT_Delete_Skin (skin_t *skin);
extern void GLT_Init_Cvars ();
extern void GLT_Init ();
extern int GLT_Load_Raw (const char *identifier, Uint width, Uint height, Uint8 *data, Uint32 *palette, int flags, int bpp);
extern int GLT_Load_image(const char *identifier, image_t *img, Uint32 *palette, int flags);
extern int GLT_Load_Pixmap (const char *name, const char *data);
extern int GLT_Load_qpic (qpic_t *pic);
extern qboolean GLT_Delete (GLuint texnum);
extern qboolean GL_Upload32 (Uint32 *data, int width, int height, int flags);

extern int		glt_solid_format;
extern int		glt_alpha_format;
extern int		glt_filter_min;
extern int		glt_filter_mag;
extern memzone_t *glt_zone;

#define TEX_NONE		0
#define TEX_ALPHA		1
#define TEX_MIPMAP		2
#define TEX_FORCE		4
#define TEX_REPLACE		8

#endif // __gl_textures_h

