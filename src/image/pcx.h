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

#ifndef __PCX_H
#define __PCX_H

typedef struct {
	Sint8       manufacturer;
	Sint8       version;
	Sint8       encoding;
	Sint8       bits_per_pixel;
	Uint16		xmin, ymin, xmax, ymax;
	Uint16		hres, vres;
	Uint8		palette[48];
	Uint8		reserved;
	Sint8		color_planes;
	Uint16		bytes_per_line;
	Uint16		palette_type;
	Sint8		filler[58];
	Uint8		data[0];				// unbounded
} pcx_t;

image_t *PCX_Load (fs_file_t *file, SDL_RWops *rw);

#endif // __PCX_H

