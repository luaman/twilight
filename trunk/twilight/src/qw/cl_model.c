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

#include "quakedef.h"
#include "client.h"
#include "crc.h"
#include "cvar.h"
#include "draw.h"
#include "gl_textures.h"
#include "host.h"
#include "mathlib.h"
#include "mdfour.h"
#include "strlib.h"
#include "sys.h"

extern model_t	*loadmodel;

extern qboolean isnotmap;

void	Mod_LoadSpriteModel (model_t *mod, void *buffer);
void	Mod_LoadBrushModel (model_t *mod, void *buffer);
void	Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t	*Mod_LoadModel (model_t *mod, qboolean crash);

Uint8	mod_novis[MAX_MAP_LEAFS / 8];

cvar_t	*gl_subdivide_size;

void	GL_SubdivideSurface (msurface_t *fa);

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
//
// allocate a new model
//
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
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) (mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	// Make room for the two r_notextures
	loadmodel->numtextures = m->nummiptex + 2;
	loadmodel->textures = Hunk_AllocName (loadmodel->numtextures
			* sizeof (*loadmodel->textures), loadmodel->name);

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
		tx = Hunk_AllocName (sizeof (texture_t), loadmodel->name);
		loadmodel->textures[i] = tx;

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

				if (Img_HasFullbrights((Uint8 *)(mtdata),
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
	loadmodel->textures[i++] = r_notexture;
	loadmodel->textures[i] = r_notexture_water;

//
// sequence the animations
//
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = loadmodel->textures[i];
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
			Host_Error ("Bad animating texture %s", tx->name);

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];
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
				Host_Error ("Bad animating texture %s", tx->name);
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
				tx2->alternate_anims = altanims[0];
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
	Uint i;
	Uint8 *in, *out, *data;
	Uint8 d;
	char litfilename[MAX_OSPATH];

	loadmodel->lightdata = NULL;

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
	out = Hunk_AllocName (count * sizeof (*out), loadmodel->name);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 8; j++)
			out->vecs[j / 4][j % 4] = 
				LittleFloat (in->vecs[j/4][j % 4]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		out->texture = NULL;
		if (loadmodel->textures)
		{
			if (miptex >= loadmodel->numtextures)
				Host_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
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
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(s->texinfo->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Host_Error ("Bad surface extents");
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
	int			ssize, tsize;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof (*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	count = l->filelen / sizeof (*in);
	out = Hunk_AllocName (count * sizeof (*out), loadmodel->name);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);
		out->flags = 0;

		if (out->numedges >= 256)
			Host_EndGame ("MOD_LoadBmodel: Too many edges in surface for %s",
					loadmodel->name);

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
		else
			out->samples = loadmodel->lightdata + i*3;

		// set the drawing flags flag

		// is it sky?
		if (!strncmp (out->texinfo->texture->name, "sky", 3))
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			// cut up polygon for warps
			GL_SubdivideSurface (out);
			continue;
		}

		// is it water?
		if (out->texinfo->texture->name[0] == '*')
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			// cut up polygon for warps
			GL_SubdivideSurface (out);
			continue;
		}

		// is it special?
		if (out->texinfo->flags & TEX_SPECIAL)
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			// cut up polygon for warps
			GL_SubdivideSurface (out);
			continue;
		}

		// It's a wall - allocate a stainmap for it
		ssize = out->smax;
		tsize = out->tmax;
		out->stainsamples = Hunk_AllocName (ssize * tsize * 3, loadmodel->name);
		memset(out->stainsamples, 255, ssize * tsize * 3);
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
	char		name[10];

	loadmodel->type = mod_brush;

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
	mod->checksum = 0;
	mod->checksum2 = 0;

	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		mod->checksum ^= Com_BlockChecksum (mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);

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
		int			k, l, j;
		float		dist, modelyawradius, modelradius, *vec;
		msurface_t	*surf;

		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
		mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
		modelyawradius = 0;
		modelradius = 0;

		// Calculate the bounding boxes, don't trust what the model says.
		surf = &mod->surfaces[mod->firstmodelsurface];
		for (j = 0; j < mod->nummodelsurfaces; j++, surf++) {
			for (k = 0; k < surf->numedges; k++) {
				l = mod->surfedges[k + surf->firstedge];
				if (l > 0)
					vec = mod->vertexes[mod->edges[l].v[0]].position;
				else
					vec = mod->vertexes[mod->edges[-l].v[1]].position;
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

qboolean	vseams[MAXALIASVERTS];
int			vremap[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];
int			numinverts;

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
	int				i, j;
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
	int					i, j, k, numframes;
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
	int					i, j, k, v;
	float				s, t;
	mdl_t				*pinmodel;
	Uint8				*datapointer, *skindata;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes, numseams;
	daliasframetype_t	*pframetype;
	qboolean			typeSingle = false;

	// Clear the arrays to NULL.
	memset (vseams, 0, sizeof(vseams));
	memset (vremap, 0, sizeof(vremap));

	if (!strcmp (loadmodel->name, "progs/player.mdl") ||
			!strcmp (loadmodel->name, "progs/eyes.mdl")) {
		int crc;

		crc = CRC_Block (buffer, com_filesize);

		Info_SetValueForKey (cls.userinfo,
				!strcmp (loadmodel->name, "progs/player.mdl") ? pmodel_name :
				emodel_name, va("%d", crc), MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, va("setinfo %s %d",
						!strcmp (loadmodel->name, "progs/player.mdl") ?
						pmodel_name : emodel_name, crc));
		}
	}

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
		Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

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

	mod->extradata = pheader;
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

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t), loadmodel->name);

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

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
								   (numframes -
									1) * sizeof (pspritegroup->frames[0]),
								   loadmodel->name);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadmodel->name);

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

	psprite = Hunk_AllocName (size, loadmodel->name);

	mod->extradata = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
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
