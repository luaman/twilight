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
#include <stdlib.h>	/* for malloc() */

#include "qtypes.h"
#include "model.h"

#include "dyngl.h"
#include "gl_arrays.h"
#include "transform.h"
#include "liquid.h"
#include "gl_textures.h"

#include "cvar.h"

cvar_t *r_wateralpha;
cvar_t *r_waterripple;

// speed up sin calculations - Ed
float       turbsin[] = {
#include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))
#define TURBSIN(f, s) turbsin[((int)(((f)*(s) + r_time) * TURBSCALE) & 255)]

void
R_Init_Liquid_Cvars ()
{
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
	r_waterripple = Cvar_Get ("r_waterripple", "0", CVAR_NONE, NULL);
}

void
R_Init_Liquid ()
{
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
static void
EmitWaterPolys (glpoly_t *p, int transform)
{
	vec3_t		temp;
	Uint		i;
	float		s, t, ripple;

	ripple = r_waterripple->fvalue;

	for (; p; p = p->next)
	{
		for (i = 0; i < p->numverts; i++)
		{
			if (transform)
				softwaretransform(p->v[i].v, temp);
			else
				VectorCopy(p->v[i].v, temp);

			if (ripple)
				temp[2] += ripple * TURBSIN(temp[0], 1/32.0f) *
					TURBSIN(temp[1], 1/32.0f) * (1/64.0f);

			s = (p->tc[i].v[0] + TURBSIN(p->tc[i].v[1], 0.125)) * (1/64.0f);
			t = (p->tc[i].v[1] + TURBSIN(p->tc[i].v[0], 0.125)) * (1/64.0f);

			VectorSet2 (tc0_array_v(i), s, t);
			VectorCopy (temp, v_array_v(i));
		}
		TWI_PreVDrawCVA (0, p->numverts);
		qglDrawArrays (GL_TRIANGLE_FAN, 0, p->numverts);
		TWI_PostVDrawCVA ();
	}
}

void
R_Draw_Liquid_Chain (chain_head_t *chain, int frame, qboolean transform)
{
	Uint			 i;
	chain_item_t	*c;
	texture_t		*st = NULL;
	qboolean		 bound;

	c = chain->items;
	bound = false;
	for (i = 0; i < chain->n_items; i++) {
		if (c[i].visframe == r_framecount) {
			if (!bound) {
				bound++;
				st = R_TextureAnimation (chain->texture, frame);
				qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			}
			EmitWaterPolys (c[i].surf->polys, transform);
		}
	}
}
