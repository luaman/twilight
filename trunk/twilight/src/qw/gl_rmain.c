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
#include "cmd.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "r_explosion.h"
#include "host.h"

// FIXME - These need to be in a header somewhere
extern void TNT_Init (void);
extern void R_InitBubble (void);
extern void R_SkyBoxChanged (cvar_t *cvar);

entity_t *currententity;
int r_framecount;						// used for dlight push checking
static mplane_t frustum[4];
int c_brush_polys, c_alias_polys;

memzone_t *vzone;

texcoord_t	*tc0_array_p;
texcoord_t	*tc1_array_p;
vertex_t	*v_array_p;
colorf_t	*cf_array_p;
colorub_t	*cub_array_p;
GLuint		*vindices;
float_int_t	*FtoUB_tmp;

GLuint v_index, i_index;
qboolean va_locked;

/*
 * view origin
 */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

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
cvar_t *r_stainmaps;
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
		VectorScale(lightcolor, l, acolors[i]);
		acolors[i][3] = 1;
	}
	TWI_FtoUB (acolors[0], c_array_v(0), paliashdr->numverts * 4);
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
		VectorScale(lightcolor, l, acolors[i]);
		acolors[i][3] = 1;
	}
	TWI_FtoUB (acolors[0], c_array_v(0), paliashdr->numverts * 4);
}

/*
=================
R_DrawSubSkin

=================
*/
void
R_DrawSubSkin (aliashdr_t *paliashdr, skin_sub_t *skin, vec4_t *color)
{
	int			i;

	if (color) {
		for (i = 0; i < paliashdr->numverts; i++)
			VectorMultiply(acolors[i], *color, cf_array_v(i));
		TWI_FtoUB (cf_array_v(0), c_array_v(0), paliashdr->numverts * 4);
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
	vec4_t			top, bottom;
	vec3_t			dist;
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
			VectorCopy4 (e->colormap->top, top);
		if ((has_bottom = !!skin->bottom[anim].num_indices))
			VectorCopy4 (e->colormap->bottom, bottom);
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
	qglColor4fv (whitev);

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
	qglDepthRange (0.0f, 0.3f);
	R_DrawAliasModel (currententity, true);
	qglDepthRange (0.0f, 1.0f);
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

	qglColor4fv (whitev);
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


/*
=============
R_SetupGL
=============
*/
static void
R_SetupGL (void)
{
	GLdouble    xmax, ymax;

	/*
	 * set up viewpoint
	 */ 
	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();

	qglViewport (0, 0, vid.width, vid.height);

	xmax = Q_tan (r_refdef.fov_x * M_PI / 360.0) * (vid.width / vid.height);
	ymax = Q_tan (r_refdef.fov_y * M_PI / 360.0);

	qglFrustum (-xmax, xmax, -ymax, ymax, 1.0f, 8192.0);

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
	if (gl_clear->ivalue)
		qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		qglClear (GL_DEPTH_BUFFER_BIT);
	qglDepthFunc (GL_LEQUAL);
}


// XXX
extern void R_DrawSkyBox (void);
/*
================
R_Render3DView

Called by R_RenderView, possibily repeatedly.
================
*/
void
R_Render3DView (void)
{
	R_DrawSkyBox ();

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



/*
==================
R_InitTextures
==================
*/
void
R_InitTextures (void)
{
	int         x, y, m;
	Uint8      *dest;

// create a simple checkerboard texture for the default
	r_notexture_mip =
		Hunk_AllocName (sizeof (texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2,
						"notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++) {
		dest = (Uint8 *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

/*
===============
R_Init_Cvars
===============
*/
void
R_Init_Cvars (void)
{
	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, NULL);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL);
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_NONE, NULL);
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL);
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_ARCHIVE, NULL);
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
	r_waterripple = Cvar_Get ("r_waterripple", "0", CVAR_NONE, NULL);
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL);
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);
	r_stainmaps = Cvar_Get ("r_stainmaps", "1", CVAR_ARCHIVE, NULL);
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL);

	r_skyname = Cvar_Get ("r_skyname", "", CVAR_NONE, &R_SkyBoxChanged);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, NULL);

	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_ARCHIVE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, NULL);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", CVAR_NONE, NULL);
	gl_flashblend = Cvar_Get ("gl_flashblend", "1", CVAR_ARCHIVE, NULL);
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_NONE, NULL);

	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_ARCHIVE, NULL);
	gl_im_transform = Cvar_Get ("gl_im_transform", "1", CVAR_ARCHIVE, NULL);

	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_ARCHIVE, NULL);
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_ARCHIVE, NULL);

	gl_oldlights = Cvar_Get ("gl_oldlights", "0", CVAR_NONE, NULL);

	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_NONE, NULL);

	gl_particletorches = Cvar_Get ("gl_particletorches", "0", CVAR_ARCHIVE, NULL);
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

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
static void
R_TimeRefresh_f (void)
{
	int         i;
	float       start, stop, time;

	if (cls.state != ca_active)
		return;

	qglFinish ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
		GL_EndRendering ();
	}

	qglFinish ();
	stop = Sys_DoubleTime ();
	time = stop - start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_EndRendering ();
}

/*
===============
R_Init
===============
*/
void
R_Init (void)
{
	vzone = Zone_AllocZone ("Vertex Arrays");

	Cmd_AddCommand ("timerefresh", &R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", &R_ReadPointFile_f);
	Cmd_AddCommand ("loadsky", &R_LoadSky_f);

	R_InitBubble ();
	R_InitParticles ();
	TNT_Init ();
	R_Explosion_Init ();
	R_InitSurf ();

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	skyboxtexnum = texture_extension_number;
	texture_extension_number += 6;

	tc0_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(texcoord_t));
	tc1_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(texcoord_t));
	v_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(vertex_t));
	vindices = Zone_Alloc(vzone, MAX_VERTEX_INDICES * sizeof(GLuint));
	cf_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorf_t));
	cub_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(colorub_t));
	FtoUB_tmp = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(float_int_t) * 4);

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc0_array_v(0)), tc0_array_p);
	qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(c_array_v(0)), cub_array_p);
	qglVertexPointer (3, GL_FLOAT, sizeof(v_array_v(0)), v_array_p);

	qglDisableClientState (GL_COLOR_ARRAY);
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);

	if (gl_mtex) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(tc1_array_v(0)), tc1_array_p);
		qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
}

/*
===============
R_NewMap
===============
*/
void
R_NewMap (void)
{
	Uint32			i;
	extern Sint32	r_dlightframecount;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_viewleaf = NULL;
	R_ClearParticles ();

	memset (&cl_network_entities, 0, sizeof(cl_network_entities));
	memset (&cl_player_entities, 0, sizeof(cl_player_entities));
	memset (&cl_static_entities, 0, sizeof(cl_static_entities));
	cl_num_static_entities = 0;

	r_dlightframecount = 0;

	GL_BuildLightmaps ();

	// identify sky texture
	skytexturenum = -1;
	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp (cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}

	// some Cvars need resetting on map change
	Cvar_Set (r_skyname, "");

	// Parse map entities
	CL_ParseEntityLump (cl.worldmodel->entities);

	r_explosion_newmap ();
}

