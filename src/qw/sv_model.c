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

#include <stdio.h>

#include "quakedef.h"
#include "bspfile.h"
#include "common.h"
#include "mathlib.h"
#include "mdfour.h"
#include "model.h"
#include "strlib.h"
#include "zone.h"
#include "server.h"
#include <math.h>

void Mod_UnloadBrushModel (model_t *mod);

Uint8       mod_novis[MAX_MAP_LEAFS / 8];

unsigned   *model_checksum;

void
Mod_UnloadModel (model_t *mod)
{
	if (!mod->loaded)
		return;

	Mod_UnloadBrushModel (mod);
	memset(mod, 0, sizeof(model_t));
}

/*
==================
Loads a model into the cache
==================
*/
model_t    *
Mod_LoadModel (model_t *mod, int flags)
{
	unsigned   *buf;

	if (mod->loaded) {
		return mod;					// not cached at all
	}
//
// load the file
//
	buf = (unsigned *) COM_LoadTempFile (mod->name, true);
	if (!buf) {
		if (flags & FLAG_CRASH)
			SV_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

//
// fill it in
//

// call the apropriate loader
	mod->loaded = true;

	Mod_LoadBrushModel (mod, buf, flags);

	mod->type = mod_brush;

	mod->modflags |= flags;

	Zone_Free (buf);

	return mod;
}


void Mod_RUnloadBrushModel (model_t *mod)
{
	mod = mod;
}

void Mod_LoadTextures (lump_t *l, model_t *mod)
{
	l = l;
	mod = mod;
}

void Mod_LoadLighting (lump_t *l, model_t *mod)
{
	l = l;
	mod = mod;
}

void Mod_LoadTexinfo (lump_t *l, model_t *mod)
{
	l = l;
	mod = mod;
}

void Mod_LoadRFaces (lump_t *l, model_t *mod)
{
	l = l;
	mod = mod;
}

void Mod_MakeChains (model_t *mod)
{
	mod = mod;
}
