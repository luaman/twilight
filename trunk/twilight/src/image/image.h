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
#include "fs.h"

#define TEX_NONE		0
#define TEX_ALPHA		BIT(0)
#define TEX_MIPMAP		BIT(1)
#define TEX_FORCE		BIT(2)
#define TEX_REPLACE		BIT(3)
#define TEX_UPLOAD		BIT(4)
#define TEX_KEEPRAW		BIT(5)
#define TEX_NEED		BIT(6)


#define IMG_QPAL	0
//define IMG_PAL		1
//define IMG_RGB		2
#define IMG_RGBA	4

typedef struct image_s
{
	fs_file_t	*file;
	Uint32		width;
	Uint32		height;
	Uint32		type;
	Uint8		*pixels;
	GLuint		texnum;
} image_t;

typedef image_t * (IMG_Load) (fs_file_t *file, SDL_RWops *rw);

typedef struct img_search_s {
	char				*ext;
	IMG_Load			*load;
	struct img_search_s	*next;
} img_search_t;

extern memzone_t		*img_zone;
extern img_search_t		*img_search;

void Image_Init (void);
image_t *Image_Load (char *name, int flags);
image_t *Image_Load_Multi (const char **names, int flags);

#endif // __IMAGE_H

