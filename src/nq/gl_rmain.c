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
#include "draw.h"
#include "gl_textures.h"
#include "host.h"
#include "image.h"
#include "mathlib.h"
#include "r_explosion.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "view.h"
#include "liquid.h"

// FIXME - These need to be in a header somewhere
extern void TNT_Init (void);
extern void R_InitBubble (void);
void R_DrawViewModel (void);
void R_DrawAliasModels (entity_t *ents[], int num_ents, qboolean viewent);

entity_t *currententity;
static mplane_t frustum[4];
Uint c_brush_polys, c_alias_polys;

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

texture_t *r_notexture;
texture_t *r_notexture_water;

int d_lightstylevalue[256];				// 8.8 fraction of base light value


void R_Torch (entity_t *ent, qboolean torch2);

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawviewmodel;
cvar_t *r_speeds;
cvar_t *r_shadows;
cvar_t *r_wireframe;
cvar_t *r_dynamic;
cvar_t *r_novis;
cvar_t *r_stainmaps;

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
cvar_t *gl_varray_size;
cvar_t *gl_iarray_size;

extern vec3_t lightcolor;
qboolean colorlights = true;

//static float shadescale = 0.0;

int gl_wireframe = 0;

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

	if (((Uint) frame >= psprite->numframes) || (frame < 0)) {
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
	Uint				i;
	int					last_tex;

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

		VectorSet2(tc_array_v(v_index + 0), 0, 1);
		VectorSet2(tc_array_v(v_index + 1), 0, 0);
		VectorSet2(tc_array_v(v_index + 2), 1, 0);
		VectorSet2(tc_array_v(v_index + 3), 1, 1);

		VectorTwiddle (e->origin, up, f->down,	right, f->left, 1,
				v_array_v(v_index + 0));
		VectorTwiddle (e->origin, up, f->up,	right, f->left, 1,
				v_array_v(v_index + 1));
		VectorTwiddle (e->origin, up, f->up,	right, f->right, 1,
				v_array_v(v_index + 2));
		VectorTwiddle (e->origin, up, f->down,	right, f->right, 1,
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

//============================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
static void
R_DrawEntitiesOnList (void)
{
	entity_t	*e;
	Uint		i;

	if (!r_drawentities->ivalue)
		return;

	for (i = 0; i < r_refdef.num_entities; i++) {
		e = r_refdef.entities[i];

		if (e->model->type == mod_brush)
			R_DrawBrushModel (e);
	}

	R_DrawSpriteModels ();

	R_DrawAliasModels (r_refdef.entities, r_refdef.num_entities, false);
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

/*
================
R_Render3DView

Called by R_RenderView, possibily repeatedly.
================
*/
void
R_Render3DView (void)
{
	if (draw_skybox) {
		R_DrawSkyBox ();
		R_DrawBrushModelSkies ();
	}
	
	// adds static entities to the list
	R_DrawWorld ();

	R_DrawEntitiesOnList ();

	R_DrawViewModel ();

	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

	R_DrawExplosions ();
	R_DrawParticles ();
	if (r_wateralpha->fvalue != 1)
		R_DrawWaterTextureChains (cl.worldmodel->extra.brush, false);
	R_DrawCoronas ();

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

	r_time = cl.time;
	r_frametime = host_frametime;

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
	int			x, y;
	Uint8		pixels[16][16][4];
	image_t		img;

	img.width = 16;
	img.height = 16;
	img.pixels = (Uint8 *)&pixels;
	img.type = IMG_RGBA;

	// Set up the notexture
	for (y = 0; y < 16; y++)
	{
		for (x = 0; x < 16; x++)
		{
			if ((y < 8) ^ (x < 8))
			{
				pixels[y][x][0] = 128;
				pixels[y][x][1] = 128;
				pixels[y][x][2] = 128;
				pixels[y][x][3] = 255;
			}
			else
			{
				pixels[y][x][0] = 64;
				pixels[y][x][1] = 64;
				pixels[y][x][2] = 64;
				pixels[y][x][3] = 255;
			}
		}
	}

	r_notexture = Hunk_AllocName (sizeof (texture_t), "notexture");
	strcpy (r_notexture->name, "notexture");
	r_notexture->width = img.width;
	r_notexture->height = img.height;
	r_notexture->gl_texturenum = R_LoadTexture (r_notexture->name, &img,
			TEX_MIPMAP);

	r_notexture_water = Hunk_AllocName (sizeof (texture_t), "notexturewater");
	*r_notexture_water = *r_notexture;

	R_InitLightTextures ();
}

/*
===============
R_WireframeChanged
===============
*/
void
R_WireframeChanged (cvar_t *cvar)
{
	if (cl.maxclients > 1) {
		gl_wireframe = 0;
		return;
	}

	gl_wireframe = cvar->ivalue;
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
	r_wireframe = Cvar_Get ("r_wireframe", "0", CVAR_NONE, &R_WireframeChanged);
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL);
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);
	r_stainmaps = Cvar_Get ("r_stainmaps", "1", CVAR_ARCHIVE, NULL);

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

	R_Init_Sky_Cvars ();
	R_Init_Liquid_Cvars ();
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

	if (cls.state != ca_connected)
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
	Cmd_AddCommand ("timerefresh", &R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", &R_ReadPointFile_f);

	qglGenTextures(6, skyboxtexnums);

	R_InitTextures ();
	R_InitBubble ();
	R_InitParticles ();
	TNT_Init ();
	R_Explosion_Init ();
	R_InitSurf ();
	R_Init_Sky ();
	R_Init_Liquid ();
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
	extern Uint		r_dlightframecount;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_viewleaf = NULL;
	R_ClearParticles ();

	r_dlightframecount = 0;

	// identify sky texture
	skytexturenum = -1;
	for (i = 0; i < cl.worldmodel->extra.brush->numtextures; i++) {
		if (!cl.worldmodel->extra.brush->textures[i])
			continue;
		if (!strncmp (cl.worldmodel->extra.brush->textures[i]->name, "sky", 3))
		{
			skytexturenum = i;
			break;
		}
	}

	// some Cvars need resetting on map change
	Cvar_Set (r_skyname, "");

	// Parse map entities
	CL_ParseEntityLump (cl.worldmodel->extra.brush->entities);

	r_explosion_newmap ();

	R_WireframeChanged (r_wireframe);
}

