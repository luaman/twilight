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

#include "quakedef.h"
#include "client.h"
#include "crc.h"
#include "cvar.h"
#include "draw.h"
#include "glquake.h"
#include "host.h"
#include "image.h"
#include "mathlib.h"
#include "pointers.h"
#include "strlib.h"
#include "sys.h"
#include "gl_textures.h"
#include "console.h"

extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor;

cvar_t *gl_max_size;
cvar_t *gl_picmip;
cvar_t *gl_constretch;
cvar_t *gl_texturemode;
cvar_t *cl_verstring;					/* FIXME: Move this? */
cvar_t *r_lerpimages;
cvar_t *r_colormiplevels;

cvar_t *hud_chsize;
cvar_t *hud_chflash;
cvar_t *hud_chspeed;
cvar_t *hud_chalpha;

qpic_t *draw_disc;
qpic_t *draw_backtile;

int         translate_texture;
int         char_texture;
int         ch_texture;					// crosshair texture

typedef struct {
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

int		gl_lightmap_format = GL_RGB;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_mag = GL_LINEAR;


int		texels;

typedef struct {
	int         texnum;
	char        identifier[64];
	int         width, height;
	qboolean    mipmap;
	unsigned short crc;
} gltexture_t;

gltexture_t gltextures[MAX_GLTEXTURES];
int         numgltextures;


/* ========================================================================= */
/* Support Routines */

typedef struct cachepic_s {
	char	name[MAX_QPATH];
	qpic_t	pic;
	Uint8	padding[32];	/* for appended glpic */
} cachepic_t;

#define		MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

Uint8		menuplyr_pixels[4096];

int			pic_texels;
int			pic_count;

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
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


typedef struct {
	char       *name;
	int         minimize, magnify;
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
		if (!strcasecmp (modes[i].name, var->svalue))
			break;
	}
	if (i == 6) {
		Cvar_Set (gl_texturemode,"GL_LINEAR_MIPMAP_NEAREST");
		Com_Printf ("Bad GL_TEXTUREMODE, valid modes are:\n");
		Com_Printf ("GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST\n");
		Com_Printf ("GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST (default), \n");
		Com_Printf ("GL_LINEAR_MIPMAP_LINEAR (trilinear)\n\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_mag = modes[i].magnify;

	/* change all the existing mipmap texture objects */
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->mipmap) {
			qglBindTexture (GL_TEXTURE_2D, glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					gl_filter_min);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					gl_filter_mag);
		}
	}
}

/*
===============
R_LoadPointer

Loads a string into a named 32x32 greyscale OpenGL texture, suitable for
crosshairs or mouse pointers.  The data string is in a format similar to an
X11 pixmap.  '0'-'7' are brightness levels, any other character is considered
transparent.

 FIXME: No error checking is performed on the input string.  Do not give users
		a way to load anything using this function without fixing it.
 FIXME: Maybe hex values (0-F) for brightness?  Used 0-7 so we could borrow
		DarkPlaces' crosshairs, but we use 32x32 instead of 16x16.
===============
*/
static int
R_LoadPointer (const char *name, const char *data)
{
	int			i;
	image_t		img;
	Uint8		pixels[32*32][4];

	for (i = 0; i < 32*32; i++)
	{
		if (data[i] >= '0' && data[i] < '8')
		{
			pixels[i][0] = 255;
			pixels[i][1] = 255;
			pixels[i][2] = 255;
			pixels[i][3] = (data[i] - '0') * 32;
		} else {
			pixels[i][0] = 255;
			pixels[i][1] = 255;
			pixels[i][2] = 255;
			pixels[i][3] = 0;
		}
	}

	img.type = IMG_RGBA;
	img.width = 32;
	img.height = 32;
	img.pixels = (Uint8 *)pixels;

	return R_LoadTexture (name, &img, TEX_ALPHA);
}

/*
===============
Draw_Init_Cvars
===============
*/
void
Draw_Init_Cvars (void)
{
	int max_tex_size = 0;
	
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	if (!max_tex_size)
		max_tex_size = 1024;

	gl_max_size = Cvar_Get ("gl_max_size", va("%d", max_tex_size), CVAR_NONE,
			NULL);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL);
	gl_constretch = Cvar_Get ("gl_constretch", "1", CVAR_ARCHIVE, NULL);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
			CVAR_ARCHIVE, Set_TextureMode_f);
	cl_verstring = Cvar_Get ("cl_verstring",
			"Project Twilight v" VERSION " NQ", CVAR_NONE, NULL);
	r_lerpimages = Cvar_Get ("r_lerpimages", "1", CVAR_ARCHIVE, NULL);
	r_colormiplevels = Cvar_Get ("r_colormiplevels", "0", CVAR_NONE, NULL);

	hud_chsize = Cvar_Get ("hud_chsize", "0.5", CVAR_ARCHIVE, NULL);
	hud_chflash = Cvar_Get ("hud_chflash", "0.0", CVAR_ARCHIVE, NULL);
	hud_chspeed = Cvar_Get ("hud_chspeed", "1.5", CVAR_ARCHIVE, NULL);
	hud_chalpha = Cvar_Get ("hud_chalpha", "1.0", CVAR_ARCHIVE, NULL);
}

static memzone_t *resamplezone;

/*
===============
Draw_Init
===============
*/
void
Draw_Init (void)
{
	image_t		*img;
	int			i;

	resamplezone = Zone_AllocZone("Texture Processing Buffers");

	img = Image_Load ("conchars");
	if (!img)
		Sys_Error ("Draw_Init: Unable to load conchars\n");
	
	char_texture = R_LoadTexture ("charset", img, TEX_ALPHA);

	// Keep track of the first crosshair texture
	ch_texture = texture_extension_number;
	for (i = 0; i < NUM_CROSSHAIRS; i++)
		R_LoadPointer (va ("crosshair%i", i), crosshairs[i]);

	/* save a texture slot for translated picture */
	translate_texture = texture_extension_number++;

	/* get the other pics we need */
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

/*
================
Draw_String_Len
================
*/
void
Draw_String_Len (float x, float y, char *str, int len, float text_size)
{
	float	frow, fcol, size = 0.0625;
	int		num, i, vnum;

	if (y <= -8)
		return;							/* totally off screen */
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglEnable (GL_BLEND);
	vnum = 0;

	for (i = 0; *str && (i < len); i++, x += text_size) {
		if ((num = *str++) != 32) {		/* Skip drawing spaces */
			frow = (float) (num >> 4) * size;
			fcol = (float) (num & 15) * size;
			VectorSet2 (tc_array_v(vnum + 0), fcol, frow);
			VectorSet2 (v_array_v(vnum + 0), x, y);
			VectorSet2 (tc_array_v(vnum + 1), fcol + size, frow);
			VectorSet2 (v_array_v(vnum + 1), x + text_size, y);
			VectorSet2 (tc_array_v(vnum + 2), fcol + size, frow + size);
			VectorSet2 (v_array_v(vnum + 2), x + text_size, y + text_size);
			VectorSet2 (tc_array_v(vnum + 3), fcol, frow + size);
			VectorSet2 (v_array_v(vnum + 3), x, y + text_size);
			vnum += 4;
			if ((vnum + 4) >= MAX_VERTEX_ARRAYS)
			{
				TWI_PreVDraw (0, vnum);
				qglDrawArrays (GL_QUADS, 0, vnum);
				TWI_PostVDraw ();
				vnum = 0;
			}
		}
	}
	if (vnum) {
		TWI_PreVDraw (0, vnum);
		qglDrawArrays (GL_QUADS, 0, vnum);
		TWI_PostVDraw ();
		vnum = 0;
	}
	qglDisable (GL_BLEND);
}

/*
================
Draw_String
================
*/
void
Draw_String (float x, float y, char *str, float text_size)
{
	Draw_String_Len (x, y, str, strlen(str), text_size);
}


/*
================
Draw_Alt_String_Len
================
*/
void
Draw_Alt_String_Len (float x, float y, char *str, int len, float text_size)
{
	float	frow, fcol, size = 0.0625;
	int		num, i, vnum;

	if (y <= -8)
		return;							/* totally off screen */
	if (!str || !str[0])
		return;

	qglBindTexture (GL_TEXTURE_2D, char_texture);

	qglEnable (GL_BLEND);
	vnum = 0;

	for (i = 0; *str && (i < len); i++, x += text_size) {
		if ((num = *str++ | 0x80) != (32 | 0x80)) {
			frow = (float) (num >> 4) * size;
			fcol = (float) (num & 15) * size;
			VectorSet2 (tc_array_v(vnum + 0), fcol, frow);
			VectorSet2 (v_array_v(vnum + 0), x, y);
			VectorSet2 (tc_array_v(vnum + 1), fcol + size, frow);
			VectorSet2 (v_array_v(vnum + 1), x + text_size, y);
			VectorSet2 (tc_array_v(vnum + 2), fcol + size, frow + size);
			VectorSet2 (v_array_v(vnum + 2), x + text_size, y + text_size);
			VectorSet2 (tc_array_v(vnum + 3), fcol, frow + size);
			VectorSet2 (v_array_v(vnum + 3), x, y + text_size);
			vnum += 4;
			if ((vnum + 4) >= MAX_VERTEX_ARRAYS) {
				TWI_PreVDraw (0, vnum);
				qglDrawArrays (GL_QUADS, 0, vnum);
				TWI_PostVDraw ();
				vnum = 0;
			}
		}
	}
	if (vnum) {
		TWI_PreVDraw (0, vnum);
		qglDrawArrays (GL_QUADS, 0, vnum);
		TWI_PostVDraw ();
		vnum = 0;
	}
	qglDisable (GL_BLEND);
}

/*
================
Draw_Alt_String
================
*/
void
Draw_Alt_String (float x, float y, char *str, float text_size)
{
	Draw_Alt_String_Len (x, y, str, strlen(str), text_size);
}

void
Draw_Crosshair (void)
{
	float			x1, y1, x2, y2;
	int				color;
	float			base[4];
	float			ofs;
	int				ctexture;

	if (crosshair->ivalue < 1)
		return;

	// FIXME: when textures have structures, fix the hardcoded 32x32 size
	
	ctexture = ch_texture + ((crosshair->ivalue - 1) % NUM_CROSSHAIRS);

	x1 = (vid.width - 32 * hud_chsize->fvalue) * 0.5
		* vid.width_2d / vid.width;
	y1 = (vid.height - 32 * hud_chsize->fvalue) * 0.5
		* vid.height_2d / vid.height;

	x2 = (vid.width + 32 * hud_chsize->fvalue) * 0.5
		* vid.width_2d / vid.width;
	y2 = (vid.height + 32 * hud_chsize->fvalue) * 0.5
		* vid.height_2d / vid.height;

	// Color selection madness
	color = crosshaircolor->ivalue % 256;
	if (color == 255 && cl.colormap)
		VectorScale (cl.colormap->bottom, 0.5, base);
	else
		VectorCopy (d_8tofloattable[color], base);

	ofs = Q_sin (cl.time * M_PI * hud_chspeed->fvalue) * hud_chflash->fvalue;
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


/*
=============
Draw_Pic
=============
*/
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
Draw_TransPicTranslate

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
				dest[u] = d_8to32table[translation[p]];
		}
	}

	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_mag);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_mag);
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);

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

	y = (vid.height_2d * 3) >> 2;

	if (cls.state != ca_connected || lines > y)
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
		int	ver_len = strlen (cl_verstring->svalue);

		Draw_Alt_String_Len (vid.width_2d - (ver_len * con->tsize) - 16,
				lines - 14, cl_verstring->svalue, ver_len, con->tsize);
	}

	if (alpha != 1.0f)
		qglDisable (GL_BLEND);

	qglColor4fv (whitev);
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
	qglBindTexture (GL_TEXTURE_2D, *(int *) draw_backtile->data);
	VectorSet2 (tc_array_v(0), x / 64.0, y / 64.0);
	VectorSet2 (v_array_v(0), x, y);
	VectorSet2 (tc_array_v(1), (x + w) / 64.0, y / 64.0);
	VectorSet2 (v_array_v(1), x + w, y);
	VectorSet2 (tc_array_v(2), (x + w) / 64.0, (y + h) / 64.0);
	VectorSet2 (v_array_v(2), x + w, y + h);
	VectorSet2 (tc_array_v(3), x / 64.0, (y + h) / 64.0);
	VectorSet2 (v_array_v(3), x, y + h);
	TWI_PreVDraw (0, 4);
	qglDrawArrays (GL_QUADS, 0, 4);
	TWI_PostVDraw ();
}


/*
=============
Draw_Fill

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

/* ========================================================================= */

/*
================
Draw_FadeScreen

================
*/
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
Draw_Disc

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


/*
================
GL_Set2D

Setup for single-pixel units
================
*/
void
GL_Set2D (void)
{
	qglViewport (glx, gly, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, vid.width_2d, vid.height_2d, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
}

/* ========================================================================= */

/*
================
R_ResampleTextureLerpLine
================
*/
void
R_ResampleTextureLerpLine (Uint8 *in, Uint8 *out, int inwidth, int outwidth)
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
		} else {
			/* last pixel of the line has no pixel to lerp to */
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
void
R_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,
		int outwidth, int outheight)
{
	if (r_lerpimages->ivalue)
	{
		int		i, j, yi, oldy, f, fstep, endy = (inheight-1);
		Uint8	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep = (int) (inheight*65536.0f/outheight);

		row1 = Zone_Alloc(tempzone, outwidth*4);
		row2 = Zone_Alloc(tempzone, outwidth*4);
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
			} else {
				if (yi != oldy)
				{
					inrow = (Uint8 *)indata + inwidth * 4 * yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth * 4);
					else
						R_ResampleTextureLerpLine (inrow, row1, inwidth,
								outwidth);
					oldy = yi;
				}
				memcpy(out, row1, outwidth * 4);
			}
		}
		Zone_Free(row1);
		Zone_Free(row2);
	} else {
		int i, j;
		unsigned frac, fracstep;
		/* relies on int32 being 4 bytes */
		Uint32 *inrow, *out;
		out = outdata;

		fracstep = inwidth*0x10000/outwidth;
		for (i = 0;i < outheight;i++)
		{
			inrow = (int *)indata + inwidth * (i * inheight / outheight);
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
	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}


static int scaledsize = 0;
static Uint32 *scaled, *scaled2;
static int transsize = 0;
static Uint32 *trans;

/*
===============
GL_Upload32
===============
*/
void
GL_Upload32 (Uint32 *data, int width, int height, int flags)
{
	int samples, scaled_width, scaled_height;

	// OpenGL textures are power of two
	scaled_width = 1;
	while (scaled_width < width)
		scaled_width <<= 1;

	scaled_height = 1;
	while (scaled_height < height)
		scaled_height <<= 1;

	// Apply gl_picmip, a setting of one cuts texture memory usage 75%!
	scaled_width >>= gl_picmip->ivalue;
	scaled_height >>= gl_picmip->ivalue;

	// Clip textures to a sane value
	scaled_width = bound (1, scaled_width, gl_max_size->ivalue);
	scaled_height = bound (1, scaled_height, gl_max_size->ivalue);

	if (scaledsize < scaled_width * scaled_height)
	{
		scaledsize = scaled_width * scaled_height;
		if (scaled)
			Zone_Free(scaled);
		if (scaled2)
			Zone_Free(scaled2);
		scaled = NULL;
		scaled2 = NULL;
	}

	samples = (flags & TEX_ALPHA) ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled == NULL)
		scaled = Zone_Alloc(resamplezone, scaledsize * 4);

	if (scaled_width == width && scaled_height == height)
	{
		if (flags ^ TEX_MIPMAP)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width,
					scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					gl_filter_mag);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					gl_filter_mag);
		}
		memcpy (scaled, data, width * height * 4);
	} else
		R_ResampleTexture (data, width, height, scaled, scaled_width,
				scaled_height);

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (flags & TEX_MIPMAP)
	{
		int miplevel = 0;
		int channel, i;

		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((Uint8 *) scaled, scaled_width, scaled_height);

			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max (scaled_width, 1);
			scaled_height = max (scaled_height, 1);

			miplevel++;

			if (r_colormiplevels->ivalue)
			{
				if (scaled2 == NULL)
					scaled2 = Zone_Alloc(resamplezone, scaledsize * 4);

				channel = (miplevel - 1) % 3;
				for (i = 0; i < (scaled_width * scaled_height); i++)
				{
					scaled2[i] = 0;
					((Uint8 *)&scaled2[i])[channel] = ((Uint8 *)&scaled[i])[channel];
					((Uint8 *)&scaled2[i])[3] = ((Uint8 *)&scaled[i])[3];
				}

				qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled2);
			}
			else
				qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}

	if (flags & TEX_MIPMAP)
	{
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_mag);
	} else {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_mag);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_mag);
	}
}


/*
===============
GL_Upload8
===============
*/
void
GL_Upload8 (Uint8 *data, int width, int height, unsigned *ttable, int flags)
{
	int i, s = width * height, p, noalpha;
	Uint32 *table = ttable ? ttable : d_8to32table;

	if (transsize < s)
	{
		transsize = s;
		if (trans)
			Zone_Free(trans);
		trans = NULL;
	}

	if (trans == NULL)
		trans = Zone_Alloc(resamplezone, transsize * 4);

	if (flags & TEX_FBMASK)
	{
		/*
		 * this is a fullbright mask, so make all non-fullbright
		 * colors transparent
		 */
		for (i = 0; i < s; i++)
		{
			p = *data++;
			if (p < FIRST_FB)
				trans[i] = 0;			/* transparent */
			else
				trans[i] = table[p];	/* fullbright */
		}
		flags |= TEX_ALPHA;
	}
	else if (TEX_ALPHA)
	{
		noalpha = true;
		for (i = 0; i < s; i++)
		{
			p = *data++;
			if (p == 255)
				noalpha = false;
			trans[i] = table[p];
		}

		if (noalpha)
			flags &= ~TEX_ALPHA;
	} else {
		if (s & 3)
			Host_EndGame ("GL_Upload8: s&3");
		for (i = 0; i < s; i += 4)
		{
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}

	GL_Upload32 (trans, width, height, flags);
}

/*
================
R_LoadTexture

FIXME: HACK HACK HACK - this is just a GL_LoadTexture for image_t's for now
		and is temporary.  It should be replaced by a function which takes
		only the name and some flags, and which returns a (yet unwritten)
		texture structure.
================
*/
int
R_LoadTexture (const char *identifier, image_t *img, int flags)
{
	int         i;
	gltexture_t *glt;
	unsigned short crc = 0;

	if (isDedicated)
		return 0;

	/* see if the texture is already present */
	if (identifier[0])
	{
		crc = CRC_Block (img->pixels, img->width * img->height);

		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (img->width == (Uint32)glt->width
						&& img->height == (Uint32)glt->height
						&& crc == glt->crc)
					return gltextures[i].texnum;
				else
					/* reload the texture into the same slot */
					goto setuptexture;
			}
		}
	} else
		glt = &gltextures[numgltextures];

	numgltextures++;
	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number++;

setuptexture:
	glt->width = img->width;
	glt->height = img->height;
	glt->mipmap = flags & TEX_MIPMAP;
	glt->crc = crc;

	qglBindTexture (GL_TEXTURE_2D, glt->texnum);

	switch (img->type)
	{
		case IMG_QPAL:
			GL_Upload8 (img->pixels, img->width, img->height, NULL, flags);
			break;
		case IMG_RGBA:
			GL_Upload32 ((Uint32 *) img->pixels, img->width, img->height,
					flags);
			break;
		default:
			return 0;
	}

	return glt->texnum;
}


/*
================
GL_LoadTexture
================
*/
int
GL_LoadTexture (const char *identifier, int width, int height, Uint8 *data,
		int flags, int bpp)
{
	int         i;
	gltexture_t *glt;
	unsigned short crc = 0;

	if (isDedicated)
		return 0;

	/* see if the texture is already present */
	if (identifier[0])
	{
		crc = CRC_Block (data, width*height);

		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (width == glt->width && height == glt->height
						&& crc == glt->crc)
					return gltextures[i].texnum;
				else
					/* reload the texture into the same slot */
					goto setuptexture;
			}
		}
	} else
		glt = &gltextures[numgltextures];

	numgltextures++;
	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number++;

setuptexture:
	glt->width = width;
	glt->height = height;
	glt->mipmap = flags & TEX_MIPMAP;
	glt->crc = crc;

	qglBindTexture (GL_TEXTURE_2D, glt->texnum);

	switch (bpp) {
		case 8:
			GL_Upload8 (data, width, height, NULL, flags);
			break;
		case 32:
			GL_Upload32 ((Uint32 *) data, width, height, flags);
			break;
		default:
			return 0;
	}

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
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, TEX_ALPHA, 8);
}

