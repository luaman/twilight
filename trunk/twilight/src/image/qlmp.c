/*
	$RCSfile$

	Copyright (C) 2002  Joseph Carter

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

#include <stdlib.h>
#include "SDL.h"

#include "common.h"
#include "image.h"
#include "qlmp.h"
#include "strlib.h"

static image_t *
QLMP_LoadQPic (Uint8 *buf)
{
	image_t	   *img;
	Uint32		numpixels;

	img = Zone_Alloc (img_zone, sizeof(image_t));
	
	img->width = LittleLong (*(Uint32 *)buf);
	buf += 4;
	img->height = LittleLong (*(Uint32 *)buf);
	buf += 4;

	if (img->width > 4096 || img->height > 4096 ||
			img->width < 1 || img->height < 1)
	{
		Com_Printf ("QLMP_Load: invalid size (%ix%i)\n",
				img->width, img->height);
		free (img);
		return NULL;
	}

	numpixels = img->width * img->height;
	img->pixels = Zone_Alloc (img_zone, numpixels * sizeof (Uint8));
	memcpy (img->pixels, buf, numpixels);

	img->type = IMG_QPAL;
	return img;
}


#define CONCHARS_W 128
#define CONCHARS_H 128
#define CONCHARS_SIZE (CONCHARS_W * CONCHARS_H)

static image_t *
QLMP_LoadFont (Uint8 *buf)
{
	Uint32		i;
	image_t	   *img;

	img = Zone_Alloc (img_zone, sizeof (image_t));
	img->pixels = Zone_Alloc (img_zone, CONCHARS_SIZE * sizeof (Uint8));

	img->width = CONCHARS_W;
	img->height = CONCHARS_H;

	for (i = 0; i < CONCHARS_SIZE; i++)
	{
		// color 0 should be transparent in font
		if (*buf == 0)
			img->pixels[i] = 255;
		else
			img->pixels[i] = *buf;
		buf++;
	}

	img->type = IMG_QPAL;
	return img;
}

image_t *
QLMP_Load (fs_file_t *file, SDL_RWops *rw)
{
	image_t	*image;
	Uint8	*buf;

	buf = Zone_Alloc (tempzone, file->len);
	SDL_RWread (rw, buf, file->len, 1);
	SDL_RWclose (rw);
	if (!strcasecmp ("gfx/conchars", file->name_base))
		image = QLMP_LoadFont (buf);
	else
		image = QLMP_LoadQPic (buf);
	Zone_Free (buf);
	return image;
}
