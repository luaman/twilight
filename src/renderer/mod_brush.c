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

#include <math.h>

#include "crc.h"
#include "cvar.h"
#include "dyngl.h"
#include "gl_info.h"
#include "gl_main.h"
#include "host.h"
#include "mathlib.h"
#include "mdfour.h"
#include "quakedef.h"
#include "sky.h"
#include "strlib.h"
#include "surface.h"
#include "sys.h"
#include "textures.h"

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/


void
Mod_LoadTextures (dlump_t *l, model_t *mod)
{
	int				i, j, num, max, altmax, what;
	dmiptex_t		*dmiptex;
	texture_t		*tx, *tx2;
	texture_t		*anims[10];
	texture_t		*altanims[10];
	dmiptexlump_t	*m;
	brushhdr_t		*bheader = mod->brush;
	char			*base_name, *map_name, *tmp;
	char			*paths[11];
	image_t			*img, *img2;

	if (!l->filelen)
	{
		bheader->textures = NULL;
		return;
	}

	base_name = Zstrdup (tempzone, mod->name);
	if ((tmp = strrchr (base_name, '.')))
		*tmp = '\0';
	if ((tmp = strrchr (base_name, '/')))
		map_name = tmp + 1;
	else
		map_name = base_name;

	m = (dmiptexlump_t *) (mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	// Make room for the two r_notextures
	bheader->numtextures = m->nummiptex + 2;
	bheader->textures = Zone_Alloc (mod->zone,
			bheader->numtextures * sizeof (*bheader->textures));

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		dmiptex = (dmiptex_t *) ((Uint8 *) m + m->dataofs[i]);
		for (j = 0; j < MIPLEVELS; j++)
			dmiptex->offsets[j] = LittleLong (dmiptex->offsets[j]);

		if ((dmiptex->width & 15) || (dmiptex->height & 15))
			Host_EndGame ("Texture %s is not 16 aligned", dmiptex->name);
		tx = Zone_Alloc (mod->zone, sizeof (texture_t));
		tx->tex_idx = i;
		bheader->textures[i] = tx;

		memcpy (tx->name, dmiptex->name, sizeof (tx->name));

#define WHAT_SKY				0
#define WHAT_WATER				1
#define WHAT_NORMAL				2

		if (tx->name[0] == '*')
			what = WHAT_WATER;
		else if (!strncasecmp (tx->name, "sky", 3))
			what = WHAT_SKY;
		else
			what = WHAT_NORMAL;

		tx->width = dmiptex->width;
		tx->height = dmiptex->height;

		paths[0] = zasprintf (tempzone, "%s/%s", base_name, tx->name);
		paths[1] = zasprintf (tempzone, "override/%s/%s", map_name, tx->name);
		paths[2] = zasprintf (tempzone, "textures/%s/%s", map_name, tx->name);
		paths[3] = zasprintf (tempzone, "override/%s", tx->name);
		paths[4] = zasprintf (tempzone, "textures/%s", tx->name);
		paths[5] = NULL;
		img = Image_Load_Multi ((const char **) paths, TEX_NEED | TEX_KEEPRAW);
		tmp = img->file->name_base;

		if (what == WHAT_SKY)
			Sky_InitSky (img);
		else if (what == WHAT_WATER)
			tx->gl_texturenum = GLT_Load_image (tmp, img, NULL,
					TEX_FORCE | TEX_MIPMAP);
		else if (img->type == IMG_QPAL) {
			tx->gl_texturenum = GLT_Load_image (tmp, img, d_palette_base,
					TEX_MIPMAP | TEX_FORCE);
			tx->fb_texturenum = GLT_Load_image (va("%s_glow", tmp), img,
					d_palette_fb, TEX_MIPMAP);
		} else {
			tx->gl_texturenum = GLT_Load_image (tmp, img, d_palette_base,
					TEX_MIPMAP | TEX_FORCE);
			for (j = 0; paths[j]; j++)
				Zone_Free (paths[j]);
			paths[0] = zasprintf (tempzone, "%s/%s_glow", base_name, tx->name);
			paths[1] = zasprintf (tempzone, "%s/%s_luma", base_name, tx->name);
			paths[2] = zasprintf (tempzone, "override/%s/%s_glow", map_name, tx->name);
			paths[3] = zasprintf (tempzone, "override/%s/%s_luma", map_name, tx->name);
			paths[4] = zasprintf (tempzone, "textures/%s/%s_glow", map_name, tx->name);
			paths[5] = zasprintf (tempzone, "textures/%s/%s_luma", map_name, tx->name);
			paths[6] = zasprintf (tempzone, "override/%s_glow", tx->name);
			paths[7] = zasprintf (tempzone, "override/%s_luma", tx->name);
			paths[8] = zasprintf (tempzone, "textures/%s_glow", tx->name);
			paths[9] = zasprintf (tempzone, "textures/%s_luma", tx->name);
			paths[10] = NULL;
			img2 = Image_Load_Multi ((const char **) paths, TEX_MIPMAP | TEX_UPLOAD);
			if (img2) {
				tx->fb_texturenum = img2->texnum;
				Zone_Free (img2);
			}
		}
		Zone_Free (img->pixels);
		img->pixels = NULL;
		Zone_Free (img);

		for (j = 0; paths[j]; j++)
			Zone_Free (paths[j]);


		// Gross hack: Fix the shells Quake1 shells box
		/*
		if (!strcmp(dmiptex->name, "shot1sid")
				&& dmiptex->width == 32 && dmiptex->height == 32
				&& CRC_Block((Uint8 *)(dmiptex + 1),
					dmiptex->width * dmiptex->height) == 65393)
		{
			// This texture in b_shell1.bsp has some of the first 32 pixels
			// painted white.  They are invisible in software, but look really
			// ugly in GL. So we just copy 32 pixels from the bottom to make
			// it look nice.
			memcpy (mtdata, mtdata + (32 * 31), 32);
		}
		*/
	}

	// Add the r_notextures to the list
	bheader->notexture.tex_idx = i;
	bheader->textures[i++] = &bheader->notexture;
	bheader->notexture_water.tex_idx = i;
	bheader->textures[i++] = &bheader->notexture_water;

//
// sequence the animations
//
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = bheader->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			// already sequenced
			continue;

		// find the number of frames in the animation
		memset (anims, 0, sizeof (anims));
		memset (altanims, 0, sizeof (altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Host_EndGame ("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = bheader->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
				Host_EndGame ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_EndGame ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alt_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_EndGame ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alt_anims = anims[0];
		}
	}

	Zone_Free (base_name);
}

void
Mod_LoadLighting (dlump_t *l, model_t *mod)
{
	Uint		i;
	Uint8		*in, *out, *data;
	Uint8		d;
	char		litfilename[MAX_OSPATH];
	brushhdr_t	*bheader = mod->brush;

	bheader->lightdata = NULL;

	strlcpy_s (litfilename, mod->name);
	COM_StripExtension(litfilename, litfilename);
	COM_DefaultExtension(litfilename, ".lit", sizeof(litfilename));
	data = (Uint8 *) COM_LoadZoneFile (litfilename, false, mod->zone);

	if (data)
	{
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
		{
			i = LittleLong(((int *)data)[1]);
			if (i == 1)
			{
				Com_DPrintf("%s loaded\n", litfilename);
				bheader->lightdata = data + 8;
				return;
			}
			else
				Com_Printf("Unknown .lit file version (%d)\n", i);
		}
		else
			Com_Printf("Corrupt .lit file (old version?), ignoring\n");
	}

	if (!l->filelen)
		return;

	bheader->lightdata = Zone_Alloc (mod->zone, l->filelen*3);
	in = bheader->lightdata + l->filelen*2;
	out = bheader->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0; i < l->filelen; i++)
	{
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}

void
Mod_LoadTexinfo (dlump_t *l, model_t *mod)
{
	texinfo_t	*in;
	mtexinfo_t	*out;
	Uint32		i, j, count;
	Uint32		miptex;
	brushhdr_t	*bheader = mod->brush;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Host_EndGame ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	bheader->texinfo = out;
	bheader->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 8; j++)
			out->vecs[j / 4][j % 4] = 
				LittleFloat (in->vecs[j/4][j % 4]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		out->texture = NULL;
		if (bheader->textures)
		{
			if (miptex >= bheader->numtextures)
				Host_Error ("miptex >= bheader->numtextures");
			out->texture = bheader->textures[miptex];
		}
		if (!out->texture)
		{
			if (out->flags & TEX_SPECIAL)
				out->texture = &bheader->notexture_water;
			else
				out->texture = &bheader->notexture;
		}
	}
}

/*
================
Fills in s->texturemins[] and s->extents[]
================
*/
static void
CalcSurfaceExtents (msurface_t *s, model_t *mod)
{
	float		mins[2], maxs[2], val;
	int			j, e;
	int			i;
	vertex_t	*v;
	mtexinfo_t	*tex;
	int			bmins[2], bmaxs[2];
	brushhdr_t	*bheader = mod->brush;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = bheader->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &bheader->vertices[bheader->edges[e].v[0]];
		else
			v = &bheader->vertices[bheader->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->v[0] * tex->vecs[j][0] +
				v->v[1] * tex->vecs[j][1] +
				v->v[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(s->texinfo->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Host_Error ("Bad surface extents (extents[%d] is %d, flags is %d)",
						i, s->extents[i], s->texinfo->flags);
	}

	s->smax = bmaxs[0] - bmins[0] + 1, 
	s->tmax = bmaxs[1] - bmins[1] + 1;
}

void
Mod_LoadRFaces (dlump_t *l, model_t *mod)
{
	dface_t			*in;
	msurface_t		*out;
	int				i, j, count;
	brushhdr_t		*bheader = mod->brush;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Host_EndGame ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	bheader->surfaces = out;
	bheader->numsurfaces = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);
		out->flags = SURF_LIGHTMAP;

		if (out->numedges >= 256)
			Host_EndGame ("MOD_LoadBmodel: Too many edges in surface for %s",
					mod->name);

		if (LittleShort (in->side))
			out->flags |= SURF_PLANEBACK;

		out->plane = &bheader->planes[LittleShort (in->planenum)];

		out->texinfo = &bheader->texinfo[LittleShort (in->texinfo)];

		CalcSurfaceExtents (out, mod);

		if (!strncmp (out->texinfo->texture->name, "sky", 3))
		{
			// Skies are sky, and have no lightmap.
			out->flags |= SURF_SKY;
			out->flags &= ~SURF_LIGHTMAP;
		}
		else if ((out->texinfo->texture->name[0] == '*')
				|| (out->texinfo->flags & TEX_SPECIAL))
		{
			// Liquids are liquids, and have no lightmap.
			out->flags |= SURF_LIQUID;
			out->flags &= ~SURF_LIGHTMAP;
			for (j = 0; j < 2; j++)
			{
				out->extents[j] = 16384;
				out->texturemins[j] = -8192;
			}
		}

		if (out->flags & SURF_LIGHTMAP) {
			// Ok, this has a lightmap.
			memcpy(out->styles, in->styles, sizeof(Uint8) * MAXLIGHTMAPS);

			j = LittleLong (in->lightofs);
			if (j == -1)
				out->samples = NULL;
			else
				out->samples = bheader->lightdata + j*3;
		}
	}
}

void
Mod_MakeChains (model_t *mod)
{
	Uint			 i, first, last, tidx, count;
	msurface_t		*surf;
	chain_head_t	*chain;
	chain_item_t	*c_item;
	int				 stain_size;
	int				 schain_cur;
	int				*tchains_cur;
	int				 allocated[LIGHTBLOCK_WIDTH];
	int				*lchains_cur = NULL;
	brushhdr_t		*bheader = mod->brush;

	tchains_cur = Zone_Alloc(tempzone, sizeof(int) * bheader->numtextures);
	bheader->tex_chains = Zone_Alloc (mod->zone,
			bheader->numtextures * sizeof (chain_head_t));

	memset(&bheader->lightblock, 0, sizeof(lightblock_t));
	memset(&bheader->sky_chain, 0, sizeof(chain_head_t));
	memset(allocated, 0, sizeof(allocated));

	first = bheader->firstmodelsurface;
	last = bheader->nummodelsurfaces + first;

	bheader->numsets = 0;

	/*
	 * First pass:
	 *
	 * Surfaces with lightmaps get a block allocated for them, and we
	 * figure out how many blocks we need.
	 * We also allocate the stain buffer here.
	 *
	 * We count how many surfaces go in the sky chain and each texture
	 * chain.
	 *
	 * And we also convert from edges to polys.
	 */


	for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++)
	{
		if (surf->flags & SURF_LIGHTMAP)
		{
			if (!bheader->lightblock.num)
				bheader->lightblock.num++;
			if (!AllocLightBlockForSurf (allocated, bheader->lightblock.num - 1,
						surf, mod->zone))
			{
				bheader->lightblock.num++;
				memset(allocated, 0, sizeof(allocated));
				AllocLightBlockForSurf (allocated, bheader->lightblock.num - 1,
						surf, mod->zone);
			}

			stain_size = surf->smax * surf->tmax * 3;
			surf->stainsamples = Zone_Alloc(mod->zone,stain_size);
			memset(surf->stainsamples, 255, stain_size);
		}

		// Sky chain
		if (surf->flags & SURF_SKY)
			bheader->sky_chain.n_items++;
		else
			bheader->tex_chains[surf->texinfo->texture->tex_idx].n_items++;

		bheader->numsets += surf->numedges;
	}

	/*
	 * Now that we know how many lightblocks we need create them.
	 * (And let GL know about them.)
	 */
	if (bheader->lightblock.num)
	{
		bheader->lightblock.chains = Zone_Alloc (mod->zone,
				sizeof(chain_head_t) * bheader->lightblock.num);
		lchains_cur = Zone_Alloc (tempzone,
				sizeof(int) * bheader->lightblock.num);
		for (i = 0; i < bheader->lightblock.num; i++)
		{
			chain = &bheader->lightblock.chains[i];
			qglGenTextures (1, &chain->l_texnum);
			chain->flags = CHAIN_LIGHTMAP;
			qglBindTexture(GL_TEXTURE_2D, chain->l_texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, colorlights ? 3 : 1,
					LIGHTBLOCK_WIDTH, LIGHTBLOCK_HEIGHT, 0, gl_lightmap_format,
					GL_UNSIGNED_BYTE, templight);
		}
	}

	/*
	 * Now that we know how many items per chain,
	 * allocate the item structs.
	 */
	if (bheader->sky_chain.n_items)
	{
		bheader->sky_chain.n_items++;
		bheader->sky_chain.flags = CHAIN_SKY;
		bheader->sky_chain.items = Zone_Alloc(mod->zone,
				sizeof(chain_item_t) * bheader->sky_chain.n_items);
	}

	for (i = 0; i < bheader->numtextures; i++)
		if (bheader->tex_chains[i].n_items)
		{
			bheader->tex_chains[i].items = Zone_Alloc (mod->zone,
						sizeof(chain_item_t) * bheader->tex_chains[i].n_items);
		}

	if (bheader->numsets) {
		bheader->verts = Zone_Alloc(mod->zone, sizeof(vertex_t) * bheader->numsets);
		bheader->tcoords[0] = Zone_Alloc(mod->zone, sizeof(texcoord_t) * bheader->numsets);
		bheader->tcoords[1] = Zone_Alloc(mod->zone, sizeof(texcoord_t) * bheader->numsets);
	} else {
		bheader->verts = NULL;
		bheader->tcoords[0] = NULL;
		bheader->tcoords[1] = NULL;
	}

	/*
	 * Second pass:
	 * Count the number of items for each lightblock chain.
	 *
	 * Add the surfaces to the sky and texture chains.
	 */
	schain_cur = 0;
	count = 0;
	for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++)
	{
		BuildGLPolyFromEdges (surf, mod, &count);

		if (surf->flags & SURF_LIGHTMAP)
			bheader->lightblock.chains[surf->lightmap_texnum].n_items++;

		if (surf->flags & SURF_SKY)
		{
			c_item = &bheader->sky_chain.items[schain_cur++];
			c_item->surf = surf;
			c_item->head = &bheader->sky_chain;
		}
		else
		{
			tidx = surf->texinfo->texture->tex_idx;
			chain = &bheader->tex_chains[tidx];

			/*
			 * If this is the first item in the chain set the texture and
			 * flags.
			 */
			if (!chain->texture) {
				chain->texture = surf->texinfo->texture;
				// Some flags to let us know what kind of chain this is.
				if (chain->texture->gl_texturenum
						|| chain->texture->fb_texturenum)
					chain->flags |= CHAIN_NORMAL;
				// If we are a liquid then we should /not/ be drawn normally.
				if (surf->flags & SURF_LIQUID) {
					chain->flags |= CHAIN_LIQUID;
					chain->flags &= ~CHAIN_NORMAL;
				}
			}
			c_item = &chain->items[tchains_cur[tidx]++];
			c_item->surf = surf;
			c_item->head = chain;
		}
		surf->tex_chain = c_item;
	}

	/*
	 * Third pass: (ONLY if we have lightmapped surfaces!)
	 * Allocate the structs for the lightblock chain items.
	 * Add the surfaces to the lightmap chains.
	 */

	if (bheader->lightblock.num)
	{
		for (i = 0; i < bheader->lightblock.num; i++)
			bheader->lightblock.chains[i].items = Zone_Alloc(mod->zone, sizeof(chain_item_t) * bheader->lightblock.chains[i].n_items);

		for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++)
		{
			if (surf->flags & SURF_LIGHTMAP)
			{
				chain = &bheader->lightblock.chains[surf->lightmap_texnum];

				c_item = &chain->items[lchains_cur[surf->lightmap_texnum]++];
				c_item->surf = surf;
				c_item->head = chain;
				surf->light_chain = c_item;
			}
		}
		Zone_Free(lchains_cur);
	}
	Zone_Free(tchains_cur);

	// Vertex Buffer Objects, VERY cool stuff!
	if (gl_vbo) {
		GLuint		buf;
		size_t		size;
		Uint8		*cur = 0;

		size = (sizeof(vertex_t) + sizeof(texcoord_t) + sizeof(texcoord_t)) * bheader->numsets;

		qglGenBuffersARB(1, &buf);

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, buf);
		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, size, NULL, GL_STATIC_DRAW_ARB);

		bheader->vbo[VBO_VERTS].buffer = buf;
		bheader->vbo[VBO_VERTS].elements = 3;
		bheader->vbo[VBO_VERTS].size = sizeof (vertex_t) * bheader->numsets;
		bheader->vbo[VBO_VERTS].type = GL_FLOAT;
		bheader->vbo[VBO_VERTS].stride = sizeof (vertex_t);
		bheader->vbo[VBO_VERTS].ptr = cur;
		cur += bheader->vbo[VBO_VERTS].size;
		qglBufferSubDataARB (GL_ARRAY_BUFFER_ARB,
				(GLintptrARB) bheader->vbo[VBO_VERTS].ptr, 
				bheader->vbo[VBO_VERTS].size, bheader->verts);

		bheader->vbo[VBO_TC0].buffer = buf;
		bheader->vbo[VBO_TC0].elements = 2;
		bheader->vbo[VBO_TC0].size = sizeof (texcoord_t) * bheader->numsets;
		bheader->vbo[VBO_TC0].type = GL_FLOAT;
		bheader->vbo[VBO_TC0].stride = sizeof (texcoord_t);
		bheader->vbo[VBO_TC0].ptr = cur;
		cur += bheader->vbo[VBO_TC0].size;
		qglBufferSubDataARB (GL_ARRAY_BUFFER_ARB,
				(GLintptrARB) bheader->vbo[VBO_TC0].ptr, 
				bheader->vbo[VBO_TC0].size, bheader->tcoords[0]);

		bheader->vbo[VBO_TC1].buffer = buf;
		bheader->vbo[VBO_TC1].elements = 2;
		bheader->vbo[VBO_TC1].size = sizeof (texcoord_t) * bheader->numsets;
		bheader->vbo[VBO_TC1].type = GL_FLOAT;
		bheader->vbo[VBO_TC1].stride = sizeof (texcoord_t);
		bheader->vbo[VBO_TC1].ptr = cur;
		cur += bheader->vbo[VBO_TC1].size;
		qglBufferSubDataARB (GL_ARRAY_BUFFER_ARB,
				(GLintptrARB) bheader->vbo[VBO_TC1].ptr, 
				bheader->vbo[VBO_TC1].size, bheader->tcoords[1]);

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

void
Mod_RUnloadBrushModel (model_t *mod)
{
	Uint i;

	for (i = 0; i < mod->brush->lightblock.num; i++)
		qglDeleteTextures (1, &mod->brush->lightblock.chains[i].l_texnum);

	for (i = 0; i < (mod->brush->numtextures - 2); i++)	// Client only.
	{
		if (!mod->brush->textures[i])	// There may be some NULL textures.
			continue;

		if (mod->brush->textures[i]->gl_texturenum)
			GLT_Delete(mod->brush->textures[i]->gl_texturenum);
		if (mod->brush->textures[i]->fb_texturenum)
			GLT_Delete(mod->brush->textures[i]->fb_texturenum);
	}
}
