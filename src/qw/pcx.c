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

// pcx.c
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <stdlib.h>

#include "SDL.h"

#include "common.h"
#include "pcx.h"
#include "strlib.h"
#include "sys.h"

typedef struct {
	char        manufacturer;
	char        version;
	char        encoding;
	char        bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char palette[48];
	char        reserved;
	char        color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char        filler[58];
	unsigned char   data;					// unbounded
} pcx_t;

/*
============
PCX_LoadBuffer
============
*/
static void
PCX_LoadBuffer (Uint8 *buf, Uint8 **pic, int *width, int *height)
{
	pcx_t      *pcx;
	Uint8        palette[768];
	Uint8       *pix, *pcx_rgb;
	int         x, y;
	int         dataByte, runLength;
	int         count;

	*pic = NULL;

//
// parse the PCX file
//
	pcx = (pcx_t *)buf;
	pix = &pcx->data;

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
		Sys_Error ("Bad pcx file\n");
		return;
	}

	memcpy (palette, buf + com_filesize - 768, 768);

	count = (pcx->xmax + 1) * (pcx->ymax + 1);

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	pcx_rgb = malloc (count * 4);
	*pic = pcx_rgb;

	for (y = 0; y <= pcx->ymax; y++, pcx_rgb += pcx->xmax+1) {
		for (x = 0; x <= pcx->xmax; ) {
			dataByte = *pix++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				dataByte = *pix++;
			} else {
				runLength = 1;
			}

			if ( ( y == pcx->ymax ) && ( x + runLength > pcx->xmax + 1 ) ) {
				runLength = pcx->xmax - x + 1;
			}

			while (runLength-- > 0) {
				pcx_rgb[0] = palette[dataByte * 3];
				pcx_rgb[1] = palette[dataByte * 3 + 1];
				pcx_rgb[2] = palette[dataByte * 3 + 2];
				pcx_rgb[3] = 255;
				pcx_rgb += 4;
				x++;
			}
		}
	}
}

/*
============
PCX_Load
============
*/
void
PCX_Load (char *name, Uint8 **image_pcx, int *width, int *height)
{
	Uint8 *buf = COM_LoadTempFile (name);

	if (buf) {
		PCX_LoadBuffer (buf, image_pcx, width, height);
	}
	else {
		image_pcx = NULL;
	}
}
