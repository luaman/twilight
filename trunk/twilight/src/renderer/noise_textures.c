/*
	noise_textures.c

	Dynamic texture generation.

	Copyright (C) 2000-2001   Zephaniah E. Hull.

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include "SDL_types.h"
#include "qtypes.h"
#include "opengl.h"

extern int texture_extension_number;

extern void FractalNoise (Uint8 *noise, int size, int startgrid);

static void TNT_InitDotParticleTexture (void);
static void TNT_InitSmokeParticleTexture (void);
static void TNT_InitSmokeRingParticleTexture (void);

int	part_tex_dot;
int	part_tex_spark;
int	part_tex_smoke;
int	part_tex_smoke_ring;


void
TNT_Init (void)
{
	TNT_InitDotParticleTexture ();
	TNT_InitSmokeParticleTexture ();
	TNT_InitSmokeRingParticleTexture ();
}

static void
TNT_InitDotParticleTexture (void)
{
	Uint8	data[16][16][2];
	int		x, y, dx2, dy, d;

	for (x = 0; x < 16; x++) {
		dx2 = x - 8;
		dx2 *= dx2;
		for (y = 0; y < 16; y++) {
			dy = y - 8;
			d = 255 - 4 * (dx2 + (dy * dy));
			if (d<=0) {
				d = 0;
				data[y][x][0] = 0;
			} else
				data[y][x][0] = 255;

			data[y][x][1] = (Uint8) d;
		}
	}

	part_tex_dot = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, part_tex_dot);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D (GL_TEXTURE_2D, 0, 2, 16, 16, 0, GL_LUMINANCE_ALPHA,
		      GL_UNSIGNED_BYTE, data);
}

static void
TNT_InitSmokeParticleTexture (void)
{
	Uint8	d;
	Uint8	data[32][32][2], noise1[32][32], noise2[32][32];
	int		dx, dy2, x, y, c;

	FractalNoise (&noise1[0][0], 32, 4);
	FractalNoise (&noise2[0][0], 32, 8);
	for (y = 0; y < 32; y++)
	{
		dy2 = y - 16;
		dy2 *= dy2;
		for (x = 0; x < 32; x++) {
			dx = x - 16;
			c = 255 - (dx*dx + dy2);
			if (c < 1)
				c = 0;
			d = (noise1[y][x] + noise2[y][x]) / 2;
			if (d > 0) {
				data[y][x][0] = 255;
				data[y][x][1] = (d * c)/255;
			} else {
				data[y][x][0] = 255;
				data[y][x][1] = 0;
			}
		}
	}
	part_tex_smoke = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, part_tex_smoke);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D (GL_TEXTURE_2D, 0, 2, 32, 32, 0, GL_LUMINANCE_ALPHA,
			GL_UNSIGNED_BYTE, data);
}

static void
TNT_InitSmokeRingParticleTexture (void)
{
	Uint8	d;
	Uint8	data[32][32][2], noise1[32][32], noise2[32][32];
	int		dx, dy, c, c2, x, y, b;

	FractalNoise (&noise1[0][0], 32, 4);
	FractalNoise (&noise2[0][0], 32, 8);
	for (y = 0; y < 32; y++)
	{
		dy = y - 16;
		dy *= dy;
		for (x = 0; x < 32; x++) {
			dx = x - 16;
			dx *= dx;
			c = 255 - (dx + dy);
			c2 = (dx + dy);
			if (c < 1) c = 0;
			if (c2 < 1) c2 = 0;
			b = (c * c2) * 512 / (255*255);
			if (b < 1) b = 0;
			d = (noise1[y][x] + noise2[y][x]) / 2;
			if (d > 0) {
				data[y][x][0] = 255;
				data[y][x][1] = (d * b)/255;
			} else {
				data[y][x][0] = 255;
				data[y][x][1] = 0;
			}
		}
	}
	part_tex_smoke_ring = texture_extension_number++;
	qglBindTexture (GL_TEXTURE_2D, part_tex_smoke_ring);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D (GL_TEXTURE_2D, 0, 2, 32, 32, 0, GL_LUMINANCE_ALPHA,
			GL_UNSIGNED_BYTE, data);
}
