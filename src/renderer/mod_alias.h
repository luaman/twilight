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

#ifndef __MOD_ALIAS_H
#define __MOD_ALIAS_H

#include "bspfile.h"
#include "common.h"
#include "modelgen.h"
#include "spritegn.h"
#include "zone.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

/*
 * normalizing factor so player model works out to about 1 pixel per triangle
 */
#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
#define MAX_LBM_HEIGHT				480

typedef struct {
	Uint8	v[3];
} avertex_t;

typedef struct {
	Uint8		*normal_indices;	// Vertex normal indices.
	avertex_t	*vertices;			// The compressed vertices.
} maliaspose_t;

typedef struct {
	int				numposes;
	float			interval;
	avertex_t		bboxmin;
	avertex_t		bboxmax;
	int				frame;
	maliaspose_t	*poses;
	char			name[16];
} maliasframedesc_t;

typedef struct mtriangle_s {
	int         vertindex[3];
} mtriangle_t;

typedef struct skin_indices_s {
	int	num;
	int	*i;
} skin_indices_t;

typedef struct skin_sub_s {
	int				texnum;
	int				num_tris;
	int				*tris;
	skin_indices_t	indices;
} skin_sub_t;

typedef struct skin_s {
	int				frames;
	float			interval;
	skin_sub_t		*base;
	skin_sub_t		*base_team;
	skin_sub_t		*fb;
	skin_sub_t		*top;
	skin_sub_t		*bottom;
	skin_indices_t	*base_fb_i;
	skin_indices_t	*base_team_fb_i;
	skin_indices_t	*top_bottom_i;
} skin_t;

#define	MAX_SKINS	32
typedef struct {
	int         ident;
	int         version;
	vec3_t      scale;
	vec3_t      scale_origin;
	float       boundingradius;
	vec3_t      eyeposition;
	int         numskins;
	int         skinwidth;
	int         skinheight;
	int         numverts;
	int         numtris;
	int         numframes;
	synctype_t  synctype;
	int         flags;
	float       size;

	int         numposes;
	int         poseverts;
	mtriangle_t	*triangles;			// Triangle list.
	texcoord_t	*tcarray;			// Texcoord array.
	maliasframedesc_t	*frames;	// Frames.

	skin_t		*skins;
} aliashdr_t;

#define	MAXALIASVERTS	1024
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048
extern struct model_s *player_model;
extern float bboxradius;
extern float bboxyawradius;

#endif // __MOD_ALIAS_H

