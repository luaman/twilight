/*
	$RCSfile$

	Copyright (C) 2000  Forest Hale

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

#ifndef __TRANSFORM_H
#define __TRANSFORM_H

#define tft_translate 1
#define tft_rotate 2

extern vec_t softwaretransform_scale;
extern vec3_t softwaretransform_offset;
extern vec3_t softwaretransform_x;
extern vec3_t softwaretransform_y;
extern vec3_t softwaretransform_z;
extern int softwaretransform_type;

extern void softwaretransformforentity (entity_t *e);
extern void softwaretransformforbrushentity (entity_t *e);
extern void softwaretransformidentity (void);
extern void softwaretransformset (vec3_t origin, vec3_t angles, vec_t scale);
extern void (*softwaretransform) (vec3_t in, vec3_t out);
extern void softwareuntransform (vec3_t in, vec3_t out);

#endif // __TRANSFORM_H

