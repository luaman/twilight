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

#include "common.h"
#include "console.h"
#include "cvar.h"
#include "glquake.h"
#include "bothdefs.h"

#define MAX_PARTICLES			2048	// default max # of particles at one
										// time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's
										// on the command line

typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob,
		pt_blob2
} ptype_t;

typedef struct particle_s {
	vec3_t      org;
	Uint8       color;
	vec3_t      vel;
	float       ramp;
	float       die;
	ptype_t     type;
} particle_t;

int         ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int         ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int         ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

particle_t *particles, **freeparticles;
int         numparticles, r_maxparticles;

inline particle_t *
particle_new (ptype_t type, vec3_t org, vec3_t vel, float die, Uint8 color,
		float ramp)
{
	particle_t *part;

	if (numparticles >= r_maxparticles) {
//		Con_Printf("FAILED PARTICLE ALLOC! %d %d\n", numparticles, r_maxparticles);
		return NULL;
	}

	part = &particles[numparticles++];
	part->type = type;
	VectorCopy(org, part->org);
	VectorCopy(vel, part->vel);
	part->die = die;
	part->color = color;
	part->ramp = ramp;

	return part;
}


/*
===============
R_InitParticles
===============
*/
void
R_InitParticles (void)
{
	int         i;

	i = COM_CheckParm ("-particles");

	if (i) {
		r_maxparticles = (int) (Q_atoi (com_argv[i + 1]));
		if (r_maxparticles < ABSOLUTE_MIN_PARTICLES)
			r_maxparticles = ABSOLUTE_MIN_PARTICLES;
	} else {
		r_maxparticles = MAX_PARTICLES;
	}

	particles = (particle_t *) calloc (r_maxparticles, sizeof (particle_t));
	freeparticles = (particle_t **) calloc (r_maxparticles,
			sizeof (particle_t *));
}

/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t      avelocities[NUMVERTEXNORMALS];
float       beamlength = 16;
vec3_t      avelocity = { 23, 7, 3 };
float       partstep = 0.01;
float       timescale = 0.01;

void
R_EntityParticles (entity_t *ent)
{
	int         i;
	float       angle;
	float       sr, sp, sy, cr, cp, cy;
	vec3_t      forward, org;
	float       dist = 64;

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
		org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
			forward[0] * beamlength;
		org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
			forward[1] * beamlength;
		org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
			forward[2] * beamlength;
		particle_new(pt_explode, org, r_origin, realtime + 0.01, 0x6f, 0);
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
	numparticles = 0;
}


void
R_ReadPointFile_f (void)
{
	FILE       *f;
	vec3_t      org;
	int         r;
	int         c;
	char        name[MAX_OSPATH];
	extern cvar_t *cl_mapname;

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

		particle_new(pt_static, org, r_origin, 99999, (-c) & 15, 0);
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
	vec3_t      org, dir;
	int         i, count, msgcount, color;

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
	int		i, j, type;
	vec3_t	porg, vel;

	for (i = 0; i < 1024; i++) {
		if (i & 1) {
			type = pt_explode;
			for (j = 0; j < 3; j++) {
				porg[j] = org[j] + ((Q_rand () % 32) - 16);
				vel[j] = (Q_rand () % 512) - 256;
			}
		} else {
			type = pt_explode2;
			for (j = 0; j < 3; j++) {
				porg[j] = org[j] + ((Q_rand () % 32) - 16);
				vel[j] = (Q_rand () % 512) - 256;
			}
		}
		particle_new(type, porg, vel, realtime + 5, ramp1[0], Q_rand () & 3);
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
	int		i, j;
	int		colorMod = 0, color;
	vec3_t	porg, vel;

	for (i = 0; i < 512; i++) {
		color = colorStart + (colorMod % colorLength);
		colorMod++;
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + ((Q_rand () % 32) - 16);
			vel[j] = (Q_rand () % 512) - 256;
		}
		particle_new(pt_blob, porg, vel, realtime + 0.3, color, 0);
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
	int		i, j;
	float	pdie;
	vec3_t	porg, pvel;

	for (i = 0; i < 1024; i++) {
		pdie = realtime + 1 + (Q_rand () & 8) * 0.05;

		if (i & 1) {
			for (j = 0; j < 3; j++) {
				porg[j] = org[j] + ((Q_rand () % 32) - 16);
				pvel[j] = (Q_rand () % 512) - 256;
			}
			particle_new (pt_blob, porg, pvel, pdie, 66 + Q_rand() % 6, 0);
		} else {
			for (j = 0; j < 3; j++) {
				porg[j] = org[j] + ((Q_rand () % 32) - 16);
				pvel[j] = (Q_rand () % 512) - 256;
			}
			particle_new (pt_blob2, porg, pvel, pdie, 150 + Q_rand() % 6, 0);
		}
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
	int		i, j, pcolor;
	float	pdie;
	vec3_t	porg, pvel;

	for (i = 0; i < count; i++) {
		if (count == 1024) {			// rocket explosion
			if (i & 1) {
				for (j = 0; j < 3; j++) {
					porg[j] = org[j] + ((Q_rand () % 32) - 16);
					pvel[j] = (Q_rand () % 512) - 256;
				}
				particle_new(pt_explode, porg, pvel, realtime + 5, ramp1[0], Q_rand () & 3);
			} else {
				for (j = 0; j < 3; j++) {
					porg[j] = org[j] + ((Q_rand () % 32) - 16);
					pvel[j] = (Q_rand () % 512) - 256;
				}
				particle_new(pt_explode2, porg, pvel, realtime + 5, ramp1[0], Q_rand () & 3);
			}
		} else {
			pdie = realtime + 0.1 * (Q_rand () % 5);
			pcolor = (color & ~7) + (Q_rand () & 7);
			for (j = 0; j < 3; j++) {
				porg[j] = org[j] + ((Q_rand () & 15) - 8);
				pvel[j] = dir[j] * 15;	// + (Q_rand()%300)-150;
			}
			particle_new(pt_slowgrav, porg, pvel, pdie, pcolor, 0);
		}
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
	int         i, j, k, pcolor;
	float       vel, pdie;
	vec3_t      dir, porg, pvel;

	for (i = -16; i < 16; i++)
		for (j = -16; j < 16; j++)
			for (k = 0; k < 1; k++) {
				pdie = realtime + 2 + (rand () & 31) * 0.02;
				pcolor = 224 + (rand () & 7);

				dir[0] = j * 8 + (Q_rand () & 7);
				dir[1] = i * 8 + (Q_rand () & 7);
				dir[2] = 256;

				porg[0] = org[0] + dir[0];
				porg[1] = org[1] + dir[1];
				porg[2] = org[2] + (Q_rand () & 63);

				VectorNormalizeFast (dir);
				vel = 50 + (Q_rand () & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_slowgrav, porg, pvel, pdie, pcolor, 0);
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
	int         i, j, k, pcolor;
	float       vel, pdie;
	vec3_t      dir, porg, pvel;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4) {
				pdie = realtime + 0.2 + (Q_rand () & 7) * 0.02;
				pcolor = 7 + (Q_rand () & 7);

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				porg[0] = org[0] + i + (Q_rand () & 3);
				porg[1] = org[1] + j + (Q_rand () & 3);
				porg[2] = org[2] + k + (Q_rand () & 3);

				VectorNormalizeFast (dir);
				vel = 50 + (Q_rand () & 63);
				VectorScale (dir, vel, pvel);
				particle_new(pt_slowgrav, porg, pvel, pdie, pcolor, 0);
			}
}

void
R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t      vec, avec, porg, pvel;
	float       len, pdie, pramp;
	int         j, lsub, pcolor;
	static int  tracercount;
	ptype_t		ptype;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 3, vec);

	while (len > 0) {
		lsub = 3;

		pdie = realtime + 2;
		VectorClear(porg);
		VectorClear(pvel);
		pramp = 0;
		pcolor = 0;
		ptype = 0;

		switch (type) {
			case 0:					// rocket trail
				pramp = (Q_rand () & 3);
				pcolor = ramp3[(int) pramp];
				ptype = pt_fire;
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () % 6) - 3);
				break;

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
				pdie = realtime + 0.5;
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
				pdie = realtime + 0.3;
				for (j = 0; j < 3; j++)
					porg[j] = start[j] + ((Q_rand () & 15) - 8);
				break;
		}

		particle_new(ptype, porg, pvel, pdie, pcolor, pramp);

		VectorScale(vec, lsub, avec);
		VectorAdd (start, avec, start);
		len -= lsub;
	}
}

/*
===============
R_DrawParticles
===============
*/
void
R_DrawParticles (void)
{
	particle_t *p;
	float       grav;
	int         i, j, k, activeparticles, maxparticle, vnum;
	float       time2, time3;
	float       time1;
	float       dvel;
	float       frametime;
	vec3_t      up, right;
	float       scale;
	float		theAlpha, *at;

	qglBindTexture (GL_TEXTURE_2D, particletexture);
	qglEnableClientState (GL_COLOR_ARRAY);

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);
	frametime = host_frametime;
	time3 = frametime * 15;
	time2 = frametime * 10;				// 15;
	time1 = frametime * 5;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	vnum = 0;

	for (k = 0, p = particles; k < numparticles; k++, p++) {
		// LordHavoc: this is probably no longer necessary, as it is
		// checked at the end, but could still happen on weird particle
		// effects, left for safety...
		if (p->die <= realtime) {
			freeparticles[j++] = p;
			continue;
		}
		maxparticle = k;
		activeparticles++;

		// hack a scale up to keep particles from disapearing
		scale =
			(p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] -
												  r_origin[1]) * vpn[1]
			+ (p->org[2] - r_origin[2]) * vpn[2];

		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		at = d_8tofloattable[p->color];

		if (p->type == pt_fire)
			theAlpha = (255 - p->ramp * (1 / 6)) / 255;
		else
			theAlpha = 1;

		VectorSet4(c_array[vnum + 0], at[0], at[1], at[2], theAlpha);
		VectorSet4(c_array[vnum + 1], at[0], at[1], at[2], theAlpha);
		VectorSet4(c_array[vnum + 2], at[0], at[1], at[2], theAlpha);
		VectorSet2(tc_array[vnum + 0], 0, 0);
		VectorSet2(tc_array[vnum + 1], 1, 0);
		VectorSet2(tc_array[vnum + 2], 0, 1);
		VectorSet3(v_array[vnum + 0], p->org[0], p->org[1], p->org[2]);
		VectorSet3(v_array[vnum + 1], p->org[0] + up[0] * scale,
					p->org[1] + up[1] * scale,
					p->org[2] + up[2] * scale);
		VectorSet3(v_array[vnum + 2], p->org[0] + right[0] * scale,
					p->org[1] + right[1] * scale,
					p->org[2] + right[2] * scale);
		vnum += 3;

		if ((vnum + 3) >= MAX_VERTEX_ARRAYS) {
			qglDrawArrays (GL_TRIANGLES, 0, vnum);
			vnum = 0;
		}
		VectorMA (p->org, frametime, p->vel, p->org);

		switch (p->type) {
			case pt_static:
				break;
			case pt_fire:
				p->ramp += time1;
				if (p->ramp >= 6)
					p->die = -1;
				else
					p->color = ramp3[(int) p->ramp];
				p->vel[2] += grav;
				break;

			case pt_explode:
				p->ramp += time2;
				if (p->ramp >= 8)
					p->die = -1;
				else
					p->color = ramp1[(int) p->ramp];
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_explode2:
				p->ramp += time3;
				if (p->ramp >= 8)
					p->die = -1;
				else
					p->color = ramp2[(int) p->ramp];
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
		}
	}

	if (vnum) {
		qglDrawArrays (GL_TRIANGLES, 0, vnum);
		vnum = 0;
	}
	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles &&
				particles[maxparticle].die <= realtime)
			maxparticle--;
	}
	numparticles = activeparticles;

	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor3f (1, 1, 1);
}
