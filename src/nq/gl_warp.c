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
// gl_warp.c -- sky and water polygons
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "cvar.h"
#include "glquake.h"
#include "strlib.h"
#include "host.h"
#include "pcx.h"
#include "tga.h"
#include "sys.h"

extern model_t *loadmodel;

int         skytexturenum;
int			skyboxtexnum;

int         solidskytexture;
int         alphaskytexture;
float       speedscale;					// for top sky and bottom sky

msurface_t *warpface;

extern cvar_t *gl_subdivide_size;

#define	MAX_CLIP_VERTS	64

cvar_t *r_skybox;
static qboolean draw_skybox = false;

void R_DrawSkyboxChain (msurface_t *s);

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
	float       s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size->value 
			* Q_floor (m / gl_subdivide_size->value + 0.5);
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
	poly->verts = Hunk_Alloc (numverts * sizeof (pvertex_t));

	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
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

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void
EmitWaterPolys (msurface_t *fa)
{
	glpoly_t   *p;
	float      *v;
	int         i;
	float       s, t, os, ot;


	for (p = fa->polys; p; p = p->next) {
		qglBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int) ((ot * 0.125 + realtime) * TURBSCALE) & 255];
			s *= (1.0 / 64);

			t = ot + turbsin[(int) ((os * 0.125 + realtime) * TURBSCALE) & 255];
			t *= (1.0 / 64);

			qglTexCoord2f (s, t);
			qglVertex3fv (v);
		}
		qglEnd ();
	}
}




/*
=============
EmitSkyPolys
=============
*/
void
EmitSkyPolys (msurface_t *fa)
{
	glpoly_t   *p;
	float      *v;
	int         i;
	float       s, t;
	vec3_t      dir;
	float       length;

	for (p = fa->polys; p; p = p->next) {
		qglBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;				// flatten the sphere

			length = 6 * 63 * Q_RSqrt (DotProduct(dir,dir));

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0 / 128);
			t = (speedscale + dir[1]) * (1.0 / 128);

			qglTexCoord2f (s, t);
			qglVertex3fv (v);
		}
		qglEnd ();
	}
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
	speedscale = realtime * 8;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);

	qglEnable (GL_BLEND);
	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys (fa);

	qglDisable (GL_BLEND);
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

	if (draw_skybox) {
		R_DrawSkyboxChain (s);
		return;
	}

	// used when gl_texsort is on
	qglBindTexture (GL_TEXTURE_2D, solidskytexture);
	speedscale = realtime * 8;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys (fa);

	qglEnable (GL_BLEND);
	qglBindTexture (GL_TEXTURE_2D, alphaskytexture);
	speedscale = realtime * 16;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys (fa);

	qglDisable (GL_BLEND);
}

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

void        GL_SelectTexture (GLenum target);

/*
==================
R_LoadSkys
==================
*/
char       *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
qboolean
R_LoadSkys (void)
{
	int   i, w, h;
	char  name[64];
	Uint8  *image_buf = NULL;

	for (i = 0; i < 6; i++) {
		snprintf (name, sizeof (name), "gfx/env/%s%s.tga", r_skybox->string, suf[i]);

		TGA_Load (name, &image_buf, &w, &h);

		if (!image_buf || 
			((w != 256) || (h != 256))) {

			name[0] = 0;
			snprintf (name, sizeof (name), "gfx/env/%s%s.pcx", r_skybox->string, suf[i]);

			PCX_Load (name, &image_buf, &w, &h);

			if (!image_buf)
				return false;
			if ((w != 256) || (h != 256)) {
				free (image_buf);
				return false;
			}
		} 

		qglBindTexture (GL_TEXTURE_2D, skyboxtexnum+i);
		qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_buf);

		free (image_buf);
		image_buf = NULL;

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
	if (cvar->string[0])
		draw_skybox = R_LoadSkys();
	else
		draw_skybox = false;
}

static int c_sky;
static float skymins[2][6], skymaxs[2][6];

static const vec3_t skyclip[6] = {
	{  1,  1, 0 },
	{  1, -1, 0 },
	{  0, -1, 1 },
	{  0,  1, 1 },
	{  1,  0, 1 },
	{ -1,  0, 1 } 
};

static const int st_to_vec[6][3] =
{
	{  3, -1, 2 },
	{ -3,  1, 2 },

	{  1,  3, 2 },
	{ -1, -3, 2 },

	{ -2, -1,  3 },		// 0 degrees yaw, look straight up
	{  2, -1, -3 }		// look straight down
};

static const int vec_to_st[6][3] =
{
	{ -2, 3,  1 },
	{  2, 3, -1 },

	{  1, 3,  2 },
	{ -1, 3, -2 },

	{ -2, -1,  3 },
	{ -2,  1, -3 }
};

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;

	// decide which face it maps to
	VectorClear (v);

	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = Q_fabs(v[0]);
	av[1] = Q_fabs(v[1]);
	av[2] = Q_fabs(v[2]);

	if ((av[0] > av[1]) && (av[0] > av[2]))
		axis = (v[0] < 0);
	else if ((av[1] > av[2]) && (av[1] > av[0]))
		axis = 2 + (v[1] < 0);
	else
		axis = 4 + (v[2] < 0);

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? 1 / vecs[j - 1] : 1 / -vecs[-j - 1];
		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;
		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		Sys_Error ("ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{
		// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct (v, skyclip[stage]);

		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;

		dists[i] = d;
	}

	if (!front || !back)
	{
		// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
			case SIDE_FRONT:
				VectorCopy (v, newv[0][newc[0]]);
				newc[0]++;
				break;
			case SIDE_BACK:
				VectorCopy (v, newv[1][newc[1]]);
				newc[1]++;
				break;
			case SIDE_ON:
				VectorCopy (v, newv[0][newc[0]]);
				newc[0]++;
				VectorCopy (v, newv[1][newc[1]]);
				newc[1]++;
				break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);

		for (j = 0; j < 3; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}

		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyboxChain (msurface_t *s)
{
	msurface_t	*fa;
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	c_sky = 0;

	// calculate vertex values for sky box
	for (fa = s; fa; fa = fa->texturechain)
	{
		for (p = fa->polys; p; p = p->next)
		{
			for (i = 0; i < p->numverts; i++)
				VectorSubtract (p->verts[i], r_origin, verts[i]);

			ClipSkyPolygon (p->numverts, verts[0], 0);
		}
	}
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;

	VectorSet (b, s*4096.5, t*4096.5, 4096.5);

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	s = bound ((1.0/512), s, (511.0/512));
	t = 1.0 - bound ((1.0/512), t, (511.0/512));

	qglTexCoord2f (s, t);
	qglVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
static int skytexorder[6] = {0, 2, 1, 3, 4, 5};

void R_DrawSkyBox (void)
{
	int		i;

	if (!draw_skybox || (skytexturenum == -1))
		return;

	GL_SelectTexture (0);

	for (i = 0; i < 6; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i]
			|| skymins[1][i] >= skymaxs[1][i])
			continue;

		qglBindTexture (GL_TEXTURE_2D, skyboxtexnum+skytexorder[i]);

		qglBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		qglEnd ();
	}
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
	Uint8       *src;
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
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


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
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
