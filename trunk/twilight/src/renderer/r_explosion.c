/*
	$RCSfile$

	Copyright (C)  2001 Forest Hale

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
#include "collision.h"
#include "common.h"
#include "cvar.h"
#include "gl_textures.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "dyngl.h"
#include "gl_arrays.h"
#include "gl_info.h"
#include "noise.h"
#include "cclient.h"

#define MAX_EXPLOSIONS 128
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONGRID*EXPLOSIONGRID*2)
#define EXPLOSIONINDICES (EXPLOSIONTRIS*3)
#define EXPLOSIONSTARTRADIUS (20.0f)
#define EXPLOSIONSTARTVEL (256.0f)
#define EXPLOSIONFADESTART (1.5f)
#define EXPLOSIONFADERATE (3.0f)


static vec3_t explosionspherevertvel[EXPLOSIONVERTS];
static float explosiontexcoords[EXPLOSIONVERTS][2];
static int explosiontris[EXPLOSIONTRIS][3];
static int explosionnoiseindex[EXPLOSIONVERTS];
static vec3_t explosionpoint[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float		starttime;
	float		time;
	float		alpha;
	vec3_t		origin;
	vec3_t		vert[EXPLOSIONVERTS];
	vec3_t		vertvel[EXPLOSIONVERTS];
}
explosion_t;


static explosion_t explosion[MAX_EXPLOSIONS];

static int	explosiontexture;
static int	explosiontexturefog;

static cvar_t *r_explosionclip, *r_drawexplosions;
static cvar_t *r_explosioncolor_r, *r_explosioncolor_g, *r_explosioncolor_b, *r_explosioncolor_a;

static int
R_ExplosionVert (int column, int row)
{
	int			i;
	float		a, b, c;

	i = row * (EXPLOSIONGRID + 1) + column;
	a = row * M_PI * 2 / EXPLOSIONGRID;
	b = column * M_PI * 2 / EXPLOSIONGRID;
	c = cos(b);

	explosionpoint[i][0] = cos(a) * c;
	explosionpoint[i][1] = sin(a) * c;
	explosionpoint[i][2] = -sin(b);
	explosionspherevertvel[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTVEL;
	explosionspherevertvel[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTVEL;
	explosionspherevertvel[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTVEL;
	explosiontexcoords[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoords[i][1] = (float) row / (float) EXPLOSIONGRID;

	// top and bottom rows are all one position...
	if (row == 0 || row == EXPLOSIONGRID)
		column = 0;

	explosionnoiseindex[i] = (row % EXPLOSIONGRID) * EXPLOSIONGRID
		+ (column % EXPLOSIONGRID);

	return i;
}


static void
r_explosion_start (void)
{
	int			x, y;
	int			j, r, g, b, a;
	Uint8		noise1[128][128];
	Uint8		noise2[128][128];
	Uint8		noise3[128][128];
	Uint8		data[128][128][4];

	FractalNoise(&noise1[0][0], 128, 32);
	FractalNoise(&noise2[0][0], 128, 4);
	FractalNoise(&noise3[0][0], 128, 4);

	for (y = 0; y < 128; y++)
	{
		for (x = 0; x < 128; x++)
		{
			j = (noise1[y][x] * noise2[y][x]) * 3 / 256 - 128;
			r = j;
			g = j;
			b = j;
			a = noise3[y][x] * 3 - 128;
			data[y][x][0] = bound(0, r, 255);
			data[y][x][1] = bound(0, g, 255);
			data[y][x][2] = bound(0, b, 255);
			data[y][x][3] = bound(0, a, 255);
		}
	}

	explosiontexture = GLT_Load_Raw ("explosiontexture", 128, 128,
			&data[0][0][0], NULL, TEX_MIPMAP|TEX_ALPHA, 32);
	for (y = 0; y < 128; y++)
		for (x = 0; x < 128; x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;

	explosiontexturefog = GLT_Load_Raw ("explosiontexturefog", 128, 128,
			&data[0][0][0], NULL, TEX_MIPMAP|TEX_ALPHA, 32);

	// note that explosions survive the restart
}


void
r_explosion_shutdown (void)
{
}

void
r_explosion_newmap (void)
{
	memset(explosion, 0, sizeof(explosion));
}

void
R_Explosion_Init (void)
{
	int			i, x, y;

	if (EXPLOSIONVERTS > MAX_VERTEX_ARRAYS)
		Sys_Error("Explosion too complex for vertex array size.");

	if (EXPLOSIONINDICES > MAX_VERTEX_INDICES)
		Sys_Error("Explosion too complex for vertex indice array size.");

	i = 0;
	for (y = 0; y < EXPLOSIONGRID; y++)
	{
		for (x = 0; x < EXPLOSIONGRID; x++)
		{
			explosiontris[i][0] = R_ExplosionVert(x    , y    );
			explosiontris[i][1] = R_ExplosionVert(x + 1, y    );
			explosiontris[i][2] = R_ExplosionVert(x    , y + 1);
			i++;
			explosiontris[i][0] = R_ExplosionVert(x + 1, y    );
			explosiontris[i][1] = R_ExplosionVert(x + 1, y + 1);
			explosiontris[i][2] = R_ExplosionVert(x    , y + 1);
			i++;
		}
	}

	r_explosionclip = Cvar_Get ("r_explosionclip", "1", CVAR_ARCHIVE, NULL);
	r_drawexplosions = Cvar_Get ("r_drawexplosions", "1", CVAR_ARCHIVE, NULL);
	r_explosioncolor_r = Cvar_Get ("r_explosioncolor_r", "1", CVAR_ARCHIVE, NULL);
	r_explosioncolor_g = Cvar_Get ("r_explosioncolor_g", "0.5", CVAR_ARCHIVE, NULL);
	r_explosioncolor_b = Cvar_Get ("r_explosioncolor_b", "0.25", CVAR_ARCHIVE, NULL);
	r_explosioncolor_a = Cvar_Get ("r_explosioncolor_a", "1", CVAR_ARCHIVE, NULL);

	r_explosion_start();
}

void
R_NewExplosion (vec3_t org)
{
	int			i, j;
	float		dist;
	Uint8		noise[EXPLOSIONGRID*EXPLOSIONGRID];
	
	// Adjust noise grid size according to explosion grid
	FractalNoise (noise, EXPLOSIONGRID, 4);

	for (i = 0; i < MAX_EXPLOSIONS; i++)
	{
		if (explosion[i].alpha <= 0.01f)
		{
			explosion[i].starttime = ccl.time;
			explosion[i].time = explosion[i].starttime - 0.1;
			explosion[i].alpha = EXPLOSIONFADESTART;
			VectorCopy(org, explosion[i].origin);
			for (j = 0; j < EXPLOSIONVERTS; j++)
			{
				// calculate start
				VectorCopy(explosion[i].origin, explosion[i].vert[j]);
				// calculate velocity
				dist = noise[explosionnoiseindex[j]] * (1.0f / 255.0f) + 0.5;
				VectorScale(explosionspherevertvel[j], dist,
						explosion[i].vertvel[j]);
			}
			break;
		}
	}
}

static void
R_MoveExplosion (explosion_t *e)
{
	int			i;
	float		dot, frictionscale, end[3], impact[3], normal[3], frametime;

	frametime = ccl.time - e->time;
	e->time = ccl.time;
	e->alpha = EXPLOSIONFADESTART - (ccl.time - e->starttime)
		* EXPLOSIONFADERATE;

	if (e->alpha <= 0.01f)
	{
		e->alpha = -1;
		return;
	}

	frictionscale = 1 - frametime;
	frictionscale = bound(0, frictionscale, 1);
	for (i = 0; i < EXPLOSIONVERTS; i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			VectorScale(e->vertvel[i], frictionscale, e->vertvel[i]);
			VectorMA(e->vert[i], frametime, e->vertvel[i], end);
			if (r_explosionclip->ivalue)
			{
				if (TraceLine
						(r_worldmodel, e->vert[i], end, impact, normal) < 1)
				{
					// clip velocity against the wall
					dot = DotProduct(e->vertvel[i], normal) * -1.125f;
					VectorMA(e->vertvel[i], dot, normal, e->vertvel[i]);
				}
				VectorCopy(impact, e->vert[i]);
			}
			else
				VectorCopy(end, e->vert[i]);
		}
	}

	for (i = 0; i < EXPLOSIONGRID; i++)
		VectorCopy(e->vert[i * (EXPLOSIONGRID + 1)],
				e->vert[i * (EXPLOSIONGRID + 1) + EXPLOSIONGRID]);

	memcpy(e->vert[EXPLOSIONGRID * (EXPLOSIONGRID + 1)], e->vert[0],
			sizeof(float[3]) * (EXPLOSIONGRID + 1));
}

void
R_MoveExplosions (void)
{
	int			i;

	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (explosion[i].alpha > 0.01f)
			R_MoveExplosion(&explosion[i]);
}

static void
R_DrawExplosion (explosion_t *e)
{
	int			i;
	float		r, g, b, a;
	float		dist;
	vec3_t		centerdir, diff;

	if (((v_index + EXPLOSIONINDICES) >= MAX_VERTEX_ARRAYS) ||
			((i_index + EXPLOSIONINDICES) >= MAX_VERTEX_INDICES))
	{
		TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
		TWI_PreVDrawCVA (0, EXPLOSIONTRIS * 3);
		qglDrawElements (GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}

	memcpy (&v_array_v(v_index), &e->vert[0][0], sizeof(float[3])
			* EXPLOSIONVERTS);
	memcpy (&tc_array_v(v_index), &explosiontexcoords[0][0], sizeof(float[2])
			* EXPLOSIONVERTS);

	for (i = 0; i < EXPLOSIONINDICES; i++)
		vindices[i_index + i] = explosiontris[0][i] + v_index;

	r = r_explosioncolor_r->fvalue;
	g = r_explosioncolor_g->fvalue;
	b = r_explosioncolor_b->fvalue;
	a = r_explosioncolor_a->fvalue * e->alpha;

	VectorSubtract(r_origin, e->origin, centerdir);
	VectorNormalizeFast(centerdir);
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		VectorSubtract(e->vert[i], e->origin, diff);
		VectorNormalizeFast(diff);
		dist = (DotProduct(diff, centerdir) * 6.0f - 4.0f) * a;
		dist = max (dist, 0);
		VectorSet4 (cf_array_v(v_index + i), dist * r, dist * g, dist * b, 1);
	}

	v_index += EXPLOSIONVERTS;
	i_index += EXPLOSIONINDICES;
}

void
R_DrawExplosions (void)
{
	int			i;

	if (!r_drawexplosions->ivalue)
		return;

	qglEnableClientState (GL_COLOR_ARRAY);
	qglBindTexture (GL_TEXTURE_2D, explosiontexture);

	if (gl_cull->ivalue)
		qglDisable (GL_CULL_FACE);

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		if (explosion[i].alpha > 0.01f)
			R_DrawExplosion(&explosion[i]);

	if (v_index || i_index)
	{
		TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
		TWI_PreVDrawCVA (0, EXPLOSIONTRIS * 3);
		qglDrawElements (GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}

	qglDisableClientState (GL_COLOR_ARRAY);
	if (gl_cull->ivalue)
		qglEnable (GL_CULL_FACE);
}

