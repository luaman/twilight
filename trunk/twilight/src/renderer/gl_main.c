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
#include "textures.h"
#include "host.h"
#include "image.h"
#include "mathlib.h"
#include "explosion.h"
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
#include "brush.h"
#include "gl_light.h"
#include "gen_textures.h"
#include "gl_main.h"
#include "gl_arrays.h"
#include "draw.h"
#include "screen.h"
#include "surface.h"
#include "hud.h"

// FIXME: These /NEED/ to move to headers.
extern void V_SetContentsColor (int contents);
extern void CL_ParseEntityLump (char *entdata);
extern void R_DrawViewModel (void);


renderer_t r;

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

qboolean colorlights = true;

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

	r.framecount++;

	// build the transformation matrix for the given view angles
	AngleVectors (r.angles, r.vpn, r.vright, r.vup);

	Vis_NewVisParams (r.worldmodel, r.origin, r.vup, r.vright, r.vpn,
			r.fov_x, r.fov_y);

	V_SetContentsColor (vis_viewleaf->contents);
	V_CalcBlend ();

	r.brush_polys = 0;
	r.alias_polys = 0;
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

	xmax = Q_tan (r.fov_x * M_PI / 360.0) * (vid.width / vid.height);
	ymax = Q_tan (r.fov_y * M_PI / 360.0);

	qglFrustum (-xmax, xmax, -ymax, ymax, 1.0f, 8192.0);

	qglCullFace (GL_FRONT);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	// put Z going up
	qglRotatef (-90, 1, 0, 0);
	qglRotatef (90, 0, 0, 1);
	qglRotatef (-r.angles[2], 1, 0, 0);
	qglRotatef (-r.angles[0], 0, 1, 0);
	qglRotatef (-r.angles[1], 0, 0, 1);
	qglTranslatef (-r.origin[0], -r.origin[1],
				  -r.origin[2]);

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

void
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
	Check_GL_Error();
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
r must be set before the first call
================
*/
void
R_RenderView (void)
{
	double		time1 = 0.0;
	double		time2;

	if (r_norefresh->ivalue)
		return;

	if (!r.worldmodel)
		Host_EndGame ("R_RenderView: NULL worldmodel");

	if (r_speeds->ivalue)
	{
		qglFinish ();
		time1 = Sys_DoubleTime ();
		r.brush_polys = 0;
		r.alias_polys = 0;
	}
	else if (gl_finish->ivalue)
		qglFinish ();

	R_Clear ();

	// render normal view
	R_SetupFrame ();

	R_SetupGL ();

	R_VisEntities ();
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
					(int) ((time2 - time1) * 1000), r.brush_polys,
					r.alias_polys);
	}
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

	GLArrays_Init_Cvars ();
	Draw_Init_Cvars ();
	GLInfo_Init_Cvars ();
	R_Liquid_Init_Cvars ();
	PAL_Init_Cvars ();
	R_Particles_Init_Cvars ();
	Sky_Init_Cvars ();
	Surf_Init_Cvars ();
	VID_Init_Cvars ();
	Vis_Init_Cvars ();
	SCR_Init_Cvars ();
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
		r.angles[1] = i * (360.0 / 128.0);
		R_RenderView ();
		GL_EndRendering ();
	}

	qglFinish ();
	stop = Sys_DoubleTime ();
	time = stop - start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_EndRendering ();
}

static void
VID_Restart_f (void)
{
	Mod_ClearAll (true);
	HUD_Shutdown ();
	R_Shutdown ();

	R_Init ();
	HUD_Init ();
	Mod_ReloadAll (FLAG_RENDER);
}


static void
GL_Defaults (void)
{
	qglClearColor (0.3f, 0.3f, 0.3f, 0.5f);
	qglCullFace (GL_FRONT);
	qglEnable (GL_TEXTURE_2D);

	qglAlphaFunc (GL_GREATER, 0.666);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void
R_Init (void)
{
	Cmd_AddCommand ("timerefresh", &R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", &R_ReadPointFile_f);
	Cmd_AddCommand ("vid_restart", &VID_Restart_f);

	VID_Init ();		// Makes the window, gets the GL pointers.
	GLT_Init_Cvars ();	// FIXME: Evil evil hack.
	GLInfo_Init ();		// Basic driver information.
	GL_Defaults ();		// Set some sane defaults.
	GLArrays_Init ();	// Vertex arrays, so we CAN draw.
	GLT_Init ();		// Ability to load textures.

	Draw_Init ();		// Basic drawing stuff.
	GT_Init ();			// Load generated textures.
	SCR_Init ();		// Screen management. (Some needs to merge with draw.)

	// Everything else, order independent.
	R_Particles_Init ();
	R_Explosion_Init ();
	GL_Light_Tables_Init ();
	Sky_Init ();
	Vis_Init ();
}

void
R_Shutdown (void)
{
	/*
	Cmd_RemoveCommand ("timerefresh", &R_TimeRefresh_f);
	Cmd_RemoveCommand ("pointfile", &R_ReadPointFile_f);
	*/

	R_Particles_Shutdown ();
	R_Explosion_Shutdown ();
	Sky_Shutdown ();
	Vis_Shutdown ();

	SCR_Shutdown ();
	GT_Shutdown ();
	Draw_Shutdown ();
	GLT_Shutdown ();
	GLArrays_Shutdown ();
	VID_Shutdown ();
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
	CL_ParseEntityLump (r.worldmodel->brush->entities);

	r_explosion_newmap ();
}
