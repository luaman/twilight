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
#include "glquake.h"

void R_InitBubble (void);


/*
==================
R_InitTextures
==================
*/
void
R_InitTextures (void)
{
	int         x, y, m;
	byte       *dest;

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
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

byte        dottexture[8][8] = {
	{0, 1, 1, 0, 0, 0, 0, 0}
	,
	{1, 1, 1, 1, 0, 0, 0, 0}
	,
	{1, 1, 1, 1, 0, 0, 0, 0}
	,
	{0, 1, 1, 0, 0, 0, 0, 0}
	,
	{0, 0, 0, 0, 0, 0, 0, 0}
	,
	{0, 0, 0, 0, 0, 0, 0, 0}
	,
	{0, 0, 0, 0, 0, 0, 0, 0}
	,
	{0, 0, 0, 0, 0, 0, 0, 0}
	,
};
void
R_InitParticleTexture (void)
{
	int         x, y;
	byte        data[8][8][4];

	// 
	// particle texture
	// 
	particletexture = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, particletexture);

	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 8, 8, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, data);

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void
R_Envmap_f (void)
{
	byte        buffer[256 * 256 * 4];

	qglDrawBuffer (GL_FRONT);
	qglReadBuffer (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof (buffer));

	envmap = false;
	qglDrawBuffer (GL_BACK);
	qglReadBuffer (GL_BACK);
	GL_EndRendering ();
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
	r_mirroralpha = Cvar_Get ("r_mirroralpha", "1", CVAR_NONE, NULL);
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL);
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);

	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_NONE, NULL);
	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_NONE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
	gl_texsort = Cvar_Get ("gl_texsort", "1", CVAR_NONE, NULL);
	gl_smoothmodels = Cvar_Get ("gl_smoothmodels", "1", CVAR_NONE, NULL);
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_NONE, NULL);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", CVAR_NONE, NULL);
	gl_flashblend = Cvar_Get ("gl_flashblend", "1", CVAR_NONE, NULL);
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);

	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "0", CVAR_NONE, NULL);
	gl_reporttjunctions = Cvar_Get ("gl_reporttjunctions", "0", CVAR_NONE, NULL);

	gl_doubleeyes = Cvar_Get ("gl_doubleeys", "1", CVAR_NONE, NULL);

	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_NONE, NULL);
	gl_im_transform = Cvar_Get ("gl_im_transform", "1", CVAR_NONE, NULL);

	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_NONE, NULL);
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_NONE, NULL);

	r_maxedges = Cvar_Get ("r_maxedges", "0", CVAR_NONE, NULL);	// Shrak
	r_maxsurfs = Cvar_Get ("r_maxsurfs", "0", CVAR_NONE, NULL); // Shrak

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
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	R_InitBubble();
	R_InitParticles ();
	R_InitParticleTexture ();

#ifdef GLTEST
	Test_Init ();
#endif

	playertextures = texture_extension_number;
	texture_extension_number += 16;
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
	int         top, bottom;
	byte        translate[256];
	unsigned    translate32[256];
	int         i, j, s;
	model_t    *model;
	aliashdr_t *paliashdr;
	byte       *original;
	unsigned    pixels[512 * 256], *out;
	unsigned    scaled_width, scaled_height;
	int         inwidth, inheight;
	byte       *inrow;
	unsigned    frac, fracstep;

	GL_DisableMultitexture ();

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i = 0; i < 256; i++)
		translate[i] = i;

	for (i = 0; i < 16; i++) {
		if (top < 128)					// the artists made some backwards
			// ranges.  sigh.
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
		original = (byte *) paliashdr + paliashdr->texels[0];
	} else
		original =
			(byte *) paliashdr + paliashdr->texels[currententity->skinnum];
	if (s & 3)
		Sys_Error ("R_TranslateSkin: s&3");

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
	qglBindTexture (GL_TEXTURE_2D, playertextures + playernum);

#if 0
	byte        translated[320 * 200];

	for (i = 0; i < s; i += 4) {
		translated[i] = translate[original[i]];
		translated[i + 1] = translate[original[i + 1]];
		translated[i + 2] = translate[original[i + 2]];
		translated[i + 3] = translate[original[i + 3]];
	}


	// don't mipmap these, because it takes too long
	GL_Upload8 (translated, paliashdr->skinwidth, paliashdr->skinheight, false,
				false, true);
#else
	scaled_width = gl_max_size->value < 512 ? gl_max_size->value : 512;
	scaled_height = gl_max_size->value < 256 ? gl_max_size->value : 256;

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int) gl_playermip->value;
	scaled_height >>= (int) gl_playermip->value;

	for (i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];

	out = pixels;
	fracstep = inwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * inheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j += 4) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width,
				  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif

}


/*
===============
R_NewMap
===============
*/
void
R_NewMap (void)
{
	int         i;

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

	GL_BuildLightmaps ();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		if (!Q_strncmp (cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		if (!Q_strncmp (cl.worldmodel->textures[i]->name, "window02_1", 10))
			mirrortexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
#ifdef QUAKE2
	R_LoadSkys ();
#endif
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

	qglDrawBuffer (GL_FRONT);
	qglFinish ();

	start = Sys_FloatTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i / 128.0 * 360.0;
		R_RenderView ();
	}

	qglFinish ();
	stop = Sys_FloatTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	qglDrawBuffer (GL_BACK);
	GL_EndRendering ();
}

void
D_FlushCaches (void)
{
}
