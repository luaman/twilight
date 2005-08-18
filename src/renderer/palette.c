/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull
	Copyright (C) 2003  Forest Hale

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

#include <string.h>

#include "common.h"
#include "cvar.h"
#include "gl_info.h"
#include "host.h"
#include "mathlib.h"
#include "sys.h"
#include "video.h"


Uint8	*host_basepal;

Uint32 d_palette_raw[256];
Uint32 d_palette_base[256];
Uint32 d_palette_fb[256];
Uint32 d_palette_base_team[256];
Uint32 d_palette_top[256];
Uint32 d_palette_bottom[256];
float d_8tofloattable[256][4];

Uint8	num_fb;
cvar_t *gl_fb;
static cvar_t *v_hwgamma;

static cvar_t *v_black;
static cvar_t *v_black_r;
static cvar_t *v_black_b;
static cvar_t *v_black_g;
static cvar_t *v_grey;
static cvar_t *v_grey_r;
static cvar_t *v_grey_b;
static cvar_t *v_grey_g;
static cvar_t *v_white;
static cvar_t *v_white_r;
static cvar_t *v_white_b;
static cvar_t *v_white_g;

static cvar_t *v_tblack;
static cvar_t *v_tblack_r;
static cvar_t *v_tblack_b;
static cvar_t *v_tblack_g;
static cvar_t *v_tgrey;
static cvar_t *v_tgrey_r;
static cvar_t *v_tgrey_b;
static cvar_t *v_tgrey_g;
static cvar_t *v_twhite;
static cvar_t *v_twhite_r;
static cvar_t *v_twhite_b;
static cvar_t *v_twhite_g;

static Uint16 hw_gamma_ramps[3][256];
static Uint8 tex_gamma_ramps[3][256];

static void
PAL_Build_Gamma_Ramp8 (Uint8 *ramp, int n, double black, double grey,
		double white)
{
	int			i;
	double		i_d, i_s, invgam, gamma, scale, base;

	gamma = invpow(0.5, grey);
	scale = white - black;
	base = black;

	i_s = 1.0 / n;
	invgam = 1.0 / gamma;

	for (i_d = i = 0; i < n; i++, i_d++)
		ramp[i]=bound_bits((pow(i_d * i_s, invgam) * scale + base) *BIT(8),8);
}

static void
PAL_Build_Gamma_Ramp16 (Uint16 *ramp, int n, double black, double grey,
		double white)
{
	int			i;
	double		i_d, i_s, invgam, gamma, scale, base;

	gamma = invpow(0.5, grey);
	scale = white - black;
	base = black;

	i_s = 1.0 / n;
	invgam = 1.0 / gamma;

	for (i_d = i = 0; i < n; i++, i_d++)
		ramp[i]=bound_bits((pow(i_d * i_s, invgam) * scale + base) *BIT(16),16);
}

static void
PAL_RebuildPalettes (void)
{
	int		i, n_fb;

	if (gl_fb->ivalue)
		n_fb = num_fb;
	else
		n_fb = 0;

	for (i = 0; i < (256 - n_fb); i++)
	{
		d_palette_base_team[i] = d_palette_base[i] = d_palette_raw[i];
		d_palette_fb[i] = d_palette_empty;
		d_palette_top[i] = d_palette_bottom[i] = d_palette_empty;
	}
	for (; i < 256; i++)
	{
		d_palette_base_team[i] = d_palette_base[i] = d_palette_empty;
		d_palette_fb[i] = d_palette_raw[i];
		d_palette_top[i] = d_palette_bottom[i] = d_palette_empty;
	}

	for (i = 0x10; i < 0x20; i++)	// Top range.
	{
		d_palette_top[i] = d_palette_raw[i - 0x10];
		d_palette_base_team[i] = d_palette_empty;
	}
	for (i = 0x60; i < 0x70; i++)	// Bottom range.
	{
		d_palette_bottom[i] = d_palette_raw[i - 0x60];
		d_palette_base_team[i] = d_palette_empty;
	}
	d_palette_base[255] = d_palette_base_team[255] = d_palette_top[255]
		= d_palette_bottom[255] = d_palette_raw[255];
}

static void
TGammaChanged (cvar_t *cvar)
{
	int			i;
	Uint8		*pal;
	Uint8		r, g, b;
	vec3_t		tex_b, tex_g, tex_w;

	cvar = cvar;

	/* Might be init, we don't want to segfault. */
	if (!(v_tblack && v_tblack_r && v_tblack_g && v_tblack_b &&
		  v_tgrey && v_tgrey_r && v_tgrey_g && v_tgrey_b &&
		  v_twhite && v_twhite_r && v_twhite_g && v_twhite_b))
		return;

	tex_b[0] = v_tblack->fvalue + v_tblack_r->fvalue;
	tex_b[1] = v_tblack->fvalue + v_tblack_g->fvalue;
	tex_b[2] = v_tblack->fvalue + v_tblack_b->fvalue;

	tex_g[0] = v_tgrey->fvalue + v_tgrey_r->fvalue;
	tex_g[1] = v_tgrey->fvalue + v_tgrey_g->fvalue;
	tex_g[2] = v_tgrey->fvalue + v_tgrey_b->fvalue;

	tex_w[0] = v_twhite->fvalue + v_twhite_r->fvalue;
	tex_w[1] = v_twhite->fvalue + v_twhite_g->fvalue;
	tex_w[2] = v_twhite->fvalue + v_twhite_b->fvalue;

	PAL_Build_Gamma_Ramp8 (tex_gamma_ramps[0], 256, tex_b[0],tex_g[0],tex_w[0]);
	PAL_Build_Gamma_Ramp8 (tex_gamma_ramps[1], 256, tex_b[1],tex_g[1],tex_w[1]);
	PAL_Build_Gamma_Ramp8 (tex_gamma_ramps[2], 256, tex_b[2],tex_g[2],tex_w[2]);

	/* 8 8 8 encoding */
	pal = host_basepal;
	for (i = 0; i < 256; i++)
	{
		r = tex_gamma_ramps[0][pal[0]];
		g = tex_gamma_ramps[1][pal[1]];
		b = tex_gamma_ramps[2][pal[2]];
		pal += 3;

		d_8tofloattable[i][0] = (float) r / 255;
		d_8tofloattable[i][1] = (float) g / 255;
		d_8tofloattable[i][2] = (float) b / 255;
		d_8tofloattable[i][3] = 1;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		d_palette_raw[i] = (r << 24) + (g << 16) + (b << 8) + (255 << 0);
#else
		d_palette_raw[i] = (r << 0) + (g << 8) + (b << 16) + (255 << 24);
#endif
	}
	d_palette_raw[255] = 0x00000000;		/* 255 is transparent */
	VectorSet4 (d_8tofloattable[255], 0, 0, 0, 0);

	PAL_RebuildPalettes ();
}

static void
GammaChanged (cvar_t *cvar)
{
	vec3_t		hw_b, hw_g, hw_w;

	cvar = cvar;

	if (!VID_Inited)
		return;

	/* Might be init, we don't want to segfault. */
	if (!(v_hwgamma && v_black && v_black_r && v_black_g && v_black_b &&
				v_grey && v_grey_r && v_grey_g && v_grey_b &&
				v_white && v_white_r && v_white_g && v_white_b))
		return;

	/* Do we have and want to use hardware black? */
	hw_b[0] = v_black->fvalue + v_black_r->fvalue;
	hw_b[1] = v_black->fvalue + v_black_g->fvalue;
	hw_b[2] = v_black->fvalue + v_black_b->fvalue;

	hw_g[0] = v_grey->fvalue + v_grey_r->fvalue;
	hw_g[1] = v_grey->fvalue + v_grey_g->fvalue;
	hw_g[2] = v_grey->fvalue + v_grey_b->fvalue;

	hw_w[0] = v_white->fvalue + v_white_r->fvalue;
	hw_w[1] = v_white->fvalue + v_white_g->fvalue;
	hw_w[2] = v_white->fvalue + v_white_b->fvalue;

	PAL_Build_Gamma_Ramp16 (hw_gamma_ramps[0], 256, hw_b[0], hw_g[0], hw_w[0]);
	PAL_Build_Gamma_Ramp16 (hw_gamma_ramps[1], 256, hw_b[1], hw_g[1], hw_w[1]);
	PAL_Build_Gamma_Ramp16 (hw_gamma_ramps[2], 256, hw_b[2], hw_g[2], hw_w[2]);

	if (SDL_SetGammaRamp (hw_gamma_ramps[0], hw_gamma_ramps[1],
				hw_gamma_ramps[2]) < 0)
	{
		Com_Printf ("Unable to set hardware gamma: (%s)\n", SDL_GetError ());
	}
}

void
PAL_Init_Cvars (void)
{
	gl_fb = Cvar_Get ("gl_fb", "1", CVAR_ARCHIVE, NULL);
	v_hwgamma = Cvar_Get ("v_hwgamma", "1", CVAR_NONE, &GammaChanged);

	v_black = Cvar_Get ("v_black", "0", CVAR_ARCHIVE, &GammaChanged);
	v_black_r = Cvar_Get ("v_black_r", "0", CVAR_ARCHIVE, &GammaChanged);
	v_black_g = Cvar_Get ("v_black_g", "0", CVAR_ARCHIVE, &GammaChanged);
	v_black_b = Cvar_Get ("v_black_b", "0", CVAR_ARCHIVE, &GammaChanged);
	v_grey = Cvar_Get ("v_grey", "0.5", CVAR_ARCHIVE, &GammaChanged);
	v_grey_r = Cvar_Get ("v_grey_r", "0", CVAR_ARCHIVE, &GammaChanged);
	v_grey_g = Cvar_Get ("v_grey_g", "0", CVAR_ARCHIVE, &GammaChanged);
	v_grey_b = Cvar_Get ("v_grey_b", "0", CVAR_ARCHIVE, &GammaChanged);
	v_white = Cvar_Get ("v_white", "1", CVAR_ARCHIVE, &GammaChanged);
	v_white_r = Cvar_Get ("v_white_r", "0", CVAR_ARCHIVE, &GammaChanged);
	v_white_g = Cvar_Get ("v_white_g", "0", CVAR_ARCHIVE, &GammaChanged);
	v_white_b = Cvar_Get ("v_white_b", "0", CVAR_ARCHIVE, &GammaChanged);

	v_tblack = Cvar_Get ("v_tblack", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tblack_r = Cvar_Get ("v_tblack_r", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tblack_g = Cvar_Get ("v_tblack_g", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tblack_b = Cvar_Get ("v_tblack_b", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tgrey = Cvar_Get ("v_tgrey", "0.5", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tgrey_r = Cvar_Get ("v_tgrey_r", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tgrey_g = Cvar_Get ("v_tgrey_g", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_tgrey_b = Cvar_Get ("v_tgrey_b", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_twhite = Cvar_Get ("v_twhite", "1", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_twhite_r = Cvar_Get ("v_twhite_r", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_twhite_g = Cvar_Get ("v_twhite_g", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
	v_twhite_b = Cvar_Get ("v_twhite_b", "0", CVAR_ARCHIVE | CVAR_ROM, &TGammaChanged);
}

void
PAL_Init (void)
{
	Uint8	*host_colormap;
	host_basepal = COM_LoadNamedFile ("gfx/palette.lmp", true);
	if (!host_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");

	host_colormap = COM_LoadTempFile ("gfx/colormap.lmp", true);
	if (!host_colormap) {
		Com_DPrintf ("Couldn't load gfx/colormap.lmp");
		num_fb = 32;
	} else {
		num_fb = host_colormap[0x4000];
		Zone_Free (host_colormap);
	}

	GammaChanged (v_grey);
	TGammaChanged (v_tgrey);
}

void
PAL_Shutdown (void)
{
	Zone_Free (host_basepal);
	host_basepal = NULL;
}
