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
// models.c -- model loading and caching
static const char rcsid[] =
    "$Id$";

// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "crc.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "mdfour.h"
#include "strlib.h"
#include "sys.h"
#include "draw.h"

model_t	*loadmodel;
char	loadname[32];				// for hunk tags

void	Mod_LoadSpriteModel (model_t *mod, void *buffer);
void	Mod_LoadBrushModel (model_t *mod, void *buffer);
void	Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t	*Mod_LoadModel (model_t *mod, qboolean crash);

Uint8	mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

cvar_t	*gl_subdivide_size;

qboolean	isnotmap;

void	GL_SubdivideSurface (msurface_t *fa);
void	GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

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
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	void		*d;
	unsigned	*buf;
	Uint8		stackbuf[1024];			// avoid dirtying the cache heap

	if (!mod->needload) {
		if (mod->type == mod_alias) {
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		} else
			return mod;					// not cached at all
	}
//
// load the file
//
	buf =
		(unsigned *) COM_LoadStackFile (mod->name, stackbuf, sizeof (stackbuf));
	if (!buf) {
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}
//
// allocate a new model
//
	COM_FileBase (mod->name, loadname);

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
        if (pixels[i] >= 224)
            return true;

    return false;
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

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
	miptex_t		*mt;
	texture_t		*tx, *tx2;
	texture_t		*anims[10];
	texture_t		*altanims[10];
	dmiptexlump_t	*m;

	if (!l->filelen) {
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) (mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures =
		Hunk_AllocName (m->nummiptex * sizeof (*loadmodel->textures), loadname);

	for (i = 0; i < m->nummiptex; i++) {
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *) ((Uint8 *) m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width * mt->height * (85 / 64);
		tx = Hunk_AllocName (sizeof (texture_t) + pixels, loadname);
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof (tx->name));
		tx->width = mt->width;
		tx->height = mt->height;
		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] =
				mt->offsets[j] + sizeof (texture_t) - sizeof (miptex_t);
		// the pixels immediately follow the structures
		memcpy (tx + 1, mt + 1, pixels);

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width==32 && mt->height==32
			&& CRC_Block((Uint8*)(mt+1), mt->width*mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (tx+1, (Uint8 *)(tx+1) + 32*31, 32);
		}

		if (!strncmp (mt->name, "sky", 3))
			R_InitSky (tx);
		else {
			if (mt->name[0] == '*')	// we don't brighten turb textures
				tx->gl_texturenum = GL_LoadTexture (mt->name, tx->width, tx->height, (Uint8 *)(tx+1), true, false, 8);
			else {
				tx->gl_texturenum = GL_LoadTexture (mt->name, tx->width, tx->height, (Uint8 *)(tx+1), true, false, 8);

				if (Img_HasFullbrights((Uint8 *)(tx+1), tx->width*tx->height)) {
					tx->fb_texturenum = GL_LoadTexture (va("@fb_%s", mt->name), tx->width, tx->height,
									(Uint8 *) (tx + 1), true, 2, 8);
				}
			}
		}
	}

//
// sequence the animations
//
	for (i = 0; i < m->nummiptex; i++) {
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;					// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof (anims));
		memset (altanims, 0, sizeof (altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9') {
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		} else if (max >= 'A' && max <= 'J') {
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		} else
			Sys_Error ("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++) {
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9') {
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			} else if (num >= 'A' && num <= 'J') {
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			} else
				Sys_Error ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++) {
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++) {
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alternate_anims = anims[0];
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
	if (!gl_colorlights->value)
	{
		if (!l->filelen) {
			loadmodel->lightdata = NULL;
			return;
		}
		loadmodel->lightdata = Hunk_AllocName (l->filelen, loadname);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
	}
	else {
		int i;
		Uint8 *in, *out, *data;
		Uint8 d;
		char litfilename[MAX_OSPATH];

		loadmodel->lightdata = NULL;

		strcpy(litfilename, loadmodel->name);
		COM_StripExtension(litfilename, litfilename);
		COM_DefaultExtension(litfilename, ".lit");
		data = (Uint8 *) COM_LoadHunkFile (litfilename);

		if (data)
		{
			if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Com_DPrintf("%s loaded\n", litfilename);
					loadmodel->lightdata = data + 8;
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

		loadmodel->lightdata = Hunk_AllocName (l->filelen*3, litfilename);
		in = loadmodel->lightdata + l->filelen*2;
		out = loadmodel->lightdata;
		memcpy (in, mod_base + l->fileofs, l->filelen);
		for (i = 0; i < l->filelen; i++)
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
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
	int			i, j, count;
	unsigned	miptex;
	float		len1, len2;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 8; j++)
			out->vecs[j / 4][j % 4] = 
				LittleFloat (in->vecs[j/4][j % 4]);
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2) * 0.5f;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures) {
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip;	// texture not found
				out->flags = 0;
			}
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
	int			i, j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int			bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

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
		bmins[i] = Q_floor (mins[i] / 16);
		bmaxs[i] = Q_ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Sys_Error ("Bad surface extents");
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
	dface_t		*in;
	msurface_t	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);
		out->flags = 0;

		planenum = LittleShort (in->planenum);
		side = LittleShort (in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);

		// lighting info

		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong (in->lightofs);

		if (i == -1)
			out->samples = NULL;
		else if (!gl_colorlights->value)
			out->samples = loadmodel->lightdata + i;
		else if (gl_colorlights->value)
			out->samples = loadmodel->lightdata + i*3;

		// set the drawing flags flag

		if (!strncmp (out->texinfo->texture->name, "sky", 3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}

		if (out->texinfo->texture->name[0] == '*')	// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++) {
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}

	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void
Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	Uint32		i, j;
	dheader_t	*header;
	dmodel_t	*bm;
	char		name[10];

	loadmodel->type = mod_brush;

	header = (dheader_t *) buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Sys_Error
			("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
			 mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (Uint8 *) header;

	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// checksum all of the map, except for entities
	mod->checksum = 0;
	mod->checksum2 = 0;

	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		mod->checksum ^= Com_BlockChecksum (mod_base + header->lumps[i].fileofs,
											header->lumps[i].filelen);

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		mod->checksum2 ^=
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

	//
	// set up the submodels (FIXME: this is confusing)
	//
	for (i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (!isnotmap && (i < mod->numsubmodels - 1))
		{
			// duplicate the basic information
			strncpy (name, va("*%i", i + 1), sizeof(name));
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

Uint8	player_8bit_texels[320 * 200];
int		player_8bit_width = 296, player_8bit_height = 194;

float	aliasbboxmin[3], aliasbboxmax[3];

/*
=================
Mod_LoadAliasFrame
=================
*/
void *
Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	int				i;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *) pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		if (frame->bboxmin.v[i] < aliasbboxmin[i])
			aliasbboxmin[i] = (float)frame->bboxmin.v[i];
		if (frame->bboxmax.v[i] > aliasbboxmax[i])
			aliasbboxmax[i] = (float)frame->bboxmax.v[i];
	}

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *) pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *
Mod_LoadAliasGroup (void *pin, maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		if (frame->bboxmin.v[i] < aliasbboxmin[i])
			aliasbboxmin[i] = (float)frame->bboxmin.v[i];
		if (frame->bboxmax.v[i] > aliasbboxmax[i])
			aliasbboxmax[i] = (float)frame->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *) (pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		poseverts[posenum] = (trivertx_t *) ((daliasframe_t *) ptemp + 1);
		posenum++;

		ptemp =
			(trivertx_t *) ((daliasframe_t *) ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

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
Mod_FloodFillSkin (Uint8 * skin, int skinwidth, int skinheight)
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

/*
===============
Mod_LoadAllSkins
===============
*/
void *
Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int						i, j, k;
	char					name[32];
	unsigned				s;
	Uint8					*skin;
	daliasskingroup_t		*pinskingroup;
	int						groupskins;
	daliasskininterval_t	*pinskinintervals;

	skin = (Uint8 *) (pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (!strcmp (loadmodel->name, "progs/player.mdl")) {
				if (s > sizeof (player_8bit_texels))
					Sys_Error ("Player skin too large");
				memcpy (player_8bit_texels, (Uint8 *) (pskintype + 1), s);
				player_8bit_width = pheader->skinwidth;
				player_8bit_height = pheader->skinheight;
			}
			snprintf (name, sizeof (name), "%s_%i", loadmodel->name, i);
			pheader->gl_texturenum[i][0] =
				pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] =
				pheader->gl_texturenum[i][3] =
				GL_LoadTexture (name, pheader->skinwidth,
								pheader->skinheight, (Uint8 *) (pskintype + 1),
								true, false, 8);

			if (Img_HasFullbrights((Uint8 *)(pskintype + 1),	pheader->skinwidth*pheader->skinheight))
				pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] =
					GL_LoadTexture (va("@fb_%s", name), pheader->skinwidth, 
					pheader->skinheight, (Uint8 *)(pskintype + 1), true, 2, 8);
			else
				pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = 0;

			pskintype = (daliasskintype_t *) ((Uint8 *) (pskintype + 1) + s);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *) pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);

			pskintype = (void *) (pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++) {
				Mod_FloodFillSkin (skin, pheader->skinwidth,
								   pheader->skinheight);
				snprintf (name, sizeof (name), "%s_%i_%i", loadmodel->name, i,
						  j);
				pheader->gl_texturenum[i][j & 3] =
					GL_LoadTexture (name, pheader->skinwidth,
									pheader->skinheight, (Uint8 *) (pskintype),
									true, false, 8);

				if (Img_HasFullbrights((Uint8 *)(pskintype),	pheader->skinwidth*pheader->skinheight))
					pheader->fb_texturenum[i][j&3] =
					GL_LoadTexture (va("@fb_%s", name), pheader->skinwidth, 
					pheader->skinheight, (Uint8 *)(pskintype), true, 2, 8);
				else
					pheader->fb_texturenum[i][j&3] = 0;

				pskintype = (daliasskintype_t *) ((Uint8 *) (pskintype) + s);
			}
			k = j;
			for ( /* */ ; j < 4; j++)
				pheader->gl_texturenum[i][j & 3] =
					pheader->gl_texturenum[i][j - k];
		}
	}

	return (void *) pskintype;
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

	// Common
	{ "progs/v_", 8, FLAG_NOSHADOW|FLAG_NO_IM_FORM },
	{ "progs/eyes.mdl", 0, FLAG_DOUBLESIZE },
	{ "progs/armor.mdl", 0, 0 },
	{ "progs/g_", 8, 0 },

	// end of list
	{ NULL }
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
	int					i, j;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, end, total;

	if (!strcmp (loadmodel->name, "progs/player.mdl") ||
		!strcmp (loadmodel->name, "progs/eyes.mdl")) {
		unsigned short crc;
		char        st[40];

		crc = CRC_Block (buffer, com_filesize);

		snprintf (st, sizeof (st), "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo,
							 !strcmp (loadmodel->name,
										"progs/player.mdl") ? pmodel_name :
							 emodel_name, st, MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			snprintf (st, sizeof (st), "setinfo %s %d",
					  !strcmp (loadmodel->name,
								 "progs/player.mdl") ? pmodel_name :
					  emodel_name, (int) crc);
			SZ_Print (&cls.netchan.message, st);
		}
	}

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *) buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				   mod->name, version, ALIAS_VERSION);

	mod->modflags = Mod_FindModelFlags(mod->name);


//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = sizeof (aliashdr_t)
		+ (LittleLong (pinmodel->numframes) - 1) * sizeof (pheader->frames[0]);
	pheader = Hunk_AllocName (size, loadname);

	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++) {
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t *) &pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *) pskintype;

	for (i = 0; i < pheader->numverts; i++) {
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *) &pinstverts[pheader->numverts];

	for (i = 0; i < pheader->numtris; i++) {
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			triangles[i].vertindex[j] =
				LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *) &pintriangles[pheader->numtris];

	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 255;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = 0;

	for (i = 0; i < numframes; i++) {
		aliasframetype_t frametype;

		frametype = LittleLong (pframetype->type);
 
		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		} else {
			pframetype = (daliasframetype_t *)
				Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
		}
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	for (i = 0; i < 3; i++)
	{
		mod->mins[i] = aliasbboxmin[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = aliasbboxmax[i] * pheader->scale[i] + pheader->scale_origin[i];
	}

	// Vic: automatically detect models 
	// that should not be interpolated
	if (mod->numframes == 1)
		if (!mod->modflags & FLAG_NO_IM_ANIM)
			mod->modflags |= FLAG_NO_IM_ANIM;

	// 
	// build the draw lists
	// 
	GL_MakeAliasModelDisplayLists (mod, pheader);

//
// move the complete, relocatable alias model to the cache
//  
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
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
	int				width, height, size, origin[2];
	char			name[64];

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t), loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf (name, sizeof (name), "%s_%i", loadmodel->name, framenum);
	pspriteframe->gl_texturenum =
		GL_LoadTexture (name, width, height, (Uint8 *) (pinframe + 1), true,
						true, 8);

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

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
								   (numframes -
									1) * sizeof (pspritegroup->frames[0]),
								   loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval<=0");

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
		Sys_Error ("%s has wrong version number "
				   "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

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

	mod->type = mod_sprite;
}

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

qboolean	used[8192];

/* the command list holds counts and s/t values that are valid for every frame */
int			commands[8192];

/*	all frames will have their vertexes rearranged and expanded
	so they are in the order expected by the command list */
int			vertexorder[8192];
int			numorder;

int			stripverts[128];
int			striptris[128];
int			stripcount;

/*
================
StripLength
================
*/
int
StripLength (int starttri, int startv)
{
	int			m1, m2;
	int			j;
	mtriangle_t	*last, *check;
	int			k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 2) % 3];
	m2 = last->vertindex[(startv + 1) % 3];

	/* look for a matching triangle */
  nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1];
		 j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			/* this is the next part of the fan */

			/* if we can't use this triangle, this tristrip is done */
			if (used[j])
				goto done;

			/* the new edge */
			if (stripcount & 1)
				m2 = check->vertindex[(k + 2) % 3];
			else
				m1 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = check->vertindex[(k + 2) % 3];
			striptris[stripcount++] = j;

			used[j] = 2;
			goto nexttri;
		}
	}
  done:

	/* clear the temp used flags */
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int
FanLength (int starttri, int startv)
{
	int			m1, m2;
	int			j;
	mtriangle_t	*last, *check;
	int			k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 0) % 3];
	m2 = last->vertindex[(startv + 2) % 3];


	/* look for a matching triangle */
  nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1];
		 j < pheader->numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			/* this is the next part of the fan */

			/* if we can't use this triangle, this tristrip is done */
			if (used[j])
				goto done;

			/* the new edge */
			m2 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = m2;
			striptris[stripcount++] = j;

			used[j] = 2;
			goto nexttri;
		}
	}
  done:

	/* clear the temp used flags */
	for (j = starttri + 1; j < pheader->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
int
BuildTris (void)
{
	int		i, j, k;
	int		startv;
	float	s, t;
	int		len, bestlen;
	int		besttype = 0;
	int		bestverts[MAXALIASVERTS];
	int		besttris[MAXALIASVERTS];
	int		type;
	int		numcommands = 0;

	/* 
		build tristrips
	*/
	numorder = 0;
	memset (used, 0, sizeof (used));
	for (i = 0; i < pheader->numtris; i++) {
		/* pick an unused triangle and start the trifan */
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0; type < 2; type++)
/* type = 1; */
		{
			for (startv = 0; startv < 3; startv++) {
				if (type == 1)
					len = StripLength (i, startv);
				else
					len = FanLength (i, startv);
				if (len > bestlen) {
					besttype = type;
					bestlen = len;
					for (j = 0; j < bestlen + 2; j++)
						bestverts[j] = stripverts[j];
					for (j = 0; j < bestlen; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		/* mark the tris on the best strip as used */
		for (j = 0; j < bestlen; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen + 2);
		else
			commands[numcommands++] = -(bestlen + 2);

		for (j = 0; j < bestlen + 2; j++) {
			/* emit a vertex into the reorder buffer */
			k = bestverts[j];
			vertexorder[numorder++] = k;

			/* emit s/t coords into the commands stream */
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;	/* on back side */
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*(float *) &commands[numcommands++] = s;
			*(float *) &commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		/* end of list marker */

	Com_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder,
				 numcommands);

	return numcommands;
}


/*
================
GL_MakeAliasModelDisplayLists
================
*/
void
GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	int			i, j;
	int			*cmds;
	trivertx_t	*verts;
	int			numcommands = BuildTris ();	/* trifans or lists */

	/* save the data out */

	hdr->poseverts = numorder;

	cmds = Hunk_Alloc (numcommands * 4);
	hdr->commands = (Uint8 *) cmds - (Uint8 *) hdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_Alloc (hdr->numposes * hdr->poseverts
						* sizeof (trivertx_t));
	hdr->posedata = (Uint8 *) verts - (Uint8 *) hdr;
	for (i = 0; i < hdr->numposes; i++)
		for (j = 0; j < numorder; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}
