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

extern model_t *loadmodel;

GLuint	skyboxtexnums[6];

GLuint	solidskytexture;
GLuint	alphaskytexture;
float	speedscale, speedscale2;	// for top sky and bottom sky

cvar_t *r_skyname;
cvar_t *r_fastsky;
qboolean draw_skybox = false;

static void
R_Emit_Sky_Chain (chain_head_t *chain, vec3_t origin,
		qboolean tex1, qboolean tex2)
{
	glpoly_t		*p;
	msurface_t		*s;
	chain_item_t	*c;
	Uint			 i, j;
	vec3_t			 dir;
	float			 s1, t1, s2, t2;
	float			 length;

	c = chain->items;
	for (j = 0; j < chain->n_items; j++) {
		if (c[j].visframe == vis_framecount) {
			s = c[j].surf;
			for (p = s->polys; p; p = p->next) 
			{
				memcpy(v_array_p, p->v, sizeof(vertex_t) * p->numverts);
				if (tex1 || tex2)
				{
					for (i = 0; i < p->numverts; i++) 
					{
						VectorSubtract (p->v[i].v, origin, dir);
						dir[2] *= 3;				// flatten the sphere

						length = 6 * 63 * Q_RSqrt (DotProduct(dir,dir));

						dir[0] *= length;
						dir[1] *= length;

						if (tex1)
						{
							s1 = (speedscale + dir[0]) * (1.0 / 128);
							t1 = (speedscale + dir[1]) * (1.0 / 128);
							VectorSet2(tc0_array_v(i), s1, t1);
						}

						if (tex2)
						{
							s2 = (speedscale2 + dir[0]) * (1.0 / 128);
							t2 = (speedscale2 + dir[1]) * (1.0 / 128);

							VectorSet2(tc1_array_v(i), s2, t2);
						}
					}
				}

				TWI_PreVDrawCVA (0, p->numverts);
				qglDrawArrays (GL_POLYGON, 0, p->numverts);
				TWI_PostVDrawCVA ();
			}
		}
	}
}

/*
===============
R_Draw_Old_Sky_Chain

Draws a sky chain as an old style sky, possibily using mtex.
===============
*/
void
R_Draw_Old_Sky_Chain (chain_head_t *chain, vec3_t origin)
{
	if (!chain || !chain->items)
		return;

	if (gl_mtex) {
		speedscale = r_time * 8;
		speedscale -= (int) speedscale & ~127;

		speedscale2 = r_time * 16;
		speedscale2 -= (int) speedscale2 & ~127;

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglBindTexture (GL_TEXTURE_2D, solidskytexture);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qglEnable (GL_TEXTURE_2D);
		qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

		R_Emit_Sky_Chain (chain, origin, true, true);

		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else {
		qglBindTexture (GL_TEXTURE_2D, solidskytexture);

		speedscale = r_time * 8;
		speedscale -= (int) speedscale & ~127;
		R_Emit_Sky_Chain (chain, origin, true, false);

		qglEnable (GL_BLEND);
		qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

		speedscale = r_time * 16;
		speedscale -= (int) speedscale & ~127;

		R_Emit_Sky_Chain (chain, origin, true, false);
		qglDisable (GL_BLEND);
	}
}

/*
===============
R_Draw_Fast_Sky_Chain

Draws a sky chain as a fast sky.
===============
*/
void
R_Draw_Fast_Sky_Chain (chain_head_t *chain, vec3_t origin)
{
	if (!chain || !chain->items)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglColor4fv (d_8tofloattable[(Uint8) r_fastsky->ivalue - 1]);

	R_Emit_Sky_Chain (chain, origin, false, false);

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
R_Draw_Depth_Sky_Chain (chain_head_t *chain, vec3_t origin)
{
	if (!chain || !chain->items)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ZERO, GL_ONE);

	R_Emit_Sky_Chain (chain, origin, false, false);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
}

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

/*
==================
R_LoadSkys
==================
*/
char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
qboolean
R_LoadSkys (cvar_t *cvar)
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

		qglBindTexture (GL_TEXTURE_2D, skyboxtexnums[i]);
		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, img->width, img->height,
				0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);

		free (img->pixels);
		free (img);

		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	return true;
}

/*
==============
R_SkyBoxChanged
==============
*/
void
R_SkyBoxChanged (cvar_t *cvar)
{
	if (cvar->svalue[0])
		draw_skybox = R_LoadSkys(cvar);
	else
		draw_skybox = false;
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
R_DrawSkyBox (void)
{
	if (!draw_skybox)
		return;

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

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

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglClear (GL_DEPTH_BUFFER_BIT);
}


//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void
R_InitSky (texture_t *unused, Uint8 *pixels)
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


	if (!solidskytexture)
		qglGenTextures(1, &solidskytexture);

	qglBindTexture (GL_TEXTURE_2D, solidskytexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, glt_solid_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_mag);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);


	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j];
			if (p == 0)
				memcpy(&trans[(i * 128) + j], &transpix, sizeof(trans[0]));
			else
				memcpy(&trans[(i * 128) + j], &d_palette_raw[p], sizeof(trans[0]));
		}

	if (!alphaskytexture)
		qglGenTextures(1, &alphaskytexture);

	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, glt_alpha_format, 128, 128, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, trans);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_mag);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);
}

/*
 * compatibility function to set r_skyname
 */
static void
R_LoadSky_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	Cvar_Set (r_skyname, Cmd_Argv(1));
}

void
R_Init_Sky (void)
{
	Cmd_AddCommand ("loadsky", &R_LoadSky_f);
}

void
R_Init_Sky_Cvars (void)
{
	r_skyname = Cvar_Get ("r_skyname", "", CVAR_NONE, &R_SkyBoxChanged);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, NULL);
}
