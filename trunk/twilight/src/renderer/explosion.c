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
#include "textures.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "dyngl.h"
#include "gl_arrays.h"
#include "gl_info.h"
#include "noise.h"
#include "cclient.h"
#include "gl_main.h"

#define MAX_EXPLOSIONS 64
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONGRID*EXPLOSIONGRID*2)
#define EXPLOSIONINDICES (EXPLOSIONTRIS*3)
#define EXPLOSIONSTARTVELOCITY (256.0f)
#define EXPLOSIONFADESTART (1.5f)
#define EXPLOSIONFADERATE (3.0f)


static float explosiontexcoord2f[EXPLOSIONVERTS][2];
static int explosiontris[EXPLOSIONTRIS][3];
static int explosionnoiseindex[EXPLOSIONVERTS];
static vec3_t explosionpoint[EXPLOSIONVERTS];
static vec3_t explosionspherevertvel[EXPLOSIONVERTS];

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

static cvar_t *r_explosionclip, *r_drawexplosions;

static int R_ExplosionVert(int column, int row);

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
			r = (j * 512) / 256;
			g = (j * 256) / 256;
			b = (j * 128) / 256;
			a = noise3[y][x] * 3 - 128;
			data[y][x][0] = bound(0, r, 255);
			data[y][x][1] = bound(0, g, 255);
			data[y][x][2] = bound(0, b, 255);
			data[y][x][3] = bound(0, a, 255);
		}
	}

	explosiontexture = GLT_Load_Raw ("explosiontexture", 128, 128,
			&data[0][0][0], NULL, TEX_MIPMAP|TEX_ALPHA, 32);

	// note that explosions survive the restart
}


void
R_Explosion_Shutdown (void)
{
	GLT_Delete (explosiontexture);
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

	r_explosion_start();
}

static int
R_ExplosionVert(int column, int row)
{
	int i;
	float yaw, pitch;
	// top and bottom rows are all one position...
	if (row == 0 || row == EXPLOSIONGRID)
		column = 0;
	i = row * (EXPLOSIONGRID + 1) + column;
	yaw = ((double) column / EXPLOSIONGRID) * M_PI * 2;
	pitch = (((double) row / EXPLOSIONGRID) - 0.5) * M_PI;
	explosionpoint[i][0] = cos(yaw) *  cos(pitch);
	explosionpoint[i][1] = sin(yaw) *  cos(pitch);
	explosionpoint[i][2] =        1 * -sin(pitch);
	explosionspherevertvel[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTVELOCITY;
	explosionspherevertvel[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTVELOCITY;
	explosionspherevertvel[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTVELOCITY;
	explosiontexcoord2f[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoord2f[i][1] = (float) row / (float) EXPLOSIONGRID;
	explosionnoiseindex[i] = (row % EXPLOSIONGRID) * EXPLOSIONGRID + (column % EXPLOSIONGRID);
	return i;
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
						(r.worldmodel, e->vert[i], end, impact, normal) < 1)
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

void
R_DrawExplosions (void)
{
	int			i;
	qboolean	drawn = false;

	if (!r_drawexplosions->ivalue)
		return;

	qglBindTexture (GL_TEXTURE_2D, explosiontexture);

	if (gl_cull->ivalue)
		qglDisable (GL_CULL_FACE);

	for (i = 0; i < MAX_EXPLOSIONS; i++)
		if (explosion[i].alpha > 0.01f) {
			TWI_ChangeVDrawArraysALL (EXPLOSIONVERTS, 1, (vertex_t *) explosion[i].vert, NULL,
					(texcoord_t *) explosiontexcoord2f, NULL,
					NULL, NULL);
			qglColor4f (explosion[i].alpha, explosion[i].alpha, explosion[i].alpha, 1);
			qglDrawElements (GL_TRIANGLES, EXPLOSIONTRIS * 3, GL_UNSIGNED_INT, explosiontris);
			drawn = true;
		}

	if (drawn) {
		TWI_ChangeVDrawArraysALL (EXPLOSIONVERTS, 0, NULL, NULL, NULL, NULL, NULL, NULL);
		qglColor4fv (whitev);
	}

	if (gl_cull->ivalue)
		qglEnable (GL_CULL_FACE);
}

