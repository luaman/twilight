/*
Copyright (C) 2001 Forest Hale

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

static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <math.h>

#include "quakedef.h"
#include "client.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "glquake.h"
#include "mathlib.h"
#include "strlib.h"

extern void FractalNoise (Uint8 *noise, int size, int startgrid);
extern float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);

#define MAX_EXPLOSIONS 64
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONVERTS*2)
#define EXPLOSIONINDICES (EXPLOSIONTRIS*3)
#define EXPLOSIONSTARTRADIUS (20.0f)
#define EXPLOSIONSTARTVELOCITY (256.0f)
#define EXPLOSIONFADESTART (1.5f)
#define EXPLOSIONFADERATE (4.5f)

#if EXPLOSIONSVERTS > MAX_VERTEX_ARRAYS
#error Explosion too complex for vertex array size.
#endif

#if EXPLOSIONINDICES > MAX_VERTEX_INDICES
#error Explosion too complex for vertex indice array size.
#endif


vec3_t explosionspherevert[EXPLOSIONVERTS];
vec3_t explosionspherevertvel[EXPLOSIONVERTS];
float explosiontexcoords[EXPLOSIONVERTS][2];
int explosiontris[EXPLOSIONTRIS][3];
int explosionnoiseindex[EXPLOSIONVERTS];
vec3_t explosionpoint[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float starttime;
	float alpha;
	vec3_t vert[EXPLOSIONVERTS];
	vec3_t vertvel[EXPLOSIONVERTS];
}
explosion_t;

explosion_t explosion[128];

int	explosiontexture;
int	explosiontexturefog;

static cvar_t *r_explosionclip, *r_drawexplosions;
static cvar_t *r_explosioncolor_r, *r_explosioncolor_g, *r_explosioncolor_b, *r_explosioncolor_a;

int R_ExplosionVert(int column, int row)
{
	int i;
	float a, b, c;
	i = row * (EXPLOSIONGRID + 1) + column;
	a = row * M_PI * 2 / EXPLOSIONGRID;
	b = column * M_PI * 2 / EXPLOSIONGRID;
	c = cos(b);
	explosionpoint[i][0] = cos(a) * c;
	explosionpoint[i][1] = sin(a) * c;
	explosionpoint[i][2] = -sin(b);
	explosionnoiseindex[i] = (row & (EXPLOSIONGRID - 1)) * EXPLOSIONGRID + (column & (EXPLOSIONGRID - 1));
	explosiontexcoords[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoords[i][1] = (float) row / (float) EXPLOSIONGRID;
	return i;
}

void r_explosion_start(void)
{
	int x, y;
	Uint8 noise1[128][128], noise2[128][128], data[128][128][4];
	FractalNoise(&noise1[0][0], 128, 2);
	FractalNoise(&noise2[0][0], 128, 2);
	for (y = 0;y < 128;y++)
	{
		for (x = 0;x < 128;x++)
		{
			int j, r, g, b, a;
			j = noise1[y][x] * 3 - 128;
			r = j;
			g = j;
			b = j;
			a = noise2[y][x];
			data[y][x][0] = bound(0, r, 255);
			data[y][x][1] = bound(0, g, 255);
			data[y][x][2] = bound(0, b, 255);
			data[y][x][3] = bound(0, a, 255);
		}
	}
	explosiontexture = GL_LoadTexture ("explosiontexture", 128, 128,
			&data[0][0][0], true, true, 32);
	for (y = 0;y < 128;y++)
		for (x = 0;x < 128;x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
	explosiontexturefog = GL_LoadTexture ("explosiontexturefog", 128, 128,
			&data[0][0][0], true, true, 32);
}

void r_explosion_shutdown(void)
{
}

void r_explosion_newmap(void)
{
	memset(explosion, 0, sizeof(explosion));
}

void R_Explosion_Init(void)
{
	int i, x, y;
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
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		explosionspherevert[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTRADIUS;
		explosionspherevert[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTRADIUS;
		explosionspherevert[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTRADIUS;
		explosionspherevertvel[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTVELOCITY;
		explosionspherevertvel[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTVELOCITY;
		explosionspherevertvel[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTVELOCITY;
	}

	r_explosionclip = Cvar_Get ("r_explosionclip", "1", CVAR_ARCHIVE, NULL);
	r_drawexplosions = Cvar_Get ("r_drawexplosions", "1", CVAR_ARCHIVE, NULL);
	r_explosioncolor_r = Cvar_Get ("r_explosioncolor_r", "1", CVAR_ARCHIVE, NULL);
	r_explosioncolor_g = Cvar_Get ("r_explosioncolor_g", "0.5", CVAR_ARCHIVE, NULL);
	r_explosioncolor_b = Cvar_Get ("r_explosioncolor_b", "0.25", CVAR_ARCHIVE, NULL);
	r_explosioncolor_a = Cvar_Get ("r_explosioncolor_a", "1", CVAR_ARCHIVE, NULL);

	r_explosion_start();
}

void R_NewExplosion(vec3_t org)
{
	int i, j;
	float dist;
	Uint8 noise[EXPLOSIONGRID*EXPLOSIONGRID];
	FractalNoise(noise, EXPLOSIONGRID, 2);
	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha <= 0.0f)
		{
			explosion[i].alpha = EXPLOSIONFADESTART;
			for (j = 0;j < EXPLOSIONVERTS;j++)
			{
				dist = noise[explosionnoiseindex[j]] * (1.0f / 512.0f) + 0.5;
				explosion[i].vert[j][0] = explosionspherevert[j][0] * dist + org[0];
				explosion[i].vert[j][1] = explosionspherevert[j][1] * dist + org[1];
				explosion[i].vert[j][2] = explosionspherevert[j][2] * dist + org[2];
				explosion[i].vertvel[j][0] = explosionspherevertvel[j][0] * dist;
				explosion[i].vertvel[j][1] = explosionspherevertvel[j][1] * dist;
				explosion[i].vertvel[j][2] = explosionspherevertvel[j][2] * dist;
			}
			break;
		}
	}
}

void R_DrawExplosion(explosion_t *e)
{
	int i;
	float r, g, b, a;

	memcpy (&v_array[v_index], &e->vert[0][0], sizeof(float[3]) * EXPLOSIONVERTS);
	memcpy (&tc_array[v_index], &explosiontexcoords[0][0], sizeof(float[2]) * EXPLOSIONVERTS);
	for (i = 0; i < EXPLOSIONINDICES; i++) {
		vindices[i_index + i] = explosiontris[0][i] + v_index;
	}

	r = r_explosioncolor_r->value;
	g = r_explosioncolor_g->value;
	b = r_explosioncolor_b->value;
	a = r_explosioncolor_a->value * e->alpha;

	for (i = 0; i < EXPLOSIONVERTS; i++)
		VectorSet4(c_array[v_index + i], r, g, b, a);

	v_index += EXPLOSIONVERTS;
	i_index += EXPLOSIONINDICES;
	
	if (((v_index + EXPLOSIONINDICES) >= MAX_VERTEX_ARRAYS) ||
			((i_index + EXPLOSIONINDICES) >= MAX_VERTEX_INDICES)) {
		TWI_PreVDrawCVA (0, EXPLOSIONTRIS * 3);
		qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}
}

void R_MoveExplosion(explosion_t *e, float frametime)
{
	int i;
	vec3_t end;
	e->alpha -= frametime * EXPLOSIONFADERATE;
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			end[0] = e->vert[i][0] + frametime * e->vertvel[i][0];
			end[1] = e->vert[i][1] + frametime * e->vertvel[i][1];
			end[2] = e->vert[i][2] + frametime * e->vertvel[i][2];
			if (r_explosionclip->value)
			{
				float f, dot;
				vec3_t impact, normal;
				f = TraceLine(e->vert[i], end, impact, normal);
				VectorCopy(impact, e->vert[i]);
				if (f < 1)
				{
					// clip velocity against the wall
					dot = -DotProduct(e->vertvel[i], normal);
					e->vertvel[i][0] += normal[0] * dot;
					e->vertvel[i][1] += normal[1] * dot;
					e->vertvel[i][2] += normal[2] * dot;
				}
			}
			else
			{
				VectorCopy(end, e->vert[i]);
			}
		}
	}
}

void R_MoveExplosions(void)
{
	int i;

	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha > 0.0f)
		{
			if (explosion[i].starttime > cl.time)
			{
				explosion[i].alpha = 0;
				continue;
			}
			R_MoveExplosion(&explosion[i], cl.time - cl.oldtime);
		}
	}
}

void R_DrawExplosions(void)
{
	int i;
	if (!r_drawexplosions->value)
		return;
	qglEnableClientState (GL_COLOR_ARRAY);
	qglBindTexture(GL_TEXTURE_2D, explosiontexture);
	if (gl_cull->value)
		qglDisable (GL_CULL_FACE);
	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha > 0.0f)
		{
			R_DrawExplosion(&explosion[i]);
		}
	}
	if (v_index || i_index) {
		TWI_PreVDrawCVA (0, EXPLOSIONTRIS * 3);
		qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}
	qglDisableClientState (GL_COLOR_ARRAY);
	if (gl_cull->value)
		qglEnable (GL_CULL_FACE);
}
