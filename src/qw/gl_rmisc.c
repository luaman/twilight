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

#include <stdlib.h>	/* for malloc() */

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "r_explosion.h"

extern void TNT_Init (void);

memzone_t	*vzone;

texcoord_t	*tc0_array_p;
texcoord_t	*tc1_array_p;
vertex_t	*v_array_p;
color_t		*c_array_p;
GLuint		*vindices;

GLuint v_index, i_index;
qboolean va_locked;

void R_InitBubble (void);
void R_SkyBoxChanged (cvar_t *cvar);
static void R_TimeRefresh_f (void);

qboolean Img_HasFullbrights (Uint8 *pixels, int size);

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
	gl_im_transform = Cvar_Get ("gl_im_transform", "1", CVAR_ARCHIVE, NULL);;

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
	}

	Cvar_Set (r_skyname, Cmd_Argv(1));
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
	c_array_p = Zone_Alloc(vzone, MAX_VERTEX_ARRAYS * sizeof(color_t));
	vindices = Zone_Alloc(vzone, MAX_VERTEX_INDICES * sizeof(GLuint));

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc0_array_v(0)), tc0_array_p);
	qglColorPointer (4, GL_FLOAT, sizeof(c_array_v(0)), c_array_p);
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
