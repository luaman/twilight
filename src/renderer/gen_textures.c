/*
	Dynamic texture generation.

	Copyright (C) 2000-2001   Zephaniah E. Hull.
	Copyright (C) 2000-2001   Ragnvald "Despair" Maartmann-Moe IV

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
#include "SDL_types.h"

#include "dyngl.h"
#include "qtypes.h"
#include "mathlib.h"
#include "zone.h"
#include "gen_textures.h"
#include "gl_textures.h"

extern void FractalNoise (Uint8 *noise, int size, int startgrid);

static void GTF_Init (void);
static int TNT_InitLightningBeamTexture (void);

const int GTF_smoke[8] = {0, 1, 2, 3, 4, 5, 6, 7};
const int GTF_rainsplash[16] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
const int GTF_dot = 24;
const int GTF_raindrop = 25;
const int GTF_bubble = 26;
const int GTF_lightning_beam = 27;
const int GTF_blooddecal[8] = {32, 33, 34, 35, 36, 37, 38, 39};

int GTF_texnum;
GTF_texture_t GTF_texture[MAX_GT_FONT_TEXTURES];

void
TNT_Init (void)
{
	GTF_Init ();
}

static int
TNT_InitLightningBeamTexture (void)
{
	Uint8	*noise1, *noise2, *data;
	int		x, y, tmp;
	float	r, g, b, intensity, fx, width, center;

	data = Zone_Alloc(tempzone, 32 * 512 * 4);
	noise1 = Zone_Alloc(tempzone, 512 * 512);
	noise2 = Zone_Alloc(tempzone, 512 * 512);
	FractalNoise(noise1, 512, 8);
	FractalNoise(noise2, 512, 16);

	for (y = 0; y < 512; y++) {
		width = noise1[y * 512] * (1.0f / 256.0f) * sq(3.0f);
		center = (noise1[y * 512 + 64] / 256.0f) * (32.0f - (width + 1.0f) * 2.0f) + (width + 1.0f);
		for (x = 0; x < 32; x++) {
			fx = (x - center) / width;
			intensity = (1.0f - sq(fx)) * (noise2[y*512+x] * (1.0f / 256.0f) * 0.33f + 0.66f);
			intensity = bound(0, intensity, 1);
			r = intensity * 2.0f - 1.0f;
			g = intensity * 3.0f - 1.0f;
			b = intensity * 3.0f;
			data[(y * 32 + x) * 4 + 0] = (Uint8)(bound(0, r, 1) * 255.0f);
			data[(y * 32 + x) * 4 + 1] = (Uint8)(bound(0, g, 1) * 255.0f);
			data[(y * 32 + x) * 4 + 2] = (Uint8)(bound(0, b, 1) * 255.0f);
			data[(y * 32 + x) * 4 + 3] = (Uint8)255;
		}
	}

	tmp = GLT_Load_Raw ("GTF_lightning", 32, 512, data, NULL, TEX_ALPHA, 32);

	Zone_Free(data);
	Zone_Free(noise1);
	Zone_Free(noise2);
	return tmp;
}

static void
setuptex(int texnum, Uint8 *data, Uint8 *font_texture_data)
{
	int basex, basey, y;
	basex = ((texnum >> 0) & 7) * 32;
	basey = ((texnum >> 3) & 7) * 32;
	GTF_texture[texnum].s1 = (basex + 1) / 256.0f;
	GTF_texture[texnum].t1 = (basey + 1) / 256.0f;
	GTF_texture[texnum].s2 = (basex + 31) / 256.0f;
	GTF_texture[texnum].t2 = (basey + 31) / 256.0f;
	for (y = 0;y < 32;y++)
		memcpy(font_texture_data + ((basey + y) * 256 + basex) * 4, data + y * 32 * 4, 32 * 4);
}

static Uint8 
shadebubble(float dx, float dy, vec3_t light)
{
	float dz, f, dot;
	vec3_t normal;
	dz = 1 - (dx*dx+dy*dy);
	if (dz > 0) // it does hit the sphere
	{
		f = 0;
		// back side
		normal[0] = dx;normal[1] = dy;normal[2] = dz;
		VectorNormalize(normal);
		dot = DotProduct(normal, light);
		if (dot > 0.5) // interior reflection
			f += ((dot *  2) - 1);
		else if (dot < -0.5) // exterior reflection
			f += ((dot * -2) - 1);
		// front side
		normal[0] = dx;normal[1] = dy;normal[2] = -dz;
		VectorNormalize(normal);
		dot = DotProduct(normal, light);
		if (dot > 0.5) // interior reflection
			f += ((dot *  2) - 1);
		else if (dot < -0.5) // exterior reflection
			f += ((dot * -2) - 1);
		f *= 128;
		f += 16; // just to give it a haze so you can see the outline
		f = bound(0, f, 255);
		return (Uint8) f;
	}
	else
		return 0;
}

static void
GTF_Init (void)
{
	int x, y, d, i, j, k, m;
	float cx, cy, dx, dy, radius, f, f2;
	Uint8 data[32][32][4], noise1[64][64], noise2[64][64];
	vec3_t light;
	Uint8 font_texture_data[256*256*4];

	memset(font_texture_data, 255, sizeof(font_texture_data));

	// smoke/blood
	for (i = 0;i < 8;i++)
	{
		do
		{
			FractalNoise(&noise1[0][0], 64, 4);
			FractalNoise(&noise2[0][0], 64, 8);
			m = 0;
			for (y = 0;y < 32;y++)
			{
				dy = y - 16;
				for (x = 0;x < 32;x++)
				{
					data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
					dx = x - 16;
					d = (noise2[y][x] - 128) * 3 + 192;
					if (d > 0)
						d = (d * (256 - (int) (dx*dx+dy*dy))) >> 8;
					d = (d * noise1[y][x]) >> 7;
					d = bound(0, d, 255);
					data[y][x][3] = (Uint8) d;
					if (m < d)
						m = d;
				}
			}
		}
		while (m < 224);

		setuptex(GTF_smoke[i], &data[0][0][0], font_texture_data);
	}

	// rain splash
	for (i = 0;i < 16;i++)
	{
		radius = i * 3.0f / 16.0f;
		f2 = 255.0f * ((15.0f - i) / 15.0f);
		for (y = 0;y < 32;y++)
		{
			dy = (y - 16) * 0.25f;
			for (x = 0;x < 32;x++)
			{
				dx = (x - 16) * 0.25f;
				data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
				f = (1.0 - fabs(radius - sqrt(dx*dx+dy*dy))) * f2;
				f = bound(0.0f, f, 255.0f);
				data[y][x][3] = (int) f;
			}
		}
		setuptex(GTF_rainsplash[i], &data[0][0][0], font_texture_data);
	}

	// normal dot
	for (y = 0;y < 32;y++)
	{
		dy = y - 16;
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			dx = x - 16;
			d = (256 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y][x][3] = (Uint8) d;
		}
	}
	setuptex(GTF_dot, &data[0][0][0], font_texture_data);

	// rain
	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 8.0), y < 24 ? (y - 24) * (1.0 / 24.0) : (y - 24) * (1.0 / 8.0), light);
		}
	}
	setuptex(GTF_raindrop, &data[0][0][0], font_texture_data);

	// bubble
	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 16.0), (y - 16) * (1.0 / 16.0), light);
		}
	}
	setuptex(GTF_bubble, &data[0][0][0], font_texture_data);

	// smoke/blood
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		for (j = 1;j < 8;j++)
		{
			for (k = 0;k < 3;k++)
			{
				cx = lhrandom(j + 1, 30 - j);
				cy = lhrandom(j + 1, 30 - j);
				for (y = 0;y < 32;y++)
				{
					for (x = 0;x < 32;x++)
					{
						dx = (x - cx);
						dy = (y - cy);
						f = 1.0f - sqrt(dx * dx + dy * dy) / j;
						if (f > 0)
						{
							data[y][x][0] = data[y][x][0] + f * 0.5 * ( 160 - data[y][x][0]);
							data[y][x][1] = data[y][x][1] + f * 0.5 * ( 32 - data[y][x][1]);
							data[y][x][2] = data[y][x][2] + f * 0.5 * ( 32 - data[y][x][2]);
						}
					}
				}
			}
		}
		// use inverted colors so we can scale them later using glColor and use
		// an inverse blend
		for (y = 0;y < 32;y++)
		{
			for (x = 0;x < 32;x++)
			{
				data[y][x][0] = 255 - data[y][x][0];
				data[y][x][1] = 255 - data[y][x][1];
				data[y][x][2] = 255 - data[y][x][2];
			}
		}
		setuptex(GTF_blooddecal[i], &data[0][0][0], font_texture_data);
	}

	GTF_texnum = GLT_Load_Raw ("GTF_main", 256, 256, font_texture_data, NULL, TEX_ALPHA, 32);
	for (i = 0;i < MAX_GT_FONT_TEXTURES;i++)
		GTF_texture[i].texture = GTF_texnum;

	i = GTF_lightning_beam;
	GTF_texture[i].texture = TNT_InitLightningBeamTexture ();
	GTF_texture[i].s1 = 0;
	GTF_texture[i].t1 = 0;
	GTF_texture[i].s2 = 1;
	GTF_texture[i].t2 = 1;
}
