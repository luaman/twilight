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
#include "gl_textures.h"
#include "host.h"
#include "mathlib.h"
#include "mdfour.h"
#include "strlib.h"
#include "sys.h"

void
Mod_UnloadModel (model_t *mod)
{
	if (!mod->loaded)
		return;

	switch (mod->type) {
		case mod_alias:
			mod->type = mod_alias;
			if (mod->modflags & FLAG_RENDER)
				Mod_UnloadAliasModel (mod);
			break;

		case mod_sprite:
			mod->type = mod_sprite;
			if (mod->modflags & FLAG_RENDER)
				Mod_UnloadSpriteModel (mod);
			break;

		case mod_brush:
			mod->type = mod_brush;
			Mod_UnloadBrushModel (mod);
			break;
	}
	memset(mod, 0, sizeof(model_t));
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *
Mod_LoadModel (model_t *mod, int flags)
{
	void		*buf;

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
			if (flags & FLAG_RENDER)
			{
				Mod_LoadAliasModel (mod, buf, flags);
				// FIXME: This is a HACK!
				if (!strcmp (mod->name, "progs/player.mdl") ||
						!strcmp (mod->name, "progs/eyes.mdl")) {
					int crc;

					crc = CRC_Block (buf, com_filesize);

					Info_SetValueForKey (cls.userinfo,
							!strcmp (mod->name, "progs/player.mdl")
							? pmodel_name : emodel_name, va("%d", crc),
							MAX_INFO_STRING);

					if (ccls.state >= ca_connected) {
						MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
						SZ_Print (&cls.netchan.message, va("setinfo %s %d",
									!strcmp (mod->name, "progs/player.mdl") ?
									pmodel_name : emodel_name, crc));
					}
				}
			}
			break;

		case IDSPRITEHEADER:
			if (flags & FLAG_RENDER)
				Mod_LoadSpriteModel (mod, buf, flags);
			break;

		default:
			Mod_LoadBrushModel (mod, buf, flags);
			break;
	}

	mod->modflags |= flags;

	Zone_Free (buf);
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
