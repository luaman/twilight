/*
	$RCSfile$ -- surface-related refresh code

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "opengl_ext.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128
#define	MAX_LIGHTMAPS	256

static int			lightmap_bytes;				// 1, 3, or 4

// LordHavoc: align TexSubImage2D updates on 4 byte boundaries
static int			lightmapalign, lightmapalignmask;

static int			lightmap_textures;

static Uint32		blocklights[BLOCK_WIDTH * BLOCK_HEIGHT * 3];
static Uint8		templight[BLOCK_WIDTH * BLOCK_HEIGHT * 4];

static glpoly_t		*lightmap_polys[MAX_LIGHTMAPS];

static int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

static glpoly_t		*fullbright_polys[MAX_GLTEXTURES];
static qboolean		drawfullbrights = false;

static int			r_pvsframecount = 1;

static void
R_RenderFullbrights (void)
{
	int         i, j;
	glpoly_t   *p;
	float		*v;

	if (!drawfullbrights || !gl_fb_bmodels->value)
		return;

	qglDepthMask (GL_FALSE);	/* don't bother writing Z */

	qglEnable (GL_BLEND);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;

		qglBindTexture (GL_TEXTURE_2D, i);

		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			qglBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
				qglTexCoord2f (v[3], v[4]);
				qglVertex3fv (v);
			}
			qglEnd ();
		}

		fullbright_polys[i] = NULL;
	}

	qglDisable (GL_BLEND);

	qglDepthMask (GL_TRUE);

	drawfullbrights = false;
}

/*
===============
R_AddDynamicLights
===============
*/
static int
R_AddDynamicLights (msurface_t *surf)
{
	int			lnum, br, colorscale, colorscale0, colorscale1, colorscale2, local[2], s, t, sd, td, _sd, _td, irad, idist, iminlight, lit = false;
	float		dist, impact[3];
	mtexinfo_t	*tex = surf->texinfo;
	Uint32		*dest;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;
		// not lit by this light
		dist = PlaneDiff (cl_dlights[lnum].origin, surf->plane);
		irad = cl_dlights[lnum].radius - Q_fabs(dist);
		iminlight = cl_dlights[lnum].minlight;

		if (irad < iminlight)
			continue;

		colorscale0 = cl_dlights[lnum].color[0] * 256;
		colorscale1 = cl_dlights[lnum].color[1] * 256;
		colorscale2 = cl_dlights[lnum].color[2] * 256;
		colorscale = ((colorscale0 + colorscale1 + colorscale2) * 85) >> 8;

		iminlight = (irad - iminlight) * 256;
		irad *= 256;

		VectorMA (cl_dlights[lnum].origin, -dist, surf->plane->normal, impact);

		local[0] = (DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0]) * 256;
		local[1] = (DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1]) * 256;

		_td = local[1];
		dest = blocklights;

		for (t = 0; t < surf->tmax; t++)
		{
			td = _td;
			_td -= 16 * 256;

			if (td < 0)
				td = -td;

			_sd = local[0];

			for (s = 0; s < surf->smax; s++)
			{
				sd = _sd;
				_sd -= 16 * 256;
				if (sd < 0)
					sd = -sd;

				if (sd > td)
					idist = sd + (td >> 1);
				else
					idist = td + (sd >> 1);

				if (idist < iminlight)
				{
					br = irad - idist;
					if (br >= 4)
					{
						lit = true;
						if (colorlights)
						{
							dest[s*3+0] += (int)(br * colorscale0) >> 8;
							dest[s*3+1] += (int)(br * colorscale1) >> 8;
							dest[s*3+2] += (int)(br * colorscale2) >> 8;
						}
						else
							dest[s] += (int)(br * colorscale) >> 8;
					}
				}
			}
			if (colorlights)
				dest += surf->smax * 3;
			else
				dest += surf->smax;
		}
	}
	return lit;
}


/*
===============
R_BuildLightmap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void
GL_BuildLightmap (msurface_t *surf)
{
	int			i, j, size, shift, outwidth, stride;
	Uint8		*lightmap, *dest;
	Uint32		scale, *bl;

	qglBindTexture (GL_TEXTURE_2D, lightmap_textures + surf->lightmaptexturenum);

	size = surf->smax * surf->tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (!cl.worldmodel->lightdata)
	{
		memset (blocklights, 255, (colorlights ? 3 : 1) * size * sizeof(Uint32));
		goto store;
	}

	// clear to no light
	memset (blocklights, 0, (colorlights ? 3 : 1) * size * sizeof(Uint32));

	// add all the lightmaps
	if (lightmap)
	{
		for (i = 0; i < MAXLIGHTMAPS && surf->styles[i] != 255; i++)
		{
			scale = d_lightstylevalue[surf->styles[i]];
			bl = blocklights;

			shift = size;
			if (colorlights)
				shift *= 3;
			for (j = 0;j < shift;j++)
				*bl++ += *lightmap++ * scale;
		}
	}
	surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
	surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
	surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
	surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

	/* add all the dynamic lights */
	if (surf->dlightframe == r_framecount)
		if (R_AddDynamicLights (surf))
			surf->cached_dlight = 1;

	/* bound, invert, and shift */
store:
	if (gl_mtexcombine)
		shift = 9;
	else if (gl_mtex)
		shift = 7;
	else
		shift = 8;

	bl = blocklights;
	dest = templight;
	outwidth = (surf->smax + lightmapalign - 1) & lightmapalignmask;
	stride = outwidth * lightmap_bytes;

	switch (gl_lightmap_format)
	{
		case GL_RGB:
			stride -= surf->smax * 3;
			for (i = 0;i < surf->tmax;i++, dest += stride)
			{
				for (j = 0;j < surf->smax; j++)
				{
					dest[0] = bound (0, bl[0] >> shift, 255);
					dest[1] = bound (0, bl[1] >> shift, 255);
					dest[2] = bound (0, bl[2] >> shift, 255);
					bl += 3;
					dest += 3;
				}
			}

			break;

		case GL_RGBA:
			stride -= surf->smax * 4;
			for (i = 0;i < surf->tmax;i++, dest += stride)
			{
				for (j = 0;j < surf->smax; j++)
				{
					dest[0] = bound (0, bl[0] >> shift, 255);
					dest[1] = bound (0, bl[1] >> shift, 255);
					dest[2] = bound (0, bl[2] >> shift, 255);
					dest[3] = 255;
					bl += 3;
					dest += 4;
				}
			}

			break;

		case GL_LUMINANCE:
			for (i = 0;i < surf->tmax;i++, bl += surf->smax, dest += stride)
				for (j = 0;j < surf->smax;j++)
					dest[j] = bound (0, bl[j] >> shift, 255);

			break;

		default:
			Sys_Error ("Bad lightmap format");
	}

	qglTexSubImage2D (GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, outwidth, surf->tmax, gl_lightmap_format, GL_UNSIGNED_BYTE, templight);
}

static void GL_UpdateLightmap (msurface_t *fa)
{
	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	if (fa->lightmappedframe != r_framecount)
	{
		fa->lightmappedframe = r_framecount;
		fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
		lightmap_polys[fa->lightmaptexturenum] = fa->polys;
	}

	if (!r_dynamic->value)
		return;

	if (fa->dlightframe == r_framecount // dynamic lighting
	 || fa->cached_dlight // previously lit
	 || d_lightstylevalue[fa->styles[0]] != fa->cached_light[0]
	 || d_lightstylevalue[fa->styles[1]] != fa->cached_light[1]
	 || d_lightstylevalue[fa->styles[2]] != fa->cached_light[2]
	 || d_lightstylevalue[fa->styles[3]] != fa->cached_light[3])
		GL_BuildLightmap(fa);
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
static texture_t  *
R_TextureAnimation (texture_t *base)
{
	int         reletive;
	int         count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	reletive = (int) (cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive) {
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


/*
================
R_BlendLightmaps
================
*/
static void
R_BlendLightmaps (void)
{
	int         i, j;
	glpoly_t	*p;
	vec3_t		nv;
	float		*v;
	float		intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	qglDepthMask (GL_FALSE);					/* don't bother writing Z */

	qglBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

	qglEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		p = lightmap_polys[i];
		if (!p)
			continue;
		qglBindTexture (GL_TEXTURE_2D, lightmap_textures + i);
		for (; p; p = p->chain) {
			if (p->flags & SURF_UNDERWATER && r_waterwarp->value) {
				qglBegin (GL_TRIANGLE_FAN);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					qglTexCoord2f (v[5], v[6]);
					nv[0] =	v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
					nv[1] =	v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
					nv[2] = v[2];
					qglVertex3fv (nv);
				}
				qglEnd ();
			} else {
				qglBegin (GL_POLYGON);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					qglTexCoord2f (v[5], v[6]);
					qglVertex3fv (v);
				}
				qglEnd ();
			}
		}
	}

	qglDisable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglDepthMask (GL_TRUE);					/* back to normal Z buffering */
}

/*
================
R_RenderBrushPolyMTex
================
*/
static void
R_RenderBrushPolyMTex (msurface_t *fa, texture_t *t)
{
	int		i;
	float	*v;
	vec3_t	nv;
	float	intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	c_brush_polys++;

	qglBindTexture(GL_TEXTURE_2D, lightmap_textures + fa->lightmaptexturenum);

	if (fa->flags & SURF_UNDERWATER && r_waterwarp->value) {
		qglBegin (GL_TRIANGLE_FAN);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[2] = v[2];
			qglVertex3fv (nv);
		}
		qglEnd ();
	} else {
		qglBegin (GL_POLYGON);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			qglVertex3fv (v);
		}
		qglEnd ();
	}

	/* add the poly to the proper fullbright chain */

	if (t->fb_texturenum) {
		fa->polys->fb_chain = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		drawfullbrights = true;
	}
}

/*
================
R_RenderBrushPoly
================
*/
static void
R_RenderBrushPoly (msurface_t *fa, texture_t *t)
{
	int		i;
	vec3_t	nv;
	float	*v;
	float	intensity = (r_waterwarp->value > 0) ? r_waterwarp->value : 8;

	c_brush_polys++;

	if (fa->flags & SURF_UNDERWATER && r_waterwarp->value) {
		qglBegin (GL_TRIANGLE_FAN);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglTexCoord2f (v[3], v[4]);
			nv[0] = v[0] + intensity * Q_sin (v[1] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[1] = v[1] + intensity * Q_sin (v[0] * 0.05 + realtime) * Q_sin (v[2] * 0.05 + realtime);
			nv[2] = v[2];
			qglVertex3fv (nv);
		}
		qglEnd ();
	} else {
		qglBegin (GL_POLYGON);
		v = fa->polys->verts[0];
		for (i = 0; i < fa->polys->numverts; i++, v += VERTEXSIZE) {
			qglTexCoord2f (v[3], v[4]);
			qglVertex3fv (v);
		}
		qglEnd ();
	}

	/* add the poly to the proper lightmap chain */

	if (t->fb_texturenum) {
		fa->polys->fb_chain = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		drawfullbrights = true;
	}
}

/*
================
DrawTextureChains
================
*/
static void
DrawTextureChains ()
{
	unsigned int	i;
	msurface_t		*s;
	texture_t		*t, *st;

	// LordHavoc: upload lightmaps early
	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
			continue;
		for (; s; s = s->texturechain)
			GL_UpdateLightmap(s);
	}

	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		st = R_TextureAnimation (s->texinfo->texture);
		if (s->flags & SURF_DRAWSKY)
		{
			R_DrawSkyChain (s);
			t->texturechain = NULL;
		}
		else if (s->flags & SURF_DRAWTURB)
		{
			// Lordhavoc: handle water here because it is just making
			// transpolys, not really drawing
			for (; s; s = s->texturechain)
				EmitWaterPolys (s, st, false);
			t->texturechain = NULL;
		}
	}

	if (gl_mtex)
	{
		qglDisable (GL_BLEND);
		if (gl_mtexcombine)
		{
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
			qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 4.0);
		}
		else
		{
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			qglActiveTextureARB (GL_TEXTURE1_ARB);
			qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qglEnable (GL_TEXTURE_2D);

		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			st = R_TextureAnimation (t);
			qglActiveTextureARB (GL_TEXTURE0_ARB);
			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			qglActiveTextureARB (GL_TEXTURE1_ARB);

			for (; s; s = s->texturechain)
				R_RenderBrushPolyMTex (s, st);

			t->texturechain = NULL;
		}

		qglDisable (GL_TEXTURE_2D);
		if (gl_mtexcombine)
			qglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		for (i = 0; i < cl.worldmodel->numtextures; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			st = R_TextureAnimation (t);

			qglBindTexture (GL_TEXTURE_2D, st->gl_texturenum);
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s, st);

			t->texturechain = NULL;
		}

		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		R_BlendLightmaps();
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void
R_DrawBrushModel (entity_t *e)
{
	int         i, k, texnum, rotated;
	vec3_t      mins, maxs;
	msurface_t *psurf;
	float       dot;
	model_t    *clmodel = e->model;
	texture_t	*t;
	vec3_t		modelorg;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	} else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	VectorSubtract (r_origin, e->origin, modelorg);
	if (rotated) {
		vec3_t      temp;
		vec3_t      forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	// LordHavoc: decide which surfs are visible and update lightmaps,
	// then render afterward
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		dot = PlaneDiff (modelorg, psurf->plane);

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
		 || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = r_framecount;
		}
		else
			psurf->visframe = -1;
	}

// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend->value) {
		for (k = 0; k < MAX_DLIGHTS; k++) {
			if ((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
				continue;

			R_MarkLightsNoVis (&cl_dlights[k], 1 << k,
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, psurf++)
		if (psurf->visframe == r_framecount)
			GL_UpdateLightmap(psurf);

	qglPushMatrix ();

	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (e->angles[0], 0, 1, 0);	/* stupid quake bug */
	qglRotatef (e->angles[2], 1, 0, 0);

	// for transpoly water
	softwaretransformforbrushentity (e);

	//
	// draw texture
	//
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			if (psurf->flags & SURF_DRAWSKY)
			{
				EmitBothSkyLayers (psurf);
				psurf->visframe = -1;
			}
		}
	}

	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			if (psurf->flags & SURF_DRAWTURB)
			{
				EmitWaterPolys (psurf, R_TextureAnimation(psurf->texinfo->texture), true);
				psurf->visframe = -1;
			}
		}
	}

	if (gl_mtexcombine) {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
		qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		qglTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 4.0);
	} else if (gl_mtex) {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	texnum = -1;
	for (i = 0, psurf = &clmodel->surfaces[clmodel->firstmodelsurface]; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->visframe == r_framecount)
		{
			t = R_TextureAnimation(psurf->texinfo->texture);
			if (gl_mtex)
			{
				if (texnum != t->gl_texturenum)
				{
					texnum = t->gl_texturenum;
					qglActiveTextureARB (GL_TEXTURE0_ARB);
					qglBindTexture (GL_TEXTURE_2D, texnum);
					qglActiveTextureARB (GL_TEXTURE1_ARB);
				}
				R_RenderBrushPolyMTex (psurf, t);
			}
			else
			{
				if (texnum != t->gl_texturenum)
				{
					texnum = t->gl_texturenum;
					qglBindTexture (GL_TEXTURE_2D, texnum);
				}
				R_RenderBrushPoly (psurf, t);
			}
		}
	}

	if (gl_mtex) {
		if (gl_mtexcombine)
			qglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	} else {
		qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		R_BlendLightmaps ();
	}

	R_RenderFullbrights ();

	qglPopMatrix ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
static void
R_RecursiveWorldNode (mnode_t *node)
{
	int         c, side;
	mplane_t   *plane;
	msurface_t *surf, **mark;
	mleaf_t    *pleaf;
	double      dot;

	if (node->contents == CONTENTS_SOLID)
		return;							/* solid */

	if (node->pvsframe != r_pvsframecount)
		return;
	if (R_CullBox (node->mins, node->maxs))
		return;

	// mark node/leaf as visible for MarkLights
	node->visframe = r_framecount;

// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}
		/* deal with model fragments in this leaf */
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}
/* node is just a decision point, so go down the apropriate sides */

/* find which side of the node we are on */
	plane = node->plane;
	dot = PlaneDiff (r_origin, plane);
	side = dot < 0;

/* recurse down the children, front side first */
	R_RecursiveWorldNode (node->children[side]);

/* draw stuff */
	c = node->numsurfaces;

	if (c) {
		surf = cl.worldmodel->surfaces + node->firstsurface;
		side = (dot < 0) ? SURF_PLANEBACK : 0;

		for (; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			/* don't backface underwater surfaces, because they warp */
			if (r_waterwarp->value &&
					!(((r_viewleaf->contents == CONTENTS_EMPTY
								&& (surf->flags & SURF_UNDERWATER))
							|| (r_viewleaf->contents != CONTENTS_EMPTY
								&& !(surf->flags & SURF_UNDERWATER)))
						&& !(surf->flags & SURF_DONTWARP))
					&& ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
			{
				// wrong side
				surf->visframe = -1;
				continue;
			}

			/* if sorting by texture, just store it out */
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}
/* recurse down the back side */
	R_RecursiveWorldNode (node->children[!side]);
}

void R_ClearSkyBox (void);
void R_DrawSkyBox (void);




/*
===============
R_MarkLeaves
===============
*/
static void
R_MarkLeaves (void)
{
	Uint8			*vis;
	mnode_t			*node;
	unsigned int	i;

	if (r_oldviewleaf == r_viewleaf && !r_novis->value)
		return;

	r_pvsframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis->value)
	{
		for (i = 0; i < cl.worldmodel->numleafs; i++)
		{
			node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
			do
			{
				if (node->pvsframe == r_pvsframecount)
					break;
				node->pvsframe = r_pvsframecount;
				node = node->parent;
			}
			while (node);
		}
	}
	else
	{
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		for (i = 0; i < cl.worldmodel->numleafs; i++)
		{
			if (vis[i >> 3] & (1 << (i & 7)))
			{
				node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
				do
				{
					if (node->pvsframe == r_pvsframecount)
						break;
					node->pvsframe = r_pvsframecount;
					node = node->parent;
				}
				while (node);
			}
		}
	}
}

/*
=============
R_DrawWorld
=============
*/
void
R_DrawWorld (void)
{
	entity_t    ent;

	memset (&ent, 0, sizeof (ent));
	ent.model = cl.worldmodel;

	currententity = &ent;

	memset (lightmap_polys, 0, sizeof (lightmap_polys));

	if (gl_fb_bmodels->value)
		memset (fullbright_polys, 0, sizeof(fullbright_polys));

	R_ClearSkyBox ();

	R_MarkLeaves ();
	R_RecursiveWorldNode (cl.worldmodel->nodes);

	R_PushDlights ();

	DrawTextureChains ();

	R_RenderFullbrights ();

	R_DrawSkyBox ();
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
static int
AllocBlock (int w, int h, int *x, int *y)
{
	int         i, j;
	int         best, best2;
	int         texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++) {
		best = BLOCK_HEIGHT;

		// LordHavoc: align TexSubImage2D on 4 byte boundaries
		for (i = 0; i < BLOCK_WIDTH - w; i += lightmapalign)
		{
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w) {				/* this is a valid spot */
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		// LordHavoc: clear texture to blank image, fragments are uploaded using subimage
		if (!allocated[texnum][0])
		{
			memset(templight, 0, sizeof(templight));
			qglBindTexture(GL_TEXTURE_2D, lightmap_textures + texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes, BLOCK_WIDTH, BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE, templight);
		}

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


static mvertex_t  *r_pcurrentvertbase;
static model_t    *currentmodel;

/* int         nColinElim; */

/*
================
BuildSurfaceDisplayList
================
*/
static void
BuildSurfaceDisplayList (msurface_t *fa)
{
	int         i, lindex, lnumverts;
	medge_t    *pedges, *r_pedge;
	int         vertpage;
	float      *vec;
	float       s, t;
	glpoly_t   *poly;

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture
	//
	poly = Hunk_Alloc (sizeof (glpoly_t));
	poly->verts = Hunk_Alloc (lnumverts * sizeof (pvertex_t));

	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s << 4;
		s += 8;
		s /= BLOCK_WIDTH << 4;			/* fa->texinfo->texture->width; */

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t << 4;
		t += 8;
		t /= BLOCK_HEIGHT << 4;			/* fa->texinfo->texture->height; */

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
static void
GL_CreateSurfaceLightmap (msurface_t *surf)
{
	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	surf->lightmaptexturenum =
		AllocBlock (surf->smax, surf->tmax, &surf->light_s, &surf->light_t);
	GL_BuildLightmap(surf);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void
GL_BuildLightmaps (void)
{
	Uint32	i, j;
	model_t	*m;

	memset (allocated, 0, sizeof (allocated));

	r_framecount = 1;					/* no dlightcache */

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	switch ((int) gl_colorlights->value) {
		case 0:
			gl_lightmap_format = GL_LUMINANCE;
			lightmap_bytes = 1;
			colorlights = false;
			break;
		default:
		case 1:
			gl_lightmap_format = GL_RGB;
			lightmap_bytes = 3;
			colorlights = true;
			break;
		case 2:
			gl_lightmap_format = GL_RGBA;
			lightmap_bytes = 4;
			colorlights = true;
			break;
	}

	// LordHavoc: TexSubImage2D needs data aligned on 4 byte boundaries unless
	// I specify glPixelStorei(GL_UNPACK_ALIGNMENT, 1), I suspect 4 byte may be
	// faster anyway, so it is aligned on 4 byte boundaries...
	lightmapalign = 1;
	while ((lightmapalign * lightmap_bytes) & 3)
		lightmapalign <<= 1;
	lightmapalignmask = ~(lightmapalign - 1);

	for (j = 1; j < MAX_MODELS; j++) {
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}
}
