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

#include <stdio.h>

#include "quakedef.h"
#include "bspfile.h"
#include "common.h"
#include "mathlib.h"
#include "mdfour.h"
#include "model.h"
#include "strlib.h"
#include "zone.h"
#include "server.h"
#include <math.h>

model_t		*loadmodel;
brushhdr_t	*bheader;

void         Mod_LoadBrushModel (model_t *mod, void *buffer);
model_t		*Mod_LoadModel (model_t *mod, qboolean crash);
void		 Mod_UnloadModel (model_t *mod);
void		 Mod_UnloadBrushModel (model_t *mod);

unsigned	*model_checksum;

void
Mod_UnloadModel (model_t *mod)
{
	if (mod->needload)
		return;

	Mod_UnloadBrushModel (mod);

	memset(mod, 0, sizeof(model_t));
	mod->needload = true;
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
	unsigned   *buf;
	Uint8       stackbuf[1024];			// avoid dirtying the cache heap

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
			SV_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}
//
// allocate a new model
//
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
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

Uint8      *mod_base;

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
	out = Hunk_AllocName (count * sizeof (*out), loadmodel->name);

	bheader->surfaces = out;
	bheader->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);
		out->flags = 0;

		planenum = LittleShort (in->planenum);
		side = LittleShort (in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = bheader->planes + planenum;

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
		SV_Error
			("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
			 mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (Uint8 *) header;

	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// load into heap
	bheader->checksum = 0;
	bheader->checksum2 = 0;

	// checksum all of the map, except for entities
	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		bheader->checksum ^= Com_BlockChecksum (mod_base + 
			header->lumps[i].fileofs,  header->lumps[i].filelen);

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;

		bheader->checksum2 ^= Com_BlockChecksum (mod_base +
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
	for (i = 0; i < bheader->numsubmodels; i++) {
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

		if (i < bheader->numsubmodels - 1)
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

void
Mod_UnloadBrushModel (model_t *mod)
{
	model_t *sub;
	Uint    i;

	if (mod->extra.brush->is_submodel)
		return;

	for (i = 1; i < mod->extra.brush->numsubmodels; i++) {
		sub = Mod_FindName(va("*%d", i));
		Mod_UnloadModel(sub);
	}
	Zone_FreeZone (&mod->extrazone);
}

