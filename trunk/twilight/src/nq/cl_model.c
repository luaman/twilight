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
#include "client.h"
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

extern model_t	*loadmodel;
extern qboolean isnotmap;

static void Mod_UnloadSpriteModel (model_t *mod);
static void Mod_UnloadAliasModel (model_t *mod);
static void Mod_UnloadBrushModel (model_t *mod);

void	Mod_LoadSpriteModel (model_t *mod, void *buffer);
void	Mod_LoadBrushModel (model_t *mod, void *buffer);
void	Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t	*Mod_LoadModel (model_t *mod, qboolean crash);

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


/*
==================
Mod_UnloadModel

Unloads a model, and frees it.
==================
*/
void
Mod_UnloadModel (model_t *mod)
{
	if (mod->needload)
		return;

	switch (mod->type) {
		case mod_alias:
			Mod_UnloadAliasModel (mod);
			break;

		case mod_sprite:
			Mod_UnloadSpriteModel (mod);
			break;

		case mod_brush:
			Mod_UnloadBrushModel (mod);
			break;
	}
	memset(mod, 0, sizeof(model_t));
	mod->needload = true;
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned	*buf;
	Uint8		stackbuf[1024];			// avoid dirtying the cache heap

	if (!mod->needload) {
		return mod;					// not cached at all
	}
//
// load the file
//
	buf = (unsigned *) COM_LoadStackFile (mod->name, stackbuf,
			sizeof (stackbuf), true);
	if (!buf) {
		if (crash)
			Host_EndGame ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;

	switch (LittleLong (*(unsigned *) buf)) {
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel (mod, buf);
			break;

		default:
			Mod_LoadBrushModel (mod, buf);
			break;
	}

	return mod;
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

		if (!strncmp (dmiptex->name, "sky", 3) && !isDedicated)
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


extern int lightmap_bytes;				// 1, 3, or 4
extern int AllocLightBlock (lightblock_t *block, int w, int h, int *x, int *y);

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

chain_head_t *
Mod_RemakeChain (chain_head_t *in, chain_head_t *out)
{
	Uint			 i;
	chain_head_t	*chain;
	chain_item_t	*c, *next;

	if (!in->n_items)
		return NULL;

	if (!out)
		chain = Zone_Alloc (loadmodel->extrazone, sizeof(chain_head_t));
	else
		chain = out;

	*chain = *in;

	chain->items = Zone_Alloc (loadmodel->extrazone,
			sizeof(chain_item_t) * chain->n_items);
	for (c = in->items, i = 0; c; c = next, i++) {
		next = c->next;
		chain->items[i].head = chain;
		chain->items[i].surf = c->surf;
		chain->items[i].next = &chain->items[i + 1];
		if (c->surf->tex_chain == c)
			c->surf->tex_chain = &chain->items[i];
		if (c->surf->light_chain == c)
			c->surf->light_chain = &chain->items[i];
		Zone_Free(c);
	}
	chain->items[i].next = NULL;

	return chain;
}

/*
=================
Mod_MakeChains
=================
*/
void
Mod_MakeChains ()
{
	Uint			 i, first, last, l_texnum;
	msurface_t		*surfs = bheader->surfaces, *s;
	chain_head_t	*chain;
	chain_item_t	*c_item, *c;
	int				 stain_size;
	chain_head_t	lchains[MAX_LIGHTMAPS];
	chain_head_t	schain;
	chain_head_t	*tchains;

	tchains = Zone_Alloc(tempzone, sizeof(chain_head_t) * bheader->numtextures);

	memset (&lchains, 0, sizeof(lchains));
	memset (&schain, 0, sizeof(schain));

	first = bheader->firstmodelsurface;
	last = first + bheader->nummodelsurfaces;

	bheader->tex_chains = Zone_Alloc (loadmodel->extrazone,
			bheader->numtextures * sizeof (*bheader->tex_chains));

	for (i = first; i < last; i++) {
		/*
		 * If we are lightmapped then we need to do some stuff:
		 * We need to allocate the lightmap space in the lightmap scrap.
		 * We need to allocate a stain sample.
		 * And we need to add ourselves to the proper light chain.
		 * (Used for non-mtex and for early updating of the lightmaps.)
		 */
		if (!(surfs[i].flags & SURF_NOLIGHTMAP)) {
			/* We need to be aligned to 4 bytes due to GL. */
			surfs[i].alignedwidth = surfs[i].smax;
			while ((surfs[i].alignedwidth * lightmap_bytes) & 3)
				surfs[i].alignedwidth++;

			surfs[i].lightmap_texnum = AllocLightBlock(&bheader->lightblock, 
					surfs[i].alignedwidth, surfs[i].tmax,
					&surfs[i].light_s, &surfs[i].light_t);

			stain_size = surfs[i].smax * surfs[i].tmax * 3;
			surfs[i].stainsamples = Zone_Alloc(loadmodel->extrazone,stain_size);
			memset(surfs[i].stainsamples, 255, stain_size);

			chain = &lchains[surfs[i].lightmap_texnum];
			chain->n_items++;

			c_item = Zone_Alloc(tempzone, sizeof(*c_item));
			c_item->next = chain->items;
			chain->items = c_item;
			c_item->surf = &surfs[i];
			c_item->head = chain;
			surfs[i].light_chain = c_item;
		}

		// Sky chain
		if (surfs[i].flags & SURF_SKY) {
			chain = &schain;
			chain->n_items++;

			chain->flags |= CHAIN_SKY;
			chain->flags &= ~CHAIN_NORMAL;

			c_item = Zone_Alloc(tempzone, sizeof(*c_item));
			c_item->next = chain->items;
			chain->items = c_item;
			c_item->surf = &surfs[i];
			c_item->head = chain;
		} else {
			// Add to the texture chain.
			chain = &tchains[surfs[i].texinfo->texture->texnum];
			// Lazy allocation of the chain structs.
			if (!chain->n_items++) {
				chain->texture = surfs[i].texinfo->texture;
				// Some flags to let us know what kind of chain this is.
				if (chain->texture->gl_texturenum)
					chain->flags |= CHAIN_NORMAL;
				if (chain->texture->fb_texturenum)
					chain->flags |= CHAIN_FB;
			}
			// If we are a liquid then we should /not/ be drawn normally.
			if (surfs[i].flags & SURF_LIQUID) {
				chain->flags |= CHAIN_LIQUID;
				chain->flags &= ~CHAIN_NORMAL;
			}

			/*
			 * If there are other textures on this chain then try to group us
			 * with other surfaces which use the same lightmap scrap.
			 */
			c_item = Zone_Alloc(tempzone, sizeof(*c_item));
			c_item->next = chain->items;
			c_item->surf = &surfs[i];
			c_item->head = chain;

			if ((c = chain->items)) {
				while (c->next && (c->surf->lightmap_texnum != surfs[i].lightmap_texnum))
					c = c->next;
				c_item->next = c->next;
				c->next = c_item;
			} else {
				chain->items = c_item;
			}
		}
		surfs[i].tex_chain = c_item;

		if (surfs[i].flags & SURF_SUBDIVIDE)
			BuildSubdividedGLPolyFromEdges (&surfs[i], loadmodel);
		else
			BuildGLPolyFromEdges (&surfs[i], loadmodel);
	}

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (lchains[i].n_items) {
			s = lchains[i].items[0].surf;
			l_texnum = bheader->lightblock.chains[s->lightmap_texnum].l_texnum;
			Mod_RemakeChain (&lchains[i], 
					&bheader->lightblock.chains[s->lightmap_texnum]);
			bheader->lightblock.chains[s->lightmap_texnum].l_texnum = l_texnum;
		}
	}

	if (schain.n_items) {
		Mod_RemakeChain (&schain, &bheader->sky_chain);
	}

	for (i = 0; i < bheader->numtextures; i++) {
		if (tchains[i].n_items) {
			s = tchains[i].items[0].surf;
			bheader->tex_chains[s->texinfo->texture->texnum] =
				Mod_RemakeChain (&tchains[i], NULL);
		}
	}

	Zone_Free(tchains);
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

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

qboolean	vseams[MAXALIASVERTS];
int			vremap[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];
Uint		numinverts;

model_t	*player_model;

vec3_t	bboxmin, bboxmax;
float	bboxradius, bboxyawradius;

static inline void
Mod_CheckMinMaxVerts8 (Uint8 t[3])
{
	vec3_t	v;
	float	dist;
	int		i;

	for (i = 0; i < 3; i++) {
		v[i] = t[i] * pheader->scale[i] + pheader->scale_origin[i];

		if (bboxmin[i] > v[i])
			bboxmin[i] = v[i];
		if (bboxmax[i] < v[i])
			bboxmax[i] = v[i];
	}

	dist = DotProduct2(v, v);
	if (bboxyawradius < dist)
		bboxyawradius = dist;
	dist = DotProduct(v, v);
	if (bboxradius < dist)
		bboxradius = dist;
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void *
Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame, model_t *mod)
{
	trivertx_t		*pinframe;
	Uint			i, j;
	daliasframe_t	*pdaliasframe;
	maliaspose_t	*pose;

	pdaliasframe = (daliasframe_t *) pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	frame->poses = Zone_Alloc(mod->extrazone, sizeof(maliaspose_t));

	pose = frame->poses;

	frame->interval = 1;
	frame->poses->normal_indices = Zone_Alloc(mod->extrazone, pheader->numverts * sizeof(Uint8));
	frame->poses->vertices = Zone_Alloc(mod->extrazone, pheader->numverts * sizeof(avertex_t));


	for (i = 0; i < numinverts; i++) {
		j = vremap[i];
		pose->normal_indices[j] = pinframe->lightnormalindex;
		VectorCopy(pinframe->v, pose->vertices[j].v);
		Mod_CheckMinMaxVerts8 (pose->vertices[j].v);

		if (vseams[i]) {
			pose->normal_indices[j + 1] = pinframe->lightnormalindex;
			VectorCopy(pinframe->v, pose->vertices[j + 1].v);
		}

		pinframe++;
	}

	return (void *) pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *
Mod_LoadAliasGroup (Uint8 *datapointer, maliasframedesc_t *frame, model_t *mod)
{
	daliasgroup_t		*pingroup;
	Uint				j;
	int					i, k, numframes;
	daliasinterval_t	*pin_intervals;
	maliaspose_t		*pose;
	daliasframe_t		*pinframe;
	trivertx_t			*vertices;

	pingroup = (daliasgroup_t *) datapointer;
	datapointer += sizeof (daliasgroup_t);

	numframes = LittleLong (pingroup->numframes);

	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *) datapointer;
	datapointer += sizeof(daliasinterval_t) * numframes;

	frame->interval = LittleFloat (pin_intervals->interval);

	frame->poses = Zone_Alloc(mod->extrazone, numframes * sizeof(maliaspose_t));

	for (i = 0; i < numframes; i++) {
		pose = &frame->poses[i];

		pinframe = (daliasframe_t *) datapointer;
		datapointer += sizeof(daliasframe_t);
		vertices = (trivertx_t *) datapointer;
		datapointer += sizeof(trivertx_t) * numinverts;

		pose->normal_indices = Zone_Alloc(mod->extrazone, pheader->numverts * sizeof(Uint8));
		pose->vertices = Zone_Alloc(mod->extrazone, pheader->numverts * sizeof(avertex_t));

		for (j = 0; j < numinverts; j++) {
			k = vremap[j];
			pose->normal_indices[k] = vertices[j].lightnormalindex;
			VectorCopy(vertices[j].v, pose->vertices[k].v);
			Mod_CheckMinMaxVerts8 (pose->vertices[k].v);

			if (vseams[j]) {
				pose->normal_indices[k + 1] = vertices[j].lightnormalindex;
				VectorCopy(vertices[j].v, pose->vertices[k + 1].v);
			}
		}
	}

	return (void *) datapointer;
}

//=========================================================

/*
===============
Mod_LoadAllSkins
===============
*/
Uint8 *
Mod_LoadAllSkins (model_t *mod, Uint8 *datapointer, qboolean load)
{
	int					i, height, width, numgroups, numskins, numskins2;
	unsigned int		s;
	float				interval;
	daliasskintype_t	*pskintype;

	numgroups = pheader->numskins;

	if (numgroups < 1 || numgroups > MAX_SKINS)
		Host_EndGame("Mod_LoadAliasModel: Invalid # of skins: %d\n", numgroups);

	height = pheader->skinheight;
	width = pheader->skinwidth;
	s = width * height;

	if (load)
		pheader->skins = Zone_Alloc(mod->extrazone, sizeof(skin_t) * numgroups);

	for (i = 0; i < numgroups; i++) {
		pskintype = (daliasskintype_t *) datapointer;
		datapointer += sizeof(daliasskintype_t);
		if (pskintype->type) {
			daliasskingroup_t	*group = (daliasskingroup_t *) datapointer;
			daliasskininterval_t *time = (daliasskininterval_t *) datapointer;
			datapointer += sizeof(daliasskingroup_t);

			numskins2 = numskins = group->numskins;
			interval = time->interval;
			datapointer += sizeof(daliasskininterval_t) * numskins;
			if ((interval - 0.00005) <= 0) {
				Com_DPrintf("Broken alias model skin group: %s %d, %d %f\n",
						mod->name, i, numskins, interval);
				interval = 1;
			}
		} else {
			numskins2 = numskins = 1;
			interval = 1;
		}

		if (load) {
			GLT_Skin_Parse(datapointer, &pheader->skins[i], pheader,
					va("%s/skins/%d", mod->name, i), width, height, numskins,
					interval);
		}
		datapointer += s * numskins2;
	}

	return datapointer;
}


//=========================================================================
typedef struct
{
	char	*name;
	int		len;
	int		flags;
} mflags_t;

mflags_t modelflags[] =
{
	// Regular Quake
	{ "progs/flame.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW|FLAG_TORCH1 },
	{ "progs/flame2.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW|FLAG_TORCH2 },
	{ "progs/fire.mdl", 0, FLAG_NOSHADOW },
	{ "progs/bolt.mdl", 10, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/laser.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/gib", 9, FLAG_NOSHADOW },
	{ "progs/missile.mdl", 0, FLAG_NOSHADOW },
	{ "progs/grenade.mdl", 0, FLAG_NOSHADOW },
	{ "progs/spike.mdl", 0, FLAG_NOSHADOW },
	{ "progs/s_spike.mdl", 0, FLAG_NOSHADOW },
	{ "progs/zom_gib.mdl", 0, FLAG_NOSHADOW },
	{ "progs/player.mdl", 0, FLAG_PLAYER },
	{ "progs/v_spike.mdl", 0, FLAG_NO_IM_FORM },
	{ "progs/boss.mdl", 0, FLAG_NOSHADOW },
	{ "progs/oldone.mdl", 0, FLAG_NOSHADOW },

	// keys and runes do not cast shadows
	{ "progs/w_s_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/m_s_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/b_s_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/w_g_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/m_g_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/b_g_key.mdl", 0, FLAG_NOSHADOW },
	{ "progs/end.mdl", 9, FLAG_NOSHADOW },

	// Dissolution of Eternity
	{ "progs/lavalball.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/beam.mdl", 0, FLAG_NOSHADOW },
	{ "progs/fireball.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/lspike.mdl", 0, FLAG_NOSHADOW },
	{ "progs/plasma.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/sphere.mdl", 0, FLAG_FULLBRIGHT|FLAG_NOSHADOW },
	{ "progs/statgib.mdl", 13, FLAG_NOSHADOW },
	{ "progs/wrthgib.mdl", 13, FLAG_NOSHADOW },
	{ "progs/eelgib.mdl", 0, FLAG_NOSHADOW },
	{ "progs/eelhead.mdl", 0, FLAG_NOSHADOW },
	{ "progs/timegib.mdl", 0, FLAG_NOSHADOW },
	{ "progs/merveup.mdl", 0, FLAG_NOSHADOW },
	{ "progs/rockup.mdl", 0, FLAG_NOSHADOW },
	{ "progs/rocket.mdl", 0, FLAG_NOSHADOW },

	// Shrak
	{ "progs/shelcase.mdl", 0, FLAG_NOSHADOW },
	{ "progs/flare.mdl", 0, FLAG_NOSHADOW },
	{ "progs/bone.mdl", 0, FLAG_NOSHADOW },
	{ "progs/spine.mdl", 0, FLAG_NOSHADOW },
	{ "progs/spidleg.mdl", 0, FLAG_NOSHADOW },
	{ "progs/gor1_gib.mdl", 0, FLAG_NOSHADOW },
	{ "progs/gor2_gib.mdl", 0, FLAG_NOSHADOW },
	{ "progs/xhairo", 12, FLAG_FULLBRIGHT|FLAG_NOSHADOW|FLAG_DOUBLESIZE },
	{ "progs/bluankey.mdl", 0, FLAG_NOSHADOW },
	{ "progs/bluplkey.mdl", 0, FLAG_NOSHADOW },
	{ "progs/gldankey.mdl", 0, FLAG_NOSHADOW },
	{ "progs/gldplkey.mdl", 0, FLAG_NOSHADOW },
	{ "progs/chip", 10, FLAG_NOSHADOW },

	// Common
	{ "progs/v_nail.mdl", 0, FLAG_NO_IM_ANIM|FLAG_NO_IM_FORM|FLAG_NOSHADOW },
	{ "progs/v_light.mdl", 0, FLAG_NO_IM_ANIM|FLAG_NO_IM_FORM|FLAG_NOSHADOW },
	{ "progs/v_", 8, FLAG_NOSHADOW|FLAG_NO_IM_FORM },
	{ "progs/eyes.mdl", 0, FLAG_EYES|FLAG_DOUBLESIZE },

	// end of list
	{ NULL, 0, 0 }
};

static int nummflags = sizeof(modelflags) / sizeof(modelflags[0]) - 1;

int
Mod_FindModelFlags(char *name)
{
	int	i;

	for (i = 0; i < nummflags; i++)
	{
		if (modelflags[i].len > 0) {
			if (!strncmp(name, modelflags[i].name, modelflags[i].len))
				return modelflags[i].flags;
		}
		else {
			if (!strcmp(name, modelflags[i].name))
				return modelflags[i].flags;
		}
	}
	
	return 0;
}

/*
=================
Mod_LoadAliasModel
=================
*/
void
Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	Uint				i, numframes, numseams;
	int					j, k, v;
	float				s, t;
	mdl_t				*pinmodel;
	Uint8				*datapointer, *skindata;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	Uint				version;
	daliasframetype_t	*pframetype;
	qboolean			typeSingle = false;

	// Clear the arrays to NULL.
	memset (vseams, 0, sizeof(vseams));
	memset (vremap, 0, sizeof(vremap));

	datapointer = buffer;

	pinmodel = (mdl_t *) datapointer;
	datapointer += sizeof(mdl_t);

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_EndGame ("%s has wrong version number (%i should be %i)",
				   mod->name, version, ALIAS_VERSION);

	mod->modflags = Mod_FindModelFlags(mod->name);


//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	mod->extrazone = Zone_AllocZone(mod->name);
	pheader = Zone_Alloc(mod->extrazone, sizeof(aliashdr_t));

	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Host_EndGame ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	numinverts = LittleLong (pinmodel->numverts);

	if (numinverts <= 0)
		Host_Error ("model %s has no vertices", mod->name);

	if (numinverts > MAXALIASVERTS)
		Host_Error ("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Host_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n",
				numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++) {
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}
	if (mod->modflags & FLAG_EYES)
		pheader->scale_origin[2] -= 30;

	if (mod->modflags & FLAG_DOUBLESIZE)
		VectorScale(pheader->scale, 2, pheader->scale);

//
// load the skins
//
	skindata = datapointer;
	datapointer = Mod_LoadAllSkins (mod, skindata, false);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *) datapointer;
	datapointer += sizeof(stvert_t) * numinverts;

	for (i = 0, numseams = 0; i < numinverts; i++) {
		vremap[i] = i + numseams;
		if ((vseams[i] = LittleLong (pinstverts[i].onseam)))
			numseams++;
	}

	pheader->numverts = numinverts + numseams;

	if (pheader->numverts > MAX_VERTEX_ARRAYS)
		Host_Error ("Model %s too big for vertex arrays! (%d %d)", mod->name,
				pheader->numverts, MAX_VERTEX_ARRAYS);

	pheader->tcarray = Zone_Alloc(mod->extrazone, pheader->numverts * sizeof(astvert_t));
	for (i = 0, j = 0; i < numinverts; i++) {
		j = vremap[i];

		s = LittleLong (pinstverts[i].s) + 0.5;
		t = LittleLong (pinstverts[i].t) + 0.5;
		pheader->tcarray[j].s = s / pheader->skinwidth;
		pheader->tcarray[j].t = t / pheader->skinheight;
		if (vseams[i]) {	// Duplicate for back texture.
			s += pheader->skinwidth / 2;
			pheader->tcarray[j + 1].s = s / pheader->skinwidth;
			pheader->tcarray[j + 1].t = t / pheader->skinheight;
		}
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *) datapointer;
	datapointer += sizeof(dtriangle_t) * pheader->numtris;

	pheader->triangles = Zone_Alloc(mod->extrazone,
			pheader->numtris * sizeof(mtriangle_t));

	for (i = 0; i < pheader->numtris; i++) {
		int facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			v = LittleLong (pintriangles[i].vertindex[j]);
			k = vremap[v];
			if (vseams[v] && !facesfront)
				pheader->triangles[i].vertindex[j] = k + 1;
			else
				pheader->triangles[i].vertindex[j] = k;
		}
	}

/*
 * load the frames
 */
	bboxmin[0] = bboxmin[1] = bboxmin[2] = 1073741824;
	bboxmax[0] = bboxmax[1] = bboxmax[2] = -1073741824;

	pheader->frames = Zone_Alloc(mod->extrazone, numframes* sizeof(maliasframedesc_t));

	for (i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		pframetype = (daliasframetype_t *) datapointer;
		datapointer += sizeof(daliasframetype_t);

		frametype = LittleLong (pframetype->type);
 
		if (frametype == ALIAS_SINGLE) {
			typeSingle = true;
			datapointer =
				Mod_LoadAliasFrame (datapointer, &pheader->frames[i], mod);
		} else {
			datapointer =
				Mod_LoadAliasGroup (datapointer, &pheader->frames[i], mod);
		}
	}

	mod->type = mod_alias;

	bboxyawradius = sqrt(bboxyawradius);
	bboxradius = sqrt(bboxradius);

	for (i = 0; i < 3; i++) {
		mod->normalmins[i] = bboxmin[i];
		mod->normalmaxs[i] = bboxmax[i];
		mod->rotatedmins[i] = -bboxradius;
		mod->rotatedmaxs[i] = bboxradius;
	}
	VectorSet(mod->yawmins, -bboxyawradius, -bboxyawradius, mod->normalmins[2]);
	VectorSet(mod->yawmaxs, bboxyawradius, bboxyawradius, mod->normalmaxs[2]);

/*
 * Actually load the skins, now that we have the triangle and texcoord data.
 */
	Mod_LoadAllSkins (mod, skindata, true);

	// Don't bother to lerp models with only one frame.
	if ((numframes == 1) && typeSingle)
		mod->modflags |= FLAG_NO_IM_ANIM;

	if (mod->modflags & FLAG_PLAYER)
		player_model = mod;

	mod->extra.alias = pheader;
}

/*
=================
Mod_UnloadAliasModel
=================
*/
void
Mod_UnloadAliasModel (model_t *mod)
{
	Uint					i;

	pheader = mod->extra.alias;

	for (i = 0; i < pheader->numskins; i++)
		GLT_Delete_Skin(&pheader->skins[i]);

	if (player_model == mod)
		player_model = NULL;

	Zone_FreeZone (&mod->extrazone);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void *
Mod_LoadSpriteFrame (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	int				width, height, size, origin[2], i;

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Zone_Alloc (loadmodel->extrazone, sizeof (mspriteframe_t));

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = origin[0] + width;

	i = max(sq(pspriteframe->left), sq(pspriteframe->right));
	i += max(sq(pspriteframe->up), sq(pspriteframe->down));
	if (bboxradius < i)
		bboxradius = i;

	pspriteframe->gl_texturenum =
		GL_LoadTexture (va("%s_%i", loadmodel->name, framenum), width, height,
				(Uint8 *) (pinframe + 1), TEX_MIPMAP|TEX_ALPHA, 8);

	return (void *) ((Uint8 *) pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void *
Mod_LoadSpriteGroup (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Zone_Alloc (loadmodel->extrazone, sizeof (mspritegroup_t) +
								   (numframes -
									1) * sizeof (pspritegroup->frames[0]));

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Zone_Alloc (loadmodel->extrazone, numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_EndGame ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		ptemp =
			Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i],
								 framenum * 100 + i);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void
Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	pin = (dsprite_t *) buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Host_EndGame ("%s has wrong version number "
				   "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	mod->extrazone = Zone_AllocZone(mod->name);
	mod->extra.sprite = psprite = Zone_Alloc(mod->extrazone, size);

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	bboxradius = 0;

//
// load the frames
//
	if (numframes < 1)
		Host_EndGame ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *) (pin + 1);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteFrame (pframetype + 1,
									 &psprite->frames[i].frameptr, i);
		} else {
			pframetype = (dspriteframetype_t *)
				Mod_LoadSpriteGroup (pframetype + 1,
									 &psprite->frames[i].frameptr, i);
		}
	}

	VectorSet (mod->normalmins, -bboxradius, -bboxradius, -bboxradius);
	VectorCopy (mod->normalmins, mod->rotatedmins);
	VectorCopy (mod->normalmins, mod->yawmins);
	VectorSet (mod->normalmaxs, bboxradius, bboxradius, bboxradius);
	VectorCopy (mod->normalmins, mod->rotatedmaxs);
	VectorCopy (mod->normalmins, mod->yawmaxs);

	mod->type = mod_sprite;
}

/*
=================
Mod_UnloadSpriteModel
=================
*/
static void
Mod_UnloadSpriteModel (model_t *mod)
{
	Uint				i, j;
	msprite_t			*psprite;
	mspritegroup_t		*pspritegroup;

	psprite = mod->extra.sprite;
	for (i = 0; i < psprite->numframes; i++) {
		if (psprite->frames[i].type == SPR_SINGLE) {
			GL_Delete_Texture (psprite->frames[i].frameptr->gl_texturenum);
		} else {
			pspritegroup = (mspritegroup_t *) psprite->frames[i].frameptr;
			for (j = 0; j < pspritegroup->numframes; j++) {
				GL_Delete_Texture (pspritegroup->frames[j]->gl_texturenum);
			}
		}
	}

	Zone_FreeZone (&mod->extrazone);
}


qboolean
Mod_MinsMaxs (model_t *mod, vec3_t org, vec3_t ang,
		vec3_t mins, vec3_t maxs)
{
#define CheckAngle(x)   (!(!x || (x == 180.0)))
		if (CheckAngle(ang[0]) || CheckAngle(ang[2])) {
			VectorAdd (org, mod->rotatedmins, mins);
			VectorAdd (org, mod->rotatedmaxs, maxs);
			return true;
		} else if (CheckAngle(ang[2])) {
			VectorAdd (org, mod->yawmins, mins);
			VectorAdd (org, mod->yawmaxs, maxs);
			return true;
		} else {
			VectorAdd (org, mod->normalmins, mins);
			VectorAdd (org, mod->normalmaxs, maxs);
			return false;
		}
#undef CheckAngle
}
