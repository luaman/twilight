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

#include "quakedef.h"
#include "client.h"
#include "crc.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "gl_textures.h"
#include <math.h>

extern model_t	*loadmodel;

void	Mod_UnloadAliasModel (model_t *mod);
void	Mod_LoadAliasModel (model_t *mod, void *buffer);

void	Mod_LoadBrushModel (model_t *mod, void *buffer);
void	Mod_UnloadBrushModel (model_t *mod);

void	Mod_LoadSpriteModel (model_t *mod, void *buffer);
void	Mod_UnloadSpriteModel (model_t *mod);

void
Mod_UnloadModel (model_t *mod)
{
	if (mod->needload)
		return;

	switch (mod->type) {
		case mod_alias:
			Mod_UnloadAliasModel (mod);
			break;

		case mod_sprite:
			Mod_UnloadSpriteModel (mod);
			break;

		case mod_brush:
			Mod_UnloadBrushModel (mod);
			break;
	}
	memset(mod, 0, sizeof(model_t));
	mod->needload = true;
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned	*buf;

	if (!mod->needload) {
		return mod;					// not cached at all
	}
//
// load the file
//
	buf = (unsigned *) COM_LoadTempFile (mod->name, true);
	if (!buf) {
		if (crash)
			Host_EndGame ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;

	switch (LittleLong (*(unsigned *) buf)) {
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel (mod, buf);
			break;

		default:
			Mod_LoadBrushModel (mod, buf);
			break;
	}

	Zone_Free (buf);
	return mod;
}

vec3_t	bboxmin, bboxmax;
float	bboxradius, bboxyawradius;

qboolean
Mod_MinsMaxs (model_t *mod, vec3_t org, vec3_t ang,
		vec3_t mins, vec3_t maxs)
{
#define CheckAngle(x)   (!(!x || (x == 180.0)))
		if (CheckAngle(ang[0]) || CheckAngle(ang[2])) {
			VectorAdd (org, mod->rotatedmins, mins);
			VectorAdd (org, mod->rotatedmaxs, maxs);
			return true;
		} else if (CheckAngle(ang[2])) {
			VectorAdd (org, mod->yawmins, mins);
			VectorAdd (org, mod->yawmaxs, maxs);
			return true;
		} else {
			VectorAdd (org, mod->normalmins, mins);
			VectorAdd (org, mod->normalmaxs, maxs);
			return false;
		}
#undef CheckAngle
}
