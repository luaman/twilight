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

#include <stdlib.h>
#include <string.h>

#include "video.h"
#include "strlib.h"
#include "pointers.h"
#include "gl_textures.h"
#include "gl_arrays.h"
#include "image.h"
#include "sys.h"
#include "wad.h"
#include "cvar.h"
#include "qtypes.h"
#include "mathlib.h"
#include "gl_info.h"
#include "gl_draw.h"
#include "cclient.h"
#include "console.h"

qpic_t *draw_disc;
static qpic_t *draw_backtile;

static GLuint	translate_texture;
static GLuint	char_texture;

/* ========================================================================= */
/* Support Routines */

typedef struct cachepic_s {
	char	name[MAX_QPATH];
	qpic_t	pic;
	Uint8	padding[32];	/* for appended glpic */
} cachepic_t;

#define		MAX_CACHED_PICS		128
static cachepic_t	GLT_cachepics[MAX_CACHED_PICS];
static int			GLT_numcachepics;

static Uint8		menuplyr_pixels[4096];

static cvar_t *gl_constretch;
extern cvar_t *cl_verstring;

static cvar_t *hud_chsize;
static cvar_t *hud_chflash;
static cvar_t *hud_chspeed;
static cvar_t *hud_chalpha;

static cvar_t *crosshair;
static cvar_t *crosshaircolor;

static GLuint   ch_textures[NUM_CROSSHAIRS];            // crosshair texture


qpic_t     *
Draw_PicFromWad (char *name)
{
	qpic_t     *p;
	glpic_t    *gl;

	p = W_GetLumpName (name);
	if (!p)
		Sys_Error ("Draw_PicFromWad: cannot find a lump named %s\n", name);

	SwapPic (p);
	gl = (glpic_t *) p->data;

	gl->texnum = GLT_Load_qpic (p);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	return p;
}


qpic_t     *
Draw_CachePic (char *path)
{
	cachepic_t *pic;
	int         i;
	qpic_t     *dat;
	glpic_t    *gl;

	for (pic = GLT_cachepics, i = 0; i < GLT_numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (GLT_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("GLT_numcachepics == MAX_CACHED_PICS");
	GLT_numcachepics++;
	strlcpy (pic->name, path, sizeof (pic->name));

	/* load the pic from disk */
	dat = (qpic_t *) COM_LoadTempFile (path, true);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	/*	HACK HACK HACK --- we need to keep the bytes for
		the translatable player picture just for the menu
		configuration dialog */
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *) pic->pic.data;
	gl->texnum = GLT_Load_qpic (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	Zone_Free (dat);

	return &pic->pic;
}

void
Draw_Init_Cvars (void)
{
	GLT_Init_Cvars ();

	gl_constretch = Cvar_Get ("gl_constretch", "1", CVAR_ARCHIVE, NULL);

	hud_chsize = Cvar_Get ("hud_chsize", "0.5", CVAR_ARCHIVE, NULL);
	hud_chflash = Cvar_Get ("hud_chflash", "0.0", CVAR_ARCHIVE, NULL);
	hud_chspeed = Cvar_Get ("hud_chspeed", "1.5", CVAR_ARCHIVE, NULL);
	hud_chalpha = Cvar_Get ("hud_chalpha", "1.0", CVAR_ARCHIVE, NULL);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE, NULL);
	crosshaircolor = Cvar_Get ("crosshaircolor", "79", CVAR_ARCHIVE, NULL);
}

void
Draw_Init (void)
{
	image_t		*img;
	int			i;

	GLT_Init ();

	img = Image_Load ("conchars");
	if (!img)
		Sys_Error ("Draw_Init: Unable to load conchars\n");

	char_texture = GLT_Load_image ("charset", img, NULL, TEX_ALPHA);

	/* save a texture slot for translated picture */
	qglGenTextures(1, &translate_texture);

	/* get the other pics we need */
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	/* Keep track of the first crosshair texture */
	for (i = 0; i < NUM_CROSSHAIRS; i++)
		ch_textures[i] = GLT_Load_Pixmap (va ("crosshair%i", i), crosshairs[i]);
}



/*
================
Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void
Draw_Character (float x, float y, int num, float text_size)
{
	float		sl, sh, tl, th;

	if (num == 32)
		return;							/* space */

	if (y <= -8)
		return;							/* totally off screen */

	num &= 255;

	sl = (num & 15) * 0.0625;
	sh = sl + 0.0625;

	tl = (num >> 4) * 0.0625;
	th = tl + 0.0625;

	qglEnable (GL_BLEND);
	qglBindTexture (GL_TEXTURE_2D, char_texture);

	VectorSet2 (tc_array_v(0), sl, tl);
	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (tc_array_v(1), sh, tl);
	VectorSet2 (v_array_v(1), x + text_size, y);
	VectorSet2 (tc_array_v(2), sh, th);
	VectorSet2 (v_array_v(2), x + text_size, y + text_size);
	VectorSet2 (tc_array_v(3), sl, th);
	VectorSet2 (v_array_v(3), x, y + text_size);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	qglDisable (GL_BLEND);
}

void
Draw_String_Len (float x, float y, char *str, int len, float text_size)
{
	float	frow, fcol, size = 0.0625;
	int		num, i;

	if (y <= -8)
		return;							/* totally off screen */
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglEnable (GL_BLEND);
	v_index = 0;

	for (i = 0; *str && (i < len); i++, x += text_size) {
		if ((num = *str++) != 32) {		/* Skip drawing spaces */
			frow = (float) (num >> 4) * size;
			fcol = (float) (num & 15) * size;
			VectorSet2 (tc_array_v(v_index + 0), fcol, frow);
			VectorSet2 (v_array_v(v_index + 0), x, y);
			VectorSet2 (tc_array_v(v_index + 1), fcol + size, frow);
			VectorSet2 (v_array_v(v_index + 1), x + text_size, y);
			VectorSet2 (tc_array_v(v_index + 2), fcol + size, frow + size);
			VectorSet2 (v_array_v(v_index + 2), x + text_size, y + text_size);
			VectorSet2 (tc_array_v(v_index + 3), fcol, frow + size);
			VectorSet2 (v_array_v(v_index + 3), x, y + text_size);
			v_index += 4;
			if ((v_index + 4) >= MAX_VERTEX_ARRAYS) {
				TWI_PreVDraw (0, v_index);
				qglDrawArrays (GL_QUADS, 0, v_index);
				TWI_PostVDraw ();
				v_index = 0;
			}
		}
	}
	if (v_index) {
		TWI_PreVDraw (0, v_index);
		qglDrawArrays (GL_QUADS, 0, v_index);
		TWI_PostVDraw ();
		v_index = 0;
	}
	qglDisable (GL_BLEND);
}

void
Draw_String (float x, float y, char *str, float text_size)
{
	Draw_String_Len (x, y, str, strlen(str), text_size);
}


void
Draw_Alt_String_Len (float x, float y, char *str, int len, float text_size)
{
	float	frow, fcol, size = 0.0625;
	int		num, i;

	if (y <= -8)
		return;							/* totally off screen */
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglEnable (GL_BLEND);
	v_index = 0;

	for (i = 0; *str && (i < len); i++, x += text_size)
	{
		if ((num = *str++ | 0x80) != (32 | 0x80))
		{
			frow = (float) (num >> 4) * size;
			fcol = (float) (num & 15) * size;
			VectorSet2 (tc_array_v(v_index + 0), fcol, frow);
			VectorSet2 (v_array_v(v_index + 0), x, y);
			VectorSet2 (tc_array_v(v_index + 1), fcol + size, frow);
			VectorSet2 (v_array_v(v_index + 1), x + text_size, y);
			VectorSet2 (tc_array_v(v_index + 2), fcol + size, frow + size);
			VectorSet2 (v_array_v(v_index + 2), x + text_size, y + text_size);
			VectorSet2 (tc_array_v(v_index + 3), fcol, frow + size);
			VectorSet2 (v_array_v(v_index + 3), x, y + text_size);
			v_index += 4;
			if ((v_index + 4) >= MAX_VERTEX_ARRAYS)
			{
				TWI_PreVDraw (0, v_index);
				qglDrawArrays (GL_QUADS, 0, v_index);
				TWI_PostVDraw ();
				v_index = 0;
			}
		}
	}
	if (v_index)
	{
		TWI_PreVDraw (0, v_index);
		qglDrawArrays (GL_QUADS, 0, v_index);
		TWI_PostVDraw ();
		v_index = 0;
	}
	qglDisable (GL_BLEND);
}

void
Draw_Alt_String (float x, float y, char *str, float text_size)
{
	Draw_Alt_String_Len (x, y, str, strlen(str), text_size);
}

void
Draw_Conv_String_Len (float x, float y, char *str, int len, float text_size)
{
	float	frow, fcol, size = 0.0625;
	int		num, i;

	if (y <= -8)
		return;							/* totally off screen */
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglEnable (GL_BLEND);
	v_index = 0;

	for (i = 0; *str && (i < len); i++, x += text_size, str++)
	{
		switch (*str) {
			case '(': num = 0x1d; break;
			case '-': num = 0x1e; break;
			case ')': num = 0x1f; break;
			case '<': num = 0x80; break;
			case '=': num = 0x81; break;
			case '>': num = 0x82; break;
			default: num = *str | 0x80; break;
		}

		if (num != (' ' | 0x80))
		{
			frow = (float) (num >> 4) * size;
			fcol = (float) (num & 15) * size;
			VectorSet2 (tc_array_v(v_index + 0), fcol, frow);
			VectorSet2 (v_array_v(v_index + 0), x, y);
			VectorSet2 (tc_array_v(v_index + 1), fcol + size, frow);
			VectorSet2 (v_array_v(v_index + 1), x + text_size, y);
			VectorSet2 (tc_array_v(v_index + 2), fcol + size, frow + size);
			VectorSet2 (v_array_v(v_index + 2), x + text_size, y + text_size);
			VectorSet2 (tc_array_v(v_index + 3), fcol, frow + size);
			VectorSet2 (v_array_v(v_index + 3), x, y + text_size);
			v_index += 4;
			if ((v_index + 4) >= MAX_VERTEX_ARRAYS)
			{
				TWI_PreVDraw (0, v_index);
				qglDrawArrays (GL_QUADS, 0, v_index);
				TWI_PostVDraw ();
				v_index = 0;
			}
		}
	}
	if (v_index)
	{
		TWI_PreVDraw (0, v_index);
		qglDrawArrays (GL_QUADS, 0, v_index);
		TWI_PostVDraw ();
		v_index = 0;
	}
	qglDisable (GL_BLEND);
}

void
Draw_Conv_String (float x, float y, char *str, float text_size)
{
	Draw_Conv_String_Len (x, y, str, strlen(str), text_size);
}


void
Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t	   *gl;

	gl = (glpic_t *) pic->data;
	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qglEnable (GL_BLEND);

	VectorSet2 (tc_array_v(0), gl->sl, gl->tl);
	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (tc_array_v(1), gl->sh, gl->tl);
	VectorSet2 (v_array_v(1), x + pic->width, y);
	VectorSet2 (tc_array_v(2), gl->sh, gl->th);
	VectorSet2 (v_array_v(2), x + pic->width, y + pic->height);
	VectorSet2 (tc_array_v(3), gl->sl, gl->th);
	VectorSet2 (v_array_v(3), x, y + pic->height);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	qglDisable (GL_BLEND);
}


void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
		int height)
{
	glpic_t	   *gl;
	float		newsl, newtl, newsh, newth;
	float		oldglwidth, oldglheight;

	gl = (glpic_t *) pic->data;

	oldglwidth = gl->sh - gl->sl;
	oldglheight = gl->th - gl->tl;

	newsl = gl->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = gl->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height;

	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	VectorSet2 (tc_array_v(0), newsl, newtl);
	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (tc_array_v(1), newsh, newtl);
	VectorSet2 (v_array_v(1), x + width, y);
	VectorSet2 (tc_array_v(2), newsh, newth);
	VectorSet2 (v_array_v(2), x + width, y + height);
	VectorSet2 (tc_array_v(3), newsl, newth);
	VectorSet2 (v_array_v(3), x, y + height);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();
}


/*
=============
Only used for the player color selection menu
=============
*/
void
Draw_TransPicTranslate (int x, int y, qpic_t *pic, Uint8 *translation)
{
	int         v, u, c;
	unsigned    trans[64 * 64], *dest;
	Uint8      *src;
	int         p;

	qglEnable (GL_BLEND);
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
				dest[u] = d_palette_raw[translation[p]];
		}
	}

	GL_Upload32 (trans, 64, 64, TEX_ALPHA);

	VectorSet2 (tc_array_v(0), 0, 0);
	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (tc_array_v(1), 1, 0);
	VectorSet2 (v_array_v(1), x + pic->width, y);
	VectorSet2 (tc_array_v(2), 1, 1);
	VectorSet2 (v_array_v(2), x + pic->width, y + pic->height);
	VectorSet2 (tc_array_v(3), 0, 1);
	VectorSet2 (v_array_v(3), x, y + pic->height);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();
	qglDisable (GL_BLEND);
}


/*
=============
Fills a box of pixels with a single color
=============
*/
void
Draw_Fill (int x, int y, int w, int h, vec4_t color)
{
	qglDisable (GL_TEXTURE_2D);

	qglColor4fv (color);

	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (v_array_v(1), x + w, y);
	VectorSet2 (v_array_v(2), x + w, y + h);
	VectorSet2 (v_array_v(3), x, y + h);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	qglColor4fv (whitev);
	qglEnable (GL_TEXTURE_2D);
}


void
Draw_Box (int x, int y, int w, int h, int t, vec4_t color1, vec4_t color2)
{
	bvec4_t c1, c2;
	Uint32  indices[] = {
		0, 2, 3, 1,
		2, 4, 5, 3,
		4, 6, 7, 5,
		6, 0, 1, 7,
	};

	TWI_FtoUB(color1, c1, 4);
	TWI_FtoUB(color2, c2, 4);

	qglDisable (GL_TEXTURE_2D);
	qglEnableClientState (GL_COLOR_ARRAY);
	qglEnable (GL_BLEND);

	VectorSet2 (v_array_v(0), x  ,     y      ); // Top left out.
	VectorCopy4 (c1, c_array_v(0));
	VectorSet2 (v_array_v(1), x   + t, y   + t); // Top left in.
	VectorCopy4 (c2, c_array_v(1));
	VectorSet2 (v_array_v(2), x+w,     y      ); // Top right out.
	VectorCopy4 (c1, c_array_v(2));
	VectorSet2 (v_array_v(3), x+w - t, y   + t); // Top right in.
	VectorCopy4 (c2, c_array_v(3));
	VectorSet2 (v_array_v(4), x+w,     y+h    ); // Bottom right out.
	VectorCopy4 (c1, c_array_v(4));
	VectorSet2 (v_array_v(5), x+w - t, y+h - t); // Bottom right in.
	VectorCopy4 (c2, c_array_v(5));
	VectorSet2 (v_array_v(6), x  ,     y+h    ); // Bottom left out.
	VectorCopy4 (c1, c_array_v(6));
	VectorSet2 (v_array_v(7), x   + t, y+h - t); // Bottom left in.
	VectorCopy4 (c2, c_array_v(7));
	qglDrawElements(GL_QUADS, sizeof(indices) / sizeof(Uint32), GL_UNSIGNED_INT, indices);

	qglDisable (GL_BLEND);
	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor4fv (whitev);
	qglEnable (GL_TEXTURE_2D);
}


/* ========================================================================= */

void
Draw_FadeScreen (void)
{
	qglEnable (GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.8);

	VectorSet2 (v_array_v(0), 0, 0);
	VectorSet2 (v_array_v(1), vid.width_2d, 0);
	VectorSet2 (v_array_v(2), vid.width_2d, vid.height_2d);
	VectorSet2 (v_array_v(3), 0, vid.height_2d);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	qglColor4fv (whitev);
	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
}

/* ========================================================================= */

/*
================
Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void
Draw_Disc (void)
{
	if (!draw_disc)
		return;
	qglDrawBuffer (GL_FRONT);
	Draw_Pic (vid.width_2d - 24, 0, draw_disc);
	qglDrawBuffer (GL_BACK);
}

void
Draw_Crosshair (void)
{
	float	x1, y1, x2, y2;
	int		color;
	float	base[4];
	float	ofs;
	int		ctexture;

	if (crosshair->ivalue < 1)
		return;

	/* FIXME: when textures have structures, fix the hardcoded 32x32 size */

	ctexture = ch_textures[((crosshair->ivalue - 1) % NUM_CROSSHAIRS)];

	x1 = (vid.width - 32 * hud_chsize->fvalue) * 0.5
		* vid.width_2d / vid.width;
	y1 = (vid.height - 32 * hud_chsize->fvalue) * 0.5
		* vid.height_2d / vid.height;

	x2 = (vid.width + 32 * hud_chsize->fvalue) * 0.5
		* vid.width_2d / vid.width;
	y2 = (vid.height + 32 * hud_chsize->fvalue) * 0.5
		* vid.height_2d / vid.height;

	/* Color selection madness */
	color = crosshaircolor->ivalue % 256;
	if (color == 255 && ccl.colormap)
		VectorScale (ccl.colormap->bottom, 0.5, base);
	else
		VectorCopy (d_8tofloattable[color], base);

	ofs = Q_sin (r_time * M_PI * hud_chspeed->fvalue) * hud_chflash->fvalue;
	ofs = boundsign (ofs, hud_chflash->fvalue);
	VectorSlide (base, ofs, base);
	base[3] = hud_chalpha->fvalue;
	qglColor4fv (base);

	qglBindTexture (GL_TEXTURE_2D, ctexture);

	qglEnable (GL_BLEND);
	VectorSet2 (tc_array_v(0), 0, 0);
	VectorSet2 (v_array_v(0), x1, y1);
	VectorSet2 (tc_array_v(1), 1, 0);
	VectorSet2 (v_array_v(1), x2, y1);
	VectorSet2 (tc_array_v(2), 1, 1);
	VectorSet2 (v_array_v(2), x2, y2);
	VectorSet2 (tc_array_v(3), 0, 1);
	VectorSet2 (v_array_v(3), x1, y2);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();
	qglColor4fv (whitev);
	qglDisable (GL_BLEND);
}


void
Draw_ConsoleBackground (int lines)
{
	int		y;
	qpic_t	*conback;
	glpic_t	*gl;
	float	alpha;
	float	ofs;

	conback = Draw_CachePic ("gfx/conback.lmp");
	gl = (glpic_t *) conback->data;

	y = (vid.height_2d * 3) >> 2;

	if (ccls.state != ca_active || lines > y)
		alpha = 1.0;
	else
		alpha = (float) (0.6 * lines / y);

	if (gl_constretch->ivalue)
		ofs = 0.0f;
	else
		ofs = (float) ((vid.height_2d - lines) / vid.height_2d);

	if (alpha != 1.0f) {
		qglColor4f (1.0f, 1.0f, 1.0f, alpha);
		qglEnable (GL_BLEND);
	}

	qglBindTexture (GL_TEXTURE_2D, gl->texnum);
	VectorSet2 (tc_array_v(0), gl->sl, gl->tl + ofs);
	VectorSet2 (v_array_v(0), 0, 0);
	VectorSet2 (tc_array_v(1), gl->sh, gl->tl + ofs);
	VectorSet2 (v_array_v(1), vid.width_2d, 0);
	VectorSet2 (tc_array_v(2), gl->sh, gl->th);
	VectorSet2 (v_array_v(2), vid.width_2d, lines);
	VectorSet2 (tc_array_v(3), gl->sl, gl->th);
	VectorSet2 (v_array_v(3), 0, lines);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();

	/* hack the version number directly into the pic */
	{
		int     ver_len = strlen (cl_verstring->svalue);

		Draw_Alt_String_Len (vid.width_2d - (ver_len * con->tsize) - 16,
				lines - 14, cl_verstring->svalue, ver_len, con->tsize);
	}
	if (alpha != 1.0f)
		qglDisable (GL_BLEND);

	qglColor4fv (whitev);
}

/*
================
Setup for single-pixel units
================
 */
void
GL_Set2D (void)
{
	qglViewport (0, 0, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, vid.width_2d, vid.height_2d, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
}

