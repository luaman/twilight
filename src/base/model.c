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

#include "sys.h"
#include "common.h"
#include "mathlib.h"
#include "strlib.h"
#include "model.h"

void Mod_Brush_Init (void);

#define	MAX_MOD_KNOWN 4096
static model_t     mod_known[MAX_MOD_KNOWN];
static int         mod_numknown;


void
Mod_Init (void)
{
	Mod_Brush_Init ();
}

void
Mod_ClearAll (qboolean keep)
{
	int         i;
	model_t    *mod;

	for (i = 0, mod = mod_known; i < MAX_MOD_KNOWN; i++, mod++)
		if (mod->loaded && mod->name[0])
			Mod_UnloadModel (mod, keep);
}

void
Mod_ReloadAll (int flags)
{
	int         i;
	model_t    *mod;

	for (i = 0, mod = mod_known; i < MAX_MOD_KNOWN; i++, mod++)
		if (mod->name[0] && !mod->loaded && !mod->submodel && mod->needload)
			Mod_LoadModel (mod, flags | mod->modflags);
}

model_t *
Mod_FindName (const char *name)
{
	int			i;
	model_t		*mod, *freemod;

	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");

	/*
	 * search the currently loaded models
	 */
	freemod = NULL;
	for (i = 0, mod = mod_known; i < MAX_MOD_KNOWN; i++, mod++)
	{
		if (mod->name[0])
		{
			if (!strcmp (mod->name, name))
				return mod;
		}
		else if (freemod == NULL)
			freemod = mod;
	}

	if (freemod)
	{
		mod_numknown++;
		mod = freemod;
		strlcpy_s (mod->name, name);
		return mod;
	}

	Sys_Error ("Mod_FindName: ran out of models\n");
	return NULL;
}

void
Mod_TouchModel (const char *name)
{
	model_t    *mod;

	mod = Mod_FindName (name);
}


/*
==================
Loads in a model for the given name
==================
*/
model_t    *
Mod_ForName (const char *name, int flags)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, flags);
}
