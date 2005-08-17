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
#include "dyngl.h"
#include "fs/wad.h"
#include "gl_info.h"
#include "image/image.h"
#include "mathlib.h"
#include "model.h"
#include "qtypes.h"
#include "strlib.h"
#include "sys.h"
#include "textures.h"
//#include "xmmintrin.h"

static Uint32 * GLT_8to32_convert (Uint8 *data, int width, int height, Uint32 *palette, qboolean check_empty);
static void GLT_FloodFill8 (Uint8 * skin, int skinwidth, int skinheight);
static qboolean GLT_Skin_CheckForInvalidTexCoords(texcoord_t *texcoords, int numverts, int width, int height);

/*
 * Stuff for tracking of loaded textures.
 */
typedef struct gltexture_s
{
	GLuint				texnum;
	char				identifier[128];
	GLuint				width, height;
	qboolean			mipmap;
	unsigned short		checksum;
	int					count;
	struct gltexture_s	*next;
}
gltexture_t;

static gltexture_t *gltextures;

/*
 * Memory zones.
 */
static memzone_t *glt_skin_zone;
static memzone_t *glt_zone;

/*
 * This stuff is entirely specific to the selection of gl filter and 
 * texture modes.
 */
static int		glt_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int		glt_filter_mag = GL_LINEAR;
static int		glt_solid_format = 3;
static int		glt_alpha_format = 4;

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
static void Set_Anisotropy_f (struct cvar_s *var);
static cvar_t *gl_texturemode;
static cvar_t *r_lerpimages;
static cvar_t *r_colormiplevels;
static cvar_t *gl_picmip;
static cvar_t *gl_max_size;
static cvar_t *gl_texture_anisotropy;

static cvar_t *r_smartflt;
static cvar_t *r_smartflt_y;
static cvar_t *r_smartflt_cb;
static cvar_t *r_smartflt_cr;

static Uint32 *conv_trans;
static int conv_trans_size;

static int scaledsize = 0;
static Uint32 *scaled, *scaled2;


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
GLT_Init_Cvars (void)
{
	int max_tex_size = 0;

	qglGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_tex_size);
	if (!max_tex_size)
		max_tex_size = 1024;

	gl_max_size = Cvar_Get ("gl_max_size", va("%d", max_tex_size), CVAR_NONE,
			GLT_verify_pow2);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
			CVAR_ARCHIVE, Set_TextureMode_f);
	r_lerpimages = Cvar_Get ("r_lerpimages", "1", CVAR_ARCHIVE, NULL);
	r_colormiplevels = Cvar_Get ("r_colormiplevels", "0", CVAR_NONE, NULL);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_ARCHIVE, NULL);
	gl_texture_anisotropy = Cvar_Get("gl_texture_anisotropy", "1", CVAR_ARCHIVE, Set_Anisotropy_f);

	r_smartflt = Cvar_Get ("r_smartflt", "0", CVAR_ARCHIVE, NULL);
	r_smartflt_y = Cvar_Get ("r_smartflt_y", "10", CVAR_ARCHIVE, NULL);
	r_smartflt_cb = Cvar_Get ("r_smartflt_cb", "50", CVAR_ARCHIVE, NULL);
	r_smartflt_cr = Cvar_Get ("r_smartflt_cr", "50", CVAR_ARCHIVE, NULL);
}

void
GLT_Init (void)
{
	glt_zone = Zone_AllocZone("GL textures");
	glt_skin_zone = Zone_AllocZone("GL textures (skins)");
}

void
GLT_Shutdown (void)
{
	gltexture_t *cur, *next;

	cur = gltextures;
	while (cur)
	{
		Com_DPrintf ("GLT_Shutdown: '%s', %d\n", cur->identifier, cur->count);
		next = cur->next;
		qglDeleteTextures (1, &cur->texnum);
		Zone_Free (cur);
		cur = next;
	}
	gltextures = NULL;

	Zone_Free (conv_trans);
	conv_trans = NULL;
	conv_trans_size = 0;
	scaledsize = 0;
	Zone_Free (scaled);
	scaled = NULL;
	Zone_Free (scaled2);
	scaled2 = NULL;

	Zone_PrintZone (true, glt_zone);
	Zone_PrintZone (true, glt_skin_zone);
	Zone_FreeZone (&glt_zone);
	Zone_FreeZone (&glt_skin_zone);
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
			qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					glt_filter_min);
			qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					glt_filter_mag);
		}
	}
}

static void
Set_Anisotropy_f (struct cvar_s *var)
{
	gltexture_t *glt;

	if (!gl_ext_anisotropy)
	{
		if (var->fvalue)
			Com_DPrintf("Ignoring anisotropy (not supported)\n");
		return;
	}

	/* change all the existing max anisotropy settings */
	for (glt = gltextures; glt != NULL; glt = glt->next)
	{
		if (glt->mipmap)
		{
			qglBindTexture (GL_TEXTURE_2D, glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					var->fvalue);
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
		texcoord_t texcoords[3], Uint32 color)
{
	Uint32		*line;
	int			x, y, start = 0, end = 0, y1, y2;

	y1 = height;
	y2 = 0;
	GLT_DoSpan(texcoords[0].v[0] * width, texcoords[0].v[1] * height, texcoords[1].v[0] * width, texcoords[1].v[1] * height, span, width, height, &y1, &y2);
	GLT_DoSpan(texcoords[1].v[0] * width, texcoords[1].v[1] * height, texcoords[2].v[0] * width, texcoords[2].v[1] * height, span, width, height, &y1, &y2);
	GLT_DoSpan(texcoords[2].v[0] * width, texcoords[2].v[1] * height, texcoords[0].v[0] * width, texcoords[0].v[1] * height, span, width, height, &y1, &y2);

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
	texcoord_t		texcoords[3];
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
GLT_Skin_CheckForInvalidTexCoords(texcoord_t *texcoords, int numverts,
		int width, int height)
{
	int i, s, t;

	for (i = 0;i < numverts;i++, texcoords++)
	{
		s = ceil(texcoords->v[0] * width);
		t = ceil(texcoords->v[1] * height);
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
	if (sub->tris)
		Zone_Free (sub->tris);
}

static void
GLT_Delete_Indices (skin_indices_t *i)
{
	if (i->i)
		Zone_Free(i->i);
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

	Zone_Free (skin->base);
	Zone_Free (skin->base_team);
	Zone_Free (skin->fb);
	Zone_Free (skin->top);
	Zone_Free (skin->bottom);
	Zone_Free (skin->base_fb_i);
	Zone_Free (skin->base_team_fb_i);
	Zone_Free (skin->top_bottom_i);
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
	int i, size, count = 0;
	Uint32	d, t;

	if (!palette)
		palette = d_palette_raw;

	size = width * height;
	if (size > conv_trans_size)
	{
		if (conv_trans)
			Zone_Free(conv_trans);
		conv_trans = Zone_Alloc(glt_zone, size * sizeof(Uint32));
		conv_trans_size = size;
	}

	for (i = 0; i < size;) {
		d = LittleLong(((Uint32 *) data)[i >> 2]);

		t = palette[d & 0xFF];
		if ((conv_trans[i++] = t) != d_palette_empty)
			count++;

		switch (size - i) {
			default:
			case 3:
				t = palette[(d & 0xFF00) >> 8];
				if ((conv_trans[i++] = t) != d_palette_empty)
					count++;
			case 2:
				t = palette[(d & 0xFF0000) >> 16];
				if ((conv_trans[i++] = t) != d_palette_empty)
					count++;
			case 1:
				t = palette[(d & 0xFF000000) >> 24];
				if ((conv_trans[i++] = t) != d_palette_empty)
					count++;
			case 0:
				break;
		}
	}

	if (!count && check_empty)
		return NULL;
	else
		return conv_trans;
}

/*
=================
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

static void
R_ResampleTextureNoLerpBase (void *indata, int inwidth, int inheight,
		void *outdata, int outwidth, int outheight)
{
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

static void
R_ResampleTextureBase (void *indata, int inwidth, int inheight, void *outdata,
		int outwidth, int outheight)
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
			int lerp2 = (f & 0xFFFF) >> 8;
			int lerp1 = 256-lerp2;

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

				*((Uint32 *) out) =
					((((r1 & 0xFF00FF00) >> 8) * lerp1 +
					  ((r2 & 0xFF00FF00) >> 8) * lerp2) & 0xFF00FF00) +
					((((r1 & 0x00FF00FF) * lerp1 +
					   (r2 & 0x00FF00FF) * lerp2) >> 8) & 0x00FF00FF);

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
}


#if defined(HAVE_GCC_ASM_X86_MMX) && 1
static void
R_ResampleTextureLerpLineMMX (Uint8 *in, Uint8 *out, int inwidth, int outwidth,
		int fstep_w)
{
	int		j, xi, oldx = 0, f, endx;
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep_w)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			Uint16	lerp = (f >> 8) & 0xFF;
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
}

static void
R_ResampleTextureMMX_EXT (void *indata, int inwidth, int inheight,
		void *outdata, int outwidth, int outheight)
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
}
#endif

/* BorderCheck - used by the SmartFlt-based resample filter to check for
   a border between the two given pixels. edge detection is done in the
   YCbCr colorspace, with different thresholds for each channel.
*/
/*
int
BorderCheck(unsigned char *src1, unsigned char* src2, int dY, int dCb, int dCr)
{
	float R1, G1, B1, A1, R2, G2, B2, A2, Y1, Cb1, Cr1, Y2, Cb2, Cr2;

	R1 = *src1; G1 = *(src1+1); B1 = *(src1+2);// A1 = *(src1+3);
	R2 = *src2; G2 = *(src2+1); B2 = *(src2+2);// A2 = *(src2+3);

	Y1 = 16 + (((R1 * 65.738) + (G1 * 129.057) + (B1 * 25.064)) / 256.0);
	Cb1 = 128 + (((R1 * -37.945) + (G1 * -74.494) + (B1 * 112.439)) / 256.0);
	Cr1 = 128 + (((R1 * 112.439) + (G1 * -94.154) + (B1 * -18.285)) / 256.0);

	Y2 = 16 + (((R2 * 65.738) + (G2 * 129.057) + (B2 * 25.064)) / 256.0);
	Cb2 = 128 + (((R2 * -37.945) + (G2 * -74.494) + (B2 * 112.439)) / 256.0);
	Cr2 = 128 + (((R2 * 112.439) + (G2 * -94.154) + (B2 * -18.285)) / 256.0);

	return ( (abs(Y1 - Y2) > dY) || (abs(Cb1 - Cb2) > dCb) || (abs(Cr1 - Cr2) > dCr) );

}
*/

/* BorderCheck macro. the pixels that get passed in should be pre-converted
   to the YCbCr colorspace.
*/
#define BorderCheck(pix1, pix2, dY, dCb, dCr) ( (abs(*pix1 - *pix2) > dY) \
											||	(abs(*(pix1+1) - *(pix2+1)) > dCb) \
											||	(abs(*(pix1+2) - *(pix2+2)) > dCr) )

/*
RGBAtoTCbCrA - converts a source RGBA pixel into a destination YCbCrA pixel
*/
static inline void
RGBAtoYCbCrA (unsigned char *dest, unsigned char *src)
{
	unsigned char s0, s1, s2;
	s0 = *(src);
	s1 = *(src+1);
	s2 = *(src+2);
#define MIX(i,n,m0,m1,m2)	(*(dest+i) = (unsigned char) (n + (((s0 * m0) + (s1 * m0) + (s2 * m2))/256.0f)))
	MIX(0, 16.0f, 65.738f, 129.057f, 25.064f);
	MIX(1, 128.0f, -37.945f, -74.494f, 112.439f);
	MIX(2, 128.0f, 112.439f, -94.154f, -18.285f);
	*(dest+3) = *(src+3);
}

#define LinearScale(src1, src2, pct) (( src1 * (1 - pct) ) + ( src2 * pct))
#define GetOffs(new, start, cur) (new + (cur - ((unsigned char*)start)))

/*
R_ResampleTextureSmartFlt - resamples the texture given in indata, of the
dimensions inwidth by inheight to outdata, of the dimensions outwidth by
outheight, using a method based on the brief description of SmartFlt
given at http://www.hiend3d.com/smartflt.html 

this could probably do with some optimizations.
TODO: add some cvars to enable this resample filter and to change the YCbCr channel
thresholds.
*/
void
R_ResampleTextureSmartFlt(void *indata, int inwidth, int inheight,
						  void *outdata, int outwidth, int outheight)
{
	float xstep = ((float)inwidth) / ((float)outwidth);
	float ystep = ((float)inheight) / ((float)outheight);

	int dY = r_smartflt_y->ivalue;
	int dCb = r_smartflt_cb->ivalue;
	int dCr = r_smartflt_cr->ivalue;

	int DestX, DestY;
	float SrcX, SrcY;

	// buffer to stor the YCbCrA version of the input texture.
	unsigned char *Ybuffer = Zone_Alloc(tempzone, inwidth * inheight * 4);

	unsigned char *id = indata;
	unsigned char *od = outdata;
	unsigned char *idrowstart = id;

	// convert the input texture to YCbCr into a temp buffer, for border detections.
	for(DestX=0, idrowstart=Ybuffer; DestX< (inwidth*inheight); DestX++, idrowstart+=4, id+=4)
		RGBAtoYCbCrA(idrowstart, id);

	for(DestY=0, SrcY=0; DestY < outheight; DestY++, SrcY+=ystep)
	{
		// four "work" pointers to make code a little nicer.
		unsigned char *w0, *w1, *w2, *w3;
		// right == clockwise, left == counter-clockwise
		unsigned char *nearest, *left, *right, *opposite;
		float pctnear, pctleft, pctright, pctopp;
		float w0pct, w1pct, w2pct, w3pct;
		float x, y, tmpx, tmpy;
		char edges[6];

		// clamp SrcY to cover for possible float error
		// to make sure the edges fall into the special cases
		if (SrcY > (inheight - 1.01f))
			SrcY = (inheight - 1.01f);

		// go to the start of the next row. "od" should be pointing at the right place already.
		idrowstart = ((unsigned char*)indata) + ((int)SrcY)*inwidth*4;

		for(DestX=0, SrcX=0; DestX < outwidth; DestX++, od+=4, SrcX+=xstep)
		{
			// clamp SrcY to cover for possible float error
			// to make sure that the edges fall into the special cases
			if (SrcX > (inwidth - 1.01f))
				SrcX = inwidth - 1.01f;

			id = idrowstart + ((int) SrcX)*4;

			x = ((int)SrcX);
			y = ((int)SrcY);

			// if we happen to be directly on a source row
			if(SrcY==y)
			{
				// and also directly on a source column
				if(SrcX==x)
				{
					// then we are directly on a source pixel
					// just copy it and move on.
					memcpy(od, id, 4);
					//*((unsigned int*)od) = *((unsigned int*)id);
					continue;
				}

				// if there is a border between the two surrounding source pixels
				if( BorderCheck(GetOffs(Ybuffer, indata, id), GetOffs(Ybuffer, indata, (id+4)), dY, dCb, dCr) )
				{
					// if we are closer to the left
					if(x == ((int)(SrcX+0.5f)))
					{
						// copy the left pixel
						memcpy(od, id, 4);
						//*((unsigned int*)od) = *((unsigned int*)id);
						continue;
					}
					else
					{
						// otherwise copy the right pixel
						memcpy(od, id+4, 4);
						//*((unsigned int*)od) = *((unsigned int*)(id+4));
						continue;
					}
				}
				else
				{
					// these two bordering pixels are part of the same region.
					// blend them using a weighted average
					x = SrcX - x;

					w0 = id;
					w1 = id + 4;

					*od = (unsigned char) LinearScale(*w0, *w1, x);
					*(od+1) = (unsigned char) LinearScale(*(w0+1), *(w1+1), x);
					*(od+2) = (unsigned char) LinearScale(*(w0+2), *(w1+2), x);
					*(od+3) = (unsigned char) LinearScale(*(w0+3), *(w1+3), x);

					continue;
				}
			}
			
			// if we aren't direcly on a source row, but we are on a source column
			if( SrcX == x )
			{
				// if there is a border between this source pixel and the one on
				// the next row
				if( BorderCheck( GetOffs(Ybuffer, indata, id), GetOffs(Ybuffer, indata, (id + inwidth*4)), dY, dCb, dCr) )
				{
					// if we are closer to the top
					if(y == ((int)(SrcY + 0.5f)))
					{
						// copy the top
						memcpy(od, id,4);
						//*((unsigned int*)od) = *((unsigned int*)id);
						continue;
					}
					else
					{
						// copy the bottom
						memcpy(od, (id + inwidth*4), 4);
						//*((unsigned int*)od) = *((unsigned int*)(id + inwidth*4));
						continue;
					}
				}
				else
				{
					// the two pixels are part of the same region, blend them
					// together with a weighted average
					y = SrcY - y;

					w0 = id;
					w1 = id + (inwidth * 4);

					*od = (unsigned char) LinearScale(*w0, *w1, y);
					*(od+1) = (unsigned char) LinearScale(*(w0+1), *(w1+1), y);
					*(od+2) = (unsigned char) LinearScale(*(w0+2), *(w1+2), y);
					*(od+3) = (unsigned char) LinearScale(*(w0+3), *(w1+3), y);

					continue;
				}
			}

			// now for the non-simple case: somewhere between four pixels.
			// w0 is top-left, w1 is top-right, w2 is bottom-left, and w3 is bottom-right
			w0 = id;
			w1 = id + 4;
			w2 = id + (inwidth * 4);
			w3 = w2 + 4;

			x = SrcX - x;
			y = SrcY - y;

// LordHavoc: added this to get it to compile, there must be a better solution
#ifdef WIN32
#define sqrtf sqrt
#endif
			w0pct = 1.0f - sqrtf( x*x + y*y);
			w1pct = 1.0f - sqrtf( (1-x)*(1-x) + y*y);
			w2pct = 1.0f - sqrtf( x*x + (1-y)*(1-y));
			w3pct = 1.0f - sqrtf( (1-x)*(1-x) + (1-y)*(1-y));

			// set up our symbolic identification.
			// "nearest" is the pixel whose quadrant we are in.
			// "left" is counter-clockwise from "nearest"
			// "right" is clockwise from "nearest"
			// "opposite" is, well, opposite.
			if ( x < 0.5f )
			{
				tmpx = x;
				if ( y < 0.5f )
				{
					nearest = w0;
					left = w2;
					right = w1;
					opposite = w3;
					pctnear = w0pct;
					pctleft = w2pct;
					pctright = w1pct;
					pctopp = w3pct;
					tmpy = y;
				}
				else
				{
					nearest = w2;
					left = w3;
					right = w0;
					opposite = w1;
					pctnear = w2pct;
					pctleft = w3pct;
					pctright = w0pct;
					pctopp = w1pct;
					tmpy = 1.0f - y;
				}
			}
			else
			{
				tmpx = 1.0f - x;
				if( y < 0.5f )
				{
					nearest = w1;
					left = w0;
					right = w3;
					opposite = w2;
					pctnear = w1pct;
					pctleft = w0pct;
					pctright = w3pct;
					pctopp = w2pct;
					tmpy = y;
				}
				else
				{
					nearest = w3;
					left = w1;
					right = w2;
					opposite = w0;
					pctnear = w3pct;
					pctleft = w1pct;
					pctright = w2pct;
					pctopp = w0pct;
					tmpy = 1.0f - y;
				}
			}

			x = tmpx;
			y = tmpy;

			w0 = GetOffs(Ybuffer, indata, nearest);
			w1 = GetOffs(Ybuffer, indata, right);
			w2 = GetOffs(Ybuffer, indata, left);
			w3 = GetOffs(Ybuffer, indata, opposite);

			edges[0] = BorderCheck(w0, w2, dY, dCb, dCr);
			edges[1] = BorderCheck(w0, w1, dY, dCb, dCr);
			edges[2] = BorderCheck(w0, w3, dY, dCb, dCr);

			edges[3] = BorderCheck(w3, w2, dY, dCb, dCr);
			edges[4] = BorderCheck(w3, w1, dY, dCb, dCr);

			edges[5] = BorderCheck(w2, w1, dY, dCb, dCr);

#undef GetOffs

			// do the edge detections.
			if ( edges[0] && edges[1] && edges[2] && !edges[5] )
			{
				// borders all around, and no border between the left and right.

				// if there is no border between the opposite side and only one
				// of the two other corners, or if we are closer to the corner
				if ( ( edges[3] && !edges[4] ) || ( !edges[3] && edges[4]) || ( x + y < 0.5f ))
				{
					// closer to to the corner.
					//pctleft = pctright = pctopp = 0.0f;
					memcpy(od, nearest, 4);
					//*((unsigned int*)od) = *((unsigned int*)nearest);
				}
				else
				{
					// closer to the center. (note, there is a diagonal line between the nearest pixel
					// and the center of the four.)

					// exclude the "nearest" pixel
					// pctnear = 0.0f;
					// if there is a border around the opposite corner,
					// exclude it from the current pixel.
					if ( edges[3] && edges[4] )
					{
						// pctopp = 0.0f;
						*od = (unsigned char) bound(0, (((*left * pctleft) + (*right * pctright)) / (pctleft + pctright)), 255);
						*(od+1) = (unsigned char) bound(0, (((*(left+1) * pctleft) + (*(right+1) * pctright)) / (pctleft + pctright)), 255);
						*(od+2) = (unsigned char) bound(0, (((*(left+2) * pctleft) + (*(right+2) * pctright)) / (pctleft + pctright)), 255);
						*(od+3) = (unsigned char) bound(0, (((*(left+3) * pctleft) + (*(right+3) * pctright)) / (pctleft + pctright)), 255);
					} else {
						*od = (unsigned char) bound(0, (((*left * pctleft) + (*right * pctright) + (*opposite * pctopp)) / (pctleft + pctright + pctopp)), 255);
						*(od+1) = (unsigned char) bound(0, (((*(left+1) * pctleft) + (*(right+1) * pctright) + (*(opposite+1) * pctopp)) / (pctleft + pctright + pctopp)), 255);
						*(od+2) = (unsigned char) bound(0, (((*(left+2) * pctleft) + (*(right+2) * pctright) + (*(opposite+2) * pctopp)) / (pctleft + pctright + pctopp)), 255);
						*(od+3) = (unsigned char) bound(0, (((*(left+3) * pctleft) + (*(right+3) * pctright) + (*(opposite+3) * pctopp)) / (pctleft + pctright + pctopp)), 255);
					}
				}
			}
			else if (edges[0] && edges[1] && edges[2]) {
				memcpy(od, nearest, 4);
				//*((unsigned int*)od) = *((unsigned int*)nearest);
			}
			else
			{
				float num[4], denom=pctnear;

				num[0] = (*nearest * pctnear);
				num[1] = (*(nearest+1) * pctnear);
				num[2] = (*(nearest+2) * pctnear);
				num[3] = (*(nearest+3) * pctnear);

				if ( !edges[0] ) {
					num[0] += *left * pctleft;
					num[1] += *(left+1) * pctleft;
					num[2] += *(left+2) * pctleft;
					num[3] += *(left+3) * pctleft;
					denom += pctleft;
					//pctleft = 0.0f;  // was if edges[0]
				}

				if ( edges[1] ) {
					num[0] += *right * pctright;
					num[1] += *(right+1) * pctright;
					num[2] += *(right+2) * pctright;
					num[3] += *(right+3) * pctright;
					denom += pctright;
					//pctright = 0.0f;  // was if edges[1]
				}

				if ( edges[2] ) {
					num[0] += *opposite * pctopp;
					num[1] += *(opposite+1) * pctopp;
					num[2] += *(opposite+2) * pctopp;
					num[3] += *(opposite+3) * pctopp;
					denom += pctopp;
					//pctopp = 0.0f;  // was if edges[2]
				}

				// blend the source pixels together to get the output pixel.
				// if a source pixel doesn't affect the output, it's percent should be set to 0 in the edge check
				// code above. if only one pixel affects the output, its percentage should be set to 1 and all
				// the others set to 0. (yeah, it is ugly, but I don't see a need to optimize this code (yet)
				//*od = (unsigned char) bound(0, (((*nearest * pctnear) + (*left * pctleft) + (*right * pctright) + (*opposite * pctopp)) / (pctnear + pctleft + pctright + pctopp)), 255);
				//*(od+1) = (unsigned char) bound(0, (((*(nearest+1) * pctnear) + (*(left+1) * pctleft) + (*(right+1) * pctright) + (*(opposite+1) * pctopp)) / (pctnear + pctleft + pctright + pctopp)), 255);
				//*(od+2) = (unsigned char) bound(0, (((*(nearest+2) * pctnear) + (*(left+2) * pctleft) + (*(right+2) * pctright) + (*(opposite+2) * pctopp)) / (pctnear + pctleft + pctright + pctopp)), 255);
				//*(od+3) = (unsigned char) bound(0, (((*(nearest+3) * pctnear) + (*(left+3) * pctleft) + (*(right+3) * pctright) + (*(opposite+3) * pctopp)) / (pctnear + pctleft + pctright + pctopp)), 255);

				*od = (unsigned char) bound(0, num[0] / denom, 255);
				*(od+1) = (unsigned char) bound(0, num[1] / denom, 255);
				*(od+2) = (unsigned char) bound(0, num[2] / denom, 255);
				*(od+3) = (unsigned char) bound(0, num[3] / denom, 255);
			}
		}
	}
	Zone_Free(Ybuffer);
}

void
R_ResampleTexture (void *id, int iw, int ih, void *od, int ow, int oh)
{
	if(r_smartflt->ivalue) {
		R_ResampleTextureSmartFlt(id, iw, ih, od, ow, oh);
		return;
	}

	if (!r_lerpimages->ivalue) {
		R_ResampleTextureNoLerpBase (id, iw, ih, od, ow, oh);
		return;
	}

#if defined(HAVE_GCC_ASM_X86_MMX) && 1
	if (1 && (cpu_flags & CPU_MMX_EXT) && !((iw & 1) || (ih & 1) || (ow & 1) || (oh & 1)))
		R_ResampleTextureMMX_EXT (id, iw, ih, od, ow, oh);
	else if (1 && cpu_flags & CPU_MMX)
		R_ResampleTextureMMX (id, iw, ih, od, ow, oh);
	else
#endif
		R_ResampleTextureBase (id, iw, ih, od, ow, oh);
}


/*
================
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

qboolean
GL_Upload32 (Uint32 *data, int width, int height, int flags)
{
	int		 samples, scaled_width, scaled_height;
	Uint32	*final;


	// OpenGL textures are power of two
	scaled_width = near_pow2_high(width);
	scaled_height = near_pow2_high(height);

	// Apply gl_picmip, a setting of one cuts texture memory usage 75%!
	// gl_picmip can also be used to enlarge the texture by using a negative number
	// this, of course, uses more texture memory
	if( gl_picmip->ivalue) {
		if (gl_picmip->ivalue > 0) {
			scaled_width >>= gl_picmip->ivalue;
			scaled_height >>= gl_picmip->ivalue;
		} else {
			scaled_width <<= -gl_picmip->ivalue;
			scaled_height <<= -gl_picmip->ivalue;
		}
	}

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
		if (gl_ext_anisotropy)
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy->fvalue);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_min);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);
		if (gl_sgis_mipmap)
			qglTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, true);
	} else {
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glt_filter_mag);
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glt_filter_mag);
		if (gl_sgis_mipmap)
			qglTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, false);
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, final);

	if (!gl_sgis_mipmap && (flags & TEX_MIPMAP))
		GL_MipMapTexture (final, scaled_width, scaled_height, samples);

	return true;
}


qboolean
GL_Upload8 (Uint8 *data, int width, int height, Uint32 *palette, int flags)
{
	Uint32 *trans;

	trans=GLT_8to32_convert(data, width, height, palette, !(flags & TEX_FORCE));
	if (!trans)
		return false;

	return GL_Upload32 (trans, width, height, flags);
}

int
GLT_Load_Raw (const char *identifier, Uint width, Uint height, Uint8 *data,
		Uint32 *palette, int flags, int bpp)
{
	gltexture_t	*glt = NULL;
	Uint16		checksum = 0;
	qboolean	ret = false;

	/* see if the texture is already present */
	if (identifier[0])
	{
		checksum = Checksum_32 ((Uint32 *) data, width*height*(bpp/8));
		if (!checksum)
			checksum = CRC_Block (data, width*height*(bpp/8));

		for (glt = gltextures; glt != NULL; glt = glt->next)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (!(flags & TEX_REPLACE) ||
						(width == glt->width && height == glt->height
						&& checksum == glt->checksum)) {
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
	glt->checksum = checksum;

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
			return img->texnum = GLT_Load_Raw (identifier, img->width,
					img->height, img->pixels, palette, flags, 8);
		case IMG_RGBA:
			return img->texnum = GLT_Load_Raw (identifier, img->width,
					img->height, img->pixels, palette, flags, 32);
		default:
			Sys_Error ("Bad bpp!");
	}

	return 0;
}

/*
===============
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


qboolean
GLT_Delete (GLuint texnum)
{
	gltexture_t *cur, **last;

	last = &gltextures;
	for (cur = gltextures; cur; cur = cur->next)
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
