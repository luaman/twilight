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

#include <string.h>

#include "qtypes.h"
#include "dlight.h"
#include "renderer/gl_light.h"

dlight_t *
CCL_AllocDlight (int key)
{
	int i = MAX_DLIGHTS;
	dlight_t *dl = NULL;

	// first look for an exact key match
	if (key)
		for (i = 0, dl = ccl.dlights; i < MAX_DLIGHTS; i++, dl++)
			if (dl->key == key)
				break;

	// look for anything else
	if (i >= MAX_DLIGHTS)
	{
		for (i = 0, dl = ccl.dlights; i < MAX_DLIGHTS; i++, dl++)
			if (dl->die < ccl.time || dl->radius <= 0.0f)
				break;

		// nothing available; fake it
		dl = &ccl.dlights[0];
	}

    memset (dl, 0, sizeof (*dl));
    dl->key = key;
    dl->color[0] = 0.2f;
    dl->color[1] = 0.1f;
    dl->color[2] = 0.0f;
    return dl;
}

void
CCL_DecayLights (void)
{
	int i;
	dlight_t *dl;

	for (i = 0, dl = ccl.dlights;i < MAX_DLIGHTS;i++, dl++)
		if (dl->radius)
			dl->radius = (ccl.time < dl->die) ?
				max(0, dl->radius - ccl.frametime * dl->decay) : 0;
}


// FIXME: Make go away
void
CCL_NewDlight (int key, vec3_t org, int effects)
{
	dlight_t *dl = CCL_AllocDlight (key);

	dl->radius = 1.0f;
	dl->die = ccl.time + 0.1f;
	VectorCopy (org, dl->origin);

	VectorClear (dl->color);

	if (effects & EF_BRIGHTLIGHT) {
		dl->color[0] += 400.0f;
		dl->color[1] += 400.0f;
		dl->color[2] += 400.0f;
	}
	if (effects & EF_DIMLIGHT) {
		dl->color[0] += 200.0f;
		dl->color[1] += 200.0f;
		dl->color[2] += 200.0f;
	}

	// QW uses DIMLIGHT _and_ RED/BLUE - looks bad. *sigh*
	if (effects & (EF_RED|EF_BLUE)) {
		VectorSet (dl->color, 20.0f, 20.0f, 20.0f);
		if (effects & EF_RED)
			dl->color[0] += 180.0f;
		if (effects & EF_BLUE)
			dl->color[2] += 180.0f;
	}
}
