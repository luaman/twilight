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
// model.c -- model loading and caching
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "console.h"
#include "crc.h"
#include "cvar.h"
#include "glquake.h"
#include "host.h"
#include "strlib.h"
#include "sys.h"

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

void        Mod_LoadBrushModel (model_t *mod, void *buffer);
model_t    *Mod_LoadModel (model_t *mod, qboolean crash);

Uint8       mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

qboolean    isnotmap;


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
Mod_Init

Caches the data if needed
===============
*/
void       *
Mod_Extradata (model_t *mod)
{
	void       *r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
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
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1) {
		if (node->contents < 0)
			return (mleaf_t *) node;
		plane = node->plane;
		d = PlaneDiff (p, plane);
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
	static Uint8	decompressed[MAX_MAP_LEAFS / 8];
	int				c;
	Uint8		   *out;
	int				row;

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
		Sys_Error ("Mod_ForName: NULL name");

//
// search the currently loaded models
//
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name))
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void
Mod_TouchModel (char *name)
{
	model_t    *mod;

	mod = Mod_FindName (name);

	if (!mod->needload) {
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		if (p == -1 || !loadmodel->visdata)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}

		if (isnotmap) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_DONTWARP;
		}
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort (in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);
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


//=============================================================================

/*
================
Mod_Print
================
*/
void
Mod_Print (void)
{
	int         i;
	model_t    *mod;

	Con_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		Con_Printf ("%8p : %s\n", mod->cache.data, mod->name);
	}
}
