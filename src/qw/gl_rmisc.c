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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "strlib.h"
#include "sys.h"

extern void TNT_Init (void);

//varray_t2f_c4f_v4f_t varray[MAX_VERTEX_ARRAYS];
GLfloat	tc_array[MAX_VERTEX_ARRAYS][2];
GLfloat	v_array[MAX_VERTEX_ARRAYS][3];
GLfloat	c_array[MAX_VERTEX_ARRAYS][4];
GLuint vindices[MAX_VERTEX_INDICES];
GLuint v_index, i_index;

void R_InitBubble (void);
void R_SkyBoxChanged (cvar_t *cvar);
void R_TimeRefresh_f (void);

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
	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, NULL);
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL);
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_NONE, NULL);
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL);
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_NONE, NULL);
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL);
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL);
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL);
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL);
	r_lightlerp = Cvar_Get ("r_lightlerp", "1", CVAR_NONE, NULL);

	r_skybox = Cvar_Get ("skybox", "", CVAR_NONE, &R_SkyBoxChanged);
	r_fastsky = Cvar_Get ("r_fastsky", "0", CVAR_NONE, NULL);

	gl_clear = Cvar_Get ("gl_clear", "1", CVAR_NONE, NULL);
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL);
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_NONE, NULL);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", CVAR_NONE, NULL);
	gl_flashblend = Cvar_Get ("gl_flashblend", "1", CVAR_NONE, NULL);
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL);
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL);
	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "1", CVAR_NONE, NULL);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_NONE, NULL);

	gl_im_animation = Cvar_Get ("gl_im_animation", "1", CVAR_NONE, NULL);

	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_NONE, NULL);
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_NONE, NULL);

	gl_oldlights = Cvar_Get ("gl_oldlights", "0", CVAR_NONE, NULL);

	gl_colorlights = Cvar_Get ("gl_colorlights", "1", CVAR_NONE, NULL);

	gl_particletorches = Cvar_Get ("gl_particletorches", "0", CVAR_NONE, NULL);
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

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS;

	// fullbright skins
	texture_extension_number += MAX_CLIENTS;

	skyboxtexnum = texture_extension_number;
	texture_extension_number += 6;

	qglTexCoordPointer (2, GL_FLOAT, sizeof(tc_array[0]), tc_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(c_array[0]), c_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(v_array[0]), v_array[0]);

	qglDisableClientState (GL_COLOR_ARRAY);
//	qglEnableClientState (GL_COLOR_ARRAY);
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
}

int fb_skins[MAX_CLIENTS];

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void
R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	Uint8		translate[256];
	unsigned	translate32[256];
	int		i, j;
	Uint8		*original;
#define PIXELS_BUFFER_SIZE	512*256*sizeof(unsigned)
	unsigned	*pixels;
//	static unsigned	pixels[512*256];
	unsigned	*out;
	unsigned	scaled_width, scaled_height;
	int		inwidth, inheight;
	int		tinwidth, tinheight;
	Uint8		*inrow;
	unsigned	frac, fracstep;
	player_info_t	*player;
	extern Uint8	player_8bit_texels[320*200];
	extern int 		player_8bit_width, player_8bit_height;
	char s[512];

	player = &cl.players[playernum];
	if (!player->name[0])
		return;
	
	pixels = malloc(PIXELS_BUFFER_SIZE);
	
	strcpy(s, Info_ValueForKey(player->userinfo, "skin"));
	COM_StripExtension(s, s);
	if (player->skin && strcasecmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor ||
		player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		top = player->topcolor;
		bottom = player->bottomcolor;
		top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
		bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
		top *= 16;
		bottom *= 16;

		for (i=0 ; i<256 ; i++)
			translate[i] = i;

		for (i=0 ; i<16 ; i++)
		{
			if (top < 128)	// the artists made some backwards ranges.  sigh.
				translate[TOP_RANGE+i] = top+i;
			else
				translate[TOP_RANGE+i] = top+15-i;
					
			if (bottom < 128)
				translate[BOTTOM_RANGE+i] = bottom+i;
			else
				translate[BOTTOM_RANGE+i] = bottom+15-i;
		}

		//
		// locate the original skin pixels
		//
		// real model width
		tinwidth = 296;
		tinheight = 194;

		if (!player->skin)
			Skin_Find(player);
		if ((original = Skin_Cache(player->skin)) != NULL) {
			//skin data width
			inwidth = 320;
			inheight = 200;
		} else {
			original = player_8bit_texels;
			inwidth = player_8bit_width;
			inheight = player_8bit_height;
		}

		// because this happens during gameplay, do it fast
		// instead of sending it through gl_upload 8
		qglBindTexture (GL_TEXTURE_2D, playertextures + playernum);

		scaled_width = gl_max_size->value < 512 ? gl_max_size->value : 512;
		scaled_height = gl_max_size->value < 256 ? gl_max_size->value : 256;

		// allow users to crunch sizes down even more if they want
		scaled_width >>= (int)gl_playermip->value;
		scaled_height >>= (int)gl_playermip->value;

		for (i=0 ; i<256 ; i++)
			translate32[i] = d_8to32table[translate[i]];

		out = pixels;
		memset(pixels, 0, PIXELS_BUFFER_SIZE);
		fracstep = tinwidth*0x10000/scaled_width;
		for (i=0 ; i<scaled_height ; i++, out += scaled_width)
		{
			inrow = original + inwidth*(i*tinheight/scaled_height);
			frac = fracstep >> 1;
			for (j=0 ; j<scaled_width ; j+=4)
			{
				out[j] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+1] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+2] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+3] = translate32[inrow[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 
			scaled_width, scaled_height, 0, GL_RGBA, 
			GL_UNSIGNED_BYTE, pixels);

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (Img_HasFullbrights ((Uint8 *)original, inwidth*inheight))
		{
			fb_skins[playernum] = playertextures + playernum + MAX_CLIENTS;

			qglBindTexture (GL_TEXTURE_2D, fb_skins[playernum]);

			out = pixels;
			memset(pixels, 0, PIXELS_BUFFER_SIZE);
			fracstep = tinwidth*0x10000/scaled_width;
			for (i=0 ; i<scaled_height ; i++, out += scaled_width)
			{
				inrow = original + inwidth*(i*tinheight/scaled_height);
				frac = fracstep >> 1;
				for (j=0 ; j<scaled_width ; j+=4)
				{
					if (out[j] < 224) out[j] = 0; else out[j] = d_8to32table[inrow[frac>>16]];
					frac += fracstep;
					if (out[j+1] < 224) out[j+1] = 0; else out[j+1] = d_8to32table[inrow[frac>>16]];
					frac += fracstep;
					if (out[j+2] < 224) out[j+2] = 0; else out[j+2] = d_8to32table[inrow[frac>>16]];
					frac += fracstep;
					if (out[j+3] < 224) out[j+3] = 0; else out[j+3] = d_8to32table[inrow[frac>>16]];
					frac += fracstep;
				}
			}	

			qglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 
				scaled_width, scaled_height, 0, GL_RGBA, 
				GL_UNSIGNED_BYTE, pixels);

			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			fb_skins[playernum] = 0;
		}

	}
	
	free(pixels);
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
	extern int r_dlightframecount;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

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
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_EndRendering ();
}
