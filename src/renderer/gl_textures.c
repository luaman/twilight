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

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
Uint32 *
GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette,
		qboolean check_empty)
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

	if (!count && check_empty)
		return NULL;
	else
		return trans;
}
#else
Uint32 *
GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette,
		qboolean check_empty)
{
	int i, size, count = 0;
	Uint32	d, t;

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

	for (i = 0; i < size;) {
		d = ((Uint32 *) data)[i >> 2];

		t = palette[d & 0xFF];
		if ((trans[i++] = t) != d_palette_empty)
			count++;

		switch (size - i) {
			default:
			case 3:
				t = palette[(d & 0xFF00) >> 8];
				if ((trans[i++] = t) != d_palette_empty)
					count++;
			case 2:
				t = palette[(d & 0xFF0000) >> 16];
				if ((trans[i++] = t) != d_palette_empty)
					count++;
			case 1:
				t = palette[(d & 0xFF000000) >> 24];
				if ((trans[i++] = t) != d_palette_empty)
					count++;
			case 0:
				break;
		}
	}

	if (!count && check_empty)
		return NULL;
	else
		return trans;
}
#endif

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
	int left, right;
} span_t;

static void
GL_DoSpan(double as, double at, double bs, double bt, span_t *span, int width, int height, int *y1, int *y2)
{
	int ay, by, ix;
	double x, slope;

	ay = ceil(at);
	by = ceil(bt);
	if (ay != by)
	{
		slope = (as - bs) / (at - bt);
		if (ay < by)
		{
			if (*y1 > ay) *y1 = ay;
			if (*y2 < by) *y2 = by;
			x = as;
			if (ay < 0)
			{
				x += slope * ay;
				ay = 0;
			}
			if (by > height - 1)
				by = height - 1;
			if (ay < by)
			{
				for (;ay < by; x += slope, ay++)
				{
					ix = ceil(x);
					span[ay].right = min(ix, width);
				}
			}
		}
		else
		{
			if (*y1 > by)
				*y1 = by;
			if (*y2 < ay)
				*y2 = ay;
			x = bs;
			if (by < 0)
			{
				x += slope * by;
				by = 0;
			}
			if (ay > height - 1)
				ay = height - 1;
			if (by < ay)
			{
				for (;by < ay; x += slope, by++)
				{
					ix = ceil(x);
					span[by].left = min(ix, width);
				}
			}
		}
	}
}

qboolean
GLT_TriangleCheck8 (Uint32 *tex, span_t *span, int width, int height,
		astvert_t texcoords[3], Uint32 color)
{
	Uint32		*line;
	int			x, y, start = 0, end = 0, y1, y2;

	y1 = height;
	y2 = 0;
	GL_DoSpan(texcoords[0].s * width, texcoords[0].t * height, texcoords[1].s * width, texcoords[1].t * height, span, width, height, &y1, &y2);
	GL_DoSpan(texcoords[1].s * width, texcoords[1].t * height, texcoords[2].s * width, texcoords[2].t * height, span, width, height, &y1, &y2);
	GL_DoSpan(texcoords[2].s * width, texcoords[2].t * height, texcoords[0].s * width, texcoords[0].t * height, span, width, height, &y1, &y2);

	for (y = y1, line = tex + y * width; y < y2; y++, line += width) {
		if (span[y].left != span[y].right) {
			start = min(span[y].left, span[y].right);
			end = max(span[y].left, span[y].right);

			for (x = start; x < end; x++)
				if (line[x] != color)
					return true;
		}
	}

	return false;
}

static void
GLT_Skin_IndicesFromSkins (aliashdr_t *amodel, int num_skins,
		skin_sub_t **skins, skin_indices_t *indices)
{
	int			alias_numtris = amodel->numtris;
	mtriangle_t	*alias_tris = amodel->triangles;
	Uint32		*tris;
	int			i, j, numtris;

	tris = Zone_Alloc(tempzone, sizeof(Uint32) * ((alias_numtris / 32) + 1));

	for (i = 0; i < num_skins; i++)
		for (j = 0; j < skins[i]->num_tris; j++)
			tris[skins[i]->tris[j] / 32] |= BIT(skins[i]->tris[j] % 32);

	for (i = numtris = 0; i < alias_numtris; i++)
		if (tris[i/32] & BIT(i%32))
			numtris++;

	if (numtris) {
		indices->num = numtris * 3;
		indices->i = Zone_Alloc(glt_zone, sizeof(int) * numtris * 3);
		for (i = j = 0; i < alias_numtris; i++)
			if (tris[i/32] & BIT(i%32)) {
				indices->i[(j * 3) + 0] = alias_tris[i].vertindex[0];
				indices->i[(j * 3) + 1] = alias_tris[i].vertindex[1];
				indices->i[(j * 3) + 2] = alias_tris[i].vertindex[2];
				j++;
			}
	}

	Zone_Free(tris);
}

static void
GLT_Skin_SubParse (aliashdr_t *amodel, skin_sub_t *skin, Uint8 *in, int width,
		int height, Uint32 *palette, qboolean tri_check, qboolean upload,
		int flags, char *name)
{
	Uint32			*mskin;
	int				i, numtris;
	int				*triangles;
	astvert_t		texcoords[3];
	span_t			*span;

	memset(skin, 0, sizeof(*skin));

	mskin = GLT_8to32_convert(in, width, height, palette, tri_check);
	if (!mskin)
		return;

	triangles = Zone_Alloc(glt_zone, sizeof(int) * amodel->numtris);

	if (tri_check && (width > 1 || height > 1)) {
		span = Zone_Alloc(tempzone, sizeof(span_t) * height);
		for (i = 0, numtris = 0; i < amodel->numtris; i++) {
			texcoords[0] = amodel->tcarray[amodel->triangles[i].vertindex[0]];
			texcoords[1] = amodel->tcarray[amodel->triangles[i].vertindex[1]];
			texcoords[2] = amodel->tcarray[amodel->triangles[i].vertindex[2]];

			if (GLT_TriangleCheck8(mskin, span, width, height, texcoords,
						d_palette_empty)) {
				triangles[numtris] = i;
				numtris++;
			}
		}
		Zone_Free (span);
	} else {
		numtris = amodel->numtris;
		for (i = 0; i < amodel->numtris; i++)
			triangles[i] = i;
	}

	if (numtris) {
		skin->num_tris = numtris;
		skin->tris = Zone_Alloc(glt_zone, sizeof(int) * numtris);
		memcpy(skin->tris, triangles, sizeof(int) * numtris);
		GLT_Skin_IndicesFromSkins (amodel, 1, &skin, &skin->indices);
		if (upload)
			skin->texnum = GL_LoadTexture (name, width, height, (Uint8 *) mskin,
					NULL, TEX_MIPMAP | TEX_ALPHA | flags, 32);
	}

	Zone_Free(triangles);
}

qboolean GLT_Skin_CheckForInvalidTexCoords(astvert_t *texcoords, int numverts, int width, int height)
{
	int i, s, t;
	for (i = 0;i < numverts;i++, texcoords++)
	{
		s = ceil(texcoords->s * width);
		t = ceil(texcoords->t * height);
		if (s < 0 || s < 0 || s > width || t > height)
			return true;
	}
	return false;
}

void
GLT_Skin_Parse (Uint8 *data, skin_t *skin, aliashdr_t *amodel, char *name,
		int width, int height, int frames, float interval)
{
	skin_sub_t	*subs[2];
	Uint8	*iskin;
	int		i;
	size_t	s;

	if (GLT_Skin_CheckForInvalidTexCoords(amodel->tcarray, amodel->numverts, width, height))
		Com_Printf("GLT_Skin_Parse: invalid texcoords detected in model %s\n", name);

	s = width * height;

	skin->frames = frames;
	skin->interval = interval;

	skin->base = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->base_team = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->fb = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->top = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);
	skin->bottom = Zone_Alloc(glt_zone, sizeof(skin_sub_t) * frames);

	skin->base_fb_i = Zone_Alloc(glt_zone, sizeof(skin_indices_t) * frames);
	skin->base_team_fb_i =Zone_Alloc(glt_zone, sizeof(skin_indices_t) * frames);
	skin->top_bottom_i = Zone_Alloc(glt_zone, sizeof(skin_indices_t) * frames);

	iskin = Zone_Alloc(glt_zone, s);

	for (i = 0; i < frames; i++, data += s) {
		memcpy(iskin, data, s);

		GLT_FloodFillSkin8 (iskin, width, height);

		GLT_Skin_SubParse (amodel, &skin->base[i], iskin, width, height,
				d_palette_base, false, true, TEX_FORCE, va("%s_base", name));

		GLT_Skin_SubParse (amodel, &skin->base_team[i], iskin, width, height,
				d_palette_base_team, false, true, TEX_FORCE,
				va("%s_base_team", name));

		GLT_Skin_SubParse (amodel, &skin->fb[i], iskin, width, height,
				d_palette_fb, true, true, 0, va("%s_fb", name));

		GLT_Skin_SubParse (amodel, &skin->top[i], iskin, width, height,
				d_palette_top, true, true, 0, va("%s_top", name));

		GLT_Skin_SubParse (amodel, &skin->bottom[i], iskin, width, height,
				d_palette_bottom, true, true, 0, va("%s_bottom", name));

		subs[0] = &skin->base[i]; subs[1] = &skin->fb[i];
		GLT_Skin_IndicesFromSkins (amodel, 2, subs, &skin->base_fb_i[i]);

		subs[0] = &skin->base_team[i]; subs[1] = &skin->fb[i];
		GLT_Skin_IndicesFromSkins (amodel, 2, subs, &skin->base_team_fb_i[i]);

		subs[0] = &skin->top[i]; subs[1] = &skin->bottom[i];
		GLT_Skin_IndicesFromSkins (amodel, 2, subs, &skin->top_bottom_i[i]);
	}

	Zone_Free(iskin);
}

void
GLT_Delete_Sub_Skin (skin_sub_t *sub)
{
	GL_DeleteTexture(sub->texnum);
	if (sub->indices.i)
		Zone_Free(sub->indices.i);
	Zone_Free(sub);
}

void
GLT_Delete_Indices (skin_indices_t *i)
{
	if (i->i)
		Zone_Free(i->i);
	Zone_Free(i);
}

void
GLT_Delete_Skin (skin_t *skin)
{
	int i;

	for (i = 0; i < skin->frames; i++) {
		GLT_Delete_Sub_Skin(&skin->base[i]);
		GLT_Delete_Sub_Skin(&skin->base_team[i]);
		GLT_Delete_Sub_Skin(&skin->fb[i]);
		GLT_Delete_Sub_Skin(&skin->top[i]);
		GLT_Delete_Sub_Skin(&skin->bottom[i]);

		GLT_Delete_Indices(&skin->base_fb_i[i]);
		GLT_Delete_Indices(&skin->base_team_fb_i[i]);
		GLT_Delete_Indices(&skin->top_bottom_i[i]);
	}
}


void
GLT_Init ()
{
	glt_zone = Zone_AllocZone("GL textures");
}
