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

#include <stdlib.h>

#include "quakedef.h"
#include "console.h"
#include "crc.h"
#include "glquake.h"

extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor;

cvar_t		*gl_max_size;
cvar_t		*gl_picmip;
cvar_t		*gl_constretch;
cvar_t		*gl_texturemode;
cvar_t		*cl_verstring;					// FIXME: Move this?
cvar_t		*r_lerpimages;

Uint8		*draw_chars;					// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int         translate_texture;
int         char_texture;
int         cs_texture, cs_square;						// crosshair texture

static Uint8 cs_data[64] = {
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static Uint8 cs_squaredata[8][8] = {
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xfe, 0xfe, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	,
};

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
int         numgltextures = 0;


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
	gl_constretch = Cvar_Get ("gl_constretch", "1", CVAR_ARCHIVE, NULL);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
			CVAR_ARCHIVE, Set_TextureMode_f);
	cl_verstring = Cvar_Get ("cl_verstring",
			"Project Twilight v" VERSION " NQ", CVAR_NONE, NULL);
	r_lerpimages = Cvar_Get ("r_lerpimages", "1", CVAR_ARCHIVE, NULL);

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

	cs_texture = GL_LoadTexture ("crosshair", 8, 8, cs_data, false, true);
	cs_square = GL_LoadTexture ("cs_square", 8, 8, (Uint8 *)cs_squaredata, false, true);

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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

void
Draw_Crosshair (void)
{
	int         x, y;
	extern vrect_t scr_vrect;
	unsigned char *pColor;

	if (crosshair->value == 2) {
		x = scr_vrect.x + scr_vrect.width / 2 - 3 + cl_crossx->value;
		y = scr_vrect.y + scr_vrect.height / 2 - 3 + cl_crossy->value;

		pColor = (Uint8 *) &d_8to32table[(Uint8) crosshaircolor->value];
		qglColor4ubv (pColor);
		qglBindTexture (GL_TEXTURE_2D, cs_texture);

		qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		qglVertex2f (x - 4, y - 4);
		qglTexCoord2f (1, 0);
		qglVertex2f (x + 12, y - 4);
		qglTexCoord2f (1, 1);
		qglVertex2f (x + 12, y + 12);
		qglTexCoord2f (0, 1);
		qglVertex2f (x - 4, y + 12);
		qglEnd ();
		qglColor4f (1, 1, 1, 1);
	}
	else if (crosshair->value == 3) {
		x = scr_vrect.x + scr_vrect.width / 2 - 3 + cl_crossx->value;
		y = scr_vrect.y + scr_vrect.height / 2 - 3 + cl_crossy->value;

		pColor = (Uint8 *) &d_8to32table[(Uint8) crosshaircolor->value];
		qglColor4ubv (pColor);
		qglBindTexture (GL_TEXTURE_2D, cs_square);

		qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		qglVertex2f (x - 4, y - 4);
		qglTexCoord2f (1, 0);
		qglVertex2f (x + 12, y - 4);
		qglTexCoord2f (1, 1);
		qglVertex2f (x + 12, y + 12);
		qglTexCoord2f (0, 1);
		qglVertex2f (x - 4, y + 12);
		qglEnd ();
		qglColor4f (1, 1, 1, 1);
	} else if (crosshair->value)
		Draw_Character (
				scr_vrect.x + scr_vrect.width / 2 - 4 +	cl_crossx->value,
				scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy->value,
				'+');
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
				dest[u] = d_8to32table[translation[p]];
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
	int         y;
	qpic_t	   *conback;
	glpic_t	   *gl;
	float		alpha;
	float		ofs;

	conback = Draw_CachePic ("gfx/conback.lmp");
	gl = (glpic_t *) conback->data;

	y = (vid.height * 3) >> 2;

	if (cls.state != ca_connected || lines > y)
		alpha = 1.0;
	else
		alpha = (float) (0.6 * lines / y);

	if (gl_constretch->value)
		ofs = 0.0f;
	else
		ofs = (float) ((vid.conheight - lines) / vid.conheight);

	qglColor4f (1.0f, 1.0f, 1.0f, alpha);

	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl + ofs);
	qglVertex2f (0, 0);
	qglTexCoord2f (gl->sh, gl->tl + ofs);
	qglVertex2f (vid.conwidth, 0);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (vid.conwidth, lines);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (0, lines);
	qglEnd ();

	// hack the version number directly into the pic
	Draw_Alt_String (vid.conwidth - strlen (cl_verstring->string) * 8 - 14,
			lines - 14, cl_verstring->string);

	qglColor3f (1.0f, 1.0f, 1.0f);
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

	// Save this for lighthalf later
//	qglColor3f (((Uint8 *)&d_8to32table[c])[0] / 255.0,
//			((Uint8 *)&d_8to32table[c])[1] / 255.0,
//			((Uint8 *)&d_8to32table[c])[2] / 255.0);
	qglColor4ubv((Uint8 *)&d_8to32table[c]);

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

void R_ResampleTextureLerpLine (Uint8 *in, Uint8 *out, int inwidth,
		int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = (int) f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			int lerp = f & 0xFFFF;
			*out++ = (Uint8) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (Uint8) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (Uint8) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (Uint8) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

/*
================
R_ResampleTexture
================
*/
void R_ResampleTexture (void *indata, int inwidth, int inheight,
		void *outdata, int outwidth, int outheight)
{
	if (r_lerpimages->value)
	{
		int		i, j, yi, oldy, f, fstep, endy = (inheight-1);
		Uint8	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep = (int) (inheight*65536.0f/outheight);

		row1 = malloc(outwidth*4);
		row2 = malloc(outwidth*4);
		inrow = indata;
		oldy = 0;
		R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
		R_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth,
				outwidth);
		for (i = 0, f = 0;i < outheight;i++,f += fstep)
		{
			yi = f >> 16;
			if (yi < endy)
			{
				int lerp = f & 0xFFFF;
				if (yi != oldy)
				{
					inrow = (Uint8 *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLine (inrow, row1, inwidth,
								outwidth);
					R_ResampleTextureLerpLine (inrow + inwidth*4, row2,
							inwidth, outwidth);
					oldy = yi;
				}
				j = outwidth - 4;
				while(j >= 0)
				{
					out[ 0] = (Uint8) ((((row2[ 0] - row1[ 0]) * lerp) >> 16)
							+ row1[ 0]);
					out[ 1] = (Uint8) ((((row2[ 1] - row1[ 1]) * lerp) >> 16)
							+ row1[ 1]);
					out[ 2] = (Uint8) ((((row2[ 2] - row1[ 2]) * lerp) >> 16)
							+ row1[ 2]);
					out[ 3] = (Uint8) ((((row2[ 3] - row1[ 3]) * lerp) >> 16)
							+ row1[ 3]);
					out[ 4] = (Uint8) ((((row2[ 4] - row1[ 4]) * lerp) >> 16)
							+ row1[ 4]);
					out[ 5] = (Uint8) ((((row2[ 5] - row1[ 5]) * lerp) >> 16)
							+ row1[ 5]);
					out[ 6] = (Uint8) ((((row2[ 6] - row1[ 6]) * lerp) >> 16)
							+ row1[ 6]);
					out[ 7] = (Uint8) ((((row2[ 7] - row1[ 7]) * lerp) >> 16)
							+ row1[ 7]);
					out[ 8] = (Uint8) ((((row2[ 8] - row1[ 8]) * lerp) >> 16)
							+ row1[ 8]);
					out[ 9] = (Uint8) ((((row2[ 9] - row1[ 9]) * lerp) >> 16)
							+ row1[ 9]);
					out[10] = (Uint8) ((((row2[10] - row1[10]) * lerp) >> 16)
							+ row1[10]);
					out[11] = (Uint8) ((((row2[11] - row1[11]) * lerp) >> 16)
							+ row1[11]);
					out[12] = (Uint8) ((((row2[12] - row1[12]) * lerp) >> 16)
							+ row1[12]);
					out[13] = (Uint8) ((((row2[13] - row1[13]) * lerp) >> 16)
							+ row1[13]);
					out[14] = (Uint8) ((((row2[14] - row1[14]) * lerp) >> 16)
							+ row1[14]);
					out[15] = (Uint8) ((((row2[15] - row1[15]) * lerp) >> 16)
							+ row1[15]);
					out += 16;
					row1 += 16;
					row2 += 16;
					j -= 4;
				}
				if (j & 2)
				{
					out[ 0] = (Uint8) ((((row2[ 0] - row1[ 0]) * lerp) >> 16)
							+ row1[ 0]);
					out[ 1] = (Uint8) ((((row2[ 1] - row1[ 1]) * lerp) >> 16)
							+ row1[ 1]);
					out[ 2] = (Uint8) ((((row2[ 2] - row1[ 2]) * lerp) >> 16)
							+ row1[ 2]);
					out[ 3] = (Uint8) ((((row2[ 3] - row1[ 3]) * lerp) >> 16)
							+ row1[ 3]);
					out[ 4] = (Uint8) ((((row2[ 4] - row1[ 4]) * lerp) >> 16)
							+ row1[ 4]);
					out[ 5] = (Uint8) ((((row2[ 5] - row1[ 5]) * lerp) >> 16)
							+ row1[ 5]);
					out[ 6] = (Uint8) ((((row2[ 6] - row1[ 6]) * lerp) >> 16)
							+ row1[ 6]);
					out[ 7] = (Uint8) ((((row2[ 7] - row1[ 7]) * lerp) >> 16)
							+ row1[ 7]);
					out += 8;
					row1 += 8;
					row2 += 8;
				}
				if (j & 1)
				{
					out[ 0] = (Uint8) ((((row2[ 0] - row1[ 0]) * lerp) >> 16)
							+ row1[ 0]);
					out[ 1] = (Uint8) ((((row2[ 1] - row1[ 1]) * lerp) >> 16)
							+ row1[ 1]);
					out[ 2] = (Uint8) ((((row2[ 2] - row1[ 2]) * lerp) >> 16)
							+ row1[ 2]);
					out[ 3] = (Uint8) ((((row2[ 3] - row1[ 3]) * lerp) >> 16)
							+ row1[ 3]);
					out += 4;
					row1 += 4;
					row2 += 4;
				}
				row1 -= outwidth*4;
				row2 -= outwidth*4;
			}
			else
			{
				if (yi != oldy)
				{
					inrow = (Uint8 *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLine (inrow, row1, inwidth,
								outwidth);
					oldy = yi;
				}
				memcpy(out, row1, outwidth * 4);
			}
		}
		free(row1);
		free(row2);
	}
	else
	{
		int i, j;
		unsigned frac, fracstep;
		// relies on int32 being 4 bytes
		Uint32 *inrow, *out;
		out = outdata;

		fracstep = inwidth*0x10000/outwidth;
		for (i = 0;i < outheight;i++)
		{
			inrow = (int *)indata + inwidth*(i*inheight/outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;
			while (j >= 0)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out[1] = inrow[frac >> 16];frac += fracstep;
				out[2] = inrow[frac >> 16];frac += fracstep;
				out[3] = inrow[frac >> 16];frac += fracstep;
				out += 4;
				j -= 4;
			}
			if (j & 2)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out[1] = inrow[frac >> 16];frac += fracstep;
				out += 2;
			}
			if (j & 1)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out += 1;
			}
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
===============
GL_Upload32
===============
*/
void
GL_Upload32 (Uint32 *data, int width, int height, qboolean mipmap,
			 int alpha)
{
	int         samples;
	static Uint32 scaled[1024 * 512];	// [512*256];
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

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height) {
		if (!mipmap) {
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width * height * 4);
	} else
		R_ResampleTexture (data, width, height, scaled, scaled_width,
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
	unsigned	*table = d_8to32table;

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
