/*
	$RCSfile$

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

#include "twiconfig.h"

#include <math.h>

#include "dyngl.h"
#include "gl_textures.h"
#include "gl_info.h"
#include "mathlib.h"
#include "qtypes.h"
#include "strlib.h"
#include "model.h"
#include "draw.h"

memzone_t	*glt_zone;

static Uint32 *trans;
static int trans_size;

Uint32 *
GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette)
{
	int i, size, count = 0;

	if (!palette)
		palette = d_palette_raw;

	size = width * height;
	if (size > trans_size)
	{
		if (trans)
			Zone_Free(trans);
		trans = Zone_Alloc(glt_zone, size * sizeof(Uint32));
		trans_size = size;
	}

	for (i = 0; i < size; i++)
		if ((trans[i] = palette[data[i]]) != d_palette_empty)
			count++;

	if (count)
		return trans;
	else
		return NULL;
}

/*
=================
GLT_FloodFillSkin8

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct {
	short	x, y;
} floodfill_t;

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
			if (d_palette_raw[i] == (255 << 0))	// alpha 1.0
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
GLT_TriangleCheck8 (Uint32 *tex, int width, int height,
		astvert_t texcoords[3], Uint32 color)
{
	astvert_t	v[3];
	span_t		*span;
	Uint32		*line;
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

static void
GLT_Skin_SubParse (aliashdr_t *amodel, skin_sub_t *skin, Uint8 *in, int width,
		int height, Uint32 *palette, qboolean tri_check, qboolean upload,
		char *name)
{
	Uint32			*mskin;
	int				i, numtris;
	int				*triangles;
	astvert_t		texcoords[3];

	skin->texnum = skin->num_indices = 0;
	skin->indices = NULL;

	mskin = GLT_8to32_convert(in, width, height, palette);
	if (!mskin)
		return;

	triangles = Zone_Alloc(glt_zone, sizeof(int) * amodel->numtris);

	if (tri_check) {
		for (i = 0, numtris = 0; i < amodel->numtris; i++) {
			texcoords[0] = amodel->tcarray[amodel->triangles[i].vertindex[0]];
			texcoords[1] = amodel->tcarray[amodel->triangles[i].vertindex[1]];
			texcoords[2] = amodel->tcarray[amodel->triangles[i].vertindex[2]];

			if (GLT_TriangleCheck8(mskin, width, height, texcoords,
						d_palette_empty)) {
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
		if (upload)
			skin->texnum = GL_LoadTexture (name, width, height, (Uint8 *) mskin,
					NULL, TEX_MIPMAP | TEX_ALPHA, 32);
	}

	Zone_Free(triangles);
}

void
GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name,
		int width, int height, int frames, float interval)
{
	Uint8	*iskin;
	int		i;
	size_t	s;

	s = width * height;

	skin->frames = frames;
	skin->interval = interval;

	skin->base = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->base_team = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->top_bottom = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->fb = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->top = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->bottom = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);

	iskin = Zone_Alloc(glt_zone, s);

	for (i = 0; i < frames; i++, data += s) {
		memcpy(iskin, data, s);

#define TOP_MASK	(BIT(1))
#define BOTTOM_MASK	(BIT(6))
#define TEAM_MASK	(TOP_MASK | BOTTOM_MASK)
#define FB_MASK		(BIT(14) | BIT(15))
#define BASE_MASK	(0xFFFF - (TOP_MASK | BOTTOM_MASK | FB_MASK))

		GLT_FloodFillSkin8 (iskin, width, height);

		GLT_Skin_SubParse (amodel, &skin->base[i], iskin, width, height,
				d_palette_base, false, true, va("%s_base", name));

		GLT_Skin_SubParse (amodel, &skin->base_team[i], iskin, width, height,
				d_palette_base_team, false, true, va("%s_base_team", name));

		GLT_Skin_SubParse (amodel, &skin->top_bottom[i], iskin, width, height,
				d_palette_top_bottom, true, false, va("%s_top_bottom", name));

		GLT_Skin_SubParse (amodel, &skin->fb[i], iskin, width, height,
				d_palette_fb, true, true, va("%s_fb", name));

		GLT_Skin_SubParse (amodel, &skin->top[i], iskin, width, height,
				d_palette_top, true, true, va("%s_top", name));

		GLT_Skin_SubParse (amodel, &skin->bottom[i], iskin, width, height,
				d_palette_bottom, true, true, va("%s_bottom", name));
	}

	Zone_Free(iskin);
}

void
GLT_Delete_Sub_Skin (skin_sub_t *sub)
{
	GL_DeleteTexture(sub->texnum);
	if (sub->indices)
		Zone_Free(sub->indices);
	Zone_Free(sub);
}

void
GLT_Delete_Skin (skin_t *skin)
{
	int i;

	for (i = 0; i < skin->frames; i++) {
		GLT_Delete_Sub_Skin(&skin->base[i]);
		GLT_Delete_Sub_Skin(&skin->base_team[i]);
		GLT_Delete_Sub_Skin(&skin->top_bottom[i]);
		GLT_Delete_Sub_Skin(&skin->fb[i]);
		GLT_Delete_Sub_Skin(&skin->top[i]);
		GLT_Delete_Sub_Skin(&skin->bottom[i]);
	}
}


void
GLT_Init ()
{
	glt_zone = Zone_AllocZone("GL textures");
}
