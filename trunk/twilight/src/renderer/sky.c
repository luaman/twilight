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

extern model_t *loadmodel;

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
					memcpy(v_array_v(0), B_Vert_v(brush, p->start),
							sizeof(vertex_t) * p->numverts);

					TWI_PreVDrawCVA (0, p->numverts);
					qglDrawArrays (GL_POLYGON, 0, p->numverts);
					TWI_PostVDrawCVA ();
				} else {
					qglDrawArrays (GL_POLYGON, p->start, p->numverts);
				}
			}
		}
	}
}

/*
===============
R_Draw_Fast_Sky_Chain

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
R_Draw_Depth_Sky_Chain

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
	qglPushMatrix ();
	qglTranslatef(r_origin[0], r_origin[1], r_origin[2]);

	qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), Sky_Sphere_Verts);
	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), Sky_Sphere_Texcoords);

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
		qglClientActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), Sky_Sphere_Texcoords);
		qglClientActiveTextureARB (GL_TEXTURE0_ARB);

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

	GLArrays_Reset_TC ();
	GLArrays_Reset_Vertex ();

	qglPopMatrix ();
	qglDepthMask (GL_TRUE);
}

/*
==================
R_LoadSkys
==================
*/
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
		img = Image_Load (name);
		if (!img)
		{
			snprintf (name, sizeof (name), "gfx/env/%s%s",
					cvar->svalue, suf[i]);
			img = Image_Load (name);
		}

		if (!img)
			return false;
		if ((img->width != 256) || (img->height != 256))
		{
			free (img->pixels);
			free (img);
			return false;
		}

		skyboxtexnums[i] = GLT_Load_image(va("%s#%d", name, i), img, NULL, 0);
	}

	return true;
}

/*
==============
R_SkyChanged
==============
*/
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

#define SKYBOXVERT(i, x, y, z, s, t)							\
	((v_array(i, 0) = (x) * 1024.0f + r_origin[0]),				\
	(v_array(i, 1) = (y) * 1024.0f + r_origin[1]),				\
	(v_array(i, 2) = (z) * 1024.0f + r_origin[2]),				\
	(tc_array(i, 0) = (s) * (254.0f/256.0f) + (1.0f/256.0f)),	\
	(tc_array(i, 1) = (t) * (254.0f/256.0f) + (1.0f/256.0f)))

/*
==============
R_DrawSkyBox
==============
*/
void
Sky_Box_Draw (void)
{
	qglDepthMask (GL_FALSE);
	// Brute force method

	// right
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[0]);
	SKYBOXVERT (0,  1,  1,  1, 1, 0);
	SKYBOXVERT (1,  1,  1, -1, 1, 1);
	SKYBOXVERT (2, -1,  1, -1, 0, 1);
	SKYBOXVERT (3, -1,  1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// back
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[1]);
	SKYBOXVERT (0, -1,  1,  1, 1, 0);
	SKYBOXVERT (1, -1,  1, -1, 1, 1);
	SKYBOXVERT (2, -1, -1, -1, 0, 1);
	SKYBOXVERT (3, -1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// left
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[2]);
	SKYBOXVERT (0, -1, -1,  1, 1, 0);
	SKYBOXVERT (1, -1, -1, -1, 1, 1);
	SKYBOXVERT (2,  1, -1, -1, 0, 1);
	SKYBOXVERT (3,  1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// front
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[3]);
	SKYBOXVERT (0,  1, -1,  1, 1, 0);
	SKYBOXVERT (1,  1, -1, -1, 1, 1);
	SKYBOXVERT (2,  1,  1, -1, 0, 1);
	SKYBOXVERT (3,  1,  1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// up
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[4]);
	SKYBOXVERT (0,  1, -1,  1, 1, 0);
	SKYBOXVERT (1,  1,  1,  1, 1, 1);
	SKYBOXVERT (2, -1,  1,  1, 0, 1);
	SKYBOXVERT (3, -1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// down
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[5]);
	SKYBOXVERT (0,  1,  1, -1, 1, 0);
	SKYBOXVERT (1,  1, -1, -1, 1, 1);
	SKYBOXVERT (2, -1, -1, -1, 0, 1);
	SKYBOXVERT (3, -1,  1, -1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	qglDepthMask (GL_TRUE);
}


//===============================================================

/*
=============
R_InitSky

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
}

void
Sky_Init_Cvars (void)
{
	r_skyname = Cvar_Get ("r_skyname", "", CVAR_NONE, &Sky_Changed);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, &Sky_Changed);
}
