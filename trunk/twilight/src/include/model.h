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
#include "../src/renderer/mod_alias.h"		// FIXME: HACK HACK HACK!
#include "../src/renderer/mod_brush.h"		// FIXME: HACK HACK HACK!
#include "../src/renderer/mod_sprite.h"		// FIXME: HACK HACK HACK!

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
	qboolean    loaded;			// bmodels and sprites don't cache normally

	modtype_t   type;
	int         numframes;
	synctype_t  synctype;

	int         flags;
	int			modflags;

//
// volume occupied by the model graphics
//
	vec3_t		rotatedmins;
	vec3_t		rotatedmaxs;
	vec3_t		normalmins;
	vec3_t		normalmaxs;
	vec3_t		yawmins;
	vec3_t		yawmaxs;

	hull_t		hulls[MAX_MAP_HULLS];

//
// additional model data
//
	memzone_t	*zone;
	brushhdr_t	*brush;
	aliashdr_t	*alias;
	msprite_t	*sprite;
} model_t;

//============================================================================

void        Mod_Init_Cvars (void);
void        Mod_Init (void);
void        Mod_ClearAll (void);
model_t    *Mod_ForName (char *name, qboolean crash);
void        Mod_TouchModel (char *name);

mleaf_t    *Mod_PointInLeaf (float *p, model_t *model);
Uint8      *Mod_LeafPVS (mleaf_t *leaf, model_t *model);

void		Mod_UnloadModel (model_t *mod);
model_t		*Mod_LoadModel (model_t *mod, qboolean crash);

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

