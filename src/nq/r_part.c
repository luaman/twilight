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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "mathlib.h"
#include "client.h"

#define MAX_PARTICLES			2048	// default max # of particles
#define ABSOLUTE_MIN_PARTICLES	2		// no fewer than this no matter what

extern int part_tex_dot;
extern int part_tex_spark;
extern int part_tex_smoke;
extern int part_tex_smoke_ring;

static cvar_t *r_particles, *r_base_particles, *r_cone_particles;

static int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

typedef enum {
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode, pt_explode2,
	pt_blob, pt_blob2,
	pt_torch, pt_torch2,
	pt_teleport1, pt_teleport2,
	pt_rtrail
} ptype_t;

typedef struct {
	// Some effects need a base origin.
	vec3_t		org1;
	vec3_t		org2;
	vec3_t		org3;

	vec3_t		normal;
	vec4_t		color1;
	vec4_t		color2;
	float		scale;
	float		ramp;
	float		die;
	ptype_t		type;
} cone_particle_t;

static cone_particle_t *cone_particles, **free_cone_particles;
static int num_cone_particles, max_cone_particles;

inline qboolean
new_cone_particle (ptype_t type, vec3_t org1, vec3_t org2, vec3_t org3,
		vec4_t color1, vec4_t color2, float ramp, float scale, float die)
{
	cone_particle_t	   *p;
	vec3_t				normal;

	if (num_cone_particles >= max_cone_particles) {
		// Out of particles.
		return false;
	}

	p = &cone_particles[num_cone_particles++];
	p->type = type;
	VectorCopy (org1, p->org1);
	VectorCopy (org2, p->org2);
	VectorCopy (org3, p->org3);
	VectorCopy4 (color1, p->color1);
	VectorCopy4 (color2, p->color2);
	p->ramp = ramp;
	p->die = realtime + die;
	p->scale = scale;

	VectorSubtract (org1, org2, normal);
	VectorNormalize (normal);
	VectorCopy (normal, p->normal);

	return true;
}
	
typedef struct {
	vec3_t		org;
	vec3_t		vel;
	vec4_t		color;
	float		scale;
	float		ramp;
	float		die;
	ptype_t		type;
} base_particle_t;

static base_particle_t *base_particles, **free_base_particles;
static int num_base_particles, max_base_particles;

inline qboolean
new_base_particle (ptype_t type, vec3_t org, vec3_t vel, vec4_t color,
		float ramp, float scale, float die)
{
	base_particle_t	   *p;

	if (num_base_particles >= max_base_particles)
		// Out of particles.
		return false;

	p = &base_particles[num_base_particles++];
	p->type = type;
	VectorCopy (org, p->org);
	VectorCopy (vel, p->vel);
	VectorCopy4 (color, p->color);
	p->ramp = ramp;
	p->die = realtime + die;
	p->scale = scale;

	return true;
}
	
inline qboolean
new_base_particle_oc (ptype_t type, vec3_t org, vec3_t vel, int color,
		float ramp, float scale, float die)
{
	vec4_t		vcolor;

	VectorCopy4 (d_8tofloattable[color], vcolor);	
	return new_base_particle (type, org, vel, vcolor, ramp, scale, die);
}
	
/*
===============
R_InitParticles
===============
*/
void
R_InitParticles (void)
{
	int			i;

	r_particles = Cvar_Get ("r_particles", "1", CVAR_NONE, NULL);
	r_base_particles = Cvar_Get ("r_base_particles", "1", CVAR_NONE, NULL);
	r_cone_particles = Cvar_Get ("r_cone_particles", "1", CVAR_NONE, NULL);

	i = COM_CheckParm ("-particles");

	if (i) {
		max_base_particles = (int) (Q_atoi (com_argv[i + 1]));
	} else {
		max_base_particles = MAX_PARTICLES;
	}

	max_base_particles = max(max_base_particles, ABSOLUTE_MIN_PARTICLES);
	max_cone_particles = max_base_particles;

	base_particles = (base_particle_t *) calloc (max_base_particles,
			sizeof (base_particle_t));
	free_base_particles = (base_particle_t **) calloc (max_base_particles,
			sizeof (base_particle_t *));
	cone_particles = (cone_particle_t *) calloc (max_cone_particles,
			sizeof (cone_particle_t));
	free_cone_particles = (cone_particle_t **) calloc (max_cone_particles,
			sizeof (cone_particle_t *));
}

/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t avelocities[NUMVERTEXNORMALS];
float beamlength = 16;

void
R_EntityParticles (entity_t *ent)
{
	int			i;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward, org;
	float		dist = 64;

	if (!avelocities[0][0]) {
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (Q_rand () & 255) * 0.01;
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		angle = realtime * avelocities[i][0];
		sy = Q_sin (angle);
		cy = Q_cos (angle);
		angle = realtime * avelocities[i][1];
		sp = Q_sin (angle);
		cp = Q_cos (angle);
		angle = realtime * avelocities[i][2];
		sr = Q_sin (angle);
		cr = Q_cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
		org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist + forward[0]
			* beamlength;
		org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist + forward[1]
			* beamlength;
		org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist + forward[2]
			* beamlength;
		new_base_particle_oc(pt_explode, org, r_origin, 0x6f, 0, -1, 0.01);
	}
}


/*
===============
R_ClearParticles
===============
*/
void
R_ClearParticles (void)
{
	num_base_particles = 0;
	num_cone_particles = 0;
}


void
R_ReadPointFile_f (void)
{
	FILE			   *f;
	vec3_t				org;
	int					r;
	int					c;
	char				name[MAX_OSPATH];
	extern cvar_t	   *cl_mapname;

	snprintf (name, sizeof (name), "maps/%s.pts", cl_mapname->string);

	COM_FOpenFile (name, &f);
	if (!f) {
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;) {
		r = fscanf (f, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		new_base_particle_oc (pt_static, org, r_origin, (-c) & 15, 0, -1,99999);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void
R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, color;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord ();
	for (i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar () * (1.0 / 16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();
	count = (msgcount == 255) ? 1024 : msgcount;

	R_RunParticleEffect (org, dir, color, count);
}

/*
===============
R_ParticleExplosion

===============
*/
void
R_ParticleExplosion (vec3_t org)
{
	int			i, j, type;
	vec3_t		porg, vel;

	for (i = 0; i < 512; i++) {
		type = (i & 1) ? pt_explode : pt_explode2;
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + ((Q_rand () % 32) - 16);
			vel[j] = (Q_rand () % 512) - 256;
		}
		new_base_particle_oc (type, porg, vel, ramp1[0], Q_rand () & 3, -1, 5);
	}
}

/*
===============
R_ParticleExplosion2

===============
*/
void
R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j;
	int			colorMod = 0, color;
	vec3_t		porg, vel;

	for (i = 0; i < 512; i++) {
		color = colorStart + (colorMod % colorLength);
		colorMod++;
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + ((Q_rand () % 32) - 16);
			vel[j] = (Q_rand () % 512) - 256;
		}
		new_base_particle_oc (pt_blob, porg, vel, color, 0, -1, 0.3);
	}
}

/*
===============
R_BlobExplosion

===============
*/
void
R_BlobExplosion (vec3_t org)
{
	int			i, j, color;
	float		pdie;
	vec3_t		porg, pvel;
	ptype_t		ptype;

	for (i = 0; i < 1024; i++) {
		pdie = 1 + (Q_rand () & 8) * 0.05;
		if (i & 1) {
			ptype = pt_blob;
			color = 66 + Q_rand() % 6;
		} else {
			ptype = pt_blob2;
			color = 150 + Q_rand() % 6;
		}

		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + ((Q_rand () % 32) - 16);
			pvel[j] = (Q_rand () % 512) - 256;
		}
		new_base_particle_oc (ptype, porg, pvel, color, 0, -1, pdie);
	}
}

/*
===============
R_RunParticleEffect

===============
*/
void
R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j, pcolor;
	float		pdie;
	vec3_t		porg, pvel;

	for (i = 0; i < count; i++) {
		pdie = 0.1 * (Q_rand () % 5);
		pcolor = (color & ~7) + (Q_rand () & 7);
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + ((Q_rand () & 15) - 8);
			pvel[j] = dir[j] * 15;	// + (Q_rand()%300)-150;
		}
		new_base_particle_oc (pt_slowgrav, porg, pvel, pcolor, 0, -1, pdie);
	}
}


/*
===============
R_LavaSplash

===============
*/
void
R_LavaSplash (vec3_t org)
{
	int			i, j, pcolor;
	float		vel, pdie;
	vec3_t		dir, porg, pvel;

	for (i = -16; i < 16; i++)
		for (j = -16; j < 16; j++) {
			pdie = 2 + (Q_rand () & 31) * 0.02;
			pcolor = 224 + (Q_rand () & 7);

			dir[0] = j * 8 + (Q_rand () & 7);
			dir[1] = i * 8 + (Q_rand () & 7);
			dir[2] = 256;

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + (Q_rand () & 63);

			VectorNormalizeFast (dir);
			vel = 50 + (Q_rand () & 63);
			VectorScale (dir, vel, pvel);
			new_base_particle_oc (pt_slowgrav, porg, pvel, pcolor, 0, -1, pdie);
		}
}

/*
===============
R_TeleportSplash

===============
*/
void
R_TeleportSplash (vec3_t org)
{
	vec3_t		porg_top, porg_bottom;
	vec4_t		color1, color2;

	org[2] += 4;

	porg_top[0] = org[0];
	porg_top[1] = org[1];
	porg_top[2] = org[2] + 40;
	porg_bottom[0] = org[0];
	porg_bottom[1] = org[1];
	porg_bottom[2] = org[2] - 40;

	VectorSet4 (color1, 0.4, 0.4, 1.0, 1);
	VectorSet4 (color2, 0, 0, 0, 0.5);

	new_cone_particle (pt_teleport1, porg_top, porg_bottom, org, color1,
			color2, 0, 20, 5);

	new_cone_particle (pt_teleport2, porg_bottom, porg_top, org, color1,
			color2, 0, 20, 5);
}

/*
==========
R_Torch

==========
*/
void 
R_Torch (entity_t *ent, qboolean torch2)
{
	vec3_t		porg, pvel;
	vec4_t		color;

	if (!r_particles->value)
		return;

//	VectorSet4 (color, 227.0 / 255.0, 151.0 / 255.0, 79.0 / 255.0, .5);
	VectorSet4 (color, 0.89, 0.59, 0.31, 0.5);
	VectorSet (pvel, (Q_rand() & 3) - 2, (Q_rand() & 3) - 2, 0);
	VectorSet (porg, ent->origin[0], ent->origin[1],
			ent->origin[2] + 4);

	if (torch2) { 
		// used for large torches (eg, start map near spawn)
		porg[2] = ent->origin[2] - 2;
		VectorSet (pvel, (Q_rand() & 7) - 4, (Q_rand() & 7) - 4, 0);
		new_base_particle (pt_torch2, porg, pvel, color,
				Q_rand () & 3, ent->frame ? 30 : 10, 5);
	} else { 
		// wall torches
		new_base_particle (pt_torch, porg, pvel, color,
				Q_rand () & 3, 10, 5);
	}
}

void
R_RocketConeTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	vec3_t		point1, point2, cur;
	vec4_t		color1, color2;
	float		len;
	int			lsub;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorCopy (start, cur);

	while (len > 0) {
		lsub = 15;

		VectorSet4 (color1, 0.8, 0.1, 0.1, 0.3);
		VectorSet4 (color2, 0.05, 0.05, 0.05, 0.0);
		VectorMA (cur, -10, vec, point1);
		VectorMA (cur, lsub + 9, vec, point2);
		new_cone_particle (pt_rtrail, point1, point2, vec3_origin, color1,
				color2, 0, 2, 15);
		VectorMA (cur, lsub, vec, cur);
		len -= lsub;
	}
}

void
R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec, avec, porg, pvel;
	float		len, pdie, pramp;
	int			j, lsub, pcolor;
	static int	tracercount;
	ptype_t		ptype;

	if (type == 0) {
		R_RocketConeTrail (start, end, type);
		return;
	}

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);

	while (len > 0) {
		lsub = 3;

		pdie = 2;
		VectorClear(porg);
		VectorClear(pvel);
		pramp = 0;
		pcolor = 0;
		ptype = 0;

		switch (type) {
			case 1:					// smoke smoke
				pramp = (Q_rand () & 3) + 2;
				pcolor = ramp3[(int) pramp];
				ptype = pt_fire;
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () % 6) - 3);
				break;

			case 2:					// blood
				ptype = pt_grav;
				pcolor = 67 + (Q_rand () & 3);
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () % 6) - 3);
				break;

			case 3:
			case 5:					// tracer
				pdie = 0.5;
				ptype = pt_static;
				if (type == 3)
					pcolor = 52 + ((tracercount & 4) << 1);
				else
					pcolor = 230 + ((tracercount & 4) << 1);

				tracercount++;

				VectorCopy (start, porg);
				if (tracercount & 1) {
					pvel[0] = 30 * vec[1];
					pvel[1] = 30 * -vec[0];
				} else {
					pvel[0] = 30 * -vec[1];
					pvel[1] = 30 * vec[0];
				}
				break;

			case 4:					// slight blood
				ptype = pt_grav;
				pcolor = 67 + (Q_rand () & 3);
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () % 6) - 3);
				lsub += 3;
				break;

			case 6:					// voor trail
				pcolor = 9 * 16 + 8 + (Q_rand () & 3);
				ptype = pt_static;
				pdie = 0.3;
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () & 15) - 8);
				break;
		}

		new_base_particle_oc (ptype, porg, pvel, pcolor, pramp, -1, pdie);

		VectorScale(vec, lsub, avec);
		VectorAdd (start, avec, start);
		len -= lsub;
	}
}

//#define MOD_POINTINLEAF
/*
===============
R_Draw_Base_Particles
===============
*/
static void
R_Draw_Base_Particles (void)
{
#ifdef MOD_POINTINLEAF
	mleaf_t				*mleaf;
#endif
	base_particle_t	   *p;
	int					i, j, k, activeparticles, maxparticle;
	float				time1, time2, time3;
	float				grav, dvel;
	float				frametime;
	float				scale, *corner;

	qglBindTexture (GL_TEXTURE_2D, part_tex_dot);

	frametime = host_frametime;
	time1 = frametime * 5;
	time2 = frametime * 10;
	time3 = frametime * 15;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	activeparticles = 0;
	maxparticle = -1;
	v_index = 0;
	j = 0;

	for (k = 0, p = base_particles; k < num_base_particles; k++, p++) {
		if (p->die <= realtime) {
			free_base_particles[j++] = p;
			continue;
		}

		maxparticle = k;
		activeparticles++;

#ifdef MOD_POINTINLEAF
		mleaf = Mod_PointInLeaf(p->org, cl.worldmodel);
		if ((mleaf->contents == CONTENTS_SOLID) ||
				(mleaf->contents == CONTENTS_SKY)) {
			p->die = -1;
			goto R_Draw_Base_Particles__physics;
		}
		if (mleaf->visframe != r_framecount)
			goto R_Draw_Base_Particles__physics;
#endif

		VectorCopy4 (p->color, c_array[v_index + 0]);
		VectorCopy4 (p->color, c_array[v_index + 1]);
		VectorCopy4 (p->color, c_array[v_index + 2]);
		VectorCopy4 (p->color, c_array[v_index + 3]);
		VectorSet2(tc_array[v_index + 0], 1, 1);
		VectorSet2(tc_array[v_index + 1], 0, 1);
		VectorSet2(tc_array[v_index + 2], 0, 0);
		VectorSet2(tc_array[v_index + 3], 1, 0);

		if (p->scale < 0) {
			scale = ((p->org[0] - r_origin[0]) * vpn[0]) + 
				((p->org[1] - r_origin[1]) * vpn[1]) +
				((p->org[2] - r_origin[2]) * vpn[2]);
			if (scale < 20)
				scale = -p->scale;
			else
				scale = (-p->scale) + scale * 0.004;
		} else {
			scale = p->scale;
		}

		corner = v_array[v_index];
		VectorTwiddleS (p->org, vup, vright, scale * -0.5, v_array[v_index]);
		VectorTwiddle (corner, vup, scale, vright, 0    , v_array[v_index + 1]);
		VectorTwiddle (corner, vup, scale, vright, scale, v_array[v_index + 2]);
		VectorTwiddle (corner, vup, 0    , vright, scale, v_array[v_index + 3]);

		v_index += 4;

		if ((v_index + 4) >= MAX_VERTEX_ARRAYS) {
			qglDrawArrays (GL_QUADS, 0, v_index);
			v_index = 0;
		}

#ifdef MOD_POINTINLEAF
R_Draw_Base_Particles__physics:
#endif
		VectorMA (p->org, frametime, p->vel, p->org);

		switch (p->type) {
			case pt_static:
				break;
			case pt_fire:
				p->ramp += time1;
				if (p->ramp >= 6)
					p->die = -1;
				else
					p->color[3] -= frametime;
				p->vel[2] += grav;
				break;

			case pt_explode:
				p->ramp += time2;
				if (p->ramp >= 8)
					p->die = -1;
				else
					VectorCopy (d_8tofloattable[ramp1[(int) p->ramp]],
							p->color);
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_explode2:
				p->ramp += time3;
				if (p->ramp >= 8)
					p->die = -1;
				else
					VectorCopy (d_8tofloattable[ramp2[(int) p->ramp]],
							p->color);
				for (i = 0; i < 3; i++)
					p->vel[i] -= p->vel[i] * frametime;
				p->vel[2] -= grav;
				break;

			case pt_blob:
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_blob2:
				for (i = 0; i < 2; i++)
					p->vel[i] -= p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_grav:
			case pt_slowgrav:
				p->vel[2] -= grav;
				break;
			case pt_torch:
				p->color[3] -= (frametime * 64 / 255);
				p->scale -= frametime * 2;
				p->vel[2] += grav * 0.4;
				if (p->scale < 0)
					p->die = -1;
				break;
			case pt_torch2:
				p->color[3] -= (frametime * 64 / 255);
				p->scale -= frametime * 4;
				p->vel[2] += grav;
				if (p->scale < 0)
					p->die = -1;
				break;
			default:
				break;
		}
		if ((p->color[3] < 0))
			p->die = -1;
	}

	if (v_index) {
		qglDrawArrays (GL_QUADS, 0, v_index);
		v_index = 0;
	}

	k = 0;
	while (maxparticle >= activeparticles) {
		*free_base_particles[k++] = base_particles[maxparticle--];
		while (maxparticle >= activeparticles &&
				base_particles[maxparticle].die <= realtime)
			maxparticle--;
	}
	num_base_particles = activeparticles;
}

extern float bubble_sintable[17], bubble_costable[17];

/*
===============
R_Draw_Cone_Particles
===============
*/
static void
R_Draw_Cone_Particles (void)
{
	cone_particle_t	   *p;
	int					i, i2, j, k, activeparticles, maxparticle;
	int					v_center, v_first, v_last;
	float				frametime, teletime;
	vec3_t				v_up, v_right;
	float			   *bub_sin, *bub_cos;

	qglBindTexture (GL_TEXTURE_2D, part_tex_smoke_ring);
	if (gl_cull->value)
		qglDisable (GL_CULL_FACE);

	frametime = host_frametime;
	teletime = frametime * 120;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	v_index = 0;
	i_index = 0;

	for (k = 0, p = cone_particles; k < num_cone_particles; k++, p++) {
		if (p->die <= realtime) {
			free_cone_particles[j++] = p;
			continue;
		}

		maxparticle = k;
		activeparticles++;

		VectorVectors (p->normal, v_right, v_up);

		VectorCopy (p->org2, v_array[v_index]);
		VectorCopy4 (p->color2, c_array[v_index]);
		VectorSet2 (tc_array[v_index], 0.5, 0.5);
		v_center = v_index++;
		v_last = -1;
		v_first = -1;
		bub_sin = bubble_sintable,
		bub_cos = bubble_costable;
		for (i = 0; i < 16; i++, bub_sin++, bub_cos++) {
			for (i2 = 0; i2 < 3; i2++)
				v_array[v_index][i2] = p->org1[i2] +
					((v_right[i2] * (*bub_cos)) +
					(v_up[i2] * (*bub_sin))) * p->scale;

			VectorSet2 (tc_array[v_index], *bub_cos * 0.5 + 0.5,
					*bub_sin * 0.5 + 0.5);
			VectorCopy4 (p->color1, c_array[v_index]);
			if (v_last != -1) {
				vindices[i_index + 0] = v_center;
				vindices[i_index + 1] = v_last;
				vindices[i_index + 2] = v_index;
				i_index += 3;
			} else {
				v_first = v_index;
			}
			v_last = v_index;
			v_index++;
		}
		vindices[i_index + 0] = v_center;
		vindices[i_index + 1] = v_last;
		vindices[i_index + 2] = v_first;
		i_index += 3;

		if (((i_index + (17 * 3)) >= MAX_VERTEX_INDICES) ||
				(v_index + 17) >= MAX_VERTEX_ARRAYS) {
			if (gl_cva)
				qglLockArraysEXT (0, v_index);
			qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
			if (gl_cva)
				qglUnlockArraysEXT ();
			v_index = 0;
			i_index = 0;
		}

		switch (p->type) {
			case pt_teleport2:
				switch ((int) p->ramp & 1) {
					case 0:
						p->org1[2] += teletime;
						p->org2[2] -= teletime;
						if (p->org1[2] >= (p->org3[2] + 40))
							p->ramp++;
						break;
					case 1:
						p->org1[2] -= teletime;
						p->org2[2] += teletime;
						if (p->org1[2] <= (p->org3[2] - 40))
							p->ramp++;
						break;
				}
				if (p->ramp >= 1)
					p->die = -1;
				break;
			case pt_teleport1:
				switch ((int) p->ramp & 1) {
					case 1:
						p->org1[2] += teletime;
						p->org2[2] -= teletime;
						if (p->org1[2] >= (p->org3[2] + 40))
							p->ramp++;
						break;
					case 0:
						p->org1[2] -= teletime;
						p->org2[2] += teletime;
						if (p->org1[2] <= (p->org3[2] - 40))
							p->ramp++;
						break;
				}
				if (p->ramp >= 1)
					p->die = -1;
				break;
			case pt_rtrail:
				p->color1[3] -= frametime * 0.5;
				if (p->color1[3] <= 0)
					p->die = -1;
//				p->color1[0] -= frametime * 0.5;
				if (p->color1[1] < p->color1[0]) {
					p->color1[1] += frametime * 0.50;
					p->color1[2] += frametime * 0.50;
					p->color1[3] += frametime * 0.50;
				}
				p->scale += frametime * 1.5;
				break;
			default:
				break;
		}
	}

	if (v_index || i_index) {
		if (gl_cva)
			qglLockArraysEXT (0, v_index);
		qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		if (gl_cva)
			qglUnlockArraysEXT ();
		v_index = 0;
		i_index = 0;
	}

	k = 0;
	while (maxparticle >= activeparticles) {
		*free_cone_particles[k++] = cone_particles[maxparticle--];
		while (maxparticle >= activeparticles &&
				cone_particles[maxparticle].die <= realtime)
			maxparticle--;
	}
	num_cone_particles = activeparticles;

	if (gl_cull->value)
		qglEnable (GL_CULL_FACE);
}

/*
===============
R_DrawParticles
===============
*/
void
R_DrawParticles (void)
{
	qglEnableClientState (GL_COLOR_ARRAY);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
	R_Draw_Base_Particles();
	R_Draw_Cone_Particles();
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor3f(1, 1, 1);
}

