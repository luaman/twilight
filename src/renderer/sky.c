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

#include "qtypes.h"
#include "model.h"

#include "dyngl.h"
#include "gl_arrays.h"
#include "gl_textures.h"
#include "sky.h"
#include "image.h"
#include "vis.h"

#include "cvar.h"
#include "cmd.h"
#include "strlib.h"

static GLuint	skyboxtexnums[6];

static GLuint	solidskytexture;
static GLuint	alphaskytexture;

cvar_t *r_skyname;
static cvar_t *r_fastsky;
sky_type_t	sky_type = SKY_SPHERE;

static void
Sky_Emit_Chain (model_t *mod, chain_head_t *chain, qboolean arranged)
{
	glpoly_t		*p;
	msurface_t		*s;
	chain_item_t	*c;
	brushhdr_t		*brush = mod->brush;
	Uint			 j;

	c = chain->items;
	for (j = 0; j < chain->n_items; j++) {
		if (c[j].visframe == vis_framecount) {
			s = c[j].surf;
			for (p = s->polys; p; p = p->next) 
			{
				if (!arranged) {
					TWI_ChangeVDrawArrays (p->numverts, 0,
							B_Vert_r(brush, p->start), NULL, NULL, NULL, NULL);

					qglDrawArrays (GL_POLYGON, 0, p->numverts);
					TWI_ChangeVDrawArrays (p->numverts, 0,
							NULL, NULL, NULL, NULL, NULL);
				} else {
					qglDrawArrays (GL_POLYGON, p->start, p->numverts);
				}
			}
		}
	}
}

/*
===============
Draws a sky chain as a fast sky.
===============
*/
void
Sky_Fast_Draw_Chain (model_t *mod, chain_head_t *chain)
{
	if (!chain || !chain->items)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglColor4fv (d_8tofloattable[(Uint8) r_fastsky->ivalue - 1]);

	Sky_Emit_Chain (mod, chain, true);

	qglColor4fv (whitev);
	qglEnable (GL_TEXTURE_2D);
}

/*
===============
Draws a sky chain only in the depth buffer.
===============
*/
void
Sky_Depth_Draw_Chain (model_t *mod, chain_head_t *chain)
{
	if (!chain || !chain->items)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ZERO, GL_ONE);

	Sky_Emit_Chain (mod, chain, false);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
}

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

#define Sky_Sphere_Grid			32
#define Sky_Sphere_Grid1		(Sky_Sphere_Grid + 1)
#define Sky_Sphere_GridRecip	(1.0f / Sky_Sphere_Grid)
#define Sky_Sphere_Numverts		(Sky_Sphere_Grid1 * Sky_Sphere_Grid1)
#define Sky_Sphere_Numele		(Sky_Sphere_Grid * Sky_Sphere_Grid * 2 * 3)

static vertex_t Sky_Sphere_Verts[Sky_Sphere_Numverts];
static texcoord_t Sky_Sphere_Texcoords[Sky_Sphere_Numverts];
static Uint Sky_Sphere_Elements[Sky_Sphere_Numele];

static void
Sky_Sphere_Calc (void)
{
	int x, y, i;
	float a, b, c, ax, ay, length;
	vec3_t v;

	for (y = 0, i = 0; y <= Sky_Sphere_Grid; y++)
	{
		a = y * Sky_Sphere_GridRecip;
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (x = 0; x <= Sky_Sphere_Grid; x++, i++)
		{
			b = x * Sky_Sphere_GridRecip;
			c = cos((b + 0.5) * M_PI);
			v[0] = ax*c * 16;
			v[1] = ay*c * 16;
			v[2] = -sin((b + 0.5) * M_PI) * (5);
			length = 3.0f / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));
			Sky_Sphere_Texcoords[i].v[0] = v[0] * length;
			Sky_Sphere_Texcoords[i].v[1] = v[1] * length;
			Sky_Sphere_Verts[i].v[0] = v[0];
			Sky_Sphere_Verts[i].v[1] = v[1];
			Sky_Sphere_Verts[i].v[2] = v[2];
		}
	}
	for (y = 0, i = 0; y < Sky_Sphere_Grid; y++)
	{
		for (x = 0; x < Sky_Sphere_Grid; x++)
		{
			Sky_Sphere_Elements[i++] =  y      * Sky_Sphere_Grid1 + x;
			Sky_Sphere_Elements[i++] =  y      * Sky_Sphere_Grid1 + x + 1;
			Sky_Sphere_Elements[i++] = (y + 1) * Sky_Sphere_Grid1 + x;
			Sky_Sphere_Elements[i++] =  y      * Sky_Sphere_Grid1 + x + 1;
			Sky_Sphere_Elements[i++] = (y + 1) * Sky_Sphere_Grid1 + x + 1;
			Sky_Sphere_Elements[i++] = (y + 1) * Sky_Sphere_Grid1 + x;
		}
	}
}

void
Sky_Sphere_Draw (void)
{
	float speedscale;

	speedscale = r_time * (8.0 / 128.0);
	speedscale -= floor(speedscale);

	qglDepthMask (GL_FALSE);
	qglDepthFunc (GL_GREATER);
	qglDepthRange (1, 1);

	qglPushMatrix ();
	qglTranslatef(r_origin[0], r_origin[1], r_origin[2]);

	TWI_ChangeVDrawArrays (Sky_Sphere_Numverts, 1, Sky_Sphere_Verts,
			Sky_Sphere_Texcoords, Sky_Sphere_Texcoords, NULL, NULL);

	qglMatrixMode (GL_TEXTURE);
	qglPushMatrix ();
	qglTranslatef (speedscale, speedscale, 0);

	qglBindTexture (GL_TEXTURE_2D, solidskytexture);

	if (!gl_mtex) {
		qglDrawElements (GL_TRIANGLES, Sky_Sphere_Numele, GL_UNSIGNED_INT,
				Sky_Sphere_Elements);

		qglEnable (GL_BLEND);

		qglTranslatef (speedscale, speedscale, 0);
		qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

		qglDrawElements (GL_TRIANGLES, Sky_Sphere_Numele, GL_UNSIGNED_INT,
				Sky_Sphere_Elements);

		qglDisable (GL_BLEND);
	} else {
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglPushMatrix ();

		speedscale *= 2;
		qglTranslatef (speedscale, speedscale, 0);
		qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qglEnable (GL_TEXTURE_2D);

		qglDrawElements (GL_TRIANGLES, Sky_Sphere_Numele, GL_UNSIGNED_INT,
				Sky_Sphere_Elements);

		qglDisable (GL_TEXTURE_2D);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		qglPopMatrix ();
		qglActiveTextureARB (GL_TEXTURE0_ARB);
	}

	qglPopMatrix ();
	qglMatrixMode (GL_MODELVIEW);

	TWI_ChangeVDrawArrays(Sky_Sphere_Numverts, 0, NULL, NULL, NULL, NULL, NULL);

	qglPopMatrix ();
	qglDepthRange (0, 1);
	qglDepthFunc (GL_LEQUAL);
	qglDepthMask (GL_TRUE);
}

static char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
static qboolean
Sky_LoadSkys (cvar_t *cvar)
{
	int			i;
	char		name[64];
	image_t	   *img;

	for (i = 0; i < 6; i++) {
		snprintf (name, sizeof (name), "env/%s%s",
				cvar->svalue, suf[i]);
		img = Image_Load (name, TEX_UPLOAD);
		if (!img)
		{
			snprintf (name, sizeof (name), "gfx/env/%s%s",
					cvar->svalue, suf[i]);
			img = Image_Load (name, TEX_UPLOAD);
		}

		if (!img)
			return false;

		skyboxtexnums[i] = img->texnum;
	}

	return true;
}

static void
Sky_Changed (cvar_t *unused)
{
	unused = unused;
	if (!r_skyname || !r_fastsky)
		return;

	if (r_fastsky->ivalue)
		sky_type = SKY_FAST;
	else if (r_skyname->svalue[0] && Sky_LoadSkys(r_skyname))
		sky_type = SKY_BOX;
	else
		sky_type = SKY_SPHERE;
}

#define SKYBOXVERT(i, x, y, z, s, t)						(	\
	(Sky_Box_Verts[i].v[0] = (x) * 1024.0f),				\
	(Sky_Box_Verts[i].v[1] = (y) * 1024.0f),				\
	(Sky_Box_Verts[i].v[2] = (z) * 1024.0f),				\
	(Sky_Box_Texcoords[i].v[0] = (s) * (254.0f/256.0f) + (1.0f/256.0f)),	\
	(Sky_Box_Texcoords[i].v[1] = (t) * (254.0f/256.0f) + (1.0f/256.0f)))

#define Sky_Box_Numverts	(4 * 6)

static vertex_t Sky_Box_Verts[Sky_Box_Numverts];
static texcoord_t Sky_Box_Texcoords[Sky_Box_Numverts];

void
Sky_Box_Calc (void)
{
	// right
	SKYBOXVERT (0,  1,  1,  1, 1, 0);
	SKYBOXVERT (1,  1,  1, -1, 1, 1);
	SKYBOXVERT (2, -1,  1, -1, 0, 1);
	SKYBOXVERT (3, -1,  1,  1, 0, 0);

	// back
	SKYBOXVERT (4, -1,  1,  1, 1, 0);
	SKYBOXVERT (5, -1,  1, -1, 1, 1);
	SKYBOXVERT (6, -1, -1, -1, 0, 1);
	SKYBOXVERT (7, -1, -1,  1, 0, 0);

	// left
	SKYBOXVERT (8, -1, -1,  1, 1, 0);
	SKYBOXVERT (9, -1, -1, -1, 1, 1);
	SKYBOXVERT (10,  1, -1, -1, 0, 1);
	SKYBOXVERT (11,  1, -1,  1, 0, 0);

	// front
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[3]);
	SKYBOXVERT (12,  1, -1,  1, 1, 0);
	SKYBOXVERT (13,  1, -1, -1, 1, 1);
	SKYBOXVERT (14,  1,  1, -1, 0, 1);
	SKYBOXVERT (15,  1,  1,  1, 0, 0);

	// up
	SKYBOXVERT (16,  1, -1,  1, 1, 0);
	SKYBOXVERT (17,  1,  1,  1, 1, 1);
	SKYBOXVERT (18, -1,  1,  1, 0, 1);
	SKYBOXVERT (19, -1, -1,  1, 0, 0);

	// down
	SKYBOXVERT (20,  1,  1, -1, 1, 0);
	SKYBOXVERT (21,  1, -1, -1, 1, 1);
	SKYBOXVERT (22, -1, -1, -1, 0, 1);
	SKYBOXVERT (23, -1,  1, -1, 0, 0);
}

void
Sky_Box_Draw (void)
{
	TWI_ChangeVDrawArrays (Sky_Box_Numverts, 1, Sky_Box_Verts,
			Sky_Box_Texcoords, NULL, NULL, NULL);

	// Brute force method
	qglDepthMask (GL_FALSE);
	qglDepthFunc (GL_GREATER);
	qglDepthRange (1, 1);
	qglPushMatrix ();
	qglTranslatef(r_origin[0], r_origin[1], r_origin[2]);

	// right
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[0]);
	qglDrawArrays (GL_QUADS, 0 * 4, 4);

	// back
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[1]);
	qglDrawArrays (GL_QUADS, 1 * 4, 4);

	// left
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[2]);
	qglDrawArrays (GL_QUADS, 2 * 4, 4);

	// front
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[3]);
	qglDrawArrays (GL_QUADS, 3 * 4, 4);

	// up
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[4]);
	qglDrawArrays (GL_QUADS, 4 * 4, 4);

	// down
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[5]);
	qglDrawArrays (GL_QUADS, 5 * 4, 4);

	qglPopMatrix ();
	qglDepthRange (0, 1);
	qglDepthFunc (GL_LEQUAL);
	qglDepthMask (GL_TRUE);

	TWI_ChangeVDrawArrays (Sky_Box_Numverts, 0, NULL, NULL, NULL, NULL, NULL);
}


//===============================================================

/*
=============
A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void
Sky_InitSky (texture_t *unused, Uint8 *pixels)
{
	int			i, j, p;
	Uint8		*src;
	unsigned	trans[128 * 128];
	int			r, g, b;
	Uint8		rgba[4], transpix[4];

	unused = unused;

	src = pixels;

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j + 128];
			memcpy(rgba, &d_palette_raw[p], sizeof(rgba));
			memcpy(&trans[(i * 128) + j], rgba, sizeof(trans[0]));
			r += ((Uint8 *) rgba)[0];
			g += ((Uint8 *) rgba)[1];
			b += ((Uint8 *) rgba)[2];
		}

	transpix[0] = r / (128 * 128);
	transpix[1] = g / (128 * 128);
	transpix[2] = b / (128 * 128);
	transpix[3] = 0;


	solidskytexture = GLT_Load_Raw ("Sky Solid", 128, 128, (Uint8 *) trans, NULL, TEX_REPLACE, 32);

	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j];
			if (p == 0)
				memcpy(&trans[(i * 128) + j], &transpix, sizeof(trans[0]));
			else
				memcpy(&trans[(i * 128) + j], &d_palette_raw[p], sizeof(trans[0]));
		}

	alphaskytexture = GLT_Load_Raw ("Sky Alpha", 128, 128, (Uint8 *) trans, NULL, TEX_REPLACE | TEX_ALPHA, 32);
}

/*
 * compatibility function to set r_skyname
 */
static void
Sky_LoadSky_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	Cvar_Set (r_skyname, Cmd_Argv(1));
}

void
Sky_Init (void)
{
	Cmd_AddCommand ("loadsky", &Sky_LoadSky_f);
	Sky_Sphere_Calc ();
	Sky_Box_Calc ();
}

void
Sky_Init_Cvars (void)
{
	r_skyname = Cvar_Get ("r_skyname", "", CVAR_NONE, &Sky_Changed);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, &Sky_Changed);
}
