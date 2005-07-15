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
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include <math.h>
#include "mdfour.h"
#include "model.h"
#include "mod_brush.h"
#include "fs/fs.h"

Uint8	*mod_base;

static Uint8 mod_novis[MAX_MAP_LEAFS / 8];

void
Mod_Brush_Init (void)
{
	memset (mod_novis, 0xff, sizeof (mod_novis));
}


mleaf_t    *
Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t    *node;
	float       d;
	mplane_t   *plane;

	if (!model || !model->brush->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->brush->nodes;
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


static Uint8 *
Mod_DecompressVis (Uint8 *in, model_t *model)
{
	static Uint8	decompressed[MAX_MAP_LEAFS / 8];
	int				c;
	Uint8		   *out;
	int				row;

	row = (model->brush->numleafs + 7) >> 3;
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
	if (leaf == model->brush->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

static void
Mod_LoadVisibility (dlump_t *l, model_t *mod)
{
	fs_file_t	*file;
	SDL_RWops	*rw;
	char		*base_name, *tmp;

	l = l;

	mod->brush->visdata = NULL;

	base_name = Zstrdup (tempzone, mod->name);
	if ((tmp = strrchr (base_name, '.')))
		*tmp = '\0';

	if (!(file = FS_FindFile (va("%s.vis", base_name))))
		goto end;
	if (!(rw = file->open (file, 0)))
		goto end;

	mod->brush->visdata = Zone_Alloc (mod->zone, file->len);
	SDL_RWread (rw, mod->brush->visdata, file->len, 1);
	SDL_RWclose (rw);
end:
	Zone_Free (base_name);
}


static void
Mod_LoadEntities (model_t *mod)
{
	fs_file_t	*file;
	SDL_RWops	*rw;
	char		*base_name, *tmp;

	mod->brush->entities = NULL;

	base_name = Zstrdup (tempzone, mod->name);
	if ((tmp = strrchr (base_name, '.')))
		*tmp = '\0';

	if (!(file = FS_FindFile (va("%s.ent", base_name))))
		goto end;
	if (!(rw = file->open (file, 0)))
		goto end;

	mod->brush->entities = Zone_Alloc (mod->zone, file->len);
	SDL_RWread (rw, mod->brush->entities, file->len, 1);
	SDL_RWclose (rw);
end:
	Zone_Free (base_name);
}


static void
Mod_LoadVertexes (dlump_t *l, model_t *mod)
{
	dvertex_t  *in;
	vertex_t  *out;
	int         i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (vertex_t));

	mod->brush->numvertices = count;
	mod->brush->vertices = out;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = LittleFloat (in->point[0]);
		out->v[1] = LittleFloat (in->point[1]);
		out->v[2] = LittleFloat (in->point[2]);
	}
}

static void
Mod_LoadSubmodels (dlump_t *l, model_t *mod)
{
	dmodel_t   *in;
	dmodel_t   *out;
	int         i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->submodels = out;
	mod->brush->numsubmodels = count;

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

static void
Mod_LoadEdges (dlump_t *l, model_t *mod)
{
	dedge_t    *in;
	medge_t    *out;
	int         i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, (count + 1) * sizeof (*out));

	mod->brush->edges = out;
	mod->brush->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort (in->v[0]);
		out->v[1] = (unsigned short) LittleShort (in->v[1]);
	}
}

static void
Mod_LoadFaces (dlump_t *l, model_t *mod)
{
	dface_t		*in;
	msurface_t	*out;
	int			 i, count;
	brushhdr_t	*bheader = mod->brush;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",
				mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	bheader->surfaces = out;
	bheader->numsurfaces = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);

		if (out->numedges >= 256)
			Sys_Error ("MOD_LoadBmodel: Too many edges in surface for %s",
					mod->name);

		if (LittleShort (in->side))
			out->flags |= SURF_PLANEBACK;

		out->plane = bheader->planes + LittleShort (in->planenum);
	}
}

static void
Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

static void
Mod_LoadNodes (dlump_t *l, model_t *mod)
{
	int         i, j, count, p;
	dnode_t    *in;
	mnode_t    *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->nodes = out;
	mod->brush->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->planenum);
		out->plane = mod->brush->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = mod->brush->nodes + p;
			else
				out->children[j] = (mnode_t *) (mod->brush->leafs + (-1 - p));
		}
	}

	Mod_SetParent (mod->brush->nodes, NULL);	// sets nodes and leafs
}

static void
Mod_LoadLeafs (dlump_t *l, model_t *mod)
{
	dleaf_t		in;
	mleaf_t		*out;
	int			i, j, count, p;
	fs_file_t	*file;
	SDL_RWops	*rw;
	char		*base_name, *tmp;
	
	l = l;

	base_name = Zstrdup (tempzone, mod->name);
	if ((tmp = strrchr (base_name, '.')))
		*tmp = '\0';

	if (!(file = FS_FindFile (va("%s.leaf", base_name))))
		goto end;
	if (!(rw = file->open (file, 0)))
		goto end;

	if (file->len % sizeof (in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = file->len / sizeof (in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->leafs = out;
	mod->brush->numleafs = count;

	for (i = 0; i < count; i++, out++) {
		SDL_RWread (rw, &in, sizeof (in), 1);
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort (in.mins[j]);
			out->maxs[j] = LittleShort (in.maxs[j]);
		}

		p = LittleLong (in.contents);
		out->contents = p;

		out->firstmarksurface = mod->brush->marksurfaces +
			LittleShort (in.firstmarksurface);
		out->nummarksurfaces = LittleShort (in.nummarksurfaces);

		p = LittleLong (in.visofs);
		if (p == -1 || !mod->brush->visdata)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = mod->brush->visdata + p;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in.ambient_level[j];
	}
	SDL_RWclose (rw);
end:
	Zone_Free (base_name);
}

static void
Mod_LoadClipnodes (dlump_t *l, model_t *mod)
{
	dclipnode_t *in, *out;
	int         i, count;
	hull_t     *hull;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->clipnodes = out;
	mod->brush->numclipnodes = count;

	hull = &mod->brush->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = mod->brush->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;
	VectorSubtract (hull->clip_maxs, hull->clip_mins, hull->clip_size);

	hull = &mod->brush->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = mod->brush->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;
	VectorSubtract (hull->clip_maxs, hull->clip_mins, hull->clip_size);

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = LittleLong (in->planenum);
		out->children[0] = LittleShort (in->children[0]);
		out->children[1] = LittleShort (in->children[1]);
	}
}

/*
=================
Deplicate the drawing hull structure as a clipping hull
=================
*/
static void
Mod_MakeHull0 (model_t *mod)
{
	mnode_t    *in, *child;
	dclipnode_t *out;
	int         i, j, count;
	hull_t     *hull;

	hull = &mod->brush->hulls[0];

	in = mod->brush->nodes;
	count = mod->brush->numnodes;
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = mod->brush->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - mod->brush->planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - mod->brush->nodes;
		}
	}
}

static void
Mod_LoadMarksurfaces (dlump_t *l, model_t *mod)
{
	Uint32		i, j, count;
	short		*in;
	msurface_t	**out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->marksurfaces = out;
	mod->brush->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort (in[i]);
		if (j >= mod->brush->numsurfaces)
			Sys_Error ("Mod_LoadMarksurfaces: bad surface number");
		out[i] = mod->brush->surfaces + j;
	}
}

static void
Mod_LoadSurfedges (dlump_t *l, model_t *mod)
{
	int         i, count;
	int        *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * sizeof (*out));

	mod->brush->surfedges = out;
	mod->brush->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong (in[i]);
}


static void
Mod_LoadPlanes (dlump_t *l, model_t *mod)
{
	int         i, j;
	mplane_t   *out;
	dplane_t   *in;
	int         count;
	int         bits;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s", mod->name);
	count = l->filelen / sizeof (*in);
	out = Zone_Alloc (mod->zone, count * 2 * sizeof (*out));

	mod->brush->planes = out;
	mod->brush->numplanes = count;

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

void
Mod_LoadBrushModel (model_t *mod, void *buffer, int flags)
{
	Uint32		i;
	dheader_t	*header;
	dmodel_t	*bm;
	model_t		*first;
	char		name[10];
	brushhdr_t	*bheader;

	first = mod;
	mod->zone = Zone_AllocZone(mod->name);
	mod->brush = Zone_Alloc(mod->zone, sizeof(brushhdr_t));
	mod->type = mod_brush;
	bheader = mod->brush;

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
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES], mod);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES], mod);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES], mod);
	if (flags & FLAG_RENDER)
		Mod_LoadTextures (&header->lumps[LUMP_TEXTURES], mod);
	if (flags & FLAG_RENDER)
		Mod_LoadLighting (&header->lumps[LUMP_LIGHTING], mod);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES], mod);
	if (flags & FLAG_RENDER)
		Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO], mod);
	if (flags & FLAG_RENDER)
		Mod_LoadRFaces (&header->lumps[LUMP_FACES], mod);
	else
		Mod_LoadFaces (&header->lumps[LUMP_FACES], mod);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES], mod);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY], mod);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], mod);
	Mod_LoadNodes (&header->lumps[LUMP_NODES], mod);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], mod);
	Mod_LoadEntities (mod);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS], mod);

	Mod_MakeHull0 (mod);

	mod->numframes = 2;					// regular and alternate animation

	//
	// set up the submodels (FIXME: this is confusing)
	//
	for (i = 0; i < mod->brush->numsubmodels; i++) {
		int			l, k;
		Uint		j;
		float		dist, modelyawradius, modelradius, *vec;
		msurface_t	*surf;

		bm = &bheader->submodels[i];

		mod->brush->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			mod->brush->hulls[j].firstclipnode = bm->headnode[j];
			mod->brush->hulls[j].lastclipnode = bheader->numclipnodes - 1;
		}

		bheader->firstmodelsurface = bm->firstface;
		bheader->nummodelsurfaces = bm->numfaces;

		if (flags & FLAG_RENDER)
			Mod_MakeChains (mod);

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
					vec = bheader->vertices[bheader->edges[l].v[0]].v;
				else
					vec = bheader->vertices[bheader->edges[-l].v[1]].v;
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
		mod->loaded = true;

		if (flags & FLAG_SUBMODELS) {
			if (i < bheader->numsubmodels - 1)
			{
				// New name.
				snprintf (name, sizeof(name), "*%d", i + 1);
				// Get a struct for this model name.
				mod = Mod_FindName (name);
				// If it was an old model then unload it first.
				if (mod->loaded) {
					Com_DPrintf ("Warning, unloading %s during model load.\n", name);
					Mod_UnloadModel (mod, false);
				}
				// Copy over the basic information.
				*mod = *first;
				mod->submodel = true;
				strlcpy_s (mod->name, name);
				// Allocate a new brush struct.
				mod->brush = Zone_Alloc(first->zone, sizeof(brushhdr_t));
				// Copy over the basics.
				*mod->brush = *first->brush;

				// And change the pointers for the next loop!
				bheader = mod->brush;
				bheader->main_model = first;
			}
		} else
			break;
	}
}

/*
=================
NOTE: Watch the 'unloading' variable, it is a simple lock to prevent
an infinite loop!
=================
*/
void
Mod_UnloadBrushModel (model_t *mod, qboolean keep)
{
	model_t			*sub;
	Uint			 i;
	static qboolean	 unloading = false;

	if (mod->brush->main_model && !unloading)
	{
		Mod_UnloadModel (mod->brush->main_model, keep);
		return;
	}

	if (mod->modflags & FLAG_RENDER)
		Mod_RUnloadBrushModel (mod);

	if (!unloading)
	{
		unloading = true;
		for (i = 1; i <= mod->brush->numsubmodels; i++) {
			sub = Mod_FindName(va("*%d", i));
			Mod_UnloadModel(sub, keep); // FIXME
		}
		unloading = false;

		Zone_FreeZone (&mod->zone);
	}
}
