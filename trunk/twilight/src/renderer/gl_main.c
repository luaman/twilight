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
#include "cclient.h"
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
#include "entities.h"
#include "video.h"
#include "vis.h"
#include "r_part.h"
#include "gl_brush.h"
#include "gl_light.h"
#include "gen_textures.h"

// FIXME: These /NEED/ to move to headers.
extern void V_SetContentsColor (int contents);
extern void CL_ParseEntityLump (char *entdata);
extern void R_DrawViewModel (void);


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


static cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawviewmodel;
static cvar_t *r_speeds;
static cvar_t *r_wireframe;
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

qboolean colorlights = true;

extern model_t *mdl_fire;

//============================================================================

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


static void
R_SetupFrame (void)
{
	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	Vis_NewVisParams (r_worldmodel, r_origin, vup, vright, vpn,
			r_refdef.fov_x, r_refdef.fov_y);

	V_SetContentsColor (vis_viewleaf->contents);
	V_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;
}


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
Called by R_RenderView, possibily repeatedly.
================
*/
static void
R_Render3DView (void)
{
	R_VisEntities ();
	R_DrawSkyEntities ();

	R_PushDlights ();

	R_DrawOpaqueEntities ();

	R_DrawViewModel ();

	qglEnable (GL_BLEND);
	qglDepthMask (GL_FALSE);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

	R_DrawAddEntities ();

	R_DrawExplosions ();
	R_DrawParticles ();
	R_DrawCoronas ();

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_TRUE);
	qglDisable (GL_BLEND);
}

/*
================
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

	if (!r_worldmodel)
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

	if (r_wireframe->ivalue != 1 || !(gl_allow & GLA_WIREFRAME))
		R_Render3DView ();

	// don't let sound get messed up if going slow
	S_ExtraUpdate ();

	if ((gl_allow & GLA_WIREFRAME) && r_wireframe->ivalue) {
		if (r_wireframe->ivalue == 3)
			qglDisable (GL_DEPTH_TEST);
		else if (r_wireframe->ivalue == 2)
			qglEnable (GL_POLYGON_OFFSET_LINE);
		qglDepthMask (GL_FALSE);
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		qglDisable (GL_TEXTURE_2D);

		R_Render3DView ();

		qglEnable (GL_TEXTURE_2D);
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		qglDepthMask (GL_TRUE);
		if (r_wireframe->ivalue == 3)
			qglEnable (GL_DEPTH_TEST);
		else if (r_wireframe->ivalue == 2)
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

void
R_Init_Cvars (void)
{
	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, NULL);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL);
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_NONE, NULL);
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL);
	r_wireframe = Cvar_Get ("r_wireframe", "0", CVAR_NONE, NULL);
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
For program optimization
====================
*/
static void
R_TimeRefresh_f (void)
{
	int         i;
	float       start, stop, time;

	if (ccls.state != ca_active)
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

void
R_NewMap (void)
{
	Uint32			i;
	extern float	scr_centertime_off;

	scr_centertime_off = 0;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	R_ClearParticles ();

	// Parse map entities
	CL_ParseEntityLump (r_worldmodel->brush->entities);

	r_explosion_newmap ();
}

