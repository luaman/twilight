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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "client.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "strlib.h"
#include "pmove.h"
#include "sound.h"
#include "view.h"
#include "sys.h"


vec3_t      modelorg, r_entorigin;
entity_t   *currententity;

int         r_visframecount;			// bumped when going to a new PVS
int         r_framecount;				// used for dlight push checking

static mplane_t    frustum[4];

int         c_brush_polys, c_alias_polys;

int         particletexture;			// little dot for particles
int         playertextures;				// up to 16 color translated skins

//
// view origin
//
vec3_t      vup;
vec3_t      vpn;
vec3_t      vright;
vec3_t      r_origin;

float       r_world_matrix[16];
float       r_base_world_matrix[16];

//
// screen size info
//
refdef_t    r_refdef;

mleaf_t    *r_viewleaf, *r_oldviewleaf;

texture_t  *r_notexture_mip;

int         d_lightstylevalue[256];		// 8.8 fraction of base light value


void        R_MarkLeaves (void);

cvar_t     *r_norefresh;
cvar_t     *r_drawentities;
cvar_t     *r_drawviewmodel;
cvar_t     *r_speeds;
cvar_t     *r_shadows;
cvar_t     *r_wateralpha;
cvar_t     *r_dynamic;
cvar_t     *r_novis;
cvar_t     *r_netgraph;

cvar_t     *gl_clear;
cvar_t     *gl_cull;
cvar_t     *gl_affinemodels;
cvar_t     *gl_polyblend;
cvar_t     *gl_flashblend;
cvar_t     *gl_playermip;
cvar_t     *gl_nocolors;
cvar_t     *gl_keeptjunctions;
cvar_t     *gl_finish;
cvar_t	   *gl_im_animation;
cvar_t	   *gl_fb_models;
cvar_t	   *gl_fb_bmodels;
cvar_t	   *gl_oldlights;
cvar_t	   *gl_colorlights;

extern cvar_t *gl_ztrick;

extern vec3_t lightcolor;
qboolean colorlights = true;

static float shadescale = 0.0;

int lastposenum = 0, lastposenum0 = 0;

qboolean PM_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f,
		vec3_t p1, vec3_t p2, pmtrace_t *trace);

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean
R_CullBox (vec3_t mins, vec3_t maxs)
{
	if (BoxOnPlaneSide (mins, maxs, &frustum[0]) == 2)
		return true;
	if (BoxOnPlaneSide (mins, maxs, &frustum[1]) == 2)
		return true;
	if (BoxOnPlaneSide (mins, maxs, &frustum[2]) == 2)
		return true;
	if (BoxOnPlaneSide (mins, maxs, &frustum[3]) == 2)
		return true;

	return false;
}


void
R_RotateForEntity (entity_t *e, qboolean shadow)
{
	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef (e->angles[1], 0, 0, 1);

	if (!shadow)
	{
		qglRotatef (-e->angles[0], 0, 1, 0);
		// ZOID: fixed z angle
		qglRotatef (e->angles[2], 1, 0, 0);
	}
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *
R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t  *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int         i, numframes, frame;
	float      *pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0)) {
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *) psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = cl.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval
		// values are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void
R_DrawSpriteModel (entity_t *e)
{
	vec3_t      point;
	mspriteframe_t *frame;
	float      *up, *right;
	vec3_t      v_forward, v_right, v_up;
	msprite_t  *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	} else {
		// normal sprite
		up = vup;
		right = vright;
	}

	qglBindTexture (GL_TEXTURE_2D, frame->gl_texturenum);

	qglBegin (GL_QUADS);

	qglTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);

	qglEnd ();
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float       r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t      shadevector;
float       shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float       r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
           ;

float      *shadedots = r_avertexnormal_dots[0];

int         lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void
GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, qboolean fb)
{
	float       l;
	trivertx_t *verts;
	int        *order;
	int         count;

	lastposenum = posenum;

	verts = (trivertx_t *) ((Uint8 *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((Uint8 *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else
			qglBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			qglTexCoord2fv ((float *) order);

			order += 2;

			// normals and vertexes come from the frame list
			if (!fb) {
				if (!colorlights) {
					l = shadedots[verts->lightnormalindex] * shadelight;
					qglColor3f (l, l, l);
				}
				else {
					l = shadedots[verts->lightnormalindex];
					qglColor3f (l*lightcolor[0], l*lightcolor[1], l*lightcolor[2]);
				}
			}

			qglVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		qglEnd ();
	}
	qglColor3f(1, 1, 1);
}

/*
	GL_DrawAliasBlendedFrame

	Interpolated model drawing
*/
void
GL_DrawAliasBlendedFrame (aliashdr_t *paliashdr, int pose1, int pose2,
		float blend, qboolean fb)
{
	float       l;
	trivertx_t *verts1;
	trivertx_t *verts2;
	int        *order;
	int         count;
	vec3_t      d;

	lastposenum0 = pose1;
	lastposenum = pose2;

	verts1 = (trivertx_t *) ((Uint8 *) paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((Uint8 *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else {
			qglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			qglTexCoord2fv ((float *) order);

			order += 2;

			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			d[0] = shadedots[verts2->lightnormalindex]
				- shadedots[verts1->lightnormalindex];

			if (!fb) {
				if (!colorlights) {
					l = shadelight * (shadedots[verts1->lightnormalindex]
							+ (blend * d[0]));
					qglColor3f (l, l, l);
				} else {
					l = shadedots[verts1->lightnormalindex] + (blend * d[0]);
					qglColor3f (l*lightcolor[0], l*lightcolor[1], l*lightcolor[2]);
				}
			}

			VectorSubtract (verts2->v, verts1->v, d);

			// blend the vertex positions from each frame together
			qglVertex3f (verts1->v[0] + (blend * d[0]),
						verts1->v[1] + (blend * d[1]),
						verts1->v[2] + (blend * d[2]));

			verts1++;
			verts2++;
		} while (--count);
		qglEnd ();
	}
	qglColor3f(1, 1, 1);
}

/*
	GL_DrawAliasShadow

	Standard shadow drawing
*/
extern vec3_t lightspot;

void
GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t *verts;
	int        *order;
	vec3_t      point;
	float       height, lheight;
	int         count;
	pmtrace_t	downtrace;
	vec3_t		downmove;
	float		s1 = 0;
	float		c1 = 0;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *) ((Uint8 *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((Uint8 *) paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	if (r_shadows->value == 2)
	{
		// better shadowing, now takes angle of ground into account
		// cast a traceline into the floor directly below the player
		// and gets normals from this
		VectorCopy (currententity->origin, downmove);
		downmove[2] = downmove[2] - 4096;
		memset (&downtrace, 0, sizeof(downtrace));
		PM_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1,
				currententity->origin, downmove, &downtrace);

		// calculate the all important angles to keep speed up
		s1 = Q_sin( currententity->angles[1]/180*M_PI);
		c1 = Q_cos( currententity->angles[1]/180*M_PI);
	}

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else
			qglBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			// (skipped for shadows) qglTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] =
				verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] =
				verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] =
				verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			if (r_shadows->value == 2)
			{
				point[0] -= shadevector[0] * point[0];
				point[1] -= shadevector[1] * point[1];
				point[2] -= shadevector[2] * point[2];

				// drop it down to floor
				point[2] = point[2] - (currententity->origin[2]
						- downtrace.endpos[2]);

				// now adjust the point with respect to all the normals of
				// the tracepoint
				point[2] += ((point[1] * (s1 * downtrace.plane.normal[0])) -
					(point[0] * (c1 * downtrace.plane.normal[0])) -
					(point[0] * (s1 * downtrace.plane.normal[1])) -
					(point[1] * (c1 * downtrace.plane.normal[1]))
					) + 20.2 - downtrace.plane.normal[2]*20.0;
			}
			else {
				point[0] -= shadevector[0]*(point[2]+lheight);
				point[1] -= shadevector[1]*(point[2]+lheight);
				point[2] = height;
			}

			qglVertex3fv (point);

			verts++;
		} while (--count);

		qglEnd ();
	}
}

/*
	GL_DrawAliasBlendedShadow
         
	Interpolated shadow drawing
*/
void
GL_DrawAliasBlendedShadow (aliashdr_t *paliashdr, int pose1, int pose2,
		entity_t *e)
{
	trivertx_t	*verts1, *verts2;
	vec3_t		point1, point2, d;
	int 		*order, count;
	float       height, lheight, blend;
	pmtrace_t	downtrace;
	vec3_t		downmove;
	float		s1 = 0.0f;
	float		c1 = 0.0f;

	blend = (realtime - e->frame_start_time) / e->frame_interval;
	blend = min (blend, 1);

	lheight = e->origin[2] - lightspot[2];
	height = -lheight + 1.0;

	verts2 = verts1 = (trivertx_t *) ((Uint8 *) paliashdr
			+ paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((Uint8 *) paliashdr + paliashdr->commands);

	if (r_shadows->value == 2)
	{
		// better shadowing, now takes angle of ground into account
		// cast a traceline into the floor directly below the player
		// and gets normals from this
		VectorCopy (currententity->origin, downmove);
		downmove[2] = downmove[2] - 4096;
		memset (&downtrace, 0, sizeof(downtrace));
		PM_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1,
				currententity->origin, downmove, &downtrace);

		// calculate the all important angles to keep speed up
		s1 = Q_sin( currententity->angles[1]/180*M_PI);
		c1 = Q_cos( currententity->angles[1]/180*M_PI);
	}

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else {
			qglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			point1[0] =	verts1->v[0] * paliashdr->scale[0]
				+ paliashdr->scale_origin[0];
			point1[1] =	verts1->v[1] * paliashdr->scale[1]
				+ paliashdr->scale_origin[1];
			point1[2] =	verts1->v[2] * paliashdr->scale[2]
				+ paliashdr->scale_origin[2];

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			point2[0] =	verts2->v[0] * paliashdr->scale[0]
				+ paliashdr->scale_origin[0];
			point2[1] =	verts2->v[1] * paliashdr->scale[1]
				+ paliashdr->scale_origin[1];
			point2[2] =	verts2->v[2] * paliashdr->scale[2]
				+ paliashdr->scale_origin[2];

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			VectorSubtract (point2, point1, d);

			if (r_shadows->value == 2)
			{	
				point1[0] = point1[0] + (blend * d[0]);
				point1[1] = point1[1] + (blend * d[1]);
				point1[2] = point1[2] + (blend * d[2]);

				// drop it down to floor
				point1[2] =  - (currententity->origin[2]
						- downtrace.endpos[2]);

				// now move the z-coordinate as appropriate
				point1[2] += ((point1[1] * (s1 * downtrace.plane.normal[0])) -
					(point1[0] * (c1 * downtrace.plane.normal[0])) -
					(point1[0] * (s1 * downtrace.plane.normal[1])) -
					(point1[1] * (c1 * downtrace.plane.normal[1]))
					) + 20.2 - downtrace.plane.normal[2]*20.0;

				qglVertex3fv (point1);
			}
			else {
				qglVertex3f (point1[0] + (blend * d[0]),
					point1[1] + (blend * d[1]),
					height);
			}

			verts1++;
			verts2++;
		} while (--count);
		qglEnd ();
	}
}

  

/*
=================
R_SetupAliasFrame

=================
*/
void
R_SetupAliasFrame (int frame, aliashdr_t *paliashdr, qboolean fb)
{
	int         pose, numposes;
	float       interval;

	if ((frame >= paliashdr->numframes) || (frame < 0)) {
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose, fb);
}


/*
	R_SetupAliasBlendedFrame


*/
void
R_SetupAliasBlendedFrame (int frame, aliashdr_t *paliashdr, entity_t *e, qboolean fb)
{
	int 	pose, numposes;
	float	blend;

	if ((frame >= paliashdr->numframes) || (frame < 0)) {
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / e->frame_interval) % numposes;
	} else {
		/*
			One tenth of a second is a good for most Quake animations. If the
			nextthink is longer then the animation is usually meant to pause
			(e.g. check out the shambler magic animation in shambler.qc).  If
			its shorter then things will still be smoothed partly, and the
			jumps will be less noticable because of the shorter time.  So,
			this is probably a good assumption.
		*/
		e->frame_interval = 0.1;
	}

	if (e->pose2 != pose) {
		e->frame_start_time = realtime;
		if (e->pose2 == -1) {
			e->pose1 = pose;
		} else {
			e->pose1 = e->pose2;
		}
		e->pose2 = pose;
		blend = 0;
	} else {
		blend = (realtime - e->frame_start_time) / e->frame_interval;
	}
//	Con_DPrintf ("numposes: %d, poses: %d %d\n", numposes, e->pose1,
//			e->pose2);

	// wierd things start happening if blend passes 1
	if (cl.paused || blend > 1)
		blend = 1;

	GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend, fb);
}

/*
=================
R_DrawAliasModel

=================
*/
void
R_DrawAliasModel (entity_t *e)
{
	int         i;
	int         lnum;
	float       add;
	model_t    *clmodel;
	vec3_t      mins, maxs;
	aliashdr_t *paliashdr;
	int         anim;
	int			texture, fb_texture, skinnum;
	dlight_t	*l;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// 
	// get lighting information
	// 

	if (!(clmodel->modflags & FLAG_FULLBRIGHT) || gl_fb_models->value)
	{
		ambientlight = shadelight = R_LightPoint (currententity->origin);

		// always give the gun some light
		if (!colorlights) {
			if (e == &cl.viewent && ambientlight < 24)
				ambientlight = shadelight = 24;
		}
		else {
			if (e == &cl.viewent)
			{
				lightcolor[0] = max (lightcolor[0], 24);
				lightcolor[1] = max (lightcolor[1], 24);
				lightcolor[2] = max (lightcolor[2], 24);
			}
		}

		for (lnum = 0, l = &cl_dlights[0]; lnum < MAX_DLIGHTS; lnum++, l++) {
			if (l->die < cl.time || !l->radius)
				continue;

			add = l->radius - VectorDistance (currententity->origin, cl_dlights[lnum].origin);

			if (add > 0.01) {
				if (!colorlights)
				{
					ambientlight += add;
					// ZOID models should be affected by dlights as well
					shadelight += add;
				}
				else {
					lightcolor[0] += add * l->color[0];
					lightcolor[1] += add * l->color[1];
					lightcolor[2] += add * l->color[2];
				}
			}
		}

		if (!colorlights)
		{
			// clamp lighting so it doesn't overbright as much
			if (ambientlight > 128)
				ambientlight = 128;
			if (ambientlight + shadelight > 192)
				shadelight = 192 - ambientlight;
		}
	}

	// ZOID: never allow players to go totally black
	if (clmodel->modflags & FLAG_PLAYER) {
		if (!colorlights)
		{
			if (ambientlight < 8)
				ambientlight = shadelight = 8;
		}
		else {
			lightcolor[0] = max (lightcolor[0], 8);
			lightcolor[1] = max (lightcolor[1], 8);
			lightcolor[2] = max (lightcolor[2], 8);
		}
	}
	
	if ((clmodel->modflags & FLAG_FULLBRIGHT) && !gl_fb_models->value) {
		if (!colorlights)
			ambientlight = shadelight = 256;
		else
			lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
	}

	shadedots = r_avertexnormal_dots[((int) (e->angles[1]
				* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	if (!colorlights) {
		shadelight = shadelight * (1.0 / 200.0);

		if (!currententity->last_light[0])
			currententity->last_light[0] = shadelight;
		else {
			shadelight = (shadelight + currententity->last_light[0])*0.5f;
			currententity->last_light[0] = shadelight;
		}
	}
	else {
		VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

		if (!currententity->last_light[0] && !currententity->last_light[1] && !currententity->last_light[2])
			VectorCopy (lightcolor, currententity->last_light);
		else {
			VectorAdd (lightcolor, currententity->last_light, lightcolor);
			VectorScale (lightcolor, 0.5f, lightcolor);
			VectorCopy (lightcolor, currententity->last_light);
		}
	}

	// 
	// locate the proper data
	// 
	paliashdr = (aliashdr_t *) Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	// 
	// draw all the triangles
	// 

	qglPushMatrix ();

	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (-e->angles[0], 0, 1, 0);
	qglRotatef (e->angles[2], 1, 0, 0);

	if (clmodel->modflags & FLAG_DOUBLESIZE) {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1],
					  paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		qglScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2,
				  paliashdr->scale[2] * 2);
	} else {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1],
					  paliashdr->scale_origin[2]);
		qglScalef (paliashdr->scale[0], paliashdr->scale[1],
				  paliashdr->scale[2]);
	}

	anim = (int) (cl.time * 10) & 3;
	skinnum = currententity->skinnum;
	texture = paliashdr->gl_texturenum[currententity->skinnum][anim];
	fb_texture = currententity->scoreboard && (clmodel->modflags & FLAG_PLAYER) ?
		fb_skins[currententity->scoreboard - cl.players] : paliashdr->fb_texturenum[skinnum][anim];

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->scoreboard && !gl_nocolors->value) {
		i = currententity->scoreboard - cl.players;
		if (!currententity->scoreboard->skin) {
			Skin_Find (currententity->scoreboard);
			R_TranslatePlayerSkin (i);
		}

		if (i >= 0 && i < MAX_CLIENTS) {
			texture = playertextures + i;
			fb_texture = fb_skins[i];
		}
	}

	if (gl_affinemodels->value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (!gl_fb_models->value)
		fb_texture = 0;

	qglBindTexture (GL_TEXTURE_2D, texture);

	if (gl_im_animation->value && !(clmodel->modflags & FLAG_NO_IM_FORM))
		R_SetupAliasBlendedFrame (currententity->frame, paliashdr,
				currententity, false);
	else
		R_SetupAliasFrame (currententity->frame, paliashdr, false);

	if (fb_texture) {
		qglEnable (GL_BLEND);
		qglBindTexture (GL_TEXTURE_2D, fb_texture);
		
		if (gl_im_animation->value && !(clmodel->modflags & FLAG_NO_IM_FORM))
			R_SetupAliasBlendedFrame (currententity->frame, paliashdr,
					currententity, true);
		else
			R_SetupAliasFrame (currententity->frame, paliashdr, true);

		qglDisable (GL_BLEND);
	}

	if (gl_affinemodels->value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix ();

	if (r_shadows->value && !(clmodel->modflags & FLAG_NOSHADOW)) {
		float an = -e->angles[1] * (M_PI / 180);

		if (!shadescale)
			shadescale = 1 / Q_sqrt(2);

		shadevector[0] = Q_cos (an) * shadescale;
		shadevector[1] = Q_sin (an) * shadescale;
		shadevector[2] = shadescale;

		qglPushMatrix ();

		qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
		qglRotatef (e->angles[1], 0, 0, 1);

		qglDisable (GL_TEXTURE_2D);
		qglEnable (GL_BLEND);
		qglColor4f (0, 0, 0, 0.5);
		if (gl_im_animation->value) {
			GL_DrawAliasBlendedShadow (paliashdr, lastposenum0, lastposenum,
					currententity);
		} else {
			GL_DrawAliasShadow (paliashdr, lastposenum);
		}
		qglDisable (GL_BLEND);
		qglColor4f (1, 1, 1, 1);
		qglEnable (GL_TEXTURE_2D);
		qglPopMatrix ();
	}
}

//===========================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void
R_DrawEntitiesOnList (void)
{
	int         i;

	if (!r_drawentities->value)
		return;

	// LordHavoc: draw brush models, models, and sprites separately because
	// of different states
	for (i = 0; i < cl_numvisedicts; i++) {
		currententity = &cl_visedicts[i];

		if (currententity->model->type == mod_brush)
			R_DrawBrushModel (currententity);
	}

	for (i = 0; i < cl_numvisedicts; i++) {
		currententity = &cl_visedicts[i];

		if (currententity->model->type == mod_alias)
			R_DrawAliasModel (currententity);
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void
R_DrawEntitiesOnList2 (void)
{
	int i;

	if (!r_drawentities->value)
		return;

	for (i = 0; i < cl_numvisedicts; i++) {
		currententity = &cl_visedicts[i];

		if (currententity->model->type == mod_sprite)
			R_DrawSpriteModel (currententity);
	}
}

/*
=============
R_DrawViewModel
=============
*/
void
R_DrawViewModel (void)
{
	currententity = &cl.viewent;

	if (!r_drawviewmodel->value || 
		!Cam_DrawViewModel () ||
		!r_drawentities->value ||
		(cl.stats[STAT_ITEMS] & IT_INVISIBILITY) ||
		(cl.stats[STAT_HEALTH] <= 0) ||
		!currententity->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity);
	qglDepthRange (gldepthmin, gldepthmax);
}


/*
============
R_PolyBlend
============
*/
void
R_PolyBlend (void)
{
	if (!gl_polyblend->value)
		return;
	if (!v_blend[3])
		return;

//Con_Printf("R_PolyBlend(): %4.2f %4.2f %4.2f %4.2f\n",v_blend[0], v_blend[1], v_blend[2], v_blend[3]);

//	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglLoadIdentity ();

	qglRotatef (-90, 1, 0, 0);			// put Z going up
	qglRotatef (90, 0, 0, 1);			// put Z going up

	qglColor4fv (v_blend);

	qglBegin (GL_QUADS);

	qglVertex3f (10, 100, 100);
	qglVertex3f (10, -100, 100);
	qglVertex3f (10, -100, -100);
	qglVertex3f (10, 100, -100);
	qglEnd ();

	qglColor3f(1, 1, 1);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
//	qglEnable (GL_ALPHA_TEST);
}


int
SignbitsForPlane (mplane_t *out)
{
	int         bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void
R_SetFrustum (void)
{
	int         i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, vup, vpn,
							 -(90 - r_refdef.fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, vup, vpn,
							 90 - r_refdef.fov_x / 2);
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector (frustum[2].normal, vright, vpn,
							 90 - r_refdef.fov_y / 2);
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector (frustum[3].normal, vright, vpn,
							 -(90 - r_refdef.fov_y / 2));

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void
R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (!Q_atoi (Info_ValueForKey (cl.serverinfo, "watervis")))
		if (r_wateralpha->value != 1)
			Cvar_Set (r_wateralpha, "1");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;

}


void
MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble    xmin, xmax, ymin, ymax;

	ymax = zNear * Q_tan (fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	qglFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}


/*
=============
R_SetupGL
=============
*/
void
R_SetupGL (void)
{
	float       screenaspect;
	int         x, x2, y2, y, w, h;

	// 
	// set up viewpoint
	// 
	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	x = r_refdef.vrect.x * vid.width / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.width / vid.width;
	y = (vid.height - r_refdef.vrect.y) * vid.height / vid.height;
	y2 = (vid.height -
		  (r_refdef.vrect.y + r_refdef.vrect.height)) * vid.height / vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.width)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < vid.height)
		y++;

	w = x2 - x;
	h = y - y2;

	qglViewport (glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	MYgluPerspective (r_refdef.fov_y, screenaspect, 4, 8193);

	qglCullFace (GL_FRONT);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	qglRotatef (-90, 1, 0, 0);			// put Z going up
	qglRotatef (90, 0, 0, 1);			// put Z going up
	qglRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	qglRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	qglRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	qglTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1],
				  -r_refdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// 
	// set drawing parms
	// 
	if (gl_cull->value)
		qglEnable (GL_CULL_FACE);
	else
		qglDisable (GL_CULL_FACE);

	qglDisable (GL_BLEND);
	qglEnable (GL_DEPTH_TEST);
}

void R_DrawSkyBox (void);

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void
R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();					// done here so we know if we're in
	// water

	R_PushDlights ();

	R_DrawWorld ();						// adds static entities to the list

	S_ExtraUpdate ();					// don't let sound get messed up if
	// going slow

	R_DrawEntitiesOnList ();
}


/*
=============
R_Clear
=============
*/
void
R_Clear (void)
{
	if (gl_ztrick->value) {
		static int  trickframe;

		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1) {
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	} else {
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}


/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void
R_RenderView (void)
{
	double      time1 = 0, time2;

	if (r_norefresh->value)
		return;

	if (!cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds->value) {
		qglFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish->value)
		qglFinish ();

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawWaterSurfaces ();

	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);

	R_DrawParticles ();
	R_DrawEntitiesOnList2 ();
	R_RenderDlights ();

	qglDepthMask (GL_TRUE);
	qglDisable (GL_BLEND);

	R_PolyBlend ();

	if (r_speeds->value) {
//      qglFinish ();
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n",
					(int) ((time2 - time1) * 1000), c_brush_polys,
					c_alias_polys);
	}
}

