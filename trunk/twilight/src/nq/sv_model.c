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
// sv_model.c -- server model loading
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "sys.h"
#include "quakedef.h"
#include "cvar.h"
#include "glquake.h"
#include "strlib.h"

extern model_t    *loadmodel;
extern char        loadname[32];				// for hunk tags
extern Uint8      *mod_base;

model_t    *Mod_LoadModel (model_t *mod, qboolean crash);
void        Mod_LoadBrushModel (model_t *mod, void *buffer);

cvar_t		*gl_subdivide_size;

/*
===============
Mod_Init_Cvars
===============
*/
void
Mod_Init_Cvars (void)
{
	gl_subdivide_size = NULL;		// FIXME?
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
// because the world is so huge, load it one piece at a time
//
	if (!crash) {

	}
//
// load the file
//
	buf =
		(unsigned *) COM_LoadStackFile (mod->name, stackbuf, sizeof (stackbuf));
	if (!buf) {
		if (crash)
			Sys_Error ("Mod_NumForName: %s not found", mod->name);
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
		out->texinfo = NULL;

		// lighting info

		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong (in->lightofs);

		out->samples = NULL;
	}
}

void Mod_LoadVisibility (lump_t *l); 
void Mod_LoadEntities (lump_t *l);
void Mod_LoadVertexes (lump_t *l);
void Mod_LoadSubmodels (lump_t *l);
void Mod_LoadEdges (lump_t *l);
void Mod_LoadNodes (lump_t *l);
void Mod_LoadLeafs (lump_t *l);
void Mod_LoadClipnodes (lump_t *l);
void Mod_LoadMarksurfaces (lump_t *l);
void Mod_LoadSurfedges (lump_t *l);
void Mod_LoadPlanes (lump_t *l);
void Mod_MakeHull0 (void);
model_t    *Mod_FindName (char *name);


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
	char        name[10];
	extern qboolean isnotmap;

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

// load into heap

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

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (!isnotmap && (i < mod->numsubmodels - 1)) 
		{	
			// duplicate the basic information
			strlcpy (name, va("*%i", i + 1), sizeof(name));
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

