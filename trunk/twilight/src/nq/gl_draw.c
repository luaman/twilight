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
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "gl_textures.h"
#include "pointers.h"
#include "strlib.h"
#include "gl_draw.h"

extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor;

static cvar_t *gl_constretch;
static cvar_t *cl_verstring;					/* FIXME: Move this? */

static cvar_t *hud_chsize;
static cvar_t *hud_chflash;
static cvar_t *hud_chspeed;
static cvar_t *hud_chalpha;

static GLuint	ch_textures[NUM_CROSSHAIRS];			// crosshair texture

/*
===============
Draw_Init_Cvars
===============
*/
void
Draw_Init_Cvars (void)
{
	GLT_Init_Cvars ();

	gl_constretch = Cvar_Get ("gl_constretch", "1", CVAR_ARCHIVE, NULL);
	cl_verstring = Cvar_Get ("cl_verstring",
			"Project Twilight v" VERSION " NQ", CVAR_NONE, NULL);

	hud_chsize = Cvar_Get ("hud_chsize", "0.5", CVAR_ARCHIVE, NULL);
	hud_chflash = Cvar_Get ("hud_chflash", "0.0", CVAR_ARCHIVE, NULL);
	hud_chspeed = Cvar_Get ("hud_chspeed", "1.5", CVAR_ARCHIVE, NULL);
	hud_chalpha = Cvar_Get ("hud_chalpha", "1.0", CVAR_ARCHIVE, NULL);
}

/*
===============
Draw_Init
===============
*/
void
Draw_Init (void)
{
	int			i;

	GLT_Init ();
	Draw_Common_Init ();

	// Keep track of the first crosshair texture
	for (i = 0; i < NUM_CROSSHAIRS; i++)
		ch_textures[i] = GLT_Load_Pixmap (va ("crosshair%i", i), crosshairs[i]);
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
	
	ctexture = ch_textures[((crosshair->ivalue - 1) % NUM_CROSSHAIRS)];

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
		int		ver_len = strlen (cl_verstring->svalue);

		Draw_Alt_String_Len (vid.width_2d - (ver_len * con->tsize) - 16,
				lines - 14, cl_verstring->svalue, ver_len, con->tsize);
	}
	if (alpha != 1.0f)
		qglDisable (GL_BLEND);

	qglColor4fv (whitev);
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
	qglViewport (0, 0, vid.width, vid.height);

	qglMatrixMode (GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, vid.width_2d, vid.height_2d, 0, -99999, 99999);

	qglMatrixMode (GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
}
