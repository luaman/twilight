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

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "world.h"
#include "r_explosion.h"
#include "host.h"

entity_t *currententity;
int r_framecount;						// used for dlight push checking
static mplane_t frustum[4];
int c_brush_polys, c_alias_polys;
int playertextures;						// up to 16 color translated skins

/*
 * view origin
 */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/*
 * screen size info
 */
refdef_t r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;

texture_t *r_notexture_mip;

int d_lightstylevalue[256];				// 8.8 fraction of base light value


void R_Torch (entity_t *ent, qboolean torch2);

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawviewmodel;
cvar_t *r_speeds;
cvar_t *r_shadows;
cvar_t *r_wateralpha;
cvar_t *r_waterripple;
cvar_t *r_wireframe;
cvar_t *r_dynamic;
cvar_t *r_novis;
cvar_t *r_lightlerp;

cvar_t *gl_clear;
cvar_t *gl_cull;
cvar_t *gl_affinemodels;
cvar_t *gl_polyblend;
cvar_t *gl_flashblend;
cvar_t *gl_playermip;
cvar_t *gl_nocolors;
cvar_t *gl_finish;
cvar_t *gl_im_animation;
cvar_t *gl_im_transform;
cvar_t *gl_fb_models;
cvar_t *gl_fb_bmodels;
cvar_t *gl_oldlights;
cvar_t *gl_colorlights;
cvar_t *gl_particletorches;

extern cvar_t *gl_ztrick;

extern vec3_t lightcolor;
qboolean colorlights = true;

//static float shadescale = 0.0;

int gl_wireframe = 0;

int lastposenum = 0, lastposenum0 = 0;
extern model_t *mdl_fire;

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
static mspriteframe_t *
R_GetSpriteFrame (entity_t *e)
{
	msprite_t		   *psprite;
	mspritegroup_t	   *pspritegroup;
	mspriteframe_t	   *pspriteframe;
	int					i, numframes, frame;
	float			   *pintervals, fullinterval, targettime, time;

	psprite = Mod_Extradata(e->model);
	frame = e->frame;

	if ((frame >= psprite->numframes) || (frame < 0)) {
		Com_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *) psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = cl.time + e->syncbase;

		/*
		 * when loading in Mod_LoadSpriteGroup, we guaranteed all interval
		 * values are positive, so we don't have to worry about division by 0
		 */
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
static void
R_DrawSpriteModels ()
{
	mspriteframe_t	   *f;
	float			   *up, *right;
	vec3_t				v_forward, v_right, v_up;
	msprite_t		   *psprite;
	entity_t		   *e;
	int					i, last_tex;

	last_tex = -1;
	v_index = 0;

	qglEnable (GL_ALPHA_TEST);

	for (i = 0; i < r_refdef.num_entities; i++) {
		e = r_refdef.entities[i];

		if (e->model->type != mod_sprite)
			continue;

		/*
		 * don't even bother culling, because it's just a single polygon without
		 * a surface cache
		 */
		f = R_GetSpriteFrame (e);
		psprite = Mod_Extradata(e->model);

		if (last_tex != f->gl_texturenum) {
			if (v_index) {
				TWI_PreVDrawCVA (0, v_index);
				qglDrawArrays (GL_QUADS, 0, v_index);
				TWI_PostVDrawCVA ();
				v_index = 0;
			}
			last_tex = f->gl_texturenum;
			qglBindTexture (GL_TEXTURE_2D, last_tex);
		}

		if (psprite->type == SPR_ORIENTED) {
			// bullet marks on walls
			AngleVectors (e->angles, v_forward, v_right, v_up);
			up = v_up;
			right = v_right;
		} else {
			// normal sprite
			up = vup;
			right = vright;
		}

		VectorSet2(tc_array[v_index + 0], 0, 1);
		VectorSet2(tc_array[v_index + 1], 0, 0);
		VectorSet2(tc_array[v_index + 2], 1, 0);
		VectorSet2(tc_array[v_index + 3], 1, 1);

		VectorTwiddle (e->origin, up, f->down,  right, f->left, 1,
				v_array[v_index + 0]);
		VectorTwiddle (e->origin, up, f->up,    right, f->left, 1,
				v_array[v_index + 1]);
		VectorTwiddle (e->origin, up, f->up,    right, f->right, 1,
				v_array[v_index + 2]);
		VectorTwiddle (e->origin, up, f->down,  right, f->right, 1,
				v_array[v_index + 3]);

		v_index += 4;
		if ((v_index + 4) >= MAX_VERTEX_ARRAYS) {
			TWI_PreVDrawCVA (0, v_index);
			qglDrawArrays (GL_QUADS, 0, v_index);
			TWI_PostVDrawCVA ();
			v_index = 0;
		}
	}

	if (v_index) {
		TWI_PreVDrawCVA (0, v_index);
		qglDrawArrays (GL_QUADS, 0, v_index);
		TWI_PostVDrawCVA ();
		v_index = 0;
	}
	qglDisable (GL_ALPHA_TEST);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t shadevector;
float shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float       r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
           ;

float *shadedots = r_avertexnormal_dots[0];
float *shadedots2 = r_avertexnormal_dots[0];
float lightlerpoffset;

#if 0
/*
=================
GL_DrawAliasShadow

Standard shadow drawing
=================
*/
extern vec3_t lightspot;

static void
GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t	   *verts;
	int			   *order;
	vec3_t			point;
	float			height, lheight;
	int				count;
	trace_t			downtrace;
	vec3_t			downmove;
	float			s1 = 0;
	float			c1 = 0;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *) ((Uint8 *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((Uint8 *) paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	if (r_shadows->value == 2)
	{
		/*
		 * better shadowing, now takes angle of ground into account cast a
		 * traceline into the floor directly below the player and gets
		 * normals from this
		 */
		VectorCopy (currententity->origin, downmove);
		downmove[2] = downmove[2] - 4096;
		memset (&downtrace, 0, sizeof(downtrace));
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1,
				currententity->origin, downmove, &downtrace);

		// calculate the all important angles to keep speed up
		s1 = Q_sin(currententity->angles[1] / 180 * M_PI);
		c1 = Q_cos(currententity->angles[1] / 180 * M_PI);
	}

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		} else
			qglBegin (GL_TRIANGLE_STRIP);

		do {
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0]
				+ paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1]
				+ paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2]
				+ paliashdr->scale_origin[2];

			if (r_shadows->value == 2)
			{
				point[0] -= shadevector[0] * point[0];
				point[1] -= shadevector[1] * point[1];
				point[2] -= shadevector[2] * point[2];

				// drop it down to floor
				point[2] = point[2] - (currententity->origin[2]
						- downtrace.endpos[2]);

				/*
				 * now adjust the point with respect to all the normals of
				 * the tracepoint
				 */
				point[2] += ((point[1] * (s1 * downtrace.plane.normal[0])) -
					(point[0] * (c1 * downtrace.plane.normal[0])) -
					(point[0] * (s1 * downtrace.plane.normal[1])) -
					(point[1] * (c1 * downtrace.plane.normal[1]))
					) + 20.2 - downtrace.plane.normal[2]*20.0;
			} else {
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
#endif




/*
=================
R_SetupAliasFrame

=================
*/
static void
R_SetupAliasFrame (aliashdr_t *paliashdr, entity_t *e)
{
	float				l, interval;
	int					pose_num, i;
	maliasframedesc_t	*frame;
	maliaspose_t		*pose;

	frame = &paliashdr->frames[e->frame];

	if (frame->numposes > 1) {
		interval = paliashdr->frames[e->frame].interval;
		pose_num = (int) (cl.time / interval) % frame->numposes;
	} else
		pose_num = 0;

	pose = &frame->poses[pose_num];

	for (i = 0; i < paliashdr->numverts; i++) {
		VectorCopy(pose->vertices[i].v, v_array[i]);
		tc_array[i][0] = paliashdr->tcarray[i].s;
		tc_array[i][1] = paliashdr->tcarray[i].t;

		l = shadedots[pose->normal_indices[i]] * shadelight;
		if (colorlights) {
			VectorScale(lightcolor, l, c_array[i]);
			c_array[i][3] = 1;
		} else
			VectorSet4(c_array[i], l, l, l, 1);
	}
}


/*
=================
R_DrawAliasModel

=================
*/
static void
R_DrawAliasModel (entity_t *e)
{
	int				i;
	int				lnum;
	float			add;
	model_t		   *clmodel = e->model;
	aliashdr_t	   *paliashdr;
	int				anim;
	int				texture, fb_texture, skinnum;
	dlight_t	   *l;

	if (gl_particletorches->value) {
		if (clmodel->modflags & (FLAG_TORCH1|FLAG_TORCH2)) {
			if (cl.time >= e->time_left) {
				R_Torch(e, clmodel->modflags & FLAG_TORCH2);
				e->time_left = cl.time + 0.10;
			}
			if (!(clmodel->modflags & FLAG_TORCH2) && mdl_fire)
				clmodel = mdl_fire;
			else
				return;
		}
	}

	if (e != &cl.viewent) {
		vec3_t      mins, maxs;

		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);

		if (R_CullBox (mins, maxs)) {
			e->pose1 = e->pose2 = 0;
			return;
		}
	}

	/*
	 * get lighting information
	 */
	if (!(clmodel->modflags & FLAG_FULLBRIGHT) || gl_fb_models->value)
	{
		ambientlight = shadelight = R_LightPoint (e->origin);

		// always give the gun some light
		if (!colorlights) {
			if (e == &cl.viewent && ambientlight < 24)
				ambientlight = shadelight = 24;
		} else {
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

			add = VectorDistance_fast (e->origin, l->origin);

			if (add <= l->radius * l->radius) {
				add = 8 * l->radius * l->radius / add; 

				if (!colorlights) {
					ambientlight += add;
					// ZOID: models should be affected by dlights as well
					shadelight += add;
				} else {
					lightcolor[0] += add * l->color[0];
					lightcolor[1] += add * l->color[1];
					lightcolor[2] += add * l->color[2];
				}
			}
		}

		if (!colorlights)
		{
			// clamp lighting so it doesn't overbright as much
			ambientlight = min (ambientlight, 128);
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
		} else {
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

		if (!e->last_light[0])
			e->last_light[0] = shadelight;
		else {
			shadelight = (shadelight + e->last_light[0])*0.5f;
			e->last_light[0] = shadelight;
		}
	} else {
		VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

		if (!e->last_light[0] && !e->last_light[1] && !e->last_light[2])
			VectorCopy (lightcolor, e->last_light);
		else {
			VectorAdd (lightcolor, e->last_light, lightcolor);
			VectorScale (lightcolor, 0.5f, lightcolor);
			VectorCopy (lightcolor, e->last_light);
		}

		shadelight = 1;
	}

	/*
	 * locate the proper data
	 */
	paliashdr = (aliashdr_t *) Mod_Extradata (clmodel);

	c_alias_polys += paliashdr->numtris;

	/*
	 * draw all the triangles
	 */
	qglPushMatrix ();

	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
	qglRotatef (e->angles[1], 0, 0, 1);
	qglRotatef (-e->angles[0], 0, 1, 0);
	qglRotatef (e->angles[2], 1, 0, 0);

	if ( clmodel->flags & FLAG_EYES ) {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
	} else {
		qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	}

	if ( clmodel->flags & FLAG_DOUBLESIZE ) {
		qglScalef (paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);
	} else {
		qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int) (cl.time * 10) & 3;
	skinnum = e->skinnum;
	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	/*
	 * we can't dynamically colormap textures, so they are cached seperately
	 * for the players.  Heads are just uncolored.
	 */
	if (e->colormap != vid.colormap && !gl_nocolors->value) {
		i = e - cl_entities;
		if (i >= 1 && i <= cl.maxclients)
			texture = playertextures - 1 + i;
	}

	if (gl_affinemodels->value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (!gl_fb_models->value)
		fb_texture = 0;

	qglBindTexture (GL_TEXTURE_2D, texture);

	R_SetupAliasFrame (paliashdr, e);

	TWI_PreVDrawCVA (0, paliashdr->numverts);

	qglEnableClientState (GL_COLOR_ARRAY);
	qglDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_INT,
			paliashdr->indices);
	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor3f (1, 1, 1);

	if (fb_texture) {
		qglBindTexture (GL_TEXTURE_2D, fb_texture);
		qglEnable (GL_BLEND);
		qglDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_INT,
				paliashdr->indices);
		qglDisable (GL_BLEND);
	}

	TWI_PostVDrawCVA ();
	v_index = 0;

	if (gl_affinemodels->value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix ();

#if 0
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
		GL_DrawAliasShadow (paliashdr, lastposenum);
		qglDisable (GL_BLEND);
		qglColor4f (1, 1, 1, 1);
		qglEnable (GL_TEXTURE_2D);
		qglPopMatrix ();
	}
#endif
}

//============================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
static void
R_DrawEntitiesOnList (void)
{
	int			i;

	if (!r_drawentities->value)
		return;

	for (i = 0; i < r_refdef.num_entities; i++) {
		currententity = r_refdef.entities[i];

		if (currententity->model->type == mod_brush)
			R_DrawBrushModel (currententity);
	}

	R_DrawSpriteModels ();

	for (i = 0; i < r_refdef.num_entities; i++) {
		currententity = r_refdef.entities[i];

		// LordHavoc: uhh, shouldn't this be done in the chase cam code?
		if (chase_active->value)
			if (currententity == &cl_entities[cl.viewentity])
				currententity->angles[0] *= 0.3;

		if (currententity->model->type == mod_alias)
			R_DrawAliasModel (currententity);
	}
}

/*
=============
R_DrawViewModel
=============
*/
static void
R_DrawViewModel (void)
{
	currententity = &cl.viewent;

	if (!r_drawviewmodel->value ||
		chase_active->value ||
		!r_drawentities->value ||
		cl.items & IT_INVISIBILITY ||
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
static void
R_PolyBlend (void)
{
	if (!gl_polyblend->value)
		return;
	if (v_blend[3] < 0.01f)
		return;

	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	// LordHavoc: replaced old polyblend code (breaks on ATI Radeon) with
	// darkplaces polyblend code (known to work on all cards)
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho  (0, 1, 1, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	qglColor4fv (v_blend);

	qglBegin (GL_TRIANGLES);
	qglVertex2f (-5, -5);
	qglVertex2f (10, -5);
	qglVertex2f (-5, 10);
	qglEnd ();

	qglColor3f (1, 1, 1);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
}


static int
SignbitsForPlane (mplane_t *out)
{
	int			bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


static void
R_SetFrustum (void)
{
	int			i;

	if (r_refdef.fov_x == 90) {
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorNormalize (frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);
		VectorNormalize (frustum[1].normal);
		VectorAdd (vpn, vup, frustum[2].normal);
		VectorNormalize (frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
		VectorNormalize (frustum[3].normal);
	} else {
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
	}

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
static void
R_SetupFrame (void)
{
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


static void
MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear,
		GLdouble zFar)
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
static void
R_SetupGL (void)
{
	float		screenaspect;
	unsigned	x, x2, y2, y, w, h;

	/*
	 * set up viewpoint
	 */ 
	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	x = r_refdef.vrect.x;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width);
	y = (vid.height - r_refdef.vrect.y);
	y2 = (vid.height -
		  (r_refdef.vrect.y + r_refdef.vrect.height));

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

	// put Z going up
	qglRotatef (-90, 1, 0, 0);
	qglRotatef (90, 0, 0, 1);
	qglRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	qglRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	qglRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	qglTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1],
				  -r_refdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	/*
	 * set drawing parms
	 */
	if (gl_cull->value)
		qglEnable (GL_CULL_FACE);
	else
		qglDisable (GL_CULL_FACE);

	qglDisable (GL_BLEND);
	qglEnable (GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
static void
R_Clear (void)
{
	if (gl_ztrick->value) {
		static int  trickframe;

		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1) {
			gldepthmin = 0.0f;
			gldepthmax = 0.49999f;
			qglDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1.0f;
			gldepthmax = 0.5f;
			qglDepthFunc (GL_GEQUAL);
		}
	} else {
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0.0f;
		gldepthmax = 1.0f;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}


/*
================
R_Render3DView

Called by R_RenderView, possibily repeatedly.
================
*/
void
R_Render3DView (void)
{
	// adds static entities to the list
	R_DrawWorld ();

	R_DrawEntitiesOnList ();

	R_DrawViewModel ();

	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

	R_DrawExplosions ();
	R_DrawParticles ();
	R_RenderDlights ();
	R_DrawWaterTextureChains ();

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_TRUE);
	qglDisable (GL_BLEND);
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
	double		time1 = 0.0;
	double		time2;

	if (r_norefresh->value)
		return;

	if (!cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		qglFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}
	else if (gl_finish->value)
		qglFinish ();

	R_Clear ();

	// render normal view
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MoveExplosions ();
	R_MoveParticles ();

	if (gl_wireframe != 1)
		R_Render3DView ();

	// don't let sound get messed up if going slow
	S_ExtraUpdate ();

	if (gl_wireframe) {
		if (gl_wireframe == 3)
			qglDisable (GL_DEPTH_TEST);
		else if (gl_wireframe == 2)
			qglEnable (GL_POLYGON_OFFSET_LINE);
		qglDepthMask (GL_FALSE);
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		qglDisable (GL_TEXTURE_2D);

		R_Render3DView ();

		qglEnable (GL_TEXTURE_2D);
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		qglDepthMask (GL_TRUE);
		if (gl_wireframe == 3)
			qglEnable (GL_DEPTH_TEST);
		else if (gl_wireframe == 2)
			qglDisable (GL_POLYGON_OFFSET_LINE);
	}
			
	R_PolyBlend ();

	if (r_speeds->value)
	{
		time2 = Sys_DoubleTime ();
		Com_Printf ("%3i ms  %4i wpoly %4i epoly\n",
					(int) ((time2 - time1) * 1000), c_brush_polys,
					c_alias_polys);
	}
}

