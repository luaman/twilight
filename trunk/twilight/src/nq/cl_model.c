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
void Mod_UnloadBrushModel (model_t *mod);

void	Mod_LoadSpriteModel (model_t *mod, void *buffer);
void	Mod_LoadBrushModel (model_t *mod, void *buffer);
void	Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t	*Mod_LoadModel (model_t *mod, qboolean crash);

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
