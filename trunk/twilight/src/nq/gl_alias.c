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

#include "render.h"
#include "client.h"
#include "cvar.h"

void R_DrawOpaqueAliasModels (entity_t *ents[], int num_ents, qboolean viewent);
extern vec3_t lightcolor;

extern void R_Torch (entity_t *ent, qboolean torch2);
extern model_t *mdl_fire;

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

/*
 * START OF NON-COMMON CODE.
 */

#include "host.h"

void
R_DrawViewModel (void)
{
	entity_t *ent_pointer;

	ent_pointer = &cl.viewent;
	if (!r_drawviewmodel->ivalue || chase_active->ivalue ||
			!r_drawentities->ivalue || cl.items & IT_INVISIBILITY ||
			(cl.stats[STAT_HEALTH] <= 0) || !cl.viewent.model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange (0.0f, 0.3f);
	R_DrawOpaqueAliasModels(&ent_pointer, 1, true);
	qglDepthRange (0.0f, 1.0f);
}

/*
=================
R_SetupAliasFrame

=================
*/
static void
R_SetupAliasFrame (aliashdr_t *paliashdr, entity_t *e)
{
	float				l;
	int					pose_num, i;
	maliasframedesc_t	*frame;
	maliaspose_t		*pose;

	frame = &paliashdr->frames[e->frame];

	if (frame->numposes > 1)
		pose_num = (int) (cl.time / frame->interval) % frame->numposes;
	else
		pose_num = 0;

	pose = &frame->poses[pose_num];

	for (i = 0; i < paliashdr->numverts; i++) {
		VectorCopy(pose->vertices[i].v, v_array_v(i));
		tc0_array(i, 0) = tc1_array(i, 0) = paliashdr->tcarray[i].s;
		tc0_array(i, 1) = tc1_array(i, 1) = paliashdr->tcarray[i].t;

		l = shadedots[pose->normal_indices[i]] * shadelight;
		VectorScale(lightcolor, l, cf_array_v(i));
		cf_array(i, 3) = 1;
	}
	TWI_FtoUB (cf_array_v(0), c_array_v(0), paliashdr->numverts * 4);
}

/*
=================
R_SetupAliasModel

=================
*/
static void
R_SetupAliasModel (entity_t *e, qboolean viewent)
{
	int			lnum;
	model_t		*clmodel = e->model;
	rdlight_t	*rd;
	vec3_t		dist;
	float		f;

	draw = false;

	if (gl_particletorches->ivalue) {
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

	c_alias_polys += paliashdr->numtris;

	skin = &paliashdr->skins[e->skinnum % paliashdr->numskins];
	anim = (int) (cl.time / skin->interval) % skin->frames;

	has_top = has_bottom = has_fb = false;

	if (e->colormap && !gl_nocolors->ivalue) {
		if ((has_top = !!skin->top[anim].indices.num))
			VectorCopy4 (e->colormap->top, top);
		if ((has_bottom = !!skin->bottom[anim].indices.num))
			VectorCopy4 (e->colormap->bottom, bottom);
	}

	has_fb = !!skin->fb[anim].indices.num;

	R_SetupAliasFrame (paliashdr, e);

	mod_origin = e->origin;
	mod_angles = e->angles;
	draw = true;
}
/*
 * END OF NON-COMMON CODE!
 */

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

	qglTranslatef (mod_origin[0], mod_origin[1], mod_origin[2]);
	qglRotatef (mod_angles[1], 0, 0, 1);
	qglRotatef (-mod_angles[0], 0, 1, 0);
	qglRotatef (mod_angles[2], 1, 0, 0);

	qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1],
			paliashdr->scale_origin[2]);

	qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	TWI_PreVDrawCVA (0, paliashdr->numverts);

	qglEnableClientState (GL_COLOR_ARRAY);

	if (!has_top || !has_bottom)
		R_DrawSubSkin (paliashdr, &skin->base[anim], NULL);
	else
		R_DrawSubSkin (paliashdr, &skin->base_team[anim], NULL);

	if (has_top || has_bottom || has_fb) {
		qglEnable (GL_BLEND);
		qglDepthMask (GL_FALSE);
		qglBlendFunc (GL_ONE, GL_ONE);
	}

	if (has_top)
		R_DrawSubSkin (paliashdr, &skin->top[anim], top);

	if (has_bottom)
		R_DrawSubSkin (paliashdr, &skin->bottom[anim], bottom);

	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor4fv (whitev);

	if (has_fb)
		R_DrawSubSkin (paliashdr, &skin->fb[anim], NULL);

	if (has_top || has_bottom || has_fb) {
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDepthMask (GL_TRUE);
		qglDisable (GL_BLEND);
	}

	TWI_PostVDrawCVA ();

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

	qglTranslatef (mod_origin[0], mod_origin[1], mod_origin[2]);
	qglRotatef (mod_angles[1], 0, 0, 1);
	qglRotatef (-mod_angles[0], 0, 1, 0);
	qglRotatef (mod_angles[2], 1, 0, 0);

	qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1],
			paliashdr->scale_origin[2]);

	qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	TWI_PreVDraw (0, paliashdr->numverts);

	if (has_fb)
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);

	if (has_fb) {
		if (!has_top && !has_bottom)
			R_DrawSubSkinNV (paliashdr, &skin->base_fb_i[anim],
					&skin->base[anim], &skin->fb[anim]);
		else
			R_DrawSubSkinNV (paliashdr, &skin->base_team_fb_i[anim],
					&skin->base_team[anim], &skin->fb[anim]);
	} else {
		if (!has_top && !has_bottom)
			R_DrawSubSkinNV (paliashdr, &skin->base[anim].indices,
					&skin->base[anim], NULL);
		else
			R_DrawSubSkinNV (paliashdr, &skin->base_team[anim].indices,
					&skin->base_team[anim], NULL);
	}

	if (has_fb)
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

	if (has_top || has_bottom) {
		qglEnable (GL_BLEND);
		qglDepthMask (GL_FALSE);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR0_NV, top);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

		qglCombinerParameterfvNV (GL_CONSTANT_COLOR1_NV, bottom);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_CONSTANT_COLOR1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);

		R_DrawSubSkinNV (paliashdr, &skin->top_bottom_i[anim], &skin->top[anim], &skin->bottom[anim]);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglDepthMask (GL_TRUE);
		qglDisable (GL_BLEND);
	}

	TWI_PostVDraw ();

	qglPopMatrix ();
}

//============================================================================

/*
=============
R_DrawOpaqueAliasModels
=============
*/
void
R_DrawOpaqueAliasModels (entity_t *ents[], int num_ents, qboolean viewent)
{
	int			i;
	entity_t	*e;

	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (gl_nv_register_combiners) {
		qglEnableClientState (GL_COLOR_ARRAY);
		qglEnable (GL_REGISTER_COMBINERS_NV);
		qglBlendFunc (GL_ONE, GL_ONE);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		qglCombinerInputNV (GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglCombinerInputNV (GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);

		qglCombinerOutputNV (GL_COMBINER0_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE);

		qglFinalCombinerInputNV (GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		qglFinalCombinerInputNV (GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA);
		for (i = 0; i < num_ents; i++) {
			e = ents[i];

			if (e->model->type == mod_alias) {
				R_SetupAliasModel (e, viewent);
				if (draw)
					R_DrawAliasModelNV ();
			}
		}

		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDisable (GL_REGISTER_COMBINERS_NV);
		qglDisableClientState (GL_COLOR_ARRAY);
		qglColor4fv (whitev);
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
		
	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}
