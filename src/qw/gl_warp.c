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

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "image.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"

extern model_t *loadmodel;

int         skytexturenum;
int			skyboxtexnum;

int         solidskytexture;
int         alphaskytexture;
float		speedscale, speedscale2;	// for top sky and bottom sky

msurface_t *warpface;

extern cvar_t *gl_subdivide_size;

#define	MAX_CLIP_VERTS	64

cvar_t *r_skyname;
cvar_t *r_fastsky;
static qboolean draw_skybox = false;

void
BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int         i, j;
	float      *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void
SubdividePolygon (int numverts, float *verts)
{
	int         i, j, k;
	vec3_t      mins, maxs;
	float       m;
	float      *v;
	vec3_t      front[64], back[64];
	int         f, b;
	float       dist[64];
	float       frac;
	glpoly_t   *poly;

	if (!numverts)
		Sys_Error ("numverts = 0!");
	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size->ivalue
			* Q_floor (m / gl_subdivide_size->ivalue + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof (glpoly_t)); 
	poly->tc = Hunk_Alloc (numverts * sizeof (texcoord_t));
	poly->v = Hunk_Alloc (numverts * sizeof (vertex_t));

	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy (verts, poly->v[i].v);
		poly->tc[i].v[0] = DotProduct (verts, warpface->texinfo->vecs[0]);
		poly->tc[i].v[1] = DotProduct (verts, warpface->texinfo->vecs[1]);
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void
GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t      verts[64];
	int         numverts;
	int         i;
	int         lindex;
	float      *vec;

	warpface = fa;

	// 
	// convert edges back to a normal polygon
	// 
	numverts = 0;
	for (i = 0; i < fa->numedges; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float       turbsin[] = {
#include "gl_warp_sin.h"
};

#define TURBSCALE (256.0 / (2 * M_PI))
#define TURBSIN(f, s) turbsin[((int)(((f)*(s) + cl.time) * TURBSCALE) & 255)]

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void
EmitWaterPolys (msurface_t *fa, texture_t *tex, int transform, float alpha)
{
	glpoly_t   *p;
	float		temp[3];
	int			i;
	float		s, t, ripple;

	ripple = r_waterripple->fvalue;

	qglColor4f (1.0f, 1.0f, 1.0f, alpha);

	for (p = fa->polys; p; p = p->next)
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

/*
=============
EmitSkyPolys
=============
*/
static void
EmitSkyPolys (msurface_t *fa)
{
	glpoly_t   *p;
	int         i;
	float       s, t;
	vec3_t      dir;
	float       length;

	for (p = fa->polys; p; p = p->next) {
		memcpy(v_array_p, p->v, sizeof(vertex_t) * p->numverts);
		for (i = 0; i < p->numverts; i++) {
			VectorSubtract (p->v[i].v, r_origin, dir);
			dir[2] *= 3;				// flatten the sphere

			length = 6 * 63 * Q_RSqrt (DotProduct(dir,dir));

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0 / 128);
			t = (speedscale + dir[1]) * (1.0 / 128);

			VectorSet2(tc0_array_v(i), s, t);
		}
		TWI_PreVDrawCVA (0, p->numverts);
		qglDrawArrays (GL_POLYGON, 0, p->numverts);
		TWI_PostVDrawCVA ();
	}
}

/*
=============
EmitSkyPolysMTEX
=============
*/
static void
EmitSkyPolysMTEX (msurface_t *fa)
{
	glpoly_t   *p;
	int         i;
	float       s1, t1, s2, t2;
	vec3_t      dir;
	float       length;

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	qglActiveTextureARB (GL_TEXTURE0_ARB);
	qglBindTexture (GL_TEXTURE_2D, solidskytexture);
	qglActiveTextureARB (GL_TEXTURE1_ARB);
	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	qglEnable (GL_TEXTURE_2D);
	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

	for (p = fa->polys; p; p = p->next) 
	{
		memcpy(v_array_p, p->v, sizeof(vertex_t) * p->numverts);
		for (i = 0; i < p->numverts; i++) 
		{
			VectorSubtract (p->v[i].v, r_origin, dir);
			dir[2] *= 3;				// flatten the sphere

			length = 6 * 63 * Q_RSqrt (DotProduct(dir,dir));

			dir[0] *= length;
			dir[1] *= length;

			s1 = (speedscale + dir[0]) * (1.0 / 128);
			t1 = (speedscale + dir[1]) * (1.0 / 128);

			s2 = (speedscale2 + dir[0]) * (1.0 / 128);
			t2 = (speedscale2 + dir[1]) * (1.0 / 128);

			VectorSet2(tc0_array_v(i), s1, t1);
			VectorSet2(tc1_array_v(i), s2, t2);
		}

		TWI_PreVDrawCVA (0, p->numverts);
		qglDrawArrays (GL_POLYGON, 0, p->numverts);
		TWI_PostVDrawCVA ();
	}

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglDisable (GL_TEXTURE_2D);
	qglActiveTextureARB (GL_TEXTURE0_ARB);
}


/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void
EmitBothSkyLayers (msurface_t *fa)
{
	if (draw_skybox)
		return;

	qglBindTexture (GL_TEXTURE_2D, solidskytexture);

	speedscale = cl.time * 8;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);

	qglEnable (GL_BLEND);
	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);

	speedscale = cl.time * 16;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);
	qglDisable (GL_BLEND);
}

void
EmitBothSkyLayersMTEX (msurface_t *fa)
{
	if (draw_skybox)
		return;

	speedscale = cl.time * 8;
	speedscale -= (int) speedscale & ~127;

	speedscale2 = cl.time * 16;
	speedscale2 -= (int) speedscale2 & ~127;

	EmitSkyPolysMTEX (fa);
}

static void
R_DrawSkyBoxChain (msurface_t *s)
{
	glpoly_t	*p;
	msurface_t	*fa;

	if (!draw_skybox)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ZERO, GL_ONE);

	for (fa = s; fa; fa = fa->texturechain)
		for (p = fa->polys; p; p = p->next) {
			memcpy (v_array_p, p->v, sizeof(vertex_t) * p->numverts);
			TWI_PreVDrawCVA (0, p->numverts);
			qglDrawArrays (GL_POLYGON, 0, p->numverts);
			TWI_PostVDrawCVA ();
		}

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
}

/*
=================
R_DrawSkyChain
=================
*/
void
R_DrawSkyChain (msurface_t *s)
{
	msurface_t *fa;

	if (r_fastsky->ivalue) {
		glpoly_t	*p;

		qglDisable (GL_TEXTURE_2D);
		qglColor4fv (d_8tofloattable[(Uint8) r_fastsky->ivalue - 1]);

		for (fa = s; fa; fa = fa->texturechain){
			for (p = fa->polys; p; p = p->next) {
				memcpy(v_array_p, p->v, sizeof(vertex_t) * p->numverts);

				TWI_PreVDrawCVA (0, p->numverts);
				qglDrawArrays (GL_POLYGON, 0, p->numverts);
				TWI_PostVDrawCVA ();
			}
		}

		qglEnable (GL_TEXTURE_2D);
		qglColor4fv (whitev);
		return;
	}

	if (draw_skybox) {
		R_DrawSkyBoxChain (s);
		return;
	}

	for (fa = s; fa; fa = fa->texturechain) {
		if (gl_mtex)
			EmitBothSkyLayersMTEX (fa);
		else 
			EmitBothSkyLayers (fa);
	}
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
R_LoadSkys (void)
{
	int			i;
	char		name[64];
	image_t	   *img;

	for (i = 0; i < 6; i++) {
		snprintf (name, sizeof (name), "env/%s%s",
				r_skyname->svalue, suf[i]);
		img = Image_Load (name);
		if (!img)
		{
			snprintf (name, sizeof (name), "gfx/env/%s%s",
					r_skyname->svalue, suf[i]);
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

		qglBindTexture (GL_TEXTURE_2D, skyboxtexnum+i);
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
void R_SkyBoxChanged (cvar_t *cvar)
{
	if (cvar->svalue[0])
		draw_skybox = R_LoadSkys();
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
	if (!draw_skybox || (skytexturenum == -1))
		return;

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// Brute force method

	// right
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 0);
	SKYBOXVERT (0,  1,  1,  1, 1, 0);
	SKYBOXVERT (1,  1,  1, -1, 1, 1);
	SKYBOXVERT (2, -1,  1, -1, 0, 1);
	SKYBOXVERT (3, -1,  1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// back
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 1);
	SKYBOXVERT (0, -1,  1,  1, 1, 0);
	SKYBOXVERT (1, -1,  1, -1, 1, 1);
	SKYBOXVERT (2, -1, -1, -1, 0, 1);
	SKYBOXVERT (3, -1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// left
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 2);
	SKYBOXVERT (0, -1, -1,  1, 1, 0);
	SKYBOXVERT (1, -1, -1, -1, 1, 1);
	SKYBOXVERT (2,  1, -1, -1, 0, 1);
	SKYBOXVERT (3,  1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// front
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 3);
	SKYBOXVERT (0,  1, -1,  1, 1, 0);
	SKYBOXVERT (1,  1, -1, -1, 1, 1);
	SKYBOXVERT (2,  1,  1, -1, 0, 1);
	SKYBOXVERT (3,  1,  1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// up
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 4);
	SKYBOXVERT (0,  1, -1,  1, 1, 0);
	SKYBOXVERT (1,  1,  1,  1, 1, 1);
	SKYBOXVERT (2, -1,  1,  1, 0, 1);
	SKYBOXVERT (3, -1, -1,  1, 0, 0);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	// down
	qglBindTexture (GL_TEXTURE_2D, skyboxtexnum + 5);
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
R_InitSky (texture_t *mt)
{
	int         i, j, p;
	Uint8      *src;
	unsigned    trans[128 * 128];
	int         r, g, b;
	unsigned char rgba[4], transpix[4];

	src = (Uint8 *) mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			memcpy(rgba, &d_8to32table[p], sizeof(rgba));
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
		solidskytexture = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, solidskytexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_mag);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_mag);


	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			if (p == 0)
				memcpy(&trans[(i * 128) + j], &transpix, sizeof(trans[0]));
			else
				memcpy(&trans[(i * 128) + j], &d_8to32table[p], sizeof(trans[0]));
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_mag);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_mag);
}

