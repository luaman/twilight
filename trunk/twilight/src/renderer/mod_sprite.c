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

#include "crc.h"
#include "cvar.h"
#include "gl_info.h"
#include "host.h"
#include "mathlib.h"
#include "mod_sprite.h"
#include "quakedef.h"
#include "strlib.h"
#include "sys.h"
#include "textures.h"

extern vec3_t	 bboxmin, bboxmax;
extern float	 bboxradius, bboxyawradius;

	
//=============================================================================

static void *
Mod_LoadSpriteFrame (void *pin, model_t *mod, mspriteframe_t **ppframe,
		int framenum)
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	int				width, height, size, origin[2], i;

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Zone_Alloc (mod->zone, sizeof (mspriteframe_t));

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = origin[0] + width;

	i = max(sq(pspriteframe->left), sq(pspriteframe->right));
	i += max(sq(pspriteframe->up), sq(pspriteframe->down));
	if (bboxradius < i)
		bboxradius = i;

	pspriteframe->gl_texturenum =
		GLT_Load_Raw (va("%s_%i", mod->name, framenum), width, height,
				(Uint8 *) (pinframe + 1), d_palette_raw,
				TEX_MIPMAP|TEX_ALPHA, 8);

	return (void *) ((Uint8 *) pinframe + sizeof (dspriteframe_t) + size);
}


static void *
Mod_LoadSpriteGroup (void *pin, model_t *mod, mspriteframe_t **ppframe,
		int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Zone_Alloc (mod->zone, sizeof (mspritegroup_t)
			+ (numframes - 1) * sizeof (pspritegroup->frames[0]));

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Zone_Alloc(mod->zone, numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_EndGame ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		ptemp =
			Mod_LoadSpriteFrame (ptemp, mod, &pspritegroup->frames[i],
								 framenum * 100 + i);
	}

	return ptemp;
}


void
Mod_LoadSpriteModel (model_t *mod, void *buffer, int flags)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	flags = flags;

	pin = (dsprite_t *) buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Host_EndGame ("%s has wrong version number "
				   "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	mod->zone = Zone_AllocZone(mod->name);
	mod->sprite = psprite = Zone_Alloc(mod->zone, size);

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	bboxradius = 0;

//
// load the frames
//
	if (numframes < 1)
		Host_EndGame ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *) (pin + 1);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteFrame (pframetype + 1, mod,
									 &psprite->frames[i].frameptr, i);
		} else {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteGroup (pframetype + 1, mod,
									 &psprite->frames[i].frameptr, i);
		}
	}

	VectorSet (mod->normalmins, -bboxradius, -bboxradius, -bboxradius);
	VectorCopy (mod->normalmins, mod->rotatedmins);
	VectorCopy (mod->normalmins, mod->yawmins);
	VectorSet (mod->normalmaxs, bboxradius, bboxradius, bboxradius);
	VectorCopy (mod->normalmins, mod->rotatedmaxs);
	VectorCopy (mod->normalmins, mod->yawmaxs);

	mod->type = mod_sprite;
}

void
Mod_UnloadSpriteModel (model_t *mod, qboolean keep)
{
	int				 i, j;
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;

	keep = keep;

	psprite = mod->sprite;
	for (i = 0; i < psprite->numframes; i++) {
		if (psprite->frames[i].type == SPR_SINGLE) {
			GLT_Delete (psprite->frames[i].frameptr->gl_texturenum);
		} else {
			pspritegroup = (mspritegroup_t *) psprite->frames[i].frameptr;
			for (j = 0; j < pspritegroup->numframes; j++) {
				GLT_Delete (pspritegroup->frames[j]->gl_texturenum);
			}
		}
	}

	Zone_FreeZone (&mod->zone);
}
