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

#include "common.h"
#include "zone.h"
#include "mod_alias.h"
#include "mod_brush.h"
#include "mod_sprite.h"

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

#define FLAG_FULLBRIGHT	BIT(0)			// always fullbright
#define FLAG_NOSHADOW	BIT(1)			// do not draw shadow
#define FLAG_EYES		BIT(2)			// eyes model, add an offset
#define FLAG_NO_IM_ANIM BIT(3)			// do not interpolate frames (1 frame only)
#define FLAG_NO_IM_FORM BIT(4)			// do not interpolate angles or position (weapons)
#define FLAG_PLAYER		BIT(5)			// always has some light
#define FLAG_TORCH1		BIT(6)			// skip drawing, add particles
#define FLAG_TORCH2		BIT(7)			// skip drawing, add particles
#define FLAG_DOUBLESIZE	BIT(8)			// double sized model

/*
 * Model loading flags.
 * FLAG_RENDER: Load the parts of a model needed to render it.
 * FLAG_SUBFLAGELS: Load the submodels (if any) of the model.
 * FLAG_CRASH: Crash if we can't load it.
 */
#define FLAG_RENDER		BIT(9)
#define FLAG_SUBMODELS	BIT(10)
#define FLAG_CRASH		BIT(11)


typedef struct model_s {
	char        name[MAX_QPATH];
	qboolean    loaded;			// bmodels and sprites don't cache normally
	qboolean	submodel;
	qboolean	needload;

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

//
// additional model data
//
	memzone_t	*zone;

	brushhdr_t	*brush;
	aliashdr_t	*alias;
	msprite_t	*sprite;
} model_t;

//============================================================================

void Mod_LoadTextures(lump_t *l, model_t *mod);
void Mod_LoadLighting(lump_t *l, model_t *mod);
void Mod_LoadTexinfo(lump_t *l, model_t *mod);
void Mod_LoadRFaces(lump_t *l, model_t *mod);
void Mod_MakeChains(model_t *mod);
void Mod_RUnloadBrushModel(model_t *mod);

void Mod_LoadAliasModel(model_t *mod, void *buffer, int flags);
void Mod_UnloadAliasModel(model_t *mod, qboolean keep);

void Mod_LoadSpriteModel(model_t *mod, void *buffer, int flags);
void Mod_UnloadSpriteModel(model_t *mod, qboolean keep);

void Mod_Init(void);
void Mod_ClearAll(qboolean keep);
void Mod_ReloadAll (int flags);
model_t *Mod_FindName(const char *name);
void Mod_TouchModel(const char *name);
model_t *Mod_ForName(const char *name, int flags);

void Mod_Brush_Init(void);
mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model);
Uint8 *Mod_LeafPVS(mleaf_t *leaf, model_t *model);
void Mod_LoadBrushModel(model_t *mod, void *buffer, int flags);
void Mod_UnloadBrushModel(model_t *mod, qboolean keep);

void Mod_UnloadModel(model_t *mod, qboolean keep);
model_t *Mod_LoadModel(model_t *mod, int flags);
qboolean Mod_MinsMaxs(model_t *mod, vec3_t org, vec3_t ang, vec3_t mins, vec3_t maxs);


#endif // __MODEL__
