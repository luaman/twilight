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

#include "quakedef.h"
#include "client.h"
#include "cvar.h"
#include "glquake.h"
#include "opengl_ext.h"
#include "mathlib.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "r_explosion.h"
#include "host.h"

entity_t *currententity;
int r_framecount;						// used for dlight push checking
static mplane_t frustum[4];
int c_brush_polys, c_alias_polys;

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
cvar_t *r_dynamic;
cvar_t *r_novis;
cvar_t *r_netgraph;

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
	frame = e->cur.frame;

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

		time = cl.time;

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
			AngleVectors (e->cur.angles, v_forward, v_right, v_up);
			up = v_up;
			right = v_right;
		} else {
			// normal sprite
			up = vup;
			right = vright;
		}

		VectorSet2(tc_array_v(v_index + 0), 0, 1);
		VectorSet2(tc_array_v(v_index + 1), 0, 0);
		VectorSet2(tc_array_v(v_index + 2), 1, 0);
		VectorSet2(tc_array_v(v_index + 3), 1, 1);

		VectorTwiddle (e->cur.origin, up, f->down,	right, f->left, 1,
				v_array_v(v_index + 0));
		VectorTwiddle (e->cur.origin, up, f->up,	right, f->left, 1,
				v_array_v(v_index + 1));
		VectorTwiddle (e->cur.origin, up, f->up,	right, f->right, 1,
				v_array_v(v_index + 2));
		VectorTwiddle (e->cur.origin, up, f->down,	right, f->right, 1,
				v_array_v(v_index + 3));

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
float shadelight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float       r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
           ;

float *shadedots = r_avertexnormal_dots[0];
GLfloat acolors[MAX_VERTEX_ARRAYS][4];


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

	frame = &paliashdr->frames[e->to.frame];

	if (frame->numposes > 1)
		pose_num = (int) (cl.time / e->to.frame_interval) % frame->numposes;
	else
		pose_num = 0;

	pose = &frame->poses[pose_num];

	for (i = 0; i < paliashdr->numverts; i++) {
		VectorCopy(pose->vertices[i].v, v_array_v(i));
		tc_array(i, 0) = paliashdr->tcarray[i].s;
		tc_array(i, 1) = paliashdr->tcarray[i].t;

		l = shadedots[pose->normal_indices[i]] * shadelight;
		VectorScale(lightcolor, l, c_array_v(i));
		VectorScale(lightcolor, l, acolors[i]);
		c_array(i, 3) = acolors[i][3] = 1;
	}
}


/*
=================
R_SetupAliasBlendedFrame

fenix@io.com: model animation interpolation
=================
*/
static void
R_SetupAliasBlendedFrame (aliashdr_t *paliashdr, entity_t *e)
{
	float				l, d;
	int					i;
	int					pose_to_num, pose_from_num;
	maliasframedesc_t	*frame_from, *frame_to;
	maliaspose_t		*pose_from, *pose_to;
	vec3_t				v1, v2;


	frame_from = &paliashdr->frames[e->from.frame];

	if (frame_from->numposes > 1)
		pose_from_num = (int) (cl.time / e->from.frame_interval) % frame_from->numposes;
	else
		pose_from_num = 0;

	frame_to = &paliashdr->frames[e->to.frame];

	if (frame_to->numposes > 1)
		pose_to_num = (int) (cl.time / e->to.frame_interval) % frame_to->numposes;
	else
		pose_to_num = 0;

	pose_from = &frame_from->poses[pose_from_num];
	pose_to = &frame_to->poses[pose_to_num];

	for (i = 0; i < paliashdr->numverts; i++) {
		VectorCopy(pose_from->vertices[i].v, v1);
		VectorCopy(pose_to->vertices[i].v, v2);
		Lerp_Vectors (v1, e->frame_blend, v2, v_array_v(i));
		tc_array(i, 0) = paliashdr->tcarray[i].s;
		tc_array(i, 1) = paliashdr->tcarray[i].t;

		d = shadedots[pose_to->normal_indices[i]] -
			shadedots[pose_from->normal_indices[i]];
		l = shadelight * (shadedots[pose_from->normal_indices[i]]
				+ (e->frame_blend * d));
		VectorScale(lightcolor, l, c_array_v(i));
		VectorScale(lightcolor, l, acolors[i]);
		c_array(i, 3) = acolors[i][3] = 1;
	}
}

/*
=================
R_DrawSubSkin

=================
*/
void
R_DrawSubSkin (aliashdr_t *paliashdr, skin_sub_t *skin, vec3_t *color)
{
	int			i;

	if (color) {
		TWI_PreVDrawCVA (0, paliashdr->numverts);
		for (i = 0; i < paliashdr->numverts; i++) {
			VectorMultiply(acolors[i], *color, c_array_v(i));
		}
		TWI_PostVDrawCVA ();
	}

	qglBindTexture (GL_TEXTURE_2D, skin->texnum);
	qglDrawRangeElements(GL_TRIANGLES, 0, paliashdr->numverts,
			skin->num_indices, GL_UNSIGNED_INT, skin->indices);
}

/*
=================
R_DrawAliasModel

=================
*/
static void
R_DrawAliasModel (entity_t *e, qboolean viewent)
{
	int				lnum, anim;
	model_t			*clmodel = e->model;
	aliashdr_t		*paliashdr;
	rdlight_t		*rd;
	skin_t			*skin;
	vec3_t			top, bottom, dist;
	float			f;
	qboolean		has_top = false, has_bottom = false, has_fb = false;

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

		Mod_MinsMaxs (clmodel, e->cur.origin, e->cur.angles, mins, maxs);

		if (R_CullBox (mins, maxs)) {
			return;
		}
	} 

	/*
	 * get lighting information
	 */
	if (!(clmodel->modflags & FLAG_FULLBRIGHT) || gl_fb_models->ivalue) {
		shadelight = R_LightPoint (e->cur.origin);

		// always give the gun some light
		if (viewent) {
			lightcolor[0] = max (lightcolor[0], 24);
			lightcolor[1] = max (lightcolor[1], 24);
			lightcolor[2] = max (lightcolor[2], 24);
		}

		for (lnum = 0; lnum < r_numdlights; lnum++)
		{
			rd = r_dlight + lnum;
			VectorSubtract (e->cur.origin, rd->origin, dist);
			f = DotProduct (dist, dist) + LIGHTOFFSET;
			if (f < rd->cullradius2)
			{
				f = (1.0f / f) - rd->lightsubtract;
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
	} else if ((clmodel->modflags & FLAG_FULLBRIGHT) && !gl_fb_models->ivalue) {
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
	}

	shadedots = r_avertexnormal_dots[((int) (e->cur.angles[1]
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
	paliashdr = (aliashdr_t *) Mod_Extradata (clmodel);

	c_alias_polys += paliashdr->numtris;

	/*
	 * draw all the triangles
	 */
	qglPushMatrix ();

	if (gl_im_transform->ivalue && !(clmodel->modflags & FLAG_NO_IM_FORM)) {
		qglTranslatef (e->cur.origin[0], e->cur.origin[1], e->cur.origin[2]);
		qglRotatef (e->cur.angles[1], 0, 0, 1);
		qglRotatef (-e->cur.angles[0], 0, 1, 0);
		qglRotatef (e->cur.angles[2], 1, 0, 0);
	} else {
		qglTranslatef (e->to.origin[0], e->to.origin[1], e->to.origin[2]);
		qglRotatef (e->to.angles[1], 0, 0, 1);
		qglRotatef (-e->to.angles[0], 0, 1, 0);
		qglRotatef (e->to.angles[2], 1, 0, 0);
	}

	qglTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1],
			paliashdr->scale_origin[2]);

	qglScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	skin = &paliashdr->skins[e->skinnum % paliashdr->numskins];
	anim = (int) (cl.time / skin->interval) % skin->frames;

	if (e->skin)
		skin = e->skin;

	if (e->colormap && !gl_nocolors->ivalue) {
		if ((has_top = !!skin->top[anim].num_indices))
			VectorCopy(e->colormap->top, top);
		if ((has_bottom = !!skin->bottom[anim].num_indices))
			VectorCopy(e->colormap->bottom, bottom);
	}

	if (gl_fb_models->ivalue)
		has_fb = !!skin->fb[anim].num_indices;

	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (gl_im_animation->ivalue && !(clmodel->modflags & FLAG_NO_IM_ANIM))
		R_SetupAliasBlendedFrame (paliashdr, e);
	else
		R_SetupAliasFrame (paliashdr, e);

	TWI_PreVDrawCVA (0, paliashdr->numverts);

	qglEnableClientState (GL_COLOR_ARRAY);

	if (!has_fb && !has_top && !has_bottom)
		R_DrawSubSkin (paliashdr, &skin->raw[anim], NULL);
	else if (!has_top || !has_bottom)
		R_DrawSubSkin (paliashdr, &skin->normal[anim], NULL);
	else
		R_DrawSubSkin (paliashdr, &skin->base[anim], NULL);

	if (has_top || has_bottom || has_fb) {
		qglEnable (GL_BLEND);
		qglDepthMask (GL_FALSE);
		qglBlendFunc (GL_ONE, GL_ONE);
	}

	if (has_top)
		R_DrawSubSkin (paliashdr, &skin->top[anim], &top);

	if (has_bottom)
		R_DrawSubSkin (paliashdr, &skin->bottom[anim], &bottom);

	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor3f (1, 1, 1);

	if (has_fb)
		R_DrawSubSkin (paliashdr, &skin->fb[anim], NULL);

	if (has_top || has_bottom || has_fb) {
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglDepthMask (GL_TRUE);
		qglDisable (GL_BLEND);
	}

	TWI_PostVDrawCVA ();

	if (gl_affinemodels->ivalue)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	qglPopMatrix ();
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

	if (!r_drawentities->ivalue)
		return;

	for (i = 0; i < r_refdef.num_entities; i++) {
		currententity = r_refdef.entities[i];

		if (currententity->model->type == mod_brush)
			R_DrawBrushModel (currententity);
	}

	R_DrawSpriteModels ();

	for (i = 0; i < r_refdef.num_entities; i++) {
		currententity = r_refdef.entities[i];

		if (currententity->model->type == mod_alias)
			R_DrawAliasModel (currententity, false);
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
	cl.viewent.times++;

	currententity = &cl.viewent;

	if (!r_drawviewmodel->ivalue ||
		!Cam_DrawViewModel () ||
		!r_drawentities->ivalue ||
		(cl.stats[STAT_ITEMS] & IT_INVISIBILITY) ||
		(cl.stats[STAT_HEALTH] <= 0) ||
		!cl.viewent.model) {
		return;
	}

	CL_Update_Origin(&cl.viewent, cl.viewent_origin, cls.realtime);
	CL_Update_Angles(&cl.viewent, cl.viewent_angles, cls.realtime);
	CL_UpdateAndLerp_Frame(&cl.viewent, cl.viewent_frame, cls.realtime);

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity, true);
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
	if (!gl_polyblend->ivalue)
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
		VectorNormalizeFast (frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);
		VectorNormalizeFast (frustum[1].normal);
		VectorAdd (vpn, vup, frustum[2].normal);
		VectorNormalizeFast (frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
		VectorNormalizeFast (frustum[3].normal);
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
	// don't allow cheats in multiplayer
	// FIXME: do this differently/elsewhere
	if (!Q_atoi (Info_ValueForKey (cl.serverinfo, "watervis")))
		if (r_wateralpha->ivalue != 1)
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
	if (gl_cull->ivalue)
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
	if (gl_ztrick->ivalue) {
		static int  trickframe;

		if (gl_clear->ivalue)
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
		if (gl_clear->ivalue)
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

	if (r_norefresh->ivalue)
		return;

	if (!cl.worldmodel)
		Host_EndGame ("R_RenderView: NULL worldmodel");

	if (r_speeds->ivalue)
	{
		qglFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}
	else if (gl_finish->ivalue)
		qglFinish ();

	R_Clear ();

	// render normal view
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MoveExplosions ();
	R_MoveParticles ();

	R_Render3DView ();

	// don't let sound get messed up if going slow
	S_ExtraUpdate ();

	R_PolyBlend ();

	if (r_speeds->ivalue)
	{
		time2 = Sys_DoubleTime ();
		Com_Printf ("%3i ms  %4i wpoly %4i epoly\n",
					(int) ((time2 - time1) * 1000), c_brush_polys,
					c_alias_polys);
	}
}

