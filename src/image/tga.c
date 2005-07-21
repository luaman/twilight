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

#include "common.h"
#include "image.h"
#include "sys.h"
#include "tga.h"

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;


static image_t *
TGA_LoadBuffer (Uint8 *buffer, const char *name)
{
	size_t		numPixels;
	int         columns, rows;
	Uint8       *pixbuf;
	int         row, column;
	TargaHeader	targa_header;
	Uint8		*targa_rgba, *buf_p;
	Uint8		red, green, blue, alphabyte;
	image_t		*img;

	img = Zone_Alloc (img_zone, sizeof(image_t));

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10 &&
		targa_header.image_type != 3)
		Sys_Error ("LoadTGA: Only type 2, 3 and 10 targa RGB images supported. (%s)\n", name);

	if (targa_header.colormap_type != 0
		|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Sys_Error
			("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps). (%s)\n", name);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	img->width = columns;
	img->height = rows;

	targa_rgba = Zone_Alloc (img_zone, numPixels*4);
	img->pixels = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment

	if (targa_header.image_type == 2 || targa_header.image_type == 3)
	{ 
		// Uncompressed RGB or gray scale image
		for (row = rows - 1; row >= 0; row--) 
		{
			pixbuf = targa_rgba + row * columns * 4;

			for(column = 0; column < columns; column++) 
			{
				switch (targa_header.pixel_size) 
				{
					case 8:
						blue = *buf_p++;
						green = blue;
						red = blue;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;

					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;

					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;

					default:
						break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10) {   // Runlength encoded RGB images
		Uint8 packetHeader, packetSize, j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for (row = rows - 1; row >= 0; row--) {

			pixbuf = targa_rgba + row*columns*4;

			for (column = 0; column < columns; ) {
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = 255;
							break;

						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							break;

						default:
							break;
					}
	
					for(j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns) { // run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else {                            // non run-length packet
					for(j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
							case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;

							case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;

							default:
								break;
						}

						column++;

						if (column == columns) { // pixel packet run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}						
					}
				}
			}

			breakOut:;
		}
	}

	img->type = IMG_RGBA;
	return img;
}

image_t *
TGA_Load (fs_file_t *file, SDL_RWops *rw)
{
	image_t	*image;
	Uint8	*buf;

	buf = Zone_Alloc (tempzone, file->len);
	SDL_RWread (rw, buf, file->len, 1);
	SDL_RWclose (rw);
	image = TGA_LoadBuffer (buf, file->name_base);
	Zone_Free (buf);
	return image;
}


qboolean
TGA_Write (char *name, int width, int height, int bpp, Uint8 *buffer)
{
	static char header[18] = "\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	FILE		*handle;
	char        tganame[MAX_OSPATH];

	header[12] = width & 255;
	header[13] = (width >> 8);
	header[14] = height & 255;
	header[15] = (height >> 8);
	header[16] = bpp << 3;

	snprintf (tganame, sizeof (tganame), "%s", name);
	COM_StripExtension (tganame, tganame);
	COM_DefaultExtension (tganame, ".tga", sizeof(tganame));

	handle = fopen (tganame, "wb");

	if (!handle) {
		Sys_Printf ("COM_WriteFile: failed on %s\n", tganame);
		return false;
	}

	Sys_Printf ("COM_WriteFile: %s\n", tganame);
	fwrite (header, 1, 18, handle);
	fwrite (buffer, 1, (size_t) width * height * bpp, handle);
	fclose (handle);

	return true;
}

