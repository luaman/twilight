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

#include <stdio.h>

#include "quakedef.h"
#include "bspfile.h"
#include "common.h"
#include "console.h"
#include "mathlib.h"
#include "mdfour.h"
#include "model.h"
#include "strlib.h"
#include "zone.h"

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

void        Mod_LoadSpriteModel (model_t *mod, void *buffer);
void        Mod_LoadBrushModel (model_t *mod, void *buffer);
void        Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t    *Mod_LoadModel (model_t *mod, qboolean crash);

Uint8       mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	256
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

texture_t   r_notexture_mip;

unsigned   *model_checksum;

/*
===============
Mod_Init
===============
*/
void
Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof (mod_novis));
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t    *
Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t    *node;
	float       d;
	mplane_t   *plane;

	if (!model || !model->nodes)
		SV_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1) {
		if (node->contents < 0)
			return (mleaf_t *) node;
		plane = node->plane;
		d = PlaneDiff(p, plane);
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;						// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
Uint8 *
Mod_DecompressVis (Uint8 *in, model_t *model)
{
	static Uint8 decompressed[MAX_MAP_LEAFS / 8];
	int         c;
	Uint8      *out;
	int         row;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

Uint8 *
Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void
Mod_ClearAll (void)
{
	int         i;
	model_t    *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

/*
==================
Mod_FindName

==================
*/
model_t    *
Mod_FindName (char *name)
{
	int         i;
	model_t    *mod;

	if (!name[0])
		SV_Error ("Mod_ForName: NULL name");

//
// search the currently loaded models
//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			SV_Error ("mod_numknown == MAX_MOD_KNOWN");
		strlcpy (mod->name, name, sizeof (mod->name));
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}


/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t    *
Mod_LoadModel (model_t *mod, qboolean crash)
{
	void       *d;
	unsigned   *buf;
	Uint8       stackbuf[1024];			// avoid dirtying the cache heap

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
			SV_Error ("Mod_NumForName: %s not found", mod->name);
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

	Mod_LoadBrushModel (mod, buf);

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t    *
Mod_ForName (char *name, qboolean crash)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

Uint8      *mod_base;



/*
=================
Mod_LoadVisibility
=================
*/
void
Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen) {
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName (l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void
Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen) {
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName (l->filelen, loadname);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void
Mod_LoadVertexes (lump_t *l)
{
	dvertex_t  *in;
	mvertex_t  *out;
	int         i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void
Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t   *in;
	dmodel_t   *out;
	int         i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {		// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void
Mod_LoadEdges (lump_t *l)
{
	dedge_t    *in;
	medge_t    *out;
	int         i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName ((count + 1) * sizeof (*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort (in->v[0]);
		out->v[1] = (unsigned short) LittleShort (in->v[1]);
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
	texinfo_t  *in;
	mtexinfo_t *out;
	int         i, j, count;
	int         miptex;
	float       len1, len2;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		if (len1 + len2 < 2 /* 0.001 */ )
			out->mipadjust = 1;
		else
			out->mipadjust = 1 / Q_floor ((len1 + len2) / 2);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures) {
			out->texture = &r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= loadmodel->numtextures)
				SV_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture) {
				out->texture = &r_notexture_mip;	// texture not found
				out->flags = 0;
			}
		}
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void
Mod_LoadFaces (lump_t *l)
{
	dface_t    *in;
	msurface_t *out;
	int         i, count, surfnum;
	int         planenum, side;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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

		out->texinfo = NULL;

		// lighting info
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong (in->lightofs);

		out->samples = NULL;
	}
}


/*
=================
Mod_SetParent
=================
*/
void
Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void
Mod_LoadNodes (lump_t *l)
{
	int         i, j, count, p;
	dnode_t    *in;
	mnode_t    *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void
Mod_LoadLeafs (lump_t *l)
{
	dleaf_t    *in;
	mleaf_t    *out;
	int         i, j, count, p;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort (in->firstmarksurface);
		out->nummarksurfaces = LittleShort (in->nummarksurfaces);

		p = LittleLong (in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void
Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int         i, count;
	hull_t     *hull;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = LittleLong (in->planenum);
		out->children[0] = LittleShort (in->children[0]);
		out->children[1] = LittleShort (in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void
Mod_MakeHull0 (void)
{
	mnode_t    *in, *child;
	dclipnode_t *out;
	int         i, j, count;
	hull_t     *hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - loadmodel->planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void
Mod_LoadMarksurfaces (lump_t *l)
{
	int         i, j, count;
	short      *in;
	msurface_t **out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort (in[i]);
		if (j >= loadmodel->numsurfaces)
			SV_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void
Mod_LoadSurfedges (lump_t *l)
{
	int         i, count;
	int        *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong (in[i]);
}

/*
=================
Mod_LoadPlanes
=================
*/
void
Mod_LoadPlanes (lump_t *l)
{
	int         i, j;
	mplane_t   *out;
	dplane_t   *in;
	int         count;
	int         bits;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		SV_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * 2 * sizeof (*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
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
	int         i, j;
	dheader_t  *header;
	dmodel_t   *bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *) buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		SV_Error
			("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
			 mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (Uint8 *) header;

	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

// load into heap

	mod->checksum = 0;
	mod->checksum2 = 0;

	// checksum all of the map, except for entities
	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		mod->checksum ^= Com_BlockChecksum (mod_base + 
			header->lumps[i].fileofs,  header->lumps[i].filelen);

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;

		mod->checksum2 ^= Com_BlockChecksum (mod_base +
			header->lumps[i].fileofs, header->lumps[i].filelen);
	}

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
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

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1) {	// duplicate the basic information
			char        name[10];

			snprintf (name, sizeof (name), "*%i", i + 1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}
