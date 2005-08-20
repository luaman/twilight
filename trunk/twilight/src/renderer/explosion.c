/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "twiconfig.h"

#include <math.h>

#include "cclient.h"
#include "collision.h"
#include "common.h"
#include "cvar.h"
#include "dyngl.h"
#include "gl_arrays.h"
#include "gl_info.h"
#include "gl_main.h"
#include "mathlib.h"
#include "noise.h"
#include "quakedef.h"
#include "strlib.h"
#include "sys.h"
#include "textures.h"

#define MAX_EXPLOSIONS 64
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONGRID*EXPLOSIONGRID*2)

static float explosiontexcoord2f[EXPLOSIONVERTS][2];
static int explosiontris[EXPLOSIONTRIS][3];
static int explosionnoiseindex[EXPLOSIONVERTS];
static vec3_t explosionpoint[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float starttime;
	float endtime;
	float time;
	float alpha;
	float fade;
	vec3_t origin;
	vec3_t vert[EXPLOSIONVERTS];
	vec3_t vertvel[EXPLOSIONVERTS];
	qboolean clipping;
}
explosion_t;

static explosion_t explosion[MAX_EXPLOSIONS];

static int explosiontexture;

static cvar_t *r_explosionclip, *r_drawexplosions, *cl_explosions_lifetime;
static cvar_t *cl_explosions_alpha_start, *cl_explosions_alpha_end;
static cvar_t *cl_explosions_size_start, *cl_explosions_size_end;


static void r_explosion_start(void)
{
	int x, y;
	Uint8 noise1[128][128], noise2[128][128], noise3[128][128], data[128][128][4];
	FractalNoise(&noise1[0][0], 128, 32);
	FractalNoise(&noise2[0][0], 128, 4);
	FractalNoise(&noise3[0][0], 128, 4);
	for (y = 0;y < 128;y++)
	{
		for (x = 0;x < 128;x++)
		{
			int j, r, g, b, a;
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

void R_Explosion_Shutdown(void)
{
	GLT_Delete (explosiontexture);
}

void r_explosion_newmap(void)
{
	memset(explosion, 0, sizeof(explosion));
}

static int R_ExplosionVert(int column, int row)
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
	explosiontexcoord2f[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoord2f[i][1] = (float) row / (float) EXPLOSIONGRID;
	explosionnoiseindex[i] = (row % EXPLOSIONGRID) * EXPLOSIONGRID + (column % EXPLOSIONGRID);
	return i;
}

void R_Explosion_Init(void)
{
	int i, x, y;

	if (EXPLOSIONVERTS > MAX_VERTEX_ARRAYS)
		Sys_Error("Explosion too complex for vertex array size.");

	if ((EXPLOSIONTRIS * 3) > MAX_VERTEX_INDICES)
		Sys_Error("Explosion too complex for vertex indice array size.");

	i = 0;
	for (y = 0;y < EXPLOSIONGRID;y++)
	{
		for (x = 0;x < EXPLOSIONGRID;x++)
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
	cl_explosions_alpha_start = Cvar_Get ("cl_explosions_alpha_start", "1.5", CVAR_ARCHIVE, NULL);
	cl_explosions_alpha_end = Cvar_Get ("cl_explosions_alpha_end", "0", CVAR_ARCHIVE, NULL);
	cl_explosions_size_start = Cvar_Get ("cl_explosions_size_start", "16", CVAR_ARCHIVE, NULL);
	cl_explosions_size_end = Cvar_Get ("cl_explosions_size_end", "128", CVAR_ARCHIVE, NULL);
	cl_explosions_lifetime = Cvar_Get ("cl_explosions_lifetime", "0.5", CVAR_ARCHIVE, NULL);

	r_explosion_start();
}

void R_NewExplosion(vec3_t org)
{
	int i, j;
	float dist, n;
	explosion_t *e;
	Uint8 noise[EXPLOSIONGRID*EXPLOSIONGRID];
	FractalNoise(noise, EXPLOSIONGRID, 4); // adjust noise grid size according to explosion
	for (i = 0, e = explosion;i < MAX_EXPLOSIONS;i++, e++)
	{
		if (ccl.time >= e->endtime)
		{
			e->starttime = ccl.time;
			e->endtime = ccl.time + cl_explosions_lifetime->fvalue;
			e->time = e->starttime;
			e->alpha = cl_explosions_alpha_start->fvalue;
			e->fade = (cl_explosions_alpha_start->fvalue - cl_explosions_alpha_end->fvalue) / cl_explosions_lifetime->fvalue;
			e->clipping = r_explosionclip->ivalue != 0;
			VectorCopy(org, e->origin);
			for (j = 0;j < EXPLOSIONVERTS;j++)
			{
				// calculate start origin and velocity
				n = noise[explosionnoiseindex[j]] * (1.0f / 255.0f) + 0.5;
				dist = n * cl_explosions_size_start->fvalue;
				VectorMA(e->origin, dist, explosionpoint[j], e->vert[j]);
				dist = n * (cl_explosions_size_end->fvalue - cl_explosions_size_start->fvalue) / cl_explosions_lifetime->fvalue;
				VectorScale(explosionpoint[j], dist, e->vertvel[j]);
				// clip start origin
				if (e->clipping)
				{
					TraceLine (r.worldmodel, e->origin, e->vert[j], e->vert[j], NULL);
				}
			}
			break;
		}
	}
}

static void R_MoveExplosion(explosion_t *e)
{
	int i;
	float dot, end[3], frametime;
	vec3_t normal;

	frametime = ccl.time - e->time;
	e->time = ccl.time;
	e->alpha = e->alpha - (e->fade * frametime);
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			VectorMA(e->vert[i], frametime, e->vertvel[i], end);
			if (e->clipping)
			{
				if (TraceLine (r.worldmodel, e->vert[i], end, e->vert[i], normal) < 1)
				{
					// clip velocity against the wall
					dot = -DotProduct(e->vertvel[i], normal);
					VectorMA(e->vertvel[i], dot, normal, e->vertvel[i]);
				}
			}
			else
				VectorCopy(end, e->vert[i]);
		}
	}
}


void R_MoveExplosions(void)
{
	int i;
	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (ccl.time < explosion[i].endtime)
			R_MoveExplosion(&explosion[i]);
}

void R_DrawExplosions(void)
{
	int i;
	qboolean	drawn = false;

	if (!r_drawexplosions->ivalue)
		return;

	qglBindTexture (GL_TEXTURE_2D, explosiontexture);

	if (gl_cull->ivalue)
		qglDisable (GL_CULL_FACE);


	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (ccl.time < explosion[i].endtime) {
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

