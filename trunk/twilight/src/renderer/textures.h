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

#include "image/image.h"
#include "mathlib.h"
#include "model.h"
#include "qtypes.h"

void GLT_Init_Cvars(void);
void GLT_Init(void);
void GLT_Shutdown (void);
void GLT_Skin_Parse(Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name, int width, int height, int frames, float interval);
void GLT_Delete_Skin(skin_t *skin);
void R_ResampleTexture(void *id, int iw, int ih, void *od, int ow, int oh);
qboolean GL_Upload32(Uint32 *data, int width, int height, int flags);
qboolean GL_Upload8(Uint8 *data, int width, int height, Uint32 *palette, int flags);
int GLT_Load_Raw(const char *identifier, Uint width, Uint height, Uint8 *data, Uint32 *palette, int flags, int bpp);
int GLT_Load_image(const char *identifier, image_t *img, Uint32 *palette, int flags);
int GLT_Load_Pixmap(const char *name, const char *data);
qboolean GLT_Delete(GLuint texnum);

#endif // __gl_textures_h

