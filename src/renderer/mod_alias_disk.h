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
//
// modelgen.h: header file for model generation program
//

#ifndef __MODELGEN_H
#define __MODELGEN_H

// *********************************************************
// * This file must be identical in the modelgen directory *
// * and in the Quake directory, because it's used to      *
// * pass data from one to the other via model files.      *
// *********************************************************

#ifdef INCLUDELIBS

#include <stdlib.h>
#include <stdio.h>

#include "cmdlib.h"
#include "lbmlib.h"
#include "mathlib.h"
#include "scriplib.h"
#include "strlib.h"
#include "trilib.h"

#endif

#define ALIAS_VERSION	6

#define ALIAS_ONSEAM				0x0020

// must match definition in spritegn.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum { ST_SYNC = 0, ST_RAND } synctype_t;
#endif

typedef enum { ALIAS_SINGLE = 0, ALIAS_GROUP } aliasframetype_t;

typedef enum { ALIAS_SKIN_SINGLE = 0, ALIAS_SKIN_GROUP } aliasskintype_t;

typedef struct {
	Sint32		ident;
	Sint32		version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	Sint32		numskins;
	Sint32		skinwidth;
	Sint32		skinheight;
	Sint32		numverts;
	Sint32		numtris;
	Sint32		numframes;
	synctype_t	synctype;
	Sint32		flags;
	float		size;
} dmdl_t;

// TODO: could be shorts

typedef struct {
	Sint32		onseam;
	Sint32		s;
	Sint32		t;
} dstvert_t;

typedef struct dtriangle_s {
	Sint32		facesfront;
	Sint32		vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT				0x0010

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data

typedef struct {
	Uint8		v[3];
	Uint8		lightnormalindex;
} trivertx_t;

typedef struct {
	trivertx_t	bboxmin;				// lightnormal isn't used
	trivertx_t	bboxmax;				// lightnormal isn't used
	char		name[16];				// frame name from grabbing
} daliasframe_t;

typedef struct {
	Sint32		numframes;
	trivertx_t	bboxmin;				// lightnormal isn't used
	trivertx_t	bboxmax;				// lightnormal isn't used
} daliasgroup_t;

typedef struct {
	Sint32		numskins;
} daliasskingroup_t;

typedef struct {
	float		interval;
} daliasinterval_t;

typedef struct {
	float		interval;
} daliasskininterval_t;

typedef struct {
	aliasframetype_t type;
} daliasframetype_t;

typedef struct {
	aliasskintype_t type;
} daliasskintype_t;

#define IDPOLYHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')
														// little-endian "IDPO"

#endif // __MODELGEN_H

