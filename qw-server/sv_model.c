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
#include "mathlib.h"
#include "mdfour.h"
#include "model.h"
#include "strlib.h"
#include "zone.h"
#include "server.h"

model_t    *loadmodel;
char        loadname[32];				// for hunk tags

void        Mod_LoadBrushModel (model_t *mod, void *buffer);
model_t    *Mod_LoadModel (model_t *mod, qboolean crash);

Uint8       mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	256
model_t     mod_known[MAX_MOD_KNOWN];
int         mod_numknown;

texture_t   r_notexture_mip;

unsigned   *model_checksum;

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
Mod_LoadBrushModel
=================
*/
void
Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	Uint32		i, j;
	dheader_t	*header;
	dmodel_t	*bm;

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
