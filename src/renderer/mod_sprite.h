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

#ifndef __MOD_SPRITE_H
#define __MOD_SPRITE_H

#include "common.h"
#include "mod_sprite_disk.h"
#include "zone.h"

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s {
	int         width;
	int         height;
	float       up, down, left, right;
	int         gl_texturenum;
} mspriteframe_t;

typedef struct {
	int         numframes;
	float      *intervals;
	mspriteframe_t *frames[1];
} mspritegroup_t;

typedef struct {
	spriteframetype_t type;
	mspriteframe_t *frameptr;
} mspriteframedesc_t;

typedef struct {
	int         type;
	int         maxwidth;
	int         maxheight;
	int         numframes;
	float       beamlength;				// remove?
	void       *cachespot;				// remove?
	mspriteframedesc_t frames[1];
} msprite_t;

#endif // __MOD_SPRITE_H

