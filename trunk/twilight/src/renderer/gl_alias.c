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

#include "cvar.h"
#include "sys.h"
#include "gl_info.h"
#include "gl_arrays.h"
#include "model.h"
#include "light.h"
#include "quakedef.h"
#include "r_part.h"
#include "vis.h"
#include "matrixlib.h"
#include "palette.h"
#include "gl_light.h"

void R_DrawOpaqueAliasModels (entity_common_t *ents[],int num_ents, qboolean viewent);

#define NUMVERTEXNORMALS	162
float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static float shadelight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static float       r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
           ;

static float *shadedots = r_avertexnormal_dots[0];

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

// These variables are passed between the setup code, and the renderer.
static int			anim;
static qboolean		has_top = false, has_bottom = false, has_fb = false, draw;
static vec4_t		top, bottom;
static skin_t		*skin;
static vec_t		*mod_origin, *mod_angles;
static aliashdr_t	*paliashdr;
static matrix4x4_t	*matrix;

#include "host.h"

static void
R_SetupAliasFrame (aliashdr_t *paliashdr, entity_common_t *e)
{
	float				l;
	int					pose_num, i;
	maliasframedesc_t	*frame;
	maliaspose_t		*pose;

	frame = &paliashdr->frames[e->frame[0]];

	if (frame->numposes > 1)
		pose_num = (int) (r_time / frame->interval) % frame->numposes;
	else
		pose_num = 0;

	pose = &frame->poses[pose_num];

	for (i = 0; i < paliashdr->numverts; i++) {
		VectorCopy(pose->vertices[i].v, v_array_v(i));

		l = shadedots[pose->normal_indices[i]] * shadelight;
		VectorScale(lightcolor, l, cf_array_v(i));
		cf_array(i, 3) = 1;
	}
	TWI_FtoUB (cf_array_v(0), c_array_v(0), paliashdr->numverts * 4);
}

/*
=================
Please forgive me for the duplicated code here..
 -- Zephaniah E. Hull.
=================
*/
static void
R_SetupAliasBlendedFrame (aliashdr_t *paliashdr, entity_common_t *e)
{
	float				d, frac;
	int					i1, i2, i, j;
	maliaspose_t		*poses[4];
	float				fracs[4];
	int					num_frames = 0;
	maliasframedesc_t	*frame;

	for (i = 0; i < 2; i++) {
		if (e->frame_frac[i] < (1.0/65536.0))
			continue;
		frame = &paliashdr->frames[e->frame[i]];
		if (frame->numposes > 1) {
			i1 = (int) (r_time / e->frame_interval[i]) % frame->numposes;
			frac = (r_time / e->frame_interval[i]);
			frac -= floor(frac);
			i2 = (i1 + 1) % frame->numposes;
			poses[num_frames] = &frame->poses[i1];
			fracs[num_frames] = (1 - frac) * e->frame_frac[i];
			if (fracs[num_frames] > (1.0/65536.0))
				num_frames++;
			poses[num_frames] = &frame->poses[i2];
			fracs[num_frames] = frac * e->frame_frac[i];
			if (fracs[num_frames] > (1.0/65536.0))
				num_frames++;
		} else {
			poses[num_frames] = &frame->poses[0];
			fracs[num_frames++] = e->frame_frac[i];
		}
	}

	switch (num_frames) {
		case 0:
			Sys_Error("Eik! %s\n", e->model->name);
			return; // Never reached.
		case 1:
			for (i = 0; i < paliashdr->numverts; i++) {
				v_array(i, 0) = poses[0]->vertices[i].v[0];
				v_array(i, 1) = poses[0]->vertices[i].v[1];
				v_array(i, 2) = poses[0]->vertices[i].v[2];

				d = shadedots[poses[0]->normal_indices[i]];
				d *= shadelight;
				VectorScale (lightcolor, d, cf_array_v(i));
				cf_array(i, 3) = 1;
			}
			break;
		case 2:
			for (i = 0; i < paliashdr->numverts; i++) {
				v_array(i, 0) = poses[0]->vertices[i].v[0] * fracs[0];
				v_array(i, 0) += poses[1]->vertices[i].v[0] * fracs[1];
				v_array(i, 1) = poses[0]->vertices[i].v[1] * fracs[0];
				v_array(i, 1) += poses[1]->vertices[i].v[1] * fracs[1];
				v_array(i, 2) = poses[0]->vertices[i].v[2] * fracs[0];
				v_array(i, 2) += poses[1]->vertices[i].v[2] * fracs[1];

				d = shadedots[poses[0]->normal_indices[i]] * fracs[0];
				d += shadedots[poses[1]->normal_indices[i]] * fracs[1];
				d *= shadelight;
				VectorScale (lightcolor, d, cf_array_v(i));
				cf_array(i, 3) = 1;
			}
			break;
		case 3:
			for (i = 0; i < paliashdr->numverts; i++) {
				v_array(i, 0) = poses[0]->vertices[i].v[0] * fracs[0];
				v_array(i, 0) += poses[1]->vertices[i].v[0] * fracs[1];
				v_array(i, 0) += poses[2]->vertices[i].v[0] * fracs[2];
				v_array(i, 1) = poses[0]->vertices[i].v[1] * fracs[0];
				v_array(i, 1) += poses[1]->vertices[i].v[1] * fracs[1];
				v_array(i, 1) += poses[2]->vertices[i].v[1] * fracs[2];
				v_array(i, 2) = poses[0]->vertices[i].v[2] * fracs[0];
				v_array(i, 2) += poses[1]->vertices[i].v[2] * fracs[1];
				v_array(i, 2) += poses[2]->vertices[i].v[2] * fracs[2];

				d = shadedots[poses[0]->normal_indices[i]] * fracs[0];
				d += shadedots[poses[1]->normal_indices[i]] * fracs[1];
				d += shadedots[poses[2]->normal_indices[i]] * fracs[2];
				d *= shadelight;
				VectorScale (lightcolor, d, cf_array_v(i));
				cf_array(i, 3) = 1;
			}
			break;
		default:
			for (i = 0; i < paliashdr->numverts; i++) {
				v_array(i, 0) = 0;
				v_array(i, 1) = 0;
				v_array(i, 2) = 0;
				d = 0;
				for (j = 0; j < num_frames; j++) {
					v_array(i, 0) += poses[j]->vertices[i].v[0] * fracs[j];
					v_array(i, 1) += poses[j]->vertices[i].v[1] * fracs[j];
					v_array(i, 2) += poses[j]->vertices[i].v[2] * fracs[j];
					d += shadedots[poses[j]->normal_indices[i]] * fracs[j];
				}

				d *= shadelight;
				VectorScale (lightcolor, d, cf_array_v(i));
				cf_array(i, 3) = 1;
			}
			break;
	}
	TWI_FtoUB (cf_array_v(0), c_array_v(0), paliashdr->numverts * 4);
}


static void
R_SetupAliasModel (entity_common_t *e, qboolean viewent)
{
	int			lnum;
	model_t		*clmodel = e->model;
	rdlight_t	*rd;
	vec3_t		dist;
	float		f;
	extern model_t *mdl_fire;

	draw = false;

	if (gl_particletorches->ivalue) {
		if (clmodel->modflags & (FLAG_TORCH1|FLAG_TORCH2)) {
			if (r_time >= e->time_left) {
				R_Torch(e, clmodel->modflags & FLAG_TORCH2);
				e->time_left = r_time + 0.10;
			}
			if (!(clmodel->modflags & FLAG_TORCH2) && mdl_fire)
				clmodel = mdl_fire;
			else
				return;
		}
	}

	if (!viewent) {
		vec3_t      mins, maxs;

		Mod_MinsMaxs (clmodel, e->origin, e->angles, mins, maxs);

		if (Vis_CullBox (mins, maxs)) {
			return;
		}
	} 

	/*
	 * get lighting information
	 */
	if (!(clmodel->modflags & FLAG_FULLBRIGHT) || gl_fb->ivalue) {
		shadelight = R_LightPoint (e->origin);

		// always give the gun some light
		if (viewent) {
			lightcolor[0] = max (lightcolor[0], 24);
			lightcolor[1] = max (lightcolor[1], 24);
			lightcolor[2] = max (lightcolor[2], 24);
		}

		for (lnum = 0; lnum < r_numdlights; lnum++)
		{
			rd = r_dlight + lnum;
			VectorSubtract (e->origin, rd->origin, dist);
			f = DotProduct (dist, dist) + LIGHTOFFSET;
			if (f < rd->cullradius2)
			{
				f = ((1.0f / f) - rd->lightsubtract) * 200.0f;
				if (f > 0)
					VectorMA (lightcolor, f, rd->light, lightcolor);
			}
		}

		// ZOID: never allow players to go totally black
		if (clmodel->modflags & FLAG_PLAYER) {
			lightcolor[0] = max (lightcolor[0], 8);
			lightcolor[1] = max (lightcolor[1], 8);
			lightcolor[2] = max (lightcolor[2], 8);
		}
	} else if ((clmodel->modflags & FLAG_FULLBRIGHT) && !gl_fb->ivalue)
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;

	shadedots = r_avertexnormal_dots[((int) (e->angles[1]
				* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

	if (!e->last_light[0] && !e->last_light[1] && !e->last_light[2])
		VectorCopy (lightcolor, e->last_light);
	else {
		VectorAdd (lightcolor, e->last_light, lightcolor);
		VectorScale (lightcolor, 0.5f, lightcolor);
		VectorCopy (lightcolor, e->last_light);
	}

	shadelight = 1;

	/*
	 * locate the proper data
	 */
	paliashdr = clmodel->alias;
	matrix = &e->matrix;

//	c_alias_polys += paliashdr->numtris;

	if (!(skin = e->skin))
		skin = &paliashdr->skins[e->skinnum % paliashdr->numskins];
	anim = (int) (r_time / skin->interval) % skin->frames;

	has_top = has_bottom = has_fb = false;

	if (e->colormap && !gl_nocolors->ivalue) {
		if ((has_top = !!skin->top[anim].indices.num))
			VectorCopy4 (e->colormap->top, top);
		if ((has_bottom = !!skin->bottom[anim].indices.num))
			VectorCopy4 (e->colormap->bottom, bottom);
	}

	has_fb = !!skin->fb[anim].indices.num;

	if (gl_im_animation->ivalue && !(clmodel->modflags & FLAG_NO_IM_ANIM))
		R_SetupAliasBlendedFrame (paliashdr, e);
	else
		R_SetupAliasFrame (paliashdr, e);

	mod_origin = e->origin;
	mod_angles = e->angles;
	draw = true;
}

static void
R_DrawSubSkin (aliashdr_t *paliashdr, skin_sub_t *skin, vec4_t color)
{
	if (color)
		TWI_FtoUBMod(cf_array_v(0), c_array_v(0), color, paliashdr->numverts*4);

	qglBindTexture (GL_TEXTURE_2D, skin->texnum);
	qglDrawRangeElements(GL_TRIANGLES, 0, paliashdr->numverts,
			skin->indices.num, GL_UNSIGNED_INT, skin->indices.i);
}

static void
R_DrawAliasModel ()
{
	qglPushMatrix ();

	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), paliashdr->tcarray);

	qglMultTransposeMatrixf ((GLfloat *) matrix);

	TWI_PreVDrawCVA (0, paliashdr->numverts);

	if (!has_top && !has_bottom)
		R_DrawSubSkin (paliashdr, &skin->base[anim], NULL);
	else
		R_DrawSubSkin (paliashdr, &skin->base_team[anim], NULL);

	TWI_PostVDrawCVA ();

	if (has_top || has_bottom || has_fb) {
		qglEnable (GL_BLEND);
		qglDepthMask (GL_FALSE);
		TWI_PreVDraw (0, paliashdr->numverts);
	}

	if (has_top)
		R_DrawSubSkin (paliashdr, &skin->top[anim], top);

	if (has_bottom)
		R_DrawSubSkin (paliashdr, &skin->bottom[anim], bottom);

	if (has_fb) {
		qglDisableClientState (GL_COLOR_ARRAY);
		qglColor4fv (whitev);

		R_DrawSubSkin (paliashdr, &skin->fb[anim], NULL);

		qglEnableClientState (GL_COLOR_ARRAY);
	}

	if (has_top || has_bottom || has_fb) {
		TWI_PostVDraw ();
		qglDepthMask (GL_TRUE);
		qglDisable (GL_BLEND);
	}

	GLArrays_Reset_TC (false);

	qglPopMatrix ();
}

static void
R_DrawSubSkinNV (aliashdr_t *paliashdr, skin_indices_t *ind, skin_sub_t *s0,
		skin_sub_t *s1)
{
	qglBindTexture (GL_TEXTURE_2D, s0->texnum);
	if (s1) {
		qglActiveTextureARB (GL_TEXTURE1_ARB);
		qglEnable (GL_TEXTURE_2D);
		qglBindTexture (GL_TEXTURE_2D, s1->texnum);
	}
	qglDrawRangeElements(GL_TRIANGLES, 0, paliashdr->numverts,
			ind->num, GL_UNSIGNED_INT, ind->i);
	if (s1) {
		qglDisable (GL_TEXTURE_2D);
		qglActiveTextureARB (GL_TEXTURE0_ARB);
	}
}

static void
R_DrawAliasModelNV ()
{
	qglPushMatrix ();

	qglClientActiveTextureARB(GL_TEXTURE1_ARB);
	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), paliashdr->tcarray);
	qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), paliashdr->tcarray);

	qglMultTransposeMatrixf ((GLfloat *) matrix);

	TWI_PreVDrawCVA (0, paliashdr->numverts);

	if (has_fb) {
		qglCombinerOutputNV (GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		qglFinalCombinerInputNV (GL_VARIABLE_D_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		if (!has_top && !has_bottom)
			R_DrawSubSkinNV (paliashdr, &skin->base_fb_i[anim],
					&skin->base[anim], &skin->fb[anim]);
		else
			R_DrawSubSkinNV (paliashdr, &skin->base_team_fb_i[anim],
					&skin->base_team[anim], &skin->fb[anim]);
		qglFinalCombinerInputNV (GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
	} else {
		qglCombinerOutputNV (GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);
		if (!has_top && !has_bottom)
			R_DrawSubSkinNV (paliashdr, &skin->base[anim].indices,
					&skin->base[anim], NULL);
		else
			R_DrawSubSkinNV (paliashdr, &skin->base_team[anim].indices,
					&skin->base_team[anim], NULL);
	}

	if (has_top || has_bottom) {
		qglEnable (GL_BLEND);
		qglDepthMask (GL_FALSE);

		qglCombinerOutputNV (GL_COMBINER0_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR0_NV, top);
		qglCombinerParameterfvNV (GL_CONSTANT_COLOR1_NV, bottom);

		R_DrawSubSkinNV (paliashdr, &skin->top_bottom_i[anim], &skin->top[anim], &skin->bottom[anim]);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR0_NV, whitev);
		qglCombinerParameterfvNV (GL_CONSTANT_COLOR1_NV, whitev);

		qglDepthMask (GL_TRUE);
		qglDisable (GL_BLEND);
	}

	TWI_PostVDrawCVA ();

	GLArrays_Reset_TC (true);

	qglPopMatrix ();
}

//============================================================================

void
R_DrawOpaqueAliasModels (entity_common_t *ents[],int num_ents, qboolean viewent)
{
	int				i;
	entity_common_t	*e;

	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	qglEnableClientState (GL_COLOR_ARRAY);
	qglBlendFunc (GL_ONE, GL_ONE);

	if (gl_nv_register_combiners) {
		qglEnable (GL_REGISTER_COMBINERS_NV);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR0_NV, whitev);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR1_NV, whitev);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);

//		qglCombinerOutputNV (GL_COMBINER0_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		qglFinalCombinerInputNV (GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		for (i = 0; i < num_ents; i++) {
			e = ents[i];

			if (e->model->type == mod_alias) {
				R_SetupAliasModel (e, viewent);
				if (draw)
					R_DrawAliasModelNV ();
			}
		}

		qglDisable (GL_REGISTER_COMBINERS_NV);
	} else {
		for (i = 0; i < num_ents; i++) {
			e = ents[i];

			if (e->model->type == mod_alias) {
				R_SetupAliasModel (e, viewent);
				if (draw)
					R_DrawAliasModel ();
			}
		}
	}
		
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor4fv (whitev);
	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}
