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
#include <string.h>

#include "model.h"
#include "qtypes.h"

#include "cclient.h"
#include "dyngl.h"
#include "gl_arrays.h"
#include "liquid.h"
#include "textures.h"
#include "vis.h"

#include "cvar.h"

cvar_t *r_wateralpha;

void
R_Liquid_Init_Cvars ()
{
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
}

void
R_Draw_Liquid_Chain (model_t *mod, chain_head_t *chain, qboolean arranged)
{
	Uint		 i;
	brushhdr_t	*brush = mod->brush;
	glpoly_t	*p;

	qglBindTexture (GL_TEXTURE_2D, chain->texture->gl_texturenum);
	qglMatrixMode (GL_TEXTURE);
	qglPushMatrix ();
	qglTranslatef (Q_sin(ccl.time) * 0.4f, Q_cos(ccl.time) * 0.06f, 0);

	if (!arranged)
		TWI_ChangeVDrawArraysALL (brush->numsets, 1, brush->verts, &brush->vbo[VBO_VERTS],
				brush->tcoords[0], &brush->vbo[VBO_TC0],
				brush->tcoords[1], &brush->vbo[VBO_TC1]);

	for (i = 0; i < chain->n_items; i++) {
		if (chain->items[i].visframe == vis_framecount) {
			for (p = chain->items[i].surf->polys; p; p = p->next)
				qglDrawArrays (GL_TRIANGLE_FAN, p->start, p->numverts);
		}
	}

	if (!arranged)
		TWI_ChangeVDrawArraysALL (brush->numsets, 0, NULL, NULL, NULL, NULL, NULL, NULL);

	qglPopMatrix ();
	qglMatrixMode (GL_MODELVIEW);
}
