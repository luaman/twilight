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

#ifndef __MODEL__
#define __MODEL__

#include "bspfile.h"
#include "common.h"
#include "modelgen.h"
#include "spritegn.h"
#include "zone.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8
#define	EF_FLAG1	 			16
#define	EF_FLAG2	 			32
#define EF_BLUE					64
#define EF_RED					128

#define EF_LIGHTMASK		(EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_BLUE | EF_RED)

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
	char        name[16];
	unsigned    width, height;
	int         gl_texturenum;
	int			fb_texturenum;			// index of fullbright mask or 0
	struct msurface_s *texturechain;	// for gl_texsort drawing
	int         anim_total;				// total tenths in sequence ( 0 = no)
	int         anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frmae 1 use these
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_DONTWARP		0x100

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
	struct glpoly_s *next;				// Next in texture chain.
	struct glpoly_s *lchain;			// Next in lightmap chain.
	struct glpoly_s	*fb_chain;			// Next in FB chain.
	int         numverts;
	int         flags;					// for SURF_UNDERWATER
	vertex_t	*v;						// Vertices
	texcoord_t	*tc;					// Texcoords
	texcoord_t	*ltc;					// Lightmap texcoords
} glpoly_t;

typedef struct msurface_s {
	// should be drawn if visframe == r_framecount (set by WorldNode
	// functions)
	int					visframe;

	// the node plane this is on, backwards if SURF_PLANEBACK flag set
	mplane_t			*plane;

	// SURF_ flags
	int					flags;

	
	// look up in model->surfedges[], negative numbers are backwards edges
	int					firstedge;
	int					numedges;

	// gl lightmap coordinates mess
	short				texturemins[2];
	short				extents[2];
	short				smax, tmax, alignedwidth, unusedpadding;
	int					light_s, light_t;

	// raw polys for this surface
	glpoly_t			*polys;

	// list of surfaces using this texture in this frame (yes, it's odd)
	struct msurface_s	*texturechain;

	// this is where the real texture information is
	mtexinfo_t			*texinfo;

	// dynamic lighting info
	int					dlightframe, lightframe, lightmappedframe;
	int					dlightbits;

	int					lightmaptexturenum;
	Uint8				styles[MAXLIGHTMAPS];

	// values currently used in lightmap
	int					cached_light[MAXLIGHTMAPS];

	// if lightmap was lit by dynamic lights, force update on next frame
	qboolean			cached_dlight;

	// RGB lighting data [numstyles][height][width][3] or white lighting
	// data [numstyles][height][width] - FIXME: This is ugly as hell
	Uint8				*samples;

	// stain to apply on lightmap (soot/dirt/blood/whatever)
	Uint8				*stainsamples;
} msurface_t;

typedef struct mnode_s {
// common with leaf
	int         contents;				// 0, to differentiate from leafs
	int         visframe;				// determined visible by WorldNode
	int			pvsframe;				// set by MarkLeaves

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
	int         visframe;				// determined visible by WorldNode
	int			pvsframe;				// set by MarkLeaves

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


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct {
	Uint8	v[3];
} avertex_t;

typedef struct {
	float s;
	float t;
} astvert_t;

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

typedef struct skin_sub_s {
	int	texnum;
	int	num_indices;
	int	*indices;
} skin_sub_t;

typedef struct skin_s {
	int			frames;
	float		interval;
	skin_sub_t	*raw;
	skin_sub_t	*base;
	skin_sub_t	*base_team;
	skin_sub_t	*top_bottom;
	skin_sub_t	*fb;
	skin_sub_t	*top;
	skin_sub_t	*bottom;
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
	astvert_t	*tcarray;			// Texcoord array.
	maliasframedesc_t	*frames;	// Frames.

	skin_t		*skins;
} aliashdr_t;

#define	MAXALIASVERTS	1024
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048
extern aliashdr_t *pheader;
extern stvert_t stverts[MAXALIASVERTS];
extern mtriangle_t triangles[MAXALIASTRIS];
extern trivertx_t *poseverts[MAXALIASFRAMES];

//===================================================================

//
// Whole model
//

typedef enum { mod_brush, mod_sprite, mod_alias } modtype_t;

#define	EF_ROCKET	1					// leave a trail
#define	EF_GRENADE	2					// leave a trail
#define	EF_GIB		4					// leave a trail
#define	EF_ROTATE	8					// rotate (bonus items)
#define	EF_TRACER	16					// green split trail
#define	EF_ZOMGIB	32					// small blood trail
#define	EF_TRACER2	64					// orange split trail + rotate
#define	EF_TRACER3	128					// purple trail

#define FLAG_FULLBRIGHT	1				// always fullbright
#define FLAG_NOSHADOW	2				// do not draw shadow
#define FLAG_EYES		4				// eyes model, add an offset
#define FLAG_NO_IM_ANIM 8				// do not interpolate frames (1 frame only)
#define FLAG_NO_IM_FORM 16				// do not interpolate angles or position (weapons)
#define FLAG_PLAYER		32				// always has some light
#define FLAG_TORCH1		64				// skip drawing, add particles
#define FLAG_TORCH2		128				// skip drawing, add particles
#define FLAG_DOUBLESIZE	256				// double sized model

typedef struct model_s {
	char        name[MAX_QPATH];
	qboolean    needload;			// bmodels and sprites don't cache normally

	modtype_t   type;
	int         numframes;
	synctype_t  synctype;

	int         flags;
	int			modflags;

//
// volume occupied by the model graphics
//      
	Uint32		c1;
	vec3_t		rotatedmins;
	Uint32		c2;
	vec3_t		rotatedmaxs;
	Uint32		c3;
	vec3_t		normalmins;
	Uint32		c4;
	vec3_t		normalmaxs;
	Uint32		c5;
	vec3_t		yawmins;
	Uint32		c6;
	vec3_t		yawmaxs;
	Uint32		c7;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	Uint32		numsubmodels;
	dmodel_t	*submodels;

	Uint32		numplanes;
	mplane_t	*planes;

	Uint32		numleafs;			// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	Uint32		numvertexes;
	mvertex_t	*vertexes;

	Uint32		numedges;
	medge_t		*edges;

	Uint32		numnodes;
	mnode_t		*nodes;

	Uint32		numtexinfo;
	mtexinfo_t	*texinfo;

	Uint32		numsurfaces;
	msurface_t	*surfaces;

	Uint32		numsurfedges;
	int			*surfedges;

	Uint32		numclipnodes;
	dclipnode_t	*clipnodes;

	Uint32		nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	Uint32		numtextures;
	texture_t	**textures;

	Uint8		*visdata;
	Uint8		*lightdata;
	char		*entities;

	Uint32		checksum;
	Uint32		checksum2;

//
// additional model data
//
	memzone_t	*extrazone;
	void		*extradata;
} model_t;

//============================================================================

void        Mod_Init_Cvars (void);
void        Mod_Init (void);
void        Mod_ClearAll (void);
model_t    *Mod_ForName (char *name, qboolean crash);
void       *Mod_Extradata (model_t *mod);	// handles caching
void        Mod_TouchModel (char *name);

mleaf_t    *Mod_PointInLeaf (float *p, model_t *model);
Uint8      *Mod_LeafPVS (mleaf_t *leaf, model_t *model);

void		Mod_LoadVisibility (lump_t *l); 
void		Mod_LoadEntities (lump_t *l);
void		Mod_LoadVertexes (lump_t *l);
void		Mod_LoadSubmodels (lump_t *l);
void		Mod_LoadEdges (lump_t *l);
void		Mod_LoadNodes (lump_t *l);
void		Mod_LoadLeafs (lump_t *l);
void		Mod_LoadClipnodes (lump_t *l);
void		Mod_LoadMarksurfaces (lump_t *l);
void		Mod_LoadSurfedges (lump_t *l);
void		Mod_LoadPlanes (lump_t *l);
void		Mod_MakeHull0 (void);
void		Mod_SetParent (mnode_t *node, mnode_t *parent);
model_t    *Mod_FindName (char *name);
qboolean	Mod_MinsMaxs (model_t *mod, vec3_t org, vec3_t ang, vec3_t mins, vec3_t maxs);


#endif // __MODEL__

