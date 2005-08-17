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

#include <stdlib.h>	/* for rand() and calloc() */
#include <string.h> /* for memcpy() */

#include "alias.h"
#include "brush.h"
#include "cclient.h"
#include "collision.h"
#include "common.h"
#include "cvar.h"
#include "explosion.h"
#include "gen_textures.h"
#include "gl_arrays.h"
#include "gl_info.h"
#include "gl_main.h"
#include "mathlib.h"
#include "quakedef.h"
#include "r_part.h"
#include "vis.h"

extern cvar_t *cl_mapname;

static inline void
VecRBetween (vec3_t c1, vec3_t c2, vec3_t out)
{
	float f = lhrandom(0, 1);
	VectorSubtract(c2, c1, out);
	VectorMA(c1, f, out, out);
}

static memzone_t *part_zone;

static cvar_t *r_particles, *r_particle_physics;
static cvar_t *r_base_particles, *r_xbeam_particles;

static int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };

typedef enum
{
	pt_normal,
	pt_fire,
	pt_explode, pt_explode2,
	pt_blob, pt_blob2,
	pt_lightning
}
ptype_t;

typedef struct
{
	vec3_t		org1;
	vec3_t		org2;
	vec3_t		normal;
	float		len;

	vec4_t		color;
	float		thickness;
	float		scroll;
	float		repeat_scale;

	float		die;
	ptype_t		type;
}
xbeam_particle_t;

static xbeam_particle_t *xbeam_particles, **free_xbeam_particles;
static int num_xbeam_particles, max_xbeam_particles;

static inline qboolean
new_xbeam_particle (ptype_t type, vec3_t org1, vec3_t org2, vec4_t color,
		float thickness, float scroll, float repeat_scale, float die)
{
	xbeam_particle_t	*p;

	if (num_xbeam_particles >= max_xbeam_particles)
		// Out of particles
		return false;

	p = &xbeam_particles[num_xbeam_particles++];
	p->type = type;
	VectorCopy (org1, p->org1);
	VectorCopy (org2, p->org2);
	VectorCopy4 (color, p->color);
	p->die = ccl.time + die;
	p->thickness = thickness;
	p->scroll = scroll;
	p->repeat_scale = repeat_scale;

	// Get the normal and length of the xbeam. (FIXME: Put in the struct!)
	VectorSubtract (p->org2, p->org1, p->normal);
	p->len = VectorNormalize(p->normal);

	return true;
}

typedef struct
{
	ptype_t		type;
	int			texnum;

	vec3_t		org;
	vec3_t		vel;
	vec4_t		color;

	float		alphadie;

	// how much bounce-back from a surface the particle hits (0 = no physics,
	// 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	float		bounce;

	float		die;
	float		ramp;
	float		scale;
	float		scale_change;
	float		gravity;
	qboolean	draw;
}
base_particle_t;

static base_particle_t *base_particles, **free_base_particles;
static int num_base_particles, max_base_particles;

static inline base_particle_t *
new_base_particle (ptype_t type, vec3_t org, vec3_t vel, vec4_t color,
		float ramp, float scale, float die, float bounce)
{
	base_particle_t		*p;

	if (num_base_particles >= max_base_particles)
		// Out of particles
		return NULL;

	p = &base_particles[num_base_particles++];
	p->type = type;
	p->texnum = GTF_dot;
	VectorCopy (org, p->org);
	VectorCopy (vel, p->vel);
	VectorCopy4 (color, p->color);
	p->ramp = ramp;
	p->die = ccl.time + die;
	p->alphadie = 0;
	p->scale = scale;
	p->scale_change = 0;
	p->gravity = 1;
	p->bounce = bounce;

	return p;
}
	
static inline base_particle_t *
new_base_particle_oc (ptype_t type, vec3_t org, vec3_t vel, int color,
		float ramp, float scale, float die, float bounce)
{
	vec4_t		vcolor;

	VectorCopy4 (d_8tofloattable[color], vcolor);	
	return new_base_particle (type, org, vel, vcolor, ramp, scale, die, bounce);
}

static void
Part_AllocArrays ()
{
	if (max_base_particles)
	{
		base_particles = (base_particle_t *) Zone_Alloc (part_zone,
				max_base_particles * sizeof (base_particle_t));
		free_base_particles = (base_particle_t **) Zone_Alloc (part_zone,
				max_base_particles * sizeof (base_particle_t *));
	}
	else
	{
		base_particles = NULL;
		free_base_particles = NULL;
	}
	if (max_xbeam_particles)
	{
		xbeam_particles = (xbeam_particle_t *) Zone_Alloc (part_zone, 
				max_xbeam_particles * sizeof (xbeam_particle_t));
		free_xbeam_particles = (xbeam_particle_t **) Zone_Alloc (part_zone,
				max_xbeam_particles * sizeof (xbeam_particle_t *));
	}
	else
	{
		xbeam_particles = NULL;
		free_xbeam_particles = NULL;
	}
}

static void
Part_FreeArrays ()
{
	if (base_particles)
	{
		Zone_Free (base_particles);
		Zone_Free (free_base_particles);
		base_particles = NULL;
		free_base_particles = NULL;
	}
	if (xbeam_particles)
	{
		Zone_Free (xbeam_particles);
		Zone_Free (free_xbeam_particles);
		xbeam_particles = NULL;
		free_xbeam_particles = NULL;
	}
}

static void
Part_CB (cvar_t *cvar)
{
	cvar = cvar;

	if (!r_particles || !r_base_particles || !r_xbeam_particles)
		return;

	if (!r_particles->ivalue)
	{
		max_base_particles = 0;
		max_xbeam_particles = 0;
	}
	else
	{
		max_base_particles = r_base_particles->ivalue;
		max_xbeam_particles = r_xbeam_particles->ivalue;
	}

	if (part_zone) {
		Part_FreeArrays ();
		Part_AllocArrays ();
	}
}

void
R_Particles_Init_Cvars (void)
{
	r_particles = Cvar_Get ("r_particles", "1", CVAR_NONE, &Part_CB);
	r_base_particles = Cvar_Get ("r_base_particles", "32768", CVAR_NONE, &Part_CB);
	r_xbeam_particles = Cvar_Get ("r_xbeam_particles", "32", CVAR_NONE, &Part_CB);
	r_particle_physics = Cvar_Get ("r_particle_physics", "0", CVAR_NONE, &Part_CB);
}

void
R_Particles_Init (void)
{
	part_zone = Zone_AllocZone ("Particle zone.");

	Part_AllocArrays ();
}

void
R_Particles_Shutdown (void)
{
	Part_FreeArrays ();
	Zone_PrintZone (true, part_zone);
	Zone_FreeZone (&part_zone);
}

#define NUMVERTEXNORMALS 162
static vec3_t avelocities[NUMVERTEXNORMALS];
#define beamlength 16.0f

void
R_EntityParticles (entity_common_t *ent)
{
	int			i;
	float		angle, dist, sr, sp, sy, cr, cp, cy;
	vec3_t		forward, org;

	if (!avelocities[0][0])
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = (rand () & 255) * 0.01;

	dist = 64;
	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		angle = ccl.time * avelocities[i][0];
		sy = Q_sin (angle);
		cy = Q_cos (angle);
		angle = ccl.time * avelocities[i][1];
		sp = Q_sin (angle);
		cp = Q_cos (angle);
		angle = ccl.time * avelocities[i][2];
		sr = Q_sin (angle);
		cr = Q_cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
		org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist
			+ forward[0] * beamlength;
		org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist
			+ forward[1] * beamlength;
		org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist
			+ forward[2] * beamlength;
		new_base_particle_oc (pt_explode, org, r.origin, 0x6f, 0, -1, 0.01, 0);
	}
}

void
R_ClearParticles (void)
{
	num_base_particles = 0;
	num_xbeam_particles = 0;
}

void
R_ReadPointFile_f (void)
{
	char			*cur, *tmp, *start;
	vec3_t			org;
	int				ret, c;
	char			name[MAX_OSPATH];
	base_particle_t	*p;

	snprintf (name, sizeof (name), "maps/%s.pts", cl_mapname->svalue);

	Com_Printf ("Reading %s...\n", name);
	cur = start = (char *) COM_LoadTempFile (name, true);

	if (!cur)
	{
		Com_Printf ("couldn't open %s\n", name);
		return;
	}

	c = 0;
	while (cur[0] && (tmp = strchr (cur, '\n'))) {
		tmp[0] = '\n';
		ret = sscanf (cur, "%f %f %f", &org[0], &org[1], &org[2]);
		cur = tmp + 1;
		if (ret != 3)
			break;

		c++;
		p = new_base_particle_oc (pt_normal, org, r.origin, (-c) & 15, 0, -1,
			99999, 0);
		if (!p)
			return;
		p->gravity = 0;
	}

	Zone_Free (start);
	Com_Printf ("%i points read\n", c);
}

/*
===============
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

void
R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j, colorMod, color;
	vec3_t		porg, vel;

	colorMod = 0;
	for (i = 0; i < 512; i++)
	{
		color = colorStart + (colorMod % colorLength);
		colorMod++;
		for (j = 0; j < 3; j++)
		{
			porg[j] = org[j] + ((rand () % 32) - 16);
			vel[j] = (rand () % 512) - 256;
		}
		new_base_particle_oc (pt_blob, porg, vel, color, 0, -1, 0.3, 0);
	}
}

void
R_BlobExplosion (vec3_t org)
{
	int			i, j, color;
	float		pdie;
	vec3_t		porg, pvel;
	ptype_t		ptype;

	for (i = 0; i < 1024; i++)
	{
		pdie = 1 + (rand () & 8) * 0.05;
		if (i & 1)
		{
			ptype = pt_blob;
			color = 66 + rand() % 6;
		}
		else
		{
			ptype = pt_blob2;
			color = 150 + rand() % 6;
		}

		for (j = 0; j < 3; j++)
		{
			porg[j] = org[j] + ((rand () % 32) - 16);
			pvel[j] = (rand () % 512) - 256;
		}
		new_base_particle_oc (ptype, porg, pvel, color, 0, -1, pdie, 0);
	}
}

void
R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j, pcolor;
	float		pdie;
	vec3_t		porg, pvel;
	base_particle_t *p;

	if (count == 1024)
	{
		// Special case used by id1 progs
		R_NewExplosion (org);
		return;
	}
	
	for (i = 0; i < count; i++)
	{
		pdie = 0.1 * (rand () % 5);
		pcolor = (color & ~7) + (rand () & 7);
		for (j = 0; j < 3; j++)
		{
			porg[j] = org[j] + ((rand () & 15) - 8);
			pvel[j] = dir[j] * 15;	// + (rand()%300)-150;
		}
		p = new_base_particle_oc (pt_normal, porg, pvel, pcolor, 0, 1, pdie, 0);
		if (!p)
			return;
		p->alphadie = 2;
	}
}


void
R_LavaSplash (vec3_t org)
{
	int			i, j, pcolor;
	float		vel, pdie;
	vec3_t		dir, porg, pvel;

	for (i = -16; i < 16; i++)
	{
		for (j = -16; j < 16; j++)
		{
			pdie = 2 + (rand () & 31) * 0.02;
			pcolor = 224 + (rand () & 7);

			dir[0] = j * 8 + (rand () & 7);
			dir[1] = i * 8 + (rand () & 7);
			dir[2] = 256;

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + (rand () & 63);

			VectorNormalizeFast (dir);
			vel = 50 + (rand () & 63);
			VectorScale (dir, vel, pvel);
			new_base_particle_oc (pt_normal,porg, pvel, pcolor, 0, -1, pdie, 0);
		}
	}
}

void 
R_Torch (entity_common_t *ent, qboolean torch2)
{
	vec3_t	porg, pvel;
	vec4_t	color;
	base_particle_t	*p;

	if (!r_particles->ivalue)
		return;

	VectorSet4 (color, 0.89, 0.59, 0.31, 0.5);
	VectorSet (pvel, (rand() & 3) - 2, (rand() & 3) - 2, 0);
	VectorSet (porg, ent->origin[0], ent->origin[1],
			ent->origin[2] + 4);

	if (torch2)
	{
		// used for large torches (eg, start map near spawn)
		porg[2] = ent->origin[2] - 2;
		VectorSet (pvel, (rand() & 7) - 4, (rand() & 7) - 4, 0);
		p = new_base_particle (pt_normal, porg, pvel, color, rand () & 3,
			ent->frame[0] ? 30 : 10, 5, 0);
		if (!p)
			return;
		p->alphadie = 0.4;
		p->scale_change = -3;
		p->gravity = -2.5;
	}
	else {
		// wall torches
		p = new_base_particle (pt_normal, porg, pvel, color, rand () & 3,
			10, 5, 0);
		if (!p)
			return;
		p->alphadie = 0.4;
		p->scale_change = -1.5;
		p->gravity = -2.0;
	}
}

void
R_RailTrail (vec3_t start, vec3_t end)
{
	vec3_t		vec, org, vel, right;
	vec4_t		color;
	float		sr, sp, sy, cr, cp, cy, len, roll = 0.0f;
	base_particle_t	*p;

	VectorSubtract (end, start, vec);
	Vector2Angles (vec, org);

	org[0] *= (M_PI * 2 / 360);
	org[1] *= (M_PI * 2 / 360);

	sp = Q_sin (org[0]);
	cp = Q_cos (org[0]);
	sy = Q_sin (org[1]);
	cy = Q_cos (org[1]);

	len = VectorNormalize(vec);
	VectorScale (vec, 1.5, vec);
	VectorSet (right, sy, cy, 0);

	while (len > 0)
	{
		VectorCopy (d_8tofloattable[(rand() & 3) + 225], color); color[3] = 0.5;
		p = new_base_particle (pt_normal, start, vec3_origin, color,
			0, 2.5, 1.0, 0);
		if (!p)
			return;
		p->alphadie = 0.5;
		p->gravity = 0;

		VectorMA (start, 4, right, org);
		VectorScale (right, 8, vel);
		VectorCopy (d_8tofloattable[(rand() & 7) + 206], color); color[3] = 0.5;
		p = new_base_particle (pt_normal, org, vel, color,
				0, 5.0, 1.0, 0);
		if (!p)
			return;
		p->alphadie = 0.5;
		p->gravity = 0;

		roll += 7.5 * (M_PI / 180.0);

		if (roll > 2*M_PI)
			roll -= 2*M_PI;

		sr = Q_sin (roll);
		cr = Q_cos (roll);
		right[0] = (cr * sy - sr * sp * cy);
		right[1] = -(sr * sp * sy + cr * cy);
		right[2] = -sr * cp;

		len -= 1.5;
		VectorAdd (start, vec, start);
	}
}

void
R_Lightning (vec3_t start, vec3_t end, float die)
{
	vec4_t	color;
	float	rs = 1.0f/1024.0f;

	if (!VectorCompare (start, end))
	{
		VectorSet4 (color, 1, 1, 1, 1);
		new_xbeam_particle(pt_lightning,start,end,color,8.0,5.0,rs,die);
	}
}

void
R_ParticleTrail (entity_common_t *ent)
{
	vec3_t		vec, pvel, start, end, c1, c2;
	float		len;
	int			j, dec, type;
	base_particle_t *p;

	if (!ent->trail_times++) {
		VectorCopy(ent->origin, ent->trail_old_org);
		return;
	}

	VectorCopy (ent->trail_old_org, start);
	VectorCopy (ent->origin, end);

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	dec = -ent->trail_len;
	ent->trail_len += len;
	if (ent->trail_len < 0.01f)
		return;
	ent->trail_len = 0;
	VectorCopy (end, ent->trail_old_org);

	type = ent->model->flags & ~EF_ROTATE;

	// Move forward to reach first puff location.
	VectorMA (start, dec, vec, start);
	len -= dec;
		
	while (len >= 0)
	{
		dec = 3;
		VectorClear(pvel);

		p = new_base_particle_oc(pt_normal, start, pvel, 0, 0, -1, 2, 0);
		if (!p)
			return;

		switch (type)
		{
			case EF_ROCKET:
				dec = 3;
				p->texnum = GTF_smoke[rand()&7];
				p->vel[0] = lhrandom(-5, 5);
				p->vel[1] = lhrandom(-5, 5);
				p->vel[2] = lhrandom(-5, 5);
				p->scale = dec;
				p->die = ccl.time + 9999;
				VectorSet(c1, 0.188, 0.188, 0.188);
				VectorSet(c2, 0.376, 0.376, 0.376);
				VecRBetween(c1, c2, p->color);
				p->color[3] = 0.245;
				p->alphadie = 0.2696;
				p->scale_change = 7;
				p->gravity = -1;

				p = new_base_particle_oc(pt_normal, start,pvel, 0, 0, -1, 2, 0);
				if (!p)
					return;
				p->texnum = GTF_smoke[rand()&7];
				p->vel[0] = lhrandom(-5, 5);
				p->vel[1] = lhrandom(-5, 5);
				p->vel[2] = lhrandom(-5, 5);
				p->scale = dec;
				p->die = ccl.time + 9999;
				VectorSet(c1, 0.502, 0.063, 0.063);
				VectorSet(c2, 1.000, 0.627, 0.125);
				VecRBetween(c1, c2, p->color);
				p->color[3] = 0.565;
				p->alphadie = 3.0196;
				p->gravity = -1;

				break;
			case EF_GRENADE: // smoke
				dec = 3;
				p->texnum = GTF_smoke[rand()&7];
				p->vel[0] = lhrandom(-5, 5);
				p->vel[1] = lhrandom(-5, 5);
				p->vel[2] = lhrandom(-5, 5);
				p->scale = dec;
				p->die = ccl.time + 9999;
				VectorSet(c1, 0.188, 0.188, 0.188);
				VectorSet(c2, 0.376, 0.376, 0.376);
				VecRBetween(c1, c2, p->color);
				p->color[3] = 0.196;
				p->scale_change = 7;
				p->alphadie = 0.216;
				p->gravity = -1;
				break;

			case EF_ZOMGIB:
				// slight blood
				dec += 3;
			case EF_GIB:
				// blood
				VecRBetween(d_8tofloattable[64],d_8tofloattable[67],p->color);
				for (j = 0; j < 3; j++)
					p->org[j] = start[j] + ((rand () % 6) - 3);
				R_Stain (start, 32, 64, 32, 32, 32, 192, 64, 64, 32);
				break;

			case EF_TRACER:		// Voor trail, yellowish.
				VecRBetween(d_8tofloattable[52],d_8tofloattable[60],p->color);
				goto tracer;

			case EF_TRACER2:	// Hell knight, fiery.
				VectorSet(c1, 0.188, 0.063, 0.000);
				VectorSet(c2, 0.314, 0.125, 0.000);
				VecRBetween(c1, c2, p->color);
				goto tracer;

			case EF_TRACER3:	// Voor trail, purple.
				VectorSet(p->color, 0.314, 0.125, 0.188);
				goto tracer;

tracer:
				dec = 6;
				p->vel[0] = lhrandom(-8, 8);
				p->vel[1] = lhrandom(-8, 8);
				p->vel[2] = lhrandom(-8, 8);
				p->scale = dec;
				p->die = ccl.time + 9999;
				p->color[3] = 0.501;
				p->alphadie = 1.505;
				p->scale_change = 0;
				p->gravity = 0;
				break;
		}

		VectorMA (start, dec, vec, start);
		len -= dec;
	}
}

static void
R_Move_Base_Particles (void)
{
	mleaf_t				*mleaf;
	base_particle_t		*p;
	int					i, j, k, activeparticles, maxparticle;
	float				grav, dvel;
	vec3_t				oldorg, v;

	if (!max_base_particles)
		return;

	grav = ccl.frametime * 800 * 0.05;
	dvel = 4 * ccl.frametime;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;

	for (k = 0, p = base_particles; k < num_base_particles; k++, p++)
	{
		if (p->die <= ccl.time)
		{
			free_base_particles[j++] = p;
			continue;
		}

		maxparticle = k;
		activeparticles++;

		p->draw = true;

#if 1
		if (r_particle_physics->ivalue)
		{
			mleaf = Mod_PointInLeaf(p->org, r.worldmodel);
			if ((mleaf->contents == CONTENTS_SOLID) ||
					(mleaf->contents == CONTENTS_SKY))
				p->die = -1;
			if (mleaf->visframe != vis_framecount)
				p->draw = false;

			VectorCopy(p->org, oldorg);
		}
#endif

		VectorMA (p->org, ccl.frametime, p->vel, p->org);
#if 1
		if (p->bounce && r_particle_physics->ivalue)
		{
			vec3_t normal;
			float dist;
			if (TraceLine (r.worldmodel, oldorg, p->org, v, normal) < 1) {
				VectorCopy (v, p->org);
				if (p->bounce < 0)
				{
					p->die = -1;
					free_base_particles[j++] = p;
					continue;
				}
				else
				{
					dist = DotProduct (p->vel, normal) * -p->bounce;
					VectorMA (p->vel, dist, normal, p->vel);
					if (DotProduct (p->vel, p->vel) < 0.03)
						VectorClear (p->vel);
				}
			}
		}
#endif

		switch (p->type)
		{
			case pt_fire:
				p->ramp += ccl.frametime * 5;
				if (p->ramp >= 6)
					p->die = -1;
				break;

			case pt_explode:
				p->ramp += ccl.frametime * 10;
				if (p->ramp >= 8)
					p->die = -1;
				else
					VectorCopy(d_8tofloattable[ramp1[(int) p->ramp]], p->color);
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				break;

			case pt_explode2:
				p->ramp += ccl.frametime * 15;
				if (p->ramp >= 8)
					p->die = -1;
				else
					VectorCopy(d_8tofloattable[ramp2[(int) p->ramp]], p->color);
				for (i = 0; i < 3; i++)
					p->vel[i] -= p->vel[i] * ccl.frametime;
				break;

			case pt_blob:
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				break;

			case pt_blob2:
				for (i = 0; i < 2; i++)
					p->vel[i] -= p->vel[i] * dvel;
				break;

			default:
				break;
		}

		if (p->alphadie) {
			p->color[3] -= p->alphadie * ccl.frametime;
			if (p->color[3] <= 0)
				p->die = -1;
		}
		if (p->scale_change) {
			p->scale += p->scale_change * ccl.frametime;
			if (p->scale <= 0)
				p->die = -1;
		}
		if (p->gravity)
			p->vel[2] -= p->gravity * grav;


		if ((p->die <= ccl.time))
		{
			free_base_particles[j++] = p;
			continue;
		}
	}

	k = 0;
	while (maxparticle >= activeparticles)
	{
		*free_base_particles[k++] = base_particles[maxparticle--];
		while (maxparticle >= activeparticles
				&& base_particles[maxparticle].die <= ccl.time)
			maxparticle--;
	}
	num_base_particles = activeparticles;
}


static void
R_Draw_Base_Particles (void)
{
	base_particle_t		*p;
	int					i;
	float				scale;
	GTF_texture_t		*tex;

	if (!max_base_particles)
		return;

	qglBindTexture (GL_TEXTURE_2D, GTF_texnum);

	v_index = 0;

	for (i = 0, p = base_particles; i < num_base_particles; i++, p++)
	{
		if (p->die <= ccl.time || !p->draw)
			continue;

		tex = &GTF_texture[p->texnum];

		if (p->scale < 0)
		{
			scale = ((p->org[0] - r.origin[0]) * r.vpn[0])
				+ ((p->org[1] - r.origin[1]) * r.vpn[1])
				+ ((p->org[2] - r.origin[2]) * r.vpn[2]);
			if (scale < 20)
				scale = -p->scale;
			else
				scale = (-p->scale) + scale * 0.004;
		}
		else
			scale = p->scale;

		VectorCopy4 (p->color, cf_array_v(v_index + 0));
		VectorCopy4 (p->color, cf_array_v(v_index + 1));
		VectorCopy4 (p->color, cf_array_v(v_index + 2));
		VectorCopy4 (p->color, cf_array_v(v_index + 3));
		VectorSet2(tc_array_v(v_index + 0), tex->s1, tex->t1);
		VectorSet2(tc_array_v(v_index + 1), tex->s1, tex->t2);
		VectorSet2(tc_array_v(v_index + 2), tex->s2, tex->t2);
		VectorSet2(tc_array_v(v_index + 3), tex->s2, tex->t1);

		VectorTwiddle(p->org, r.vright, 1,r.vup,-1,scale, v_array_v(v_index+0));
		VectorTwiddle(p->org, r.vright,-1,r.vup,-1,scale, v_array_v(v_index+1));
		VectorTwiddle(p->org, r.vright,-1,r.vup, 1,scale, v_array_v(v_index+2));
		VectorTwiddle(p->org, r.vright, 1,r.vup, 1,scale, v_array_v(v_index+3));

		v_index += 4;

		if ((v_index + 4) >= MAX_VERTEX_ARRAYS)
		{
			TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
			TWI_PreVDrawCVA (0, v_index);
			qglDrawArrays (GL_QUADS, 0, v_index);
			TWI_PostVDrawCVA ();
			v_index = 0;
		}
	}

	if (v_index)
	{
		TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
		TWI_PreVDrawCVA (0, v_index);
		qglDrawArrays (GL_QUADS, 0, v_index);
		TWI_PostVDrawCVA ();
		v_index = 0;
	}
}

static void
R_Move_XBeam_Particles (void)
{
	xbeam_particle_t	*p;
	int					i, j, activeparticles, maxparticle;

	if (!max_xbeam_particles)
		return;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;

	for (i = 0, p = xbeam_particles; i < num_xbeam_particles; i++, p++)
	{
		if (p->die < ccl.time)
		{
			free_xbeam_particles[j++] = p;
			continue;
		}

		maxparticle = i;
		activeparticles++;

		switch (p->type)
		{
			case pt_lightning:
				break;
			default:
				break;
		}

		if (p->die <= ccl.time)
		{
			free_xbeam_particles[j++] = p;
			continue;
		}
	}

	i = 0;
	while (maxparticle >= activeparticles)
	{
		*free_xbeam_particles[i++] = xbeam_particles[maxparticle--];
		while (maxparticle >= activeparticles &&
				xbeam_particles[maxparticle].die <= ccl.time)
			maxparticle--;
	}
	num_xbeam_particles = activeparticles;
}

extern inline void
Calc_XBeam_Verts (int i, vec3_t start, vec3_t end, vec3_t offset,
		float t1, float t2)
{
	// near right corner
	VectorAdd     (start, offset, v_array_v(i + 0));
	VectorSet2 (tc_array_v(i + 0), 0, t1);
	// near left corner
	VectorSubtract(start, offset, v_array_v(i + 1));
	VectorSet2 (tc_array_v(i + 1), 1, t1);
	// far left corner
	VectorSubtract(end  , offset, v_array_v(i + 2));
	VectorSet2 (tc_array_v(i + 2), 1, t2);
	// far right corner
	VectorAdd     (end  , offset, v_array_v(i + 3));
	VectorSet2 (tc_array_v(i + 3), 0, t2);
}

static int xbeam_elements[18] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};

static void
DrawXBeam (xbeam_particle_t *p)
{
	int		i;
	float	t1, t2;
	vec3_t	v_up, v_right, v_offset;

	// Up, pointing towards view, and rotates around beam normal.
	// Get direction from start of beam to viewer.
	VectorSubtract (r.origin, p->org1, v_up);
	// Remove the portion of the vector that moves along the beam.
	// (This leaves only a a vector pointing directly away from the beam.)
	t1 = -DotProduct(v_up, p->normal);
	VectorMA(v_up, t1, p->normal, v_up);
	// Normalize the up.
	VectorNormalizeFast(v_up);
	// Generate right vector from dir and up, result already normalized.
	CrossProduct (p->normal, v_up, v_right);

	// Calculate the T coordinates, scrolling. (Texcoords)
	t1 = ccl.time * -p->scroll + p->repeat_scale * DotProduct(p->org1, p->normal);
	t1 = t1 - floor(t1);
	t2 = t1 + p->repeat_scale * p->len;
	
	/*
	 * The beam is 3 polygons in this configuration:
	 *  *   2
	 *   * *
	 * 1******
	 *   * *
	 *  *   3
	 * They are showing different portions of the beam texture, creating an
	 * illusion of a beam that appears to curl around in 3D space.
	 * (Realize that the whole polygon assembly orients itself to face
	 *  the viewer)
	 */

	// Polygon 1, verts 0-3.
	VectorScale(v_right, p->thickness, v_offset);
	Calc_XBeam_Verts(0, p->org1, p->org2, v_offset, t1, t2);
	// Polygon 2, verts 4-7.
	VectorAdd(v_right, v_up, v_offset);
	VectorScale(v_offset, p->thickness * 0.70710681f, v_offset);
	Calc_XBeam_Verts(4, p->org1, p->org2, v_offset, t1 + 0.33, t2 + 0.33);
	// Polygon 3, verts 8-11.
	VectorSubtract(v_right, v_up, v_offset);
	VectorScale(v_offset, p->thickness * 0.70710681f, v_offset);
	Calc_XBeam_Verts(8, p->org1, p->org2, v_offset, t1 + 0.66, t2 + 0.66);

	for (i = 0; i < 12; i++)
		VectorCopy4 (p->color, cf_array_v(v_index + i));

	memcpy(vindices + i_index, xbeam_elements, sizeof(xbeam_elements));

	i_index += 18;
	v_index += 12;
}

static void
R_Draw_XBeam_Particles (void)
{
	xbeam_particle_t	*p;
	int					k;

	if (!max_xbeam_particles)
		return;

	qglBindTexture (GL_TEXTURE_2D, GT_lightning_beam->texnum);
	v_index = 0;
	i_index = 0;

	for (k = 0, p = xbeam_particles; k < num_xbeam_particles; k++, p++)
	{
		if (p->die < ccl.time)
			continue;

		DrawXBeam (p);

		if (((i_index + 18) > MAX_VERTEX_INDICES)
				|| ((v_index + 12) > MAX_VERTEX_ARRAYS))
		{
			TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
			TWI_PreVDrawCVA (0, v_index);
			qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
			TWI_PostVDrawCVA ();
			v_index = 0;
			i_index = 0;
		}
	}

	if (v_index || i_index)
	{
		TWI_FtoUB (cf_array_v(0), c_array_v(0), v_index * 4);
		TWI_PreVDrawCVA (0, v_index);
		qglDrawElements(GL_TRIANGLES, i_index, GL_UNSIGNED_INT, vindices);
		TWI_PostVDrawCVA ();
		v_index = 0;
		i_index = 0;
	}
}

void
R_MoveParticles (void)
{
	R_Move_Base_Particles();
	R_Move_XBeam_Particles();
}

void
R_DrawParticles (void)
{
	qglEnableClientState (GL_COLOR_ARRAY);
	if (gl_cull->ivalue)
		qglDisable (GL_CULL_FACE);

	R_Draw_Base_Particles();
	R_Draw_XBeam_Particles();

	if (gl_cull->ivalue)
		qglEnable (GL_CULL_FACE);

	qglDisableClientState (GL_COLOR_ARRAY);
	qglColor4fv (whitev);
}

