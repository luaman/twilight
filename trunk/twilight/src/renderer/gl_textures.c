/*
	$RCSfile$
	OpenGL Texture management.

	Copyright (C) 2002  Zephaniah E. Hull.

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

#include <math.h>

#include "gl_textures.h"
#include "mathlib.h"
#include "opengl.h"
#include "qtypes.h"
#include "strlib.h"
#include "model.h"
#include "draw.h"

memzone_t	*glt_zone;

/*
=================
GLT_FloodFillSkin8

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct {
	short	x, y;
} floodfill_t;

extern unsigned d_8to32table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void
GLT_FloodFillSkin8 (Uint8 * skin, int skinwidth, int skinheight)
{
	Uint8		fillcolor = *skin;		// assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int			inpt = 0, outpt = 0;
	int			filledcolor = -1;
	int			i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to32table[i] == (255 << 0))	// alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}
	// can't fill to filled color or to transparent color (used as visited
	// marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		// printf( "not filling skin from %d to %d\n", fillcolor, filledcolor
		// );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int         x = fifo[outpt].x, y = fifo[outpt].y;
		int         fdc = filledcolor;
		Uint8      *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP (-1, -1, 0);
		if (x < skinwidth - 1)
			FLOODFILL_STEP (1, 1, 0);
		if (y > 0)
			FLOODFILL_STEP (-skinwidth, 0, -1);
		if (y < skinheight - 1)
			FLOODFILL_STEP (skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}


typedef struct {
	int	lines;
	int	x0, x1;
} span_t;

static inline void
GL_DoSpan(astvert_t *a, astvert_t *b, span_t *span)
{
	int		y;
	double	x, slope;

	if ((int) a->t != (int) b->t) {
		slope = (a->s - b->s) / (a->t - b->t);
		if (a->t < b->t) {
			for (x = a->s, y = ceil(a->t); y < b->t; x += slope, y++)
				if (!span[y].lines++)
					span[y].x0 = x;
				else
					span[y].x1 = x;
		} else {
			for (x = b->s, y = ceil(b->t); y < a->t; x += slope, y++)
				if (!span[y].lines++)
					span[y].x0 = x;
				else
					span[y].x1 = x;
		}
	}
}

qboolean
GLT_TriangleCheck8 (Uint8 *tex, int width, int height,
		astvert_t texcoords[3], Uint8 color)
{
	astvert_t	v[3];
	span_t		*span;
	Uint8		*line;
	int			x, y, start = 0, end = 0;

	span = Zone_Alloc(tempzone, sizeof(span_t) * height);

	v[0].s = texcoords[0].s * width;
	v[0].t = (texcoords[0].t) * height;
	v[1].s = texcoords[1].s * width;
	v[1].t = (texcoords[1].t) * height;
	v[2].s = texcoords[2].s * width;
	v[2].t = (texcoords[2].t) * height;

	GL_DoSpan(&v[0], &v[1], span);
	GL_DoSpan(&v[1], &v[2], span);
	GL_DoSpan(&v[2], &v[0], span);

	line = tex;
	for (y = 0; y < height; y++, line += width) {
		if (span[y].lines) {
			start = min(span[y].x0, span[y].x1);
			end = max(span[y].x0, span[y].x1);

			for (x = start; x < end; x++)
				if (line[x] != color) {
					Zone_Free (span);
					return true;
				}
		}
	}

	Zone_Free (span);
	return false;
}


int
GLT_Mangle8 (Uint8 *in, Uint8 *out, int width, int height, short mask,
		Uint8 to, qboolean bleach)
{
	int			i, pixels, mangled = 0;
	qboolean	rows[16];

	for (i = 0; i < 16; i++) {
		rows[i] = !!(mask & (1 << i));
	}

	pixels = width * height;
	for (i = 0; i < pixels; i++, in++, out++) {
		if (rows[*in >> 4]) {
			mangled++;
			if (bleach) {
				if (*in >= 128 && *in < 224)	// Reversed.
					*out = (*in & 15) ^ 15;
				else
					*out = *in & 15;
			} else
				*out = *in;
		} else
			*out = to;
	}

	return mangled;
}

static void
GLT_Skin_SubParse (aliashdr_t *amodel, skin_sub_t *skin, Uint8 *in, int width,
		int height, short bits, Uint8 color, qboolean bleach,
		qboolean tri_check, char *name)
{
	Uint8		*mskin;
	int			i, numtris;
	int			*triangles;
	astvert_t	texcoords[3];

	skin->texnum = skin->num_indices = 0;
	skin->indices = NULL;

	if (bits) {
		mskin = Zone_Alloc(glt_zone, width * height);
		if (!GLT_Mangle8(in, mskin, width, height, bits, color, bleach)) {
			Zone_Free(mskin);
			return;
		}
	} else
		mskin = in;


	triangles = Zone_Alloc(glt_zone, sizeof(int) * amodel->numtris);

	if (tri_check) {
		for (i = 0, numtris = 0; i < amodel->numtris; i++) {
			texcoords[0] = amodel->tcarray[amodel->triangles[i].vertindex[0]];
			texcoords[1] = amodel->tcarray[amodel->triangles[i].vertindex[1]];
			texcoords[2] = amodel->tcarray[amodel->triangles[i].vertindex[2]];

			if (GLT_TriangleCheck8(mskin, width, height, texcoords, color)) {
				triangles[numtris] = i;
				numtris++;
			}
		}
	} else {
		numtris = amodel->numtris;
		for (i = 0; i < amodel->numtris; i++)
			triangles[i] = i;
	}

	if (numtris) {
		mtriangle_t	*atris = amodel->triangles;

		skin->num_indices = numtris * 3;
		skin->indices = Zone_Alloc(glt_zone, sizeof(int) * numtris * 3);
		for (i = 0; i < numtris; i++) {
			skin->indices[(i * 3) + 0] = atris[triangles[i]].vertindex[0];
			skin->indices[(i * 3) + 1] = atris[triangles[i]].vertindex[1];
			skin->indices[(i * 3) + 2] = atris[triangles[i]].vertindex[2];
		}
		skin->texnum = GL_LoadTexture (name, width, height, mskin,
				TEX_MIPMAP | TEX_ALPHA, 8);
	}

	if (bits)
		Zone_Free(mskin);

	Zone_Free(triangles);
}

void
GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name,
		int width, int height, int frames, float interval)
{
	Uint8	*iskin;
	int		s, i;

	s = width * height;

	skin->frames = frames;
	skin->interval = interval;

	skin->raw = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->base = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->normal = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->fb = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->top = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->bottom = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);

	iskin = Zone_Alloc(glt_zone, s);

	for (i = 0; i < frames; i++, data += s) {
		memcpy(iskin, data, s);

		GLT_FloodFillSkin8 (iskin, width, height);

		GLT_Skin_SubParse (amodel, &skin->raw[i], iskin, width, height, 0,
				0, false, false, va("%s_raw", name));

		GLT_Skin_SubParse (amodel, &skin->base[i], iskin, width, height,
				0xFFFF - (BIT(1) | BIT(6) | BIT(14) | BIT(15)),
				0, false, false, va("%s_base", name));

		GLT_Skin_SubParse (amodel, &skin->normal[i], iskin, width, height,
				0xFFFF - (BIT(14) | BIT(15)),
				0, false, false, va("%s_normal", name));

		GLT_Skin_SubParse (amodel, &skin->fb[i], iskin, width, height,
				BIT(14) | BIT(15), 0, false, true, va("%s_fb", name));

		GLT_Skin_SubParse (amodel, &skin->top[i], iskin, width, height,
				BIT(1), 0, true, true, va("%s_top", name));

		GLT_Skin_SubParse (amodel, &skin->bottom[i], iskin, width, height,
				BIT(6), 0, true, true, va("%s_bottom", name));
	}

	Zone_Free(iskin);
}

void
GLT_Init ()
{
	glt_zone = Zone_AllocZone("GL textures");
}
