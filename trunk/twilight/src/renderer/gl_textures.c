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

#include "cpu.h"
#include "crc.h"
#include "draw.h"
#include "dyngl.h"
#include "gl_info.h"
#include "gl_textures.h"
#include "image.h"
#include "mathlib.h"
#include "model.h"
#include "qtypes.h"
#include "strlib.h"
#include "sys.h"
#include "wad.h"

static Uint32 * GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette, qboolean check_empty);
static void GLT_FloodFill8 (Uint8 * skin, int skinwidth, int skinheight);
static qboolean GLT_Skin_CheckForInvalidTexCoords(astvert_t *texcoords, int numverts, int width, int height);

/*
 * Stuff for tracking of loaded textures.
 */
typedef struct gltexture_s
{
	GLuint				texnum;
	char				identifier[128];
	GLuint				width, height;
	qboolean			mipmap;
	unsigned short		crc;
	int					count;
	struct gltexture_s	*next;
}
gltexture_t;

gltexture_t *gltextures;

/*
 * Memory zones.
 */
static memzone_t *glt_skin_zone;
memzone_t *glt_zone;

/*
 * This stuff is entirely specific to the selection of gl filter and 
 * texture modes.
 */
int		glt_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		glt_filter_mag = GL_LINEAR;
int		glt_solid_format = 3;
int		glt_alpha_format = 4;

typedef struct {
	char	*name;
	int		 minimize, magnify;
} glmode_t;

static glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

static void Set_TextureMode_f (struct cvar_s *var);
cvar_t *gl_texturemode;
cvar_t *r_lerpimages;
cvar_t *r_colormiplevels;
cvar_t *gl_picmip;
cvar_t *gl_max_size;

static void
GLT_verify_pow2 (cvar_t *cvar)
{
	int out;
	out = near_pow2_low(cvar->ivalue);
	if (out != cvar->ivalue)
		Cvar_Set(cvar, va("%d", out));
}

/*
 * The Init functions, by now all the types and variables should be declared.
 */

void
GLT_Init_Cvars ()
{
	int max_tex_size = 0;

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	if (!max_tex_size)
		max_tex_size = 1024;

	gl_max_size = Cvar_Get ("gl_max_size", va("%d", max_tex_size), CVAR_NONE,
			GLT_verify_pow2);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
			CVAR_ARCHIVE, Set_TextureMode_f);
	r_lerpimages = Cvar_Get ("r_lerpimages", "1", CVAR_ARCHIVE, NULL);
	r_colormiplevels = Cvar_Get ("r_colormiplevels", "0", CVAR_NONE, NULL);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL);
}

void
GLT_Init ()
{
	glt_zone = Zone_AllocZone("GL textures");
	glt_skin_zone = Zone_AllocZone("GL textures (skins)");
}

/*
 * Control the texture filtering modes.
 */
static void
Set_TextureMode_f (struct cvar_s *var)
{
	int         i;
	gltexture_t *glt;

	for (i = 0; i < 6; i++)
	{
		if (!strcasecmp (modes[i].name, var->svalue))
			break;
	}
	if (i == 6)
	{
		Cvar_Set (gl_texturemode,"GL_LINEAR_MIPMAP_NEAREST");
		Com_Printf ("Bad GL_TEXTUREMODE, valid modes are:\n");
		Com_Printf ("GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST\n");
		Com_Printf ("GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST (default), \n");
		Com_Printf ("GL_LINEAR_MIPMAP_LINEAR (trilinear)\n\n");
		return;
	}

	glt_filter_min = modes[i].minimize;
	glt_filter_mag = modes[i].magnify;

	/* change all the existing mipmap texture objects */
	for (glt = gltextures; glt != NULL; glt = glt->next)
	{
		if (glt->mipmap)
		{
			qglBindTexture (GL_TEXTURE_2D, glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					glt_filter_min);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					glt_filter_mag);
		}
	}
}

/*
 * =========================================================================
 *                          Start of the skin code.
 * =========================================================================
 */

typedef struct {
	int left, right;
} span_t;

static void
GLT_DoSpan(double as, double at, double bs, double bt, span_t *span,
		int width, int height, int *y1, int *y2)
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

static qboolean
GLT_TriangleCheck8 (Uint32 *tex, span_t *span, int width, int height,
		astvert_t texcoords[3], Uint32 color)
{
	Uint32		*line;
	int			x, y, start = 0, end = 0, y1, y2;

	y1 = height;
	y2 = 0;
	GLT_DoSpan(texcoords[0].s * width, texcoords[0].t * height, texcoords[1].s * width, texcoords[1].t * height, span, width, height, &y1, &y2);
	GLT_DoSpan(texcoords[1].s * width, texcoords[1].t * height, texcoords[2].s * width, texcoords[2].t * height, span, width, height, &y1, &y2);
	GLT_DoSpan(texcoords[2].s * width, texcoords[2].t * height, texcoords[0].s * width, texcoords[0].t * height, span, width, height, &y1, &y2);

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
		indices->i = Zone_Alloc(glt_skin_zone, sizeof(int) * numtris * 3);
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

	triangles = Zone_Alloc(glt_skin_zone, sizeof(int) * amodel->numtris);

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
		skin->tris = Zone_Alloc(glt_skin_zone, sizeof(int) * numtris);
		memcpy(skin->tris, triangles, sizeof(int) * numtris);
		GLT_Skin_IndicesFromSkins (amodel, 1, &skin, &skin->indices);
		if (upload)
			skin->texnum = GLT_Load_Raw (name, width, height, (Uint8 *) mskin,
					NULL, TEX_MIPMAP | TEX_ALPHA | flags, 32);
	}

	Zone_Free(triangles);
}

static qboolean
GLT_Skin_CheckForInvalidTexCoords(astvert_t *texcoords, int numverts,
		int width, int height)
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

	skin->base = Zone_Alloc(glt_skin_zone, sizeof(skin_sub_t) * frames);
	skin->base_team = Zone_Alloc(glt_skin_zone, sizeof(skin_sub_t) * frames);
	skin->fb = Zone_Alloc(glt_skin_zone, sizeof(skin_sub_t) * frames);
	skin->top = Zone_Alloc(glt_skin_zone, sizeof(skin_sub_t) * frames);
	skin->bottom = Zone_Alloc(glt_skin_zone, sizeof(skin_sub_t) * frames);

	skin->base_fb_i = Zone_Alloc(glt_skin_zone, sizeof(skin_indices_t) * frames);
	skin->base_team_fb_i =Zone_Alloc(glt_skin_zone, sizeof(skin_indices_t) * frames);
	skin->top_bottom_i = Zone_Alloc(glt_skin_zone, sizeof(skin_indices_t) * frames);

	iskin = Zone_Alloc(glt_skin_zone, s);

	for (i = 0; i < frames; i++, data += s) {
		memcpy(iskin, data, s);

		GLT_FloodFill8 (iskin, width, height);

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

static void
GLT_Delete_Sub_Skin (skin_sub_t *sub)
{
	GLT_Delete(sub->texnum);
	if (sub->indices.i)
		Zone_Free(sub->indices.i);
	Zone_Free(sub);
}

static void
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
/*
 * =========================================================================
 *                           End of the skin code.
 * =========================================================================
 */

/*
 * =========================================================================
 *                    Start of the texture mangling code.
 * =========================================================================
 */

static Uint32 *
GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette,
		qboolean check_empty)
{
	static Uint32 *trans;
	static int trans_size;
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
		d = LittleLong(((Uint32 *) data)[i >> 2]);

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

/*
=================
GLT_FloodFill8

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

static void
GLT_FloodFill8 (Uint8 * skin, int skinwidth, int skinheight)
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

/*
================
R_ResampleTextureLerpLine
================
*/
static void
R_ResampleTextureLerpLineBase (Uint8 *in, Uint8 *out, int inwidth, int outwidth)
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
static void
R_ResampleTextureBase (void *indata, int inwidth, int inheight, void *outdata,
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
		R_ResampleTextureLerpLineBase (inrow, row1, inwidth, outwidth);
		R_ResampleTextureLerpLineBase (inrow + inwidth*4, row2, inwidth,
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
						R_ResampleTextureLerpLineBase (inrow, row1, inwidth,
								outwidth);
					R_ResampleTextureLerpLineBase (inrow + inwidth*4, row2,
							inwidth, outwidth);
					oldy = yi;
				}
				for (j = 0; j < outwidth; j++)  
				{
					Uint32	r1, r2, out_32;
					r1 = *((Uint32 *) row1);
					r2 = *((Uint32 *) row2);
					out_32 = r1;

					out_32  = (((((((r2 & 0xFF000000) - (r1 & 0xFF000000)) >> 16) * lerp)) + (r1 & 0xFF000000)) & 0xFF000000);
					out_32 |= (((((((r2 & 0x00FF0000) - (r1 & 0x00FF0000)) >> 16) * lerp)) + (r1 & 0x00FF0000)) & 0x00FF0000);
					out_32 |= (((((((r2 & 0x0000FF00) - (r1 & 0x0000FF00))) * lerp) >> 16) + (r1 & 0x0000FF00)) & 0x0000FF00);
					out_32 |= (((((((r2 & 0x000000FF) - (r1 & 0x000000FF))) * lerp) >> 16) + (r1 & 0x000000FF)) & 0x000000FF);

					*((Uint32 *) out) = out_32;
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
						R_ResampleTextureLerpLineBase (inrow, row1, inwidth,
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


#ifdef HAVE_MMX
static void
R_ResampleTextureLerpLineMMX (Uint8 *in, Uint8 *out, int inwidth, int outwidth,
		int fstep_w)
{
	int		j, xi, oldx = 0, f, endx;
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep_w)
	{
		xi = (int) f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			Uint16	lerp = (f & 0xFFFF) >> 8;
			Uint16	lerp1_4[4] = {lerp, lerp, lerp, lerp};
			Uint16	lerp2_4[4] = {256-lerp, 256-lerp, 256-lerp, 256-lerp};
			Uint64	zero = 0;
			asm ("\n"
					"movd					%1, %%mm0\n"
					"movd					%2, %%mm1\n"
					"punpcklbw				%5, %%mm0\n"
					"punpcklbw				%5, %%mm1\n"
					"pmullw					%3, %%mm0\n"
					"pmullw					%4, %%mm1\n"
					"psrlw					$8, %%mm0\n"
					"psrlw					$8, %%mm1\n"
					"paddsw					%%mm1, %%mm0\n"
					"packuswb				%5, %%mm0\n"
					"movd					%%mm0, %0\n"
					: "=m" (*(Uint32 *)out) :
					"m" (*(Uint32 *)(&in[0])),
					"m" (*(Uint32 *)(&in[4])),
					"m" (*(Uint64 *)lerp2_4),
					"m" (*(Uint64 *)lerp1_4),
					"m" (zero) : "mm0", "mm1");
		} else {
			/* last pixel of the line has no pixel to lerp to */
			*(Uint32 *) out = *(Uint32 *) in;
		}
		out += 4;
	}
}

static void
R_ResampleTextureMMX (void *indata, int inwidth, int inheight, void *outdata,
		int outwidth, int outheight)
{
	if (r_lerpimages->ivalue)
	{
		int		i, j, yi, oldy, f, fstep_h, fstep_w, endy = (inheight-1);
		Uint8	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep_h = (int) (inheight*65536.0f/outheight);
		fstep_w = (int) (inwidth*65536.0f/outwidth);

		row1 = Zone_Alloc(tempzone, outwidth*4);
		row2 = Zone_Alloc(tempzone, outwidth*4);
		inrow = indata;
		oldy = 0;
		R_ResampleTextureLerpLineMMX (inrow, row1, inwidth, outwidth, fstep_w);
		R_ResampleTextureLerpLineMMX (inrow + inwidth*4, row2, inwidth,
				outwidth, fstep_w);
		for (i = 0, f = 0;i < outheight;i++,f += fstep_h)
		{
			yi = f >> 16;
			if (yi < endy)
			{
				Uint16	lerp = (f & 0xFFFF) >> 8;
				Uint16	lerp1_4[4] = {lerp, lerp, lerp, lerp};
				Uint16	lerp2_4[4] = {256-lerp, 256-lerp, 256-lerp, 256-lerp};
				if (yi != oldy)
				{
					inrow = (Uint8 *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLineMMX (inrow, row1, inwidth,
								outwidth, fstep_w);
					R_ResampleTextureLerpLineMMX (inrow + inwidth*4, row2,
							inwidth, outwidth, fstep_w);
					oldy = yi;
				}
				for (j = 0; j < outwidth; j++)  
				{
					asm ("\n"
							"movd					%1, %%mm0\n"
							"movd					%2, %%mm1\n"
							"pxor					%%mm2, %%mm2\n"
							"punpcklbw				%%mm2, %%mm0\n"
							"punpcklbw				%%mm2, %%mm1\n"
							"pmullw					%3, %%mm0\n"
							"pmullw					%4, %%mm1\n"
							"psrlw					$8, %%mm0\n"
							"psrlw					$8, %%mm1\n"
							"paddsw					%%mm1, %%mm0\n"
							"packuswb				%%mm2, %%mm0\n"
							"movd					%%mm0, %0\n"
							: "=m" (*(Uint32 *)out) :
							"m" (*(Uint32 *)row1),
							"m" (*(Uint32 *)row2),
							"m" (*(Uint64 *)lerp2_4),
							"m" (*(Uint64 *)lerp1_4)
							: "mm0", "mm1");

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
						R_ResampleTextureLerpLineMMX (inrow, row1, inwidth,
								outwidth, fstep_w);
					oldy = yi;
				}
				memcpy(out, row1, outwidth * 4);
			}
		}
		asm volatile ("emms\n");
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
			inrow = (Uint32 *)indata + inwidth * (i * inheight / outheight);
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

static void
R_ResampleTextureMMX_EXT (void *indata, int inwidth, int inheight, void *outdata,
		int outwidth, int outheight)
{
	if (r_lerpimages->ivalue)
	{
		int		i, j, yi, oldy, f, fstep_h, fstep_w, endy = (inheight-1);
		Uint8	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep_h = (int) (inheight*65536.0f/outheight);
		fstep_w = (int) (inwidth*65536.0f/outwidth);

		row1 = Zone_Alloc(tempzone, outwidth*4);
		row2 = Zone_Alloc(tempzone, outwidth*4);
		inrow = indata;
		oldy = 0;
		R_ResampleTextureLerpLineMMX (inrow, row1, inwidth, outwidth, fstep_w);
		R_ResampleTextureLerpLineMMX (inrow + inwidth*4, row2, inwidth,
				outwidth, fstep_w);
		for (i = 0, f = 0;i < outheight;i++,f += fstep_h)
		{
			yi = f >> 16;
			if (yi < endy)
			{
				Uint16	lerp = (f & 0xFFFF) >> 8;
				Uint16	lerp1_4[4] = {lerp, lerp, lerp, lerp};
				Uint16	lerp2_4[4] = {256-lerp, 256-lerp, 256-lerp, 256-lerp};
				if (yi != oldy)
				{
					inrow = (Uint8 *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLineMMX (inrow, row1, inwidth,
								outwidth, fstep_w);
					R_ResampleTextureLerpLineMMX (inrow + inwidth*4, row2,
							inwidth, outwidth, fstep_w);
					oldy = yi;
				}
				for (j = 0; j < outwidth; j += 2)  
				{
					asm ("\n"
							"pxor					%%mm0, %%mm0\n"
							"pxor					%%mm1, %%mm1\n"
							"pxor					%%mm2, %%mm2\n"
							"pxor					%%mm3, %%mm3\n"
							"punpcklbw				%1, %%mm0\n"
							"punpcklbw				4%1, %%mm2\n"
							"punpcklbw				%2, %%mm1\n"
							"punpcklbw				4%2, %%mm3\n"
							"pmulhuw				%3, %%mm0\n"
							"pmulhuw				%3, %%mm2\n"
							"pmulhuw				%4, %%mm1\n"
							"pmulhuw				%4, %%mm3\n"
							"packuswb				%%mm2, %%mm0\n"
							"packuswb				%%mm3, %%mm1\n"
							"paddusb				%%mm1, %%mm0\n"
							"movq					%%mm0, %0\n"
							: "=o" (*(Uint64 *)out) :
							"o" (*(Uint64 *)row1),
							"o" (*(Uint64 *)row2),
							"m" (*(Uint64 *)lerp2_4),
							"m" (*(Uint64 *)lerp1_4)
							: "memory", "mm0", "mm1", "mm2", "mm3");

					out += 8;
					row1 += 8;
					row2 += 8;
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
						R_ResampleTextureLerpLineMMX (inrow, row1, inwidth,
								outwidth, fstep_w);
					oldy = yi;
				}
				memcpy(out, row1, outwidth * 4);
			}
		}
		asm volatile ("emms\n");
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
#endif

static void
R_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,
		int outwidth, int outheight)
{
#ifdef HAVE_MMX
	if ((cpu_flags & CPU_MMX_EXT) && (inwidth != 1) && (inheight != 1) && (outwidth != 1) && (outheight != 1))
		R_ResampleTextureMMX_EXT (indata, inwidth, inheight, outdata, outwidth, outheight);
	else if (cpu_flags & CPU_MMX)
		R_ResampleTextureMMX (indata, inwidth, inheight, outdata, outwidth, outheight);
	else
#endif
		R_ResampleTextureBase (indata, inwidth, inheight, outdata, outwidth, outheight);
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void
GL_MipMap (Uint8 *in, Uint8 *out, int width, int height)
{
	int i, j;

	width <<= 2;
	height >>= 1;
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

static void
AssertScaledBuffer (int scaled_width, int scaled_height)
{
	if (scaledsize < scaled_width * scaled_height) {
		scaledsize = scaled_width * scaled_height;
		if (scaled)
			Zone_Free(scaled);
		if (scaled2)
			Zone_Free(scaled2);
		scaled = NULL;
		scaled2 = NULL;
	}

	if (scaled == NULL)
		scaled = Zone_Alloc(glt_zone, scaledsize * 4);
	if (scaled2 == NULL)
		scaled2 = Zone_Alloc(glt_zone, scaledsize * 4);
}

static void
GL_MipMapTexture (Uint32 *data, int width, int height, int samples)
{
	int		 miplevel = 0;
	int		 channel, i;
	Uint8	*in = (Uint8 *) data;

	AssertScaledBuffer (width, height);

	while (width > 1 || height > 1)
	{
		GL_MipMap (in, (Uint8 *) scaled, width, height);
		in = (Uint8 *) scaled;

		width >>= 1;
		height >>= 1;
		width = max (width, 1);
		height = max (height, 1);

		miplevel++;

		if (r_colormiplevels->ivalue)
		{
			channel = (miplevel - 1) % 3;
			for (i = 0; i < (width * height); i++)
			{
				scaled2[i] = 0;
				((Uint8 *)&scaled2[i])[channel] =((Uint8 *)&scaled[i])[channel];
				((Uint8 *)&scaled2[i])[3] = ((Uint8 *)&scaled[i])[3];
			}

			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, width,
					height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled2);
		}
		else
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, width,
					height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
}

/*
 * =========================================================================
 *                     End of the texture mangling code.
 * =========================================================================
 */

/*
 * =========================================================================
 *                   Start of the texture management code. 
 * =========================================================================
 */

/*
===============
GL_Upload32
===============
*/
qboolean
GL_Upload32 (Uint32 *data, int width, int height, int flags)
{
	int		 samples, scaled_width, scaled_height;
	Uint32	*final;

	// OpenGL textures are power of two
	scaled_width = near_pow2_high(width);
	scaled_height = near_pow2_high(height);

	// Apply gl_picmip, a setting of one cuts texture memory usage 75%!
	scaled_width >>= gl_picmip->ivalue;
	scaled_height >>= gl_picmip->ivalue;

	// Clip textures to a sane value
	scaled_width = bound (1, scaled_width, gl_max_size->ivalue);
	scaled_height = bound (1, scaled_height, gl_max_size->ivalue);

	samples = (flags & TEX_ALPHA) ? glt_alpha_format : glt_solid_format;

	if ((scaled_width != width) || (scaled_height != height))
	{
		AssertScaledBuffer (scaled_width, scaled_height);
		R_ResampleTexture (data, width, height, scaled, scaled_width,
				scaled_height);
		final = scaled;
	} else
		final = data;

	if (flags & TEX_MIPMAP) {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_min);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);
		if (gl_sgis_mipmap)
			qglTexParameterf (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, true);
	} else {
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_mag);
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);
		if (gl_sgis_mipmap)
			qglTexParameterf (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, false);
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, final);

	if (!gl_sgis_mipmap && (flags & TEX_MIPMAP))
		GL_MipMapTexture (final, scaled_width, scaled_height, samples);

	return true;
}


/*
===============
GL_Upload8
===============
*/
qboolean
GL_Upload8 (Uint8 *data, int width, int height, Uint32 *palette, int flags)
{
	Uint32 *trans;

	trans=GLT_8to32_convert(data, width, height, palette, !(flags & TEX_FORCE));
	if (!trans)
		return false;

	return GL_Upload32 (trans, width, height, flags);
}

/*
================
GL_Load_Raw
================
*/
int
GLT_Load_Raw (const char *identifier, Uint width, Uint height, Uint8 *data,
		Uint32 *palette, int flags, int bpp)
{
	gltexture_t	*glt = NULL;
	Uint16		crc = 0;
	qboolean	ret = false;

	/* see if the texture is already present */
	if (identifier[0])
	{
		crc = CRC_Block (data, width*height);

		for (glt = gltextures; glt != NULL; glt = glt->next)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if ((flags & TEX_REPLACE) ||
						(width == glt->width && height == glt->height
						&& crc == glt->crc)) {
					glt->count++;
					return glt->texnum;
				} else
					/* reload the texture into the same slot */
					goto setuptexture;
			}
		}
	}

	glt = Zone_Alloc (glt_zone, sizeof (gltexture_t));
	glt->next = gltextures;
	glt->count = 1;
	gltextures = glt;

	strlcpy (glt->identifier, identifier, sizeof (glt->identifier));
	qglGenTextures(1, &glt->texnum);

setuptexture:
	glt->width = width;
	glt->height = height;
	glt->mipmap = flags & TEX_MIPMAP;
	glt->crc = crc;

	qglBindTexture (GL_TEXTURE_2D, glt->texnum);

	switch (bpp) {
		case 8:
			ret = GL_Upload8 (data, width, height, palette, flags);
			break;
		case 32:
			ret = GL_Upload32 ((Uint32 *) data, width, height, flags);
			break;
		default:
			Sys_Error ("Bad bpp!");
	}

	if (ret)
		return glt->texnum;

	GLT_Delete (glt->texnum);
	return 0;
}

/*
================
GLT_Load_image

FIXME: HACK HACK HACK - this is just a GL_LoadTexture for image_t's for now
		and is temporary.  It should be replaced by a function which takes
		only the name and some flags, and which returns a (yet unwritten)
		texture structure.
================
*/
int
GLT_Load_image(const char *identifier, image_t *img, Uint32 *palette, int flags)
{
	switch (img->type) {
		case IMG_QPAL:
			return GLT_Load_Raw (identifier, img->width, img->height,
					img->pixels, palette, flags, 8);
		case IMG_RGBA:
			return GLT_Load_Raw (identifier, img->width, img->height,
					img->pixels, palette, flags, 32);
		default:
			Sys_Error ("Bad bpp!");
	}

	return 0;
}

/*
===============
GLT_Load_Pixmap

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
int
GLT_Load_Pixmap (const char *name, const char *data)
{
	int			i;
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

	return GLT_Load_Raw (name, 32, 32, (Uint8 *) pixels, NULL, TEX_ALPHA, 32);
}

/*
================
GL_LoadPicTexture
================
*/
int
GLT_Load_qpic (qpic_t *pic)
{
	return GLT_Load_Raw ("", pic->width, pic->height, pic->data, NULL, TEX_ALPHA, 8);
}


qboolean
GLT_Delete (GLuint texnum)
{
	gltexture_t *cur, **last;

	last = &gltextures;
	for (cur = gltextures; cur != NULL; cur = cur->next)
	{
		if (cur->texnum == texnum) {
			if (!--cur->count) {
				*last = cur->next;
				qglDeleteTextures (1, &cur->texnum);
				Zone_Free (cur);
			}
			return true;
		}
		last = &cur->next;
	}
	return false;
}
