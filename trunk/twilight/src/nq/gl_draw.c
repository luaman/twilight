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
// draw.c -- this is the only file outside the refresh that touches the
// vid buffer
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

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t		*gl_max_size;
cvar_t		*gl_picmip;
cvar_t		*gl_texturemode;

Uint8		*draw_chars;					// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int         translate_texture;
int         char_texture;

typedef struct {
	int         texnum;
	float       sl, tl, sh, th;
} glpic_t;

Uint8       conback_buffer[sizeof (qpic_t) + sizeof (glpic_t)];
qpic_t     *conback = (qpic_t *) &conback_buffer;

int         gl_lightmap_format = GL_RGB;
int         gl_solid_format = 3;
int         gl_alpha_format = 4;

int         gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int         gl_filter_max = GL_LINEAR;


int         texels;

typedef struct {
	int         texnum;
	char        identifier[64];
	int         width, height;
	qboolean    mipmap;
	unsigned short crc;
} gltexture_t;

gltexture_t gltextures[MAX_GLTEXTURES];
int         numgltextures;


//=============================================================================
/* Support Routines */

typedef struct cachepic_s {
	char        name[MAX_QPATH];
	qpic_t      pic;
	Uint8       padding[32];			// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t  menu_cachepics[MAX_CACHED_PICS];
int         menu_numcachepics;

Uint8       menuplyr_pixels[4096];

int         pic_texels;
int         pic_count;

qpic_t     *
Draw_PicFromWad (char *name)
{
	qpic_t     *p;
	glpic_t    *gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *) p->data;

	gl->texnum = GL_LoadPicTexture (p);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t     *
Draw_CachePic (char *path)
{
	cachepic_t *pic;
	int         i;
	qpic_t     *dat;
	glpic_t    *gl;

	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *) COM_LoadTempFile (path);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *) pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


void
Draw_CharToConback (int num, Uint8 *dest)
{
	int         row, col;
	Uint8      *source;
	int         drawline;
	int         x;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

	drawline = 8;

	while (drawline--) {
		for (x = 0; x < 8; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

typedef struct {
	char       *name;
	int         minimize, maximize;
} glmode_t;

glmode_t    modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Set_TextureMode_f
===============
*/
void
Set_TextureMode_f (struct cvar_s *var)
{
	int         i;
	gltexture_t *glt;

	for (i = 0; i < 6; i++) {
		if (!strcasecmp (modes[i].name, var->string))
			break;
	}
	if (i == 6) {
		Cvar_Set (gl_texturemode,"GL_LINEAR_MIPMAP_NEAREST");
		Con_Printf ("Bad GL_TEXTUREMODE, valid modes are:\n");
		Con_Printf ("GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST\n");
		Con_Printf ("GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST (default), \n");
		Con_Printf ("GL_LINEAR_MIPMAP_LINEAR\n\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->mipmap) {
			qglBindTexture (GL_TEXTURE_2D, glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
Draw_Init_Cvars
===============
*/
void
Draw_Init_Cvars (void)
{
	gl_max_size = Cvar_Get ("gl_max_size", "1024", CVAR_NONE, NULL);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, Set_TextureMode_f);

	// 3dfx can only handle 256 wide textures
	if (!strncasecmp ((char *) gl_renderer, "3dfx", 4) ||
		strstr ((char *) gl_renderer, "Glide"))
		Cvar_Set (gl_max_size, "256");
}

/*
===============
Draw_Init
===============
*/
void
Draw_Init (void)
{
	int         i;
	qpic_t     *cb;
	Uint8      *dest;
	int         x, y;
	char        ver[40];
	glpic_t    *gl;
	int         start;
	Uint8      *ncdata;

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	// now turn them into textures
	char_texture =
		GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);

	start = Hunk_LowMark ();

	cb = (qpic_t *) COM_LoadTempFile ("gfx/conback.lmp");
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	// hack the version number directly into the pic
	snprintf (ver, sizeof (ver), "twilight %-7s", VERSION);
	y = strlen (ver);
	dest = cb->data + 320 * 186 + 320 - 11 - 8 * y;

	for (x = 0; x < y; x++)
		Draw_CharToConback (ver[x], dest + (x << 3));

	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *) conback->data;
	gl->texnum =
		GL_LoadTexture ("conback", conback->width, conback->height, ncdata,
						false, false);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark (start);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// 
	// get the other pics we need
	// 
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void
Draw_Character (int x, int y, int num)
{
	int         row, col;
	float       frow, fcol, size;

	if (num == 32)
		return;							// space

	num &= 255;

	if (y <= -8)
		return;							// totally off screen

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + size, frow);
	qglVertex2f (x + 8, y);
	qglTexCoord2f (fcol + size, frow + size);
	qglVertex2f (x + 8, y + 8);
	qglTexCoord2f (fcol, frow + size);
	qglVertex2f (x, y + 8);
	qglEnd ();
}

/*
================
Draw_String
================
*/
void
Draw_String (int x, int y, char *str)
{
	float       frow, fcol;
	int         num;

	if (y <= -8)
		return;							// totally off screen
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglBegin (GL_QUADS);

	while (*str)						// stop rendering when out of
										// characters
	{
		if ((num = *str++) != 32)		// skip spaces
		{
			frow = (float) (num >> 4) * 0.0625;
			fcol = (float) (num & 15) * 0.0625;
			qglTexCoord2f (fcol, frow);
			qglVertex2f (x, y);
			qglTexCoord2f (fcol + 0.0625, frow);
			qglVertex2f (x + 8, y);
			qglTexCoord2f (fcol + 0.0625, frow + 0.0625);
			qglVertex2f (x + 8, y + 8);
			qglTexCoord2f (fcol, frow + 0.0625);
			qglVertex2f (x, y + 8);
		}

		x += 8;
	}

	qglEnd ();
}


/*
================
Draw_Alt_String
================
*/
void
Draw_Alt_String (int x, int y, char *str)
{
	float       frow, fcol;
	int         num;

	if (y <= -8)
		return;							// totally off screen
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglBegin (GL_QUADS);

	while (*str)						// stop rendering when out of
										// characters
	{
		if ((num = *str++ | 0x80) != (32 | 0x80)) {
			frow = (float) (num >> 4) * 0.0625;
			fcol = (float) (num & 15) * 0.0625;
			qglTexCoord2f (fcol, frow);
			qglVertex2f (x, y);
			qglTexCoord2f (fcol + 0.0625, frow);
			qglVertex2f (x + 8, y);
			qglTexCoord2f (fcol + 0.0625, frow + 0.0625);
			qglVertex2f (x + 8, y + 8);
			qglTexCoord2f (fcol, frow + 0.0625);
			qglVertex2f (x, y + 8);
		}

		x += 8;
	}

	qglEnd ();
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void
Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/
void
Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t    *gl;

	gl = (glpic_t *) pic->data;
//	qglDisable (GL_ALPHA_TEST);
//	qglEnable (GL_BLEND);
//  qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//  qglCullFace(GL_FRONT);
	qglColor4f (1, 1, 1, alpha);
	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x + pic->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x + pic->width, y + pic->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y + pic->height);
	qglEnd ();
	qglColor4f (1, 1, 1, 1);
//	qglEnable (GL_ALPHA_TEST);
//	qglDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void
Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t    *gl;

	gl = (glpic_t *) pic->data;
	qglColor4f (1, 1, 1, 1);
	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x + pic->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x + pic->width, y + pic->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y + pic->height);
	qglEnd ();
}


/*
=============
Draw_TransPic
=============
*/
void
Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned) (x + pic->width) > vid.width || y < 0 ||
		(unsigned) (y + pic->height) > vid.height) {
		Sys_Error ("Draw_TransPic: bad coordinates");
	}

	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void
Draw_TransPicTranslate (int x, int y, qpic_t *pic, Uint8 * translation)
{
	int         v, u, c;
	unsigned    trans[64 * 64], *dest;
	Uint8      *src;
	int         p;

	qglBindTexture (GL_TEXTURE_2D, translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64) {
		src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
		for (u = 0; u < 64; u++) {
			p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] = d_8to24table[translation[p]];
		}
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	qglColor3f (1, 1, 1);
	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x + pic->width, y);
	qglTexCoord2f (1, 1);
	qglVertex2f (x + pic->width, y + pic->height);
	qglTexCoord2f (0, 1);
	qglVertex2f (x, y + pic->height);
	qglEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void
Draw_ConsoleBackground (int lines)
{
	int         y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic (0, lines - vid.height, conback);
	else
		Draw_AlphaPic (0, lines - vid.height, conback,
					   (float) (1.2 * lines) / y);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void
Draw_TileClear (int x, int y, int w, int h)
{
	qglColor3f (1, 1, 1);
	qglBindTexture (GL_TEXTURE_2D, *(int *) draw_backtile->data);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x / 64.0, y / 64.0);
	qglVertex2f (x, y);
	qglTexCoord2f ((x + w) / 64.0, y / 64.0);
	qglVertex2f (x + w, y);
	qglTexCoord2f ((x + w) / 64.0, (y + h) / 64.0);
	qglVertex2f (x + w, y + h);
	qglTexCoord2f (x / 64.0, (y + h) / 64.0);
	qglVertex2f (x, y + h);
	qglEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	qglDisable (GL_TEXTURE_2D);
	qglColor3f (host_basepal[c * 3] / 255.0,
			   host_basepal[c * 3 + 1] / 255.0,
			   host_basepal[c * 3 + 2] / 255.0);

	qglBegin (GL_QUADS);

	qglVertex2f (x, y);
	qglVertex2f (x + w, y);
	qglVertex2f (x + w, y + h);
	qglVertex2f (x, y + h);

	qglEnd ();
	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void
Draw_FadeScreen (void)
{
//	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.8);
	qglBegin (GL_QUADS);

	qglVertex2f (0, 0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1, 1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
//	qglDisable (GL_BLEND);

	Sbar_Changed ();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void
Draw_BeginDisc (void)
{
	if (!draw_disc)
		return;
	qglDrawBuffer (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	qglDrawBuffer (GL_BACK);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void
Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void
GL_Set2D (void)
{
	qglViewport (glx, gly, glwidth, glheight);

	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//	qglEnable (GL_ALPHA_TEST);
////  qglDisable (GL_ALPHA_TEST);

	qglColor4f (1, 1, 1, 1);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int
GL_FindTexture (char *identifier)
{
	int         i;
	gltexture_t *glt;

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void
GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,
					int outwidth, int outheight)
{
	int         i, j;
	unsigned   *inrow;
	unsigned    frac, fracstep;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j += 4) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 1] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 2] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 3] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
void
GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	int         i, j;
	unsigned char *inrow;
	unsigned    frac, fracstep;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j += 4) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 1] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 2] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 3] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void
GL_MipMap (Uint8 *in, int width, int height)
{
	int         i, j;
	Uint8      *out;

	width <<= 2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width) {
		for (j = 0; j < width; j += 8, out += 4, in += 8) {
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void
GL_MipMap8Bit (Uint8 *in, int width, int height)
{
	int         i, j;
	unsigned short r, g, b;
	Uint8      *out, *at1, *at2, *at3, *at4;

//  width <<=2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width) {
		for (j = 0; j < width; j += 2, out += 1, in += 2) {
			at1 = (Uint8 *) (d_8to24table + in[0]);
			at2 = (Uint8 *) (d_8to24table + in[1]);
			at3 = (Uint8 *) (d_8to24table + in[width + 0]);
			at4 = (Uint8 *) (d_8to24table + in[width + 1]);

			r = (at1[0] + at2[0] + at3[0] + at4[0]);
			r >>= 5;
			g = (at1[1] + at2[1] + at3[1] + at4[1]);
			g >>= 5;
			b = (at1[2] + at2[2] + at3[2] + at4[2]);
			b >>= 5;

			out[0] = d_15to8table[(r << 0) + (g << 5) + (b << 10)];
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void
GL_Upload32 (unsigned *data, int width, int height, qboolean mipmap,
			 int alpha)
{
	int         samples;
	static unsigned scaled[1024 * 512];	// [512*256];
	int         scaled_width, scaled_height;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= (int) gl_picmip->value;
	scaled_height >>= (int) gl_picmip->value;

	if (scaled_width > gl_max_size->value)
		scaled_width = gl_max_size->value;
	if (scaled_height > gl_max_size->value)
		scaled_height = gl_max_size->value;

	if (scaled_width * scaled_height > sizeof (scaled) / 4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

#if 1
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA,
						   GL_UNSIGNED_BYTE, data);
	else if (scaled_width == width && scaled_height == height)
		qglTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA,
					  GL_UNSIGNED_BYTE, data);
	else {
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, data,
					   scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
					  GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else
	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height) {
		if (!mipmap) {
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width * height * 4);
	} else
		GL_ResampleTexture (data, width, height, scaled, scaled_width,
							scaled_height);

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap ((Uint8 *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
  done:;
#endif


	if (mipmap) {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

void
GL_Upload8_EXT (Uint8 *data, int width, int height, qboolean mipmap,
				qboolean alpha)
{
	int         i, s;
	qboolean    noalpha;
	int         samples;
	static unsigned char scaled[1024 * 512];	// [512*256];
	int         scaled_width, scaled_height;

	s = width * height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		noalpha = true;
		for (i = 0; i < s; i++) {
			if (data[i] == 255)
				noalpha = false;
		}

		if (alpha && noalpha)
			alpha = false;
	}
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= (int) gl_picmip->value;
	scaled_height >>= (int) gl_picmip->value;

	if (scaled_width > gl_max_size->value)
		scaled_width = gl_max_size->value;
	if (scaled_height > gl_max_size->value)
		scaled_height = gl_max_size->value;

	if (scaled_width * scaled_height > sizeof (scaled))
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1;						// alpha ? gl_alpha_format :
	// gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height) {
		if (!mipmap) {
			qglTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
						  scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
						  data);
			goto done;
		}
		memcpy (scaled, data, width * height);
	} else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width,
								scaled_height);

	qglTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
				  scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap8Bit ((Uint8 *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
						  scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						  GL_UNSIGNED_BYTE, scaled);
		}
	}
  done:;


	if (mipmap) {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

/*
===============
GL_Upload8
===============
*/
void
GL_Upload8 (Uint8 *data, int width, int height, qboolean mipmap, int alpha)
{
	static unsigned trans[640 * 480];	// FIXME, temporary
	int         i, s;
	qboolean    noalpha;
	int         p;
	unsigned	*table = d_8to24table;

	s = width * height;

	if (alpha == 2)
	{
	// this is a fullbright mask, so make all non-fullbright
	// colors transparent
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p < 224)
				trans[i] = 0; // transparent 
			else
				trans[i] = table[p];	// fullbright
		}
	}
	else if (alpha) {
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
		noalpha = true;
		for (i = 0; i < s; i++) {
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	} else {
		if (s & 3)
			Sys_Error ("GL_Upload8: s&3");
		for (i = 0; i < s; i += 4) {
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}

	GL_Upload32 (trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int
GL_LoadTexture (char *identifier, int width, int height, Uint8 *data,
				qboolean mipmap, int alpha)
{
	int         i;
	gltexture_t *glt;
	unsigned short crc = 0;

	if (isDedicated)
		return 0;

	// see if the texture is already present
	if (identifier[0]) {
		crc = CRC_Block (data, width*height);

		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (!strcmp (identifier, glt->identifier)) {

				if (width == glt->width && height == glt->height && crc == glt->crc)
					return gltextures[i].texnum;
				else
					goto setuptexture;	// reload the texture into the same slot	
			}
		}
	}
	else {
		glt = &gltextures[numgltextures];
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES");

	numgltextures++;
	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number++;

setuptexture:
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->crc = crc;

	qglBindTexture (GL_TEXTURE_2D, glt->texnum);

	GL_Upload8 (data, width, height, mipmap, alpha);

	return glt->texnum;
}

/*
================
GL_LoadPicTexture
================
*/
int
GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true);
}

/****************************************/

static GLenum oldtarget = 0;
extern GLenum gl_mtex_enum;

void
GL_SelectTexture (GLenum target)
{
	if (!gl_mtexable)
		return;
	qglSelectTexture (gl_mtex_enum + target);
	if (target == oldtarget)
		return;
	cnttextures[oldtarget] = currenttexture;
	currenttexture = cnttextures[target];
	oldtarget = target;
}
