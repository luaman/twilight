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

#ifndef __IMAGE_H
#define __IMAGE_H

#include "qtypes.h"

#define IMG_QPAL	0
//define IMG_PAL		1
//define IMG_RGB		2
#define IMG_RGBA	4

typedef struct image_s
{
	Uint32		width;
	Uint32		height;
	Uint32		type;
	Uint8		*pixels;
} image_t;

image_t *Image_Load (char *name);

#endif // __IMAGE_H

