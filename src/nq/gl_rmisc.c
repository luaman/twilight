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
// r_misc.c
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
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "strlib.h"
#include "sys.h"

GLfloat tc_array[MAX_VERTEX_ARRAYS][2];
GLfloat v_array[MAX_VERTEX_ARRAYS][3];
GLfloat c_array[MAX_VERTEX_ARRAYS][4];
unsigned int vindices[MAX_VERTEX_INDICES];

void R_InitBubble (void);
void R_SkyBoxChanged (cvar_t *cvar);
void R_TimeRefresh_f (void);
extern void TNT_Init (void);

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

void
R_InitParticleTexture (void)
{
	int     x,y,d;
	float   dx, dy;
	Uint8    data[64][64][4];
	
	// 
	// particle texture
	// 
	particletexture = texture_extension_number++;
	qglBindTexture(GL_TEXTURE_2D, particletexture);
	
	for (x=0 ; x<64; x++) {
		for (y=0 ; y<64 ; y++) {
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			dx = x - 16; dy = y - 16;
			d = (255 - (dx*dx+dy*dy));
			if (d < 0) d = 0;
			data[y][x][3] = (Uint8) d;
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Init_Cvars
===============
*/
void
R_Init_Cvars (void)
{
	extern cvar_t *gl_finish;
	extern cvar_t *r_maxedges, *r_maxsurfs;		// Shrak

	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, NULL);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL);
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_NONE, NULL);
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL);
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_NONE, NULL);
	r_lightmap = Cvar_Get ("r_lightmap", "0", CVAR_NONE, NULL);
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_NONE, NULL);
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL);
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);
	r_lightlerp = Cvar_Get ("r_lightlerp", "1", CVAR_NONE, NULL);

	r_skybox = Cvar_Get ("skybox", "", CVAR_NONE, &R_SkyBoxChanged);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, NULL);

	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_NONE, NULL);
	gl_clear = Cvar_Get ("gl_clear", "1", CVAR_NONE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
	gl_texsort = Cvar_Get ("gl_texsort", "1", CVAR_NONE, NULL);
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_NONE, NULL);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", CVAR_NONE, NULL);
	gl_flashblend = Cvar_Get ("gl_flashblend", "1", CVAR_NONE, NULL);
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);

	gl_reporttjunctions = Cvar_Get ("gl_reporttjunctions", "0", CVAR_NONE, NULL);

	gl_doubleeyes = Cvar_Get ("gl_doubleeys", "1", CVAR_NONE, NULL);

	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_NONE, NULL);
	gl_im_transform = Cvar_Get ("gl_im_transform", "1", CVAR_NONE, NULL);

	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_NONE, NULL);
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_NONE, NULL);

	gl_oldlights = Cvar_Get ("gl_oldlights", "0", CVAR_NONE, NULL);

	gl_particletorches = Cvar_Get ("gl_particletorches", "0", CVAR_ARCHIVE, NULL);

	r_maxedges = Cvar_Get ("r_maxedges", "0", CVAR_NONE, NULL);	// Shrak
	r_maxsurfs = Cvar_Get ("r_maxsurfs", "0", CVAR_NONE, NULL); // Shrak

	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_NONE, NULL);

	r_particles = Cvar_Get ("r_particles", "1", CVAR_NONE, NULL);

	if (gl_mtexable)
		Cvar_Set (gl_texsort, "0");
}

/*
===============
R_Init
===============
*/
void
R_Init (void)
{
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	R_InitBubble ();
	R_InitParticles ();
	R_InitParticleTexture ();
	TNT_Init ();

	playertextures = texture_extension_number;
	texture_extension_number += 16;

	skyboxtexnum = texture_extension_number;
	texture_extension_number += 6;

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc_array[0]), tc_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(c_array[0]), c_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(v_array[0]), v_array[0]);

	qglDisableClientState (GL_COLOR_ARRAY);
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void
R_TranslatePlayerSkin (int playernum)
{
	Sint32		top, bottom;
	Uint8		translate[256];
	Uint32		translate32[256];
	Sint32		i, s;
	model_t		*model;
	aliashdr_t	*paliashdr;
	Uint8		*original;

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i = 0; i < 256; i++)
		translate[i] = i;

	for (i = 0; i < 16; i++) {
		if (top < 128)					// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	// 
	// locate the original skin pixels
	// 
	currententity = &cl_entities[1 + playernum];
	model = currententity->model;
	if (!model)
		return;							// player doesn't have a model yet
	if (model->type != mod_alias)
		return;							// only translate skins on alias models

	paliashdr = (aliashdr_t *) Mod_Extradata (model);
	s = paliashdr->skinwidth * paliashdr->skinheight;
	if (currententity->skinnum < 0
		|| currententity->skinnum >= paliashdr->numskins) {
		Con_Printf ("(%d): Invalid player skin #%d\n", playernum,
					currententity->skinnum);
		original = (Uint8 *) paliashdr + paliashdr->texels[0];
	} else
		original =
			(Uint8 *) paliashdr + paliashdr->texels[currententity->skinnum];

	if (s & 3)
		Sys_Error ("R_TranslateSkin: s&3");

	for (i = 0; i < 256; i++)
		translate32[i] = d_8to32table[translate[i]];

	qglBindTexture (GL_TEXTURE_2D, playertextures + playernum);
	GL_Upload8 (original, paliashdr->skinwidth, paliashdr->skinheight, true, false, translate32);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


/*
===============
R_NewMap
===============
*/
void
R_NewMap (void)
{
	Uint32		i;
	extern int	r_dlightframecount;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

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
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void
R_TimeRefresh_f (void)
{
	int         i;
	float       start, stop, time;

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
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_EndRendering ();
}

void
D_FlushCaches (void)
{
}
