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

#ifndef __DRAW_H
#define __DRAW_H

extern struct qpic_s *draw_disc;				// also used on sbar

// Need to make sure this structure exists
struct image_s;

void Draw_Init_Cvars (void);
void Draw_Init (void);
void Draw_Character (float x, float y, int num, float text_size);
void Draw_SubPic (int x, int y, struct qpic_s *pic,
		int srcx, int srcy, int width, int height);
void Draw_Pic (int x, int y, struct qpic_s *pic);
void Draw_TransPicTranslate (int x, int y, struct qpic_s *pic,
		Uint8 *translation);
void Draw_ConsoleBackground (int lines);
void Draw_Disc (void);
void Draw_Fill (int x, int y, int w, int h, vec4_t color);
void Draw_FadeScreen (void);
void Draw_String (float x, float y, char *str, float text_size);
void Draw_String_Len (float x, float y, char *str, int len, float text_size);
void Draw_Alt_String (float x, float y, char *str, float text_size);
void Draw_Alt_String_Len (float x, float y, char *str, int len, float text_size);
struct qpic_s *Draw_PicFromWad (char *name);
struct qpic_s *Draw_CachePic (char *path);
void Draw_Crosshair (void);

int GL_LoadTexture (const char *identifier, int width, int height,
		Uint8 *data, int flags, int bpp);
int GL_LoadPicTexture (struct qpic_s *pic);
void GL_Set2D (void);

int R_LoadTexture (const char *identifier, struct image_s *img, int flags);

#endif // __DRAW_H

