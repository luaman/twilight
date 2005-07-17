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
#include <math.h>

#include "quakedef.h"
#include "client.h"
#include "crc.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "renderer/textures.h"

void
Mod_UnloadModel (model_t *mod, qboolean keep)
{
	if (!mod->loaded)
		return;

	switch (mod->type) {
		case mod_alias:
			if (mod->modflags & FLAG_RENDER)
				Mod_UnloadAliasModel (mod, keep);
			break;

		case mod_sprite:
			if (mod->modflags & FLAG_RENDER)
				Mod_UnloadSpriteModel (mod, keep);
			break;

		case mod_brush:
			Mod_UnloadBrushModel (mod, keep);
			break;
	}

	Com_DPrintf ("Unloaded model: %s\n", mod->name);

	if (keep) {
		char		name[MAX_QPATH];
		qboolean	submodel;
		int			flags;

		strcpy (name, mod->name);
		submodel = mod->submodel;
		flags = mod->modflags;

		memset(mod, 0, sizeof(model_t));

		mod->submodel = submodel;
		strcpy (mod->name, name);
		if (!submodel) {
			mod->needload = true;
			mod->modflags = flags;
		}
	} else {
		memset(mod, 0, sizeof(model_t));
	}
}

/*
==================
Loads a model into the cache
==================
*/
model_t *
Mod_LoadModel (model_t *mod, int flags)
{
	unsigned	*buf;

	if (mod->loaded) {
		return mod;					// not cached at all
	}
//
// load the file
//
	buf = (unsigned *) COM_LoadTempFile (mod->name, true);
	if (!buf) {
		if (flags & FLAG_CRASH)
			Host_EndGame ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

//
// fill it in
//

// call the apropriate loader
	mod->loaded = true;

	switch (LittleLong (*(unsigned *) buf)) {
		case IDPOLYHEADER:
			mod->type = mod_alias;
			if (flags & FLAG_RENDER)
				Mod_LoadAliasModel (mod, buf, flags);
			break;

		case IDSPRITEHEADER:
			mod->type = mod_sprite;
			if (flags & FLAG_RENDER)
				Mod_LoadSpriteModel (mod, buf, flags);
			break;

		default:
			mod->type = mod_brush;
			Mod_LoadBrushModel (mod, buf, flags);
			break;
	}

	mod->modflags |= flags;

	Zone_Free (buf);
	Com_DPrintf ("Loaded model: %s\n", mod->name);
	return mod;
}

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
