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

static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdlib.h>

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "image.h"
#include "pcx.h"
#include "strlib.h"
#include "sys.h"


static image_t *
PCX_LoadBuffer (Uint8 *buf)
{
	pcx_t		*pcx;
	Uint8		palette[768];
	Uint8       *pix, *pcx_rgb, *raw;
	int         x, y;
	int         dataByte, runLength;
	size_t		count;
	image_t		*img;

//
// parse the PCX file
//
	pcx = (pcx_t *)buf;
	raw = pcx->data;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	if (pcx->manufacturer != 0x0a || 
		pcx->version != 5 || 
		pcx->encoding != 1 || 
		pcx->bits_per_pixel != 8 || 
		pcx->xmax >= 320 || 
		pcx->ymax >= 256) {
		return NULL;
	}

	img = Zone_Alloc (img_zone, sizeof(image_t));

	memcpy (palette, buf + com_filesize - 768, 768);

	count = (pcx->xmax + 1) * (pcx->ymax + 1);

	img->width = pcx->xmax+1;
	img->height = pcx->ymax+1;

	pcx_rgb = Zone_Alloc (img_zone, count * 4);
	img->pixels = pcx_rgb;
	pix = pcx_rgb;

	for (y = 0; y <= pcx->ymax; y++) {
		for (x = 0; x <= pcx->xmax; ) {
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			} else {
				runLength = 1;
			}

			if ( ( y == pcx->ymax ) && ( x + runLength > pcx->xmax + 1 ) ) {
				runLength = pcx->xmax - x + 1;
			}

			while (runLength-- > 0) {
				pix[0] = palette[dataByte * 3];
				pix[1] = palette[dataByte * 3 + 1];
				pix[2] = palette[dataByte * 3 + 2];
				pix[3] = 255;
				pix += 4;
				x++; 
			}
		}
	}

	img->type = IMG_RGBA;
	return img;
}


image_t *
PCX_Load (fs_file_t *file, SDL_RWops *rw)
{
	image_t	*image;
	Uint8	*buf;

	buf = Zone_Alloc (tempzone, file->len);
	SDL_RWread (rw, buf, file->len, 1);
	SDL_RWclose (rw);
	image = PCX_LoadBuffer (buf);
	Zone_Free (buf);
	return image;

	return NULL;
}

