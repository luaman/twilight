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
#include "sky.h"
#include "liquid.h"

// FIXME - These need to be in a header somewhere
extern void TNT_Init (void);
extern void R_InitBubble (void);
void R_DrawViewModel (void);

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

texture_t *r_notexture;
texture_t *r_notexture_water;

int d_lightstylevalue[256];				// 8.8 fraction of base light value


cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawviewmodel;
cvar_t *r_speeds;
cvar_t *r_shadows;
cvar_t *r_wireframe;
cvar_t *r_dynamic;
cvar_t *r_stainmaps;

cvar_t *gl_clear;
cvar_t *gl_polyblend;
cvar_t *gl_flashblend;
cvar_t *gl_playermip;
cvar_t *gl_finish;
cvar_t *gl_im_transform;
cvar_t *gl_oldlights;
cvar_t *gl_colorlights;

extern vec3_t lightcolor;
qboolean colorlights = true;

//static float shadescale = 0.0;

int gl_wireframe = 0;

extern model_t *mdl_fire;

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
R_GetSpriteFrame (entity_common_t *e)
{
	msprite_t		   *psprite;
	mspritegroup_t	   *pspritegroup;
	mspriteframe_t	   *pspriteframe;
	int					i, numframes, frame;
	float			   *pintervals, fullinterval, targettime, time;

	psprite = e->model->sprite;
	frame = e->frame[0];

	if (!e->real_ent)
		Sys_Error("No real ent! EVIL!\n");

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

		time = cl.time + e->real_ent->syncbase;

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
R_DrawOpaqueSpriteModels (void)
{
	mspriteframe_t	   *f;
	float			   *up, *right;
	vec3_t				v_forward, v_right, v_up;
	msprite_t		   *psprite;
	int					i, last_tex;
	entity_common_t		*ce;

	last_tex = -1;
	v_index = 0;

	qglEnable (GL_ALPHA_TEST);

	for (i = 0; i < r_refdef.num_entities; i++) {
		ce = r_refdef.entities[i];

		if (ce->model->type != mod_sprite)
			continue;

		/*
		 * don't even bother culling, because it's just a single polygon without
		 * a surface cache
		 */
		f = R_GetSpriteFrame (ce);
		psprite = ce->model->sprite;

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
			AngleVectors (ce->angles, v_forward, v_right, v_up);
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

		VectorTwiddle (ce->origin, up, f->down,	right, f->left, 1,
				v_array_v(v_index + 0));
		VectorTwiddle (ce->origin, up, f->up,	right, f->left, 1,
				v_array_v(v_index + 1));
		VectorTwiddle (ce->origin, up, f->up,	right, f->right, 1,
				v_array_v(v_index + 2));
		VectorTwiddle (ce->origin, up, f->down,	right, f->right, 1,
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
R_VisBrushModels
=============
 */
static void
R_VisBrushModels (void)
{
	entity_common_t *ce;
	vec3_t		 mins, maxs;
	int			 i;

	// First off, the world.

	Vis_MarkLeaves (cl.worldmodel);
	Vis_RecursiveWorldNode (cl.worldmodel->brush->nodes,cl.worldmodel,r_origin);

	// Now everything else.

	if (!r_drawentities->ivalue)
		return;

	for (i = 0; i < r_refdef.num_entities; i++) {
		ce = r_refdef.entities[i];

		if (ce->model->type == mod_brush) {
			Mod_MinsMaxs (ce->model, ce->origin, ce->angles, mins, maxs);
			if (Vis_CullBox (mins, maxs))
				continue;
			R_VisBrushModel (ce);
		}
	}
}

static void
R_DrawOpaqueBrushModels ()
{
	entity_common_t *ce;
	vec3_t		 mins, maxs;
	int			 i;

	R_DrawTextureChains (cl.worldmodel, 0, NULL, NULL);

	if (!r_drawentities->ivalue)
		return;

	for (i = 0; i < r_refdef.num_entities; i++) {
		ce = r_refdef.entities[i];

		if (ce->model->type == mod_brush) {
			Mod_MinsMaxs (ce->model, ce->origin, ce->angles, mins, maxs);
			if (Vis_CullBox (mins, maxs))
				continue;

			R_DrawOpaqueBrushModel (ce);
		}
	}
}

static void
R_DrawAddBrushModels ()
{
	entity_common_t *ce;
	vec3_t		 mins, maxs;
	int			 i;

	if (r_wateralpha->fvalue == 1)
		return;

	qglColor4f (1, 1, 1, r_wateralpha->fvalue);

	R_DrawLiquidTextureChains (cl.worldmodel, false);

	if (!r_drawentities->ivalue) {
		qglColor4fv (whitev);
		return;
	}

	for (i = 0; i < r_refdef.num_entities; i++) {
		ce = r_refdef.entities[i];

		if (ce->model->type == mod_brush) {
			Mod_MinsMaxs (ce->model, ce->origin, ce->angles, mins, maxs);
			if (Vis_CullBox (mins, maxs))
				continue;

			R_DrawAddBrushModel (ce);
		}
	}

	qglColor4fv (whitev);
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
	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_TEXTURE_2D);
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

	Vis_NewVisParams (cl.worldmodel, r_origin, vup, vright, vpn,
			r_refdef.fov_x, r_refdef.fov_y);

	V_SetContentsColor (vis_viewleaf->contents);
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
static void
R_Render3DView (void)
{
	R_VisBrushModels ();

	if (sky_type != SKY_FAST) {
		if (sky_type == SKY_BOX)
			Sky_Box_Draw ();
		else if (sky_type == SKY_SPHERE)
			Sky_Sphere_Draw ();
		R_DrawBrushDepthSkies ();
	}
	
	R_PushDlights ();

	// adds static entities to the list
	R_DrawOpaqueBrushModels ();

	// FIXME: For GL_NV_occlusion_query support we should do the tests here.
	// R_VisAliasModels ();
	R_DrawOpaqueSpriteModels ();
	// FIXME: Any way to avoid the arguments sanely?
	R_DrawOpaqueAliasModels (r_refdef.entities, r_refdef.num_entities, false);

	R_DrawViewModel ();

	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

	R_DrawAddBrushModels ();
//	R_DrawAddAliasModels ();		// FIXME: None exist.
//	R_DrawAddSpriteModels ();		// FIXME: None exist.

	R_DrawExplosions ();
	R_DrawParticles ();
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
static void
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

	r_notexture = Zone_Alloc (glt_zone, sizeof(texture_t));
	strcpy (r_notexture->name, "notexture");
	r_notexture->width = img.width;
	r_notexture->height = img.height;
	r_notexture->gl_texturenum = GLT_Load_image (r_notexture->name, &img,
			NULL, TEX_MIPMAP);

	r_notexture_water = Zone_Alloc (glt_zone, sizeof(texture_t));
	*r_notexture_water = *r_notexture;

	R_InitLightTextures ();
}

/*
===============
R_WireframeChanged
===============
*/
static void
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
	r_stainmaps = Cvar_Get ("r_stainmaps", "1", CVAR_ARCHIVE, NULL);

	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_ARCHIVE, NULL);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", CVAR_NONE, NULL);
	gl_flashblend = Cvar_Get ("gl_flashblend", "1", CVAR_ARCHIVE, NULL);
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_NONE, NULL);

	gl_im_transform = Cvar_Get ("gl_im_transform", "1", CVAR_ARCHIVE, NULL);


	gl_oldlights = Cvar_Get ("gl_oldlights", "0", CVAR_NONE, NULL);
	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_NONE, NULL);



	Sky_Init_Cvars ();
	R_Init_Liquid_Cvars ();
	Vis_Init_Cvars ();
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

	R_InitTextures ();
	R_InitBubble ();
	R_InitParticles ();
	TNT_Init ();
	R_Explosion_Init ();
	R_InitSurf ();
	Sky_Init ();
	R_Init_Liquid ();
	Vis_Init ();
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

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	R_ClearParticles ();

	// Parse map entities
	CL_ParseEntityLump (cl.worldmodel->brush->entities);

	r_explosion_newmap ();

	R_WireframeChanged (r_wireframe);
}

