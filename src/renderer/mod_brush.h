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

#ifndef __MOD_BRUSH_H
#define __MOD_BRUSH_H

#include "bspfile.h"
#include "common.h"
#include "modelgen.h"
#include "spritegn.h"
#include "zone.h"
#include "qtypes.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct {
	vec3_t      position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


typedef struct texture_s {
	char				 name[16];
	Uint				 width, height;
	Uint				 texnum;		// Index into the bmodel texture array.
	GLuint				 gl_texturenum;
	GLuint				 fb_texturenum;	// index of fullbright mask or 0
	int					 anim_total;	// total tenths in sequence ( 0 = no)
	int					 anim_min;		// time for this frame min <=time< max
	int					 anim_max;		// time for this frame min <=time< max
	struct texture_s	*anim_next;		// in the animation sequence
	struct texture_s	*alt_anims;		// bmodels in frmae 1 use these
} texture_t;

#define SURF_PLANEBACK		BIT(1)
#define	SURF_SKY			BIT(2)
#define SURF_LIQUID			BIT(3)
#define SURF_UNDERWATER		BIT(4)
#define SURF_NOLIGHTMAP		BIT(5)
#define SURF_SUBDIVIDE		BIT(6)

typedef struct {
	unsigned short v[2];
	unsigned int cachededgeoffset;
} medge_t;

typedef struct {
	float       vecs[2][4];
	texture_t  *texture;
	int         flags;
} mtexinfo_t;

typedef struct glpoly_s {
	struct glpoly_s	*next;				// Next polygon on surface.
	Uint			 numverts;
	vertex_t		*v;					// Vertices
	texcoord_t		*tc;				// Texcoords
	texcoord_t		*ltc;				// Lightmap texcoords
} glpoly_t;

typedef struct msurface_s {
	// should be drawn if visframe == r_framecount (set by WorldNode
	// functions)
	Uint				visframe;

	// the node plane this is on, backwards if SURF_PLANEBACK flag set
	mplane_t			*plane;

	// SURF_ flags
	int					flags;

	
	// look up in model->surfedges[], negative numbers are backwards edges
	Uint				firstedge;
	Uint				numedges;

	// gl lightmap coordinates mess
	short				texturemins[2];
	short				extents[2];
	short				smax, tmax, alignedwidth;
	int					light_s, light_t;

	// raw polygon for this surface
	glpoly_t			*polys;

	// this is where the real texture information is
	mtexinfo_t			*texinfo;

	// dynamic lighting info
	Uint				dlightframe, lightframe, lightmappedframe;
	int					dlightbits;

	int					lightmap_texnum;
	Uint8				styles[MAXLIGHTMAPS];

	// values currently used in lightmap
	int					cached_light[MAXLIGHTMAPS];

	// if lightmap was lit by dynamic lights, force update on next frame
	qboolean			cached_dlight;

	struct chain_item_s	*tex_chain;			// Tex chain item for us.
	struct chain_item_s	*light_chain;		// Light chain item for us.

	// RGB lighting data [numstyles][height][width][3] or white lighting
	// data [numstyles][height][width] - FIXME: This is ugly as hell
	Uint8				*samples;

	// stain to apply on lightmap (soot/dirt/blood/whatever)
	Uint8				*stainsamples;
} msurface_t;

/*
 * Flags for different chain types.
 * (FIXME: Do these need to be flags, or just an enum?)
 * (FIXED: Yes, a chain can be both normal and FB.)
 */
#define CHAIN_NORMAL							BIT(0) // Chain is normal.
#define CHAIN_LIQUID							BIT(1) // Chain is liquid.
#define CHAIN_SKY								BIT(2) // Chain is sky.
#define CHAIN_FB								BIT(3) // Chain is fullbright.
#define CHAIN_LIGHTMAP							BIT(4) // Chain is lightmap.

typedef struct chain_head_s {
	texture_t			*texture;
	GLuint				 l_texnum;
	Uint				 flags, n_items, visframe;
	struct chain_item_s	*items;
} chain_head_t;

typedef struct chain_item_s {
	Uint				 visframe;
	msurface_t			*surf;
	chain_head_t		*head;
	struct chain_item_s	*next;
} chain_item_t;
		

typedef struct mnode_s {
// common with leaf
	int         contents;				// 0, to differentiate from leafs
	Uint        visframe;				// determined visible by WorldNode
	Uint		pvsframe;				// set by MarkLeaves

	vec3_t      mins;					// for bounding box culling
	vec3_t		maxs;					// for bounding box culling

	struct mnode_s *parent;

// node specific
	mplane_t   *plane;
	struct mnode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mnode_t;



typedef struct mleaf_s {
// common with node
	int         contents;				// wil be a negative contents number
	Uint        visframe;				// determined visible by WorldNode
	Uint		pvsframe;				// set by MarkLeaves

	vec3_t      mins;					// for bounding box culling
	vec3_t		maxs;					// for bounding box culling

	struct mnode_s *parent;

// leaf specific
	Uint8      *compressed_vis;

	msurface_t **firstmarksurface;
	int         nummarksurfaces;
	int         key;					// BSP sequence number for leaf's
	// contents
	Uint8       ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct hull_s {
	dclipnode_t *clipnodes;
	mplane_t   *planes;
	int         firstclipnode;
	int         lastclipnode;
	vec3_t      clip_mins;
	vec3_t      clip_maxs;
	vec3_t		clip_size;
} hull_t;

#define LIGHTBLOCK_WIDTH	512
#define LIGHTBLOCK_HEIGHT	512
#define MAX_LIGHTMAPS		256

typedef struct lightsubblock_s {
	int						 allocated[LIGHTBLOCK_WIDTH];
	chain_head_t			 chain;
	struct lightsubblock_s	*next;
} lightsubblock_t;

typedef struct lightblock_s {
	Uint			 num;
	lightsubblock_t	*b;
} lightblock_t;

typedef struct brushhdr_s {
	qboolean		is_submodel;
	Uint			firstmodelsurface, nummodelsurfaces;

	lightblock_t	*lightblock;

	Uint32			numsubmodels;
	dmodel_t		*submodels;

	Uint32			numplanes;
	mplane_t		*planes;

	Uint32			numleafs;		// number of visible leafs, not counting 0
	mleaf_t			*leafs;

	Uint32			numvertexes;
	mvertex_t		*vertexes;

	Uint32			numedges;
	medge_t			*edges;

	Uint32			numnodes;
	mnode_t			*nodes;

	Uint32			numtexinfo;
	mtexinfo_t		*texinfo;

	Uint32			numsurfaces;
	msurface_t		*surfaces;

	Uint32			numsurfedges;
	int				*surfedges;

	Uint32			numclipnodes;
	dclipnode_t		*clipnodes;

	Uint32			nummarksurfaces;
	msurface_t		**marksurfaces;

	hull_t			hulls[MAX_MAP_HULLS];

	Uint32			numtextures;
	texture_t		**textures;
	chain_head_t	**tex_chains;

	chain_head_t	sky_chain;

	Uint8			*visdata;
	Uint8			*lightdata;
	char			*entities;

	Uint32			checksum;
	Uint32			checksum2;
} brushhdr_t;

#endif // __MOD_BRUSH_H

