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

#include "quakedef.h"
#include "crc.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "gl_textures.h"
#include <math.h>
#include "surface.h"
#include "sky.h"
#include "mdfour.h"
#include "dyngl.h"

extern model_t	*loadmodel;
extern qboolean isnotmap;
extern texture_t *r_notexture;
extern texture_t *r_notexture_water;


void Mod_UnloadBrushModel (model_t *mod);

void Mod_LoadBrushModel (model_t *mod, void *buffer);
extern model_t *Mod_LoadModel (model_t *mod, qboolean crash);
extern void Mod_UnloadModel (model_t *mod);


cvar_t	*gl_subdivide_size;

/*
===============
Mod_Init_Cvars
===============
*/
void
Mod_Init_Cvars (void)
{
	gl_subdivide_size = Cvar_Get ("gl_subdivide_size", "128", CVAR_ARCHIVE, NULL);
}

qboolean 
Img_HasFullbrights (Uint8 *pixels, int size)
{
    int	i;

    for (i = 0; i < size; i++)
        if (pixels[i] >= FIRST_FB)
            return true;

    return false;
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

brushhdr_t	*bheader;
Uint8	*mod_base;


/*
=================
Mod_LoadTextures
=================
*/
void
Mod_LoadTextures (lump_t *l)
{
	int				i, j, pixels, num, max, altmax;
	miptex_t		*dmiptex;
	texture_t		*tx, *tx2;
	texture_t		*anims[10];
	texture_t		*altanims[10];
	dmiptexlump_t	*m;
	Uint8			*mtdata;

	if (!l->filelen)
	{
		bheader->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) (mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	// Make room for the two r_notextures
	bheader->numtextures = m->nummiptex + 2;
	bheader->textures = Zone_Alloc (loadmodel->extrazone,
			bheader->numtextures * sizeof (*bheader->textures));

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		dmiptex = (miptex_t *) ((Uint8 *) m + m->dataofs[i]);
		dmiptex->width = LittleLong (dmiptex->width);
		dmiptex->height = LittleLong (dmiptex->height);
		for (j = 0; j < MIPLEVELS; j++)
			dmiptex->offsets[j] = LittleLong (dmiptex->offsets[j]);

		if ((dmiptex->width & 15) || (dmiptex->height & 15))
			Host_EndGame ("Texture %s is not 16 aligned", dmiptex->name);
		pixels = dmiptex->width * dmiptex->height * (85 / 64);
		tx = Zone_Alloc (loadmodel->extrazone, sizeof (texture_t));
		tx->texnum = i;
		bheader->textures[i] = tx;

		memcpy (tx->name, dmiptex->name, sizeof (tx->name));
		tx->width = dmiptex->width;
		tx->height = dmiptex->height;

		mtdata = (Uint8 *)(dmiptex) + dmiptex->offsets[0];

		// Gross hack: Fix the shells Quake1 shells box
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

		if (!strncmp (dmiptex->name, "sky", 3))
			R_InitSky (tx, mtdata);
		else
		{
			if (dmiptex->name[0] == '*')
				// we don't brighten turb textures
				tx->gl_texturenum = GL_LoadTexture (dmiptex->name, tx->width,
						tx->height, (Uint8 *)(mtdata), TEX_MIPMAP, 8);
			else
			{
				tx->gl_texturenum = GL_LoadTexture (dmiptex->name, tx->width,
						tx->height, (Uint8 *)(mtdata), TEX_MIPMAP, 8);

				if (Img_HasFullbrights ((Uint8 *)(mtdata),
							tx->width * tx->height))
				{
					tx->fb_texturenum = GL_LoadTexture (
							va("@fb_%s", dmiptex->name),
							tx->width, tx->height, (Uint8 *)(mtdata),
							TEX_MIPMAP|TEX_FBMASK, 8);
				}
			}
		}
	}

	// Add the r_notextures to the list
	bheader->textures[i++] = r_notexture;
	bheader->textures[i++] = r_notexture_water;

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
}

/*
=================
Mod_LoadLighting
=================
*/
void
Mod_LoadLighting (lump_t *l)
{
	Uint		i;
	Uint8		*in, *out, *data;
	Uint8		d;
	char		litfilename[MAX_OSPATH];

	bheader->lightdata = NULL;

	strcpy(litfilename, loadmodel->name);
	COM_StripExtension(litfilename, litfilename);
	COM_DefaultExtension(litfilename, ".lit");
	data = (Uint8 *) COM_LoadHunkFile (litfilename, false);

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

	bheader->lightdata = Zone_Alloc (loadmodel->extrazone, l->filelen*3);
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

/*
=================
Mod_LoadTexinfo
=================
*/
void
Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t	*in;
	mtexinfo_t	*out;
	Uint32		i, j, count;
	Uint32		miptex;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Host_EndGame ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (loadmodel->extrazone, count * sizeof (*out));

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
				out->texture = r_notexture_water;
			else
				out->texture = r_notexture;
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void
CalcSurfaceExtents (msurface_t *s)
{
	float		mins[2], maxs[2], val;
	int			j, e;
	Uint		i;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int			bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = bheader->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &bheader->vertexes[bheader->edges[e].v[0]];
		else
			v = &bheader->vertexes[bheader->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
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
			Host_Error (va ("Bad surface extents (s->extents[%i] is %i, s->texinfo->flags is %i)",
						i, s->extents[i], s->texinfo->flags));
	}

	s->smax = bmaxs[0] - bmins[0] + 1, 
	s->tmax = bmaxs[1] - bmins[1] + 1;
}

/*
=================
Mod_LoadFaces
=================
*/
void
Mod_LoadFaces (lump_t *l)
{
	dface_t			*in;
	msurface_t		*out;
	int				i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (loadmodel->extrazone, count * sizeof (*out));

	bheader->surfaces = out;
	bheader->numsurfaces = count;

	for (i = 0; i < count; i++) {
		out[i].firstedge = LittleLong (in[i].firstedge);
		out[i].numedges = LittleShort (in[i].numedges);
		out[i].flags = 0;

		if (out[i].numedges >= 256)
			Host_EndGame ("MOD_LoadBmodel: Too many edges in surface for %s",
					loadmodel->name);

		if (LittleShort (in[i].side))
			out[i].flags |= SURF_PLANEBACK;

		out[i].plane = bheader->planes + LittleShort (in[i].planenum);

		out[i].texinfo = bheader->texinfo + LittleShort (in[i].texinfo);

		CalcSurfaceExtents (&out[i]);

		// No lightmap.
		if (out[i].texinfo->flags & TEX_SPECIAL)
			out[i].flags |= SURF_NOLIGHTMAP;

		if (out[i].texinfo->texture->name[0] == '*') {
			// Liquids are liquids, have no lightmap, and are subdivided.
			out[i].flags |= SURF_LIQUID | SURF_NOLIGHTMAP | SURF_SUBDIVIDE;
			for (j = 0; j < 2; j++)
			{
				out[i].extents[j] = 16384;
				out[i].texturemins[j] = -8192;
			}
		} else if (!strncmp (out[i].texinfo->texture->name, "sky", 3)) {
			// Skies are sky, have no lightmap, and are subdivided.
			out[i].flags |= SURF_SKY | SURF_NOLIGHTMAP | SURF_SUBDIVIDE;
		}

		if (!(out[i].flags & SURF_NOLIGHTMAP)) {
			// Ok, this has a lightmap.
			for (j = 0; j < MAXLIGHTMAPS; j++)
				out[i].styles[j] = in[i].styles[j];

			j = LittleLong (in[i].lightofs);
			if (j == -1)
				out[i].samples = NULL;
			else
				out[i].samples = bheader->lightdata + j*3;
		}
	}
}

extern Uint8 templight[LIGHTBLOCK_WIDTH * LIGHTBLOCK_HEIGHT * 4];
extern int lightmap_bytes;				// 1, 3, or 4
extern int gl_lightmap_format;
extern qboolean colorlights;
extern qboolean AllocLightBlockForSurf (int *allocated, int num, msurface_t *surf, memzone_t *zone);

/*
=================
Mod_MakeChains
=================
*/
void
Mod_MakeChains ()
{
	Uint			 i, first, last, tnum;
	msurface_t		*surf;
	chain_head_t	*chain;
	chain_item_t	*c_item;
	int				 stain_size;
	int				 schain_count, schain_cur;
	int				*tchains_count, *tchains_cur;
	lightblock_t	*lblock;
	int				 allocated[LIGHTBLOCK_WIDTH];
	int				*lchains_cur = NULL;

	tchains_count = Zone_Alloc(tempzone, sizeof(int) * bheader->numtextures);
	tchains_cur = Zone_Alloc(tempzone, sizeof(int) * bheader->numtextures);
	lblock = &bheader->lightblock;

	memset(&bheader->lightblock, 0, sizeof(lightblock_t));
	memset(&bheader->sky_chain, 0, sizeof(chain_head_t));
	memset(allocated, 0, sizeof(allocated));

	first = bheader->firstmodelsurface;
	last = first + bheader->nummodelsurfaces;

	bheader->tex_chains = Zone_Alloc (loadmodel->extrazone,
			bheader->numtextures * sizeof (*bheader->tex_chains));

	schain_count = 0;

	for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++) {
		/*
		 * If we are lightmapped then we need to do some stuff:
		 * We need to allocate the lightmap space in the lightmap scrap.
		 * We need to allocate a stain sample.
		 * And we need to add ourselves to the proper light chain.
		 * (Used for non-mtex and for early updating of the lightmaps.)
		 */
		if (!(surf->flags & SURF_NOLIGHTMAP)) {
			if (!lblock->num)
				lblock->num++;
			if (!AllocLightBlockForSurf (allocated, lblock->num - 1, surf,
						loadmodel->extrazone))
			{
				lblock->num++;
				memset(allocated, 0, sizeof(allocated));
				AllocLightBlockForSurf (allocated, lblock->num - 1, surf,
						loadmodel->extrazone);
			}

			stain_size = surf->smax * surf->tmax * 3;
			surf->stainsamples = Zone_Alloc(loadmodel->extrazone,stain_size);
			memset(surf->stainsamples, 255, stain_size);
		}

		// Sky chain
		if (surf->flags & SURF_SKY)
			schain_count++;
		else
			tchains_count[surf->texinfo->texture->texnum]++;

		if (surf->flags & SURF_SUBDIVIDE)
			BuildSubdividedGLPolyFromEdges (surf, loadmodel);
		else
			BuildGLPolyFromEdges (surf, loadmodel);
	}

	if (lblock->num) {
		lblock->chains = Zone_Alloc (loadmodel->extrazone,
				sizeof(chain_head_t) * lblock->num);
		lchains_cur = Zone_Alloc (tempzone, sizeof(int) * lblock->num);
		for (i = 0; i < lblock->num; i++) {
			chain = &lblock->chains[i];
			qglGenTextures(1, &chain->l_texnum);
			chain->flags = CHAIN_LIGHTMAP;
			qglBindTexture(GL_TEXTURE_2D, chain->l_texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			memset(templight, 64, sizeof(templight));
			qglTexImage2D (GL_TEXTURE_2D, 0, colorlights ? 3 : 1,
					LIGHTBLOCK_WIDTH, LIGHTBLOCK_HEIGHT, 0, gl_lightmap_format,
					GL_UNSIGNED_BYTE, templight);
		}
	}

	if (schain_count) {
		chain = &bheader->sky_chain;
		chain->n_items = schain_count;
		chain->flags = CHAIN_SKY;
		chain->items = Zone_Alloc(loadmodel->extrazone,
				sizeof(chain_item_t) * chain->n_items);
	}
	for (i = 0; i < bheader->numtextures; i++)
		if (tchains_count[i]) {
			chain = bheader->tex_chains[i] =
				Zone_Alloc (loadmodel->extrazone, sizeof(chain_head_t));
			chain->n_items = tchains_count[i];
			chain->items = Zone_Alloc (loadmodel->extrazone,
						sizeof(chain_item_t) * chain->n_items + 1);
		}
	schain_cur = 0;

	for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++) {
		if (!(surf->flags & SURF_NOLIGHTMAP))
			lblock->chains[surf->lightmap_texnum].n_items++;

		if (surf->flags & SURF_SKY) {
			chain = &bheader->sky_chain;

			c_item = &chain->items[schain_cur++];
			c_item->surf = surf;
			c_item->head = chain;
		} else {
			tnum = surf->texinfo->texture->texnum;
			chain = bheader->tex_chains[tnum];

			/*
			 * If this is the first item in the chain set the texture and
			 * flags.
			 */
			if (!chain->texture) {
				chain->texture = surf->texinfo->texture;
				// Some flags to let us know what kind of chain this is.
				if (chain->texture->gl_texturenum)
					chain->flags |= CHAIN_NORMAL;
				if (chain->texture->fb_texturenum)
					chain->flags |= CHAIN_FB;
				// If we are a liquid then we should /not/ be drawn normally.
				if (surf->flags & SURF_LIQUID) {
					chain->flags |= CHAIN_LIQUID;
					chain->flags &= ~CHAIN_NORMAL;
				}
			}
			c_item = &chain->items[tchains_cur[tnum]++];
			c_item->surf = surf;
			c_item->head = chain;
		}
		surf->tex_chain = c_item;
	}

	if (lblock->num) {
		for (i = 0; i < lblock->num; i++)
			lblock->chains[i].items = Zone_Alloc(loadmodel->extrazone,
					sizeof(chain_item_t) * lblock->chains[i].n_items);

		for (i = first, surf = &bheader->surfaces[i]; i < last; i++, surf++) {
			if (!(surf->flags & SURF_NOLIGHTMAP)) {
				chain = &lblock->chains[surf->lightmap_texnum];

				c_item = &chain->items[lchains_cur[surf->lightmap_texnum]++];
				c_item->surf = surf;
				c_item->head = chain;
				surf->light_chain = c_item;
			}
		}
		Zone_Free(lchains_cur);
	}
	Zone_Free(tchains_cur);
	Zone_Free(tchains_count);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void
Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	Uint32		i;
	dheader_t	*header;
	dmodel_t	*bm;
	model_t		*first;
	char		name[10];

	first = mod;
	mod->extrazone = Zone_AllocZone(mod->name);
	mod->extra.brush = Zone_Alloc(mod->extrazone, sizeof(brushhdr_t));
	mod->type = mod_brush;
	bheader = mod->extra.brush;

	header = (dheader_t *) buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Host_EndGame
			("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
			 mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (Uint8 *) header;

	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// checksum all of the map, except for entities
	bheader->checksum = 0;
	bheader->checksum2 = 0;

	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		bheader->checksum ^= Com_BlockChecksum (mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		bheader->checksum2 ^=
			Com_BlockChecksum (mod_base + header->lumps[i].fileofs,
					header->lumps[i].filelen);
	}

	// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;					// regular and alternate animation

//	bheader->lightblock = Zone_Alloc(mod->extrazone, sizeof(lightblock_t));

	//
	// set up the submodels (FIXME: this is confusing)
	//
	for (i = 0; i < mod->extra.brush->numsubmodels; i++) {
		int			l;
		Uint		k, j;
		float		dist, modelyawradius, modelradius, *vec;
		msurface_t	*surf;

		bm = &bheader->submodels[i];

		bheader->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			bheader->hulls[j].firstclipnode = bm->headnode[j];
			bheader->hulls[j].lastclipnode = bheader->numclipnodes - 1;
		}

		bheader->firstmodelsurface = bm->firstface;
		bheader->nummodelsurfaces = bm->numfaces;

		Mod_MakeChains ();

		mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
		mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
		modelyawradius = 0;
		modelradius = 0;

		// Calculate the bounding boxes, don't trust what the model says.
		surf = &bheader->surfaces[bheader->firstmodelsurface];
		for (j = 0; j < bheader->nummodelsurfaces; j++, surf++) {
			for (k = 0; k < surf->numedges; k++) {
				l = bheader->surfedges[k + surf->firstedge];
				if (l > 0)
					vec = bheader->vertexes[bheader->edges[l].v[0]].position;
				else
					vec = bheader->vertexes[bheader->edges[-l].v[1]].position;
				if (mod->normalmins[0] > vec[0]) mod->normalmins[0] = vec[0];
				if (mod->normalmins[1] > vec[1]) mod->normalmins[1] = vec[1];
				if (mod->normalmins[2] > vec[2]) mod->normalmins[2] = vec[2];
				if (mod->normalmaxs[0] < vec[0]) mod->normalmaxs[0] = vec[0];
				if (mod->normalmaxs[1] < vec[1]) mod->normalmaxs[1] = vec[1];
				if (mod->normalmaxs[2] < vec[2]) mod->normalmaxs[2] = vec[2];
				dist = vec[0]*vec[0]+vec[1]*vec[1];
				if (modelyawradius < dist)
					modelyawradius = dist;
				dist += vec[2]*vec[2];
				if (modelradius < dist)
					modelradius = dist;
			}
		}

		modelyawradius = sqrt(modelyawradius);
		modelradius = sqrt(modelradius);
		mod->yawmins[0] = mod->yawmins[1] = -modelyawradius;
		mod->yawmaxs[0] = mod->yawmaxs[1] = modelyawradius;
		mod->yawmins[2] = mod->normalmins[2];
		mod->yawmaxs[2] = mod->normalmaxs[2];
		VectorSet(mod->rotatedmins, -modelradius, -modelradius, -modelradius);
		VectorSet(mod->rotatedmaxs, modelradius, modelradius, modelradius);

		bheader->numleafs = bm->visleafs;
		mod->needload = false;

		if (!isnotmap && (i < bheader->numsubmodels - 1))
		{
			// duplicate the basic information
			strncpy (name, va("*%d", i + 1), sizeof(name));
			loadmodel = Mod_FindName (name);
			if (!loadmodel->needload)
				Mod_UnloadModel (loadmodel);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			loadmodel->extra.brush = Zone_Alloc(first->extrazone, sizeof(brushhdr_t));
			*loadmodel->extra.brush = *mod->extra.brush;
			bheader = loadmodel->extra.brush;
			mod = loadmodel;
			mod->extra.brush->is_submodel = true;
		}
	}
}

/*
=================
Mod_UnloadBrushModel
=================
*/
void
Mod_UnloadBrushModel (model_t *mod)
{
	model_t	*sub;
	Uint	i;

	if (mod->extra.brush->is_submodel)
		return;

	for (i = 1; i < mod->extra.brush->numsubmodels; i++) {
		sub = Mod_FindName(va("*%d", i));
		Mod_UnloadModel(sub);
	}

	for (i = 0; i < (mod->extra.brush->numtextures - 2); i++) {
		if (!mod->extra.brush->textures[i])	// There may be some NULL textures.
			continue;

		if (mod->extra.brush->textures[i]->gl_texturenum)
			GL_Delete_Texture(mod->extra.brush->textures[i]->gl_texturenum);
		if (mod->extra.brush->textures[i]->fb_texturenum)
			GL_Delete_Texture(mod->extra.brush->textures[i]->fb_texturenum);
	}
	Zone_FreeZone (&mod->extrazone);
}
