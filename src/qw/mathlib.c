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
// mathlib.c -- math primitives
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <SDL_types.h>
#include <math.h>
#include "quakedef.h"
#include "gl_model.h"

void        Sys_Error (char *error, ...);

vec3_t      vec3_origin = { 0, 0, 0 };

/*-----------------------------------------------------------------*/

// some q3 stuff here

static float sintable[1024];

static void 
Math_BuildSinTable(void) 
{
	int i;

	for (i = 0; i < 1024; i++)
		sintable[i] = (float)sin(i * M_PI / 2048.0f);
}

double 
Q_sin(double x)
{
	int	index = (int)(1024 * x / (M_PI * 0.5));
	int	quad = index >> 10;

	index &= 1023;
	quad &= 3;

	switch (quad) 
	{
		case 0:
			return sintable[index];
		case 1:
			return sintable[1023-index];
		case 2:
			return -sintable[index];
		case 3:
			return -sintable[1023-index];
	}

	return 0;
}

double 
Q_cos(double x) 
{
	int	index = (int)(1024 * x / (M_PI * 0.5));
	int	quad = index >> 10;

	index &= 1023;
	quad &= 3;

	switch (quad)
	{
		case 0:
			return sintable[1023-index];
		case 1:
			return -sintable[index];
		case 2:
			return -sintable[1023-index];
		case 3:
			return sintable[index];
	}

	return 0;
}

double 
Q_asin(double x)
{
	return x * (M_PI / 2048);
}

double 
Q_atan2(double y, double x) 
{
	float	base = 0;
	float	temp;
	float	dir = 1;
	float	test;
	int		i;
	double  x1 = x, y1 = y;

	if (x1 < 0) 
	{
		if (y1 >= 0) 
		{
			// quad 1
			base = M_PI * 0.5;
			temp = x1;
			x1 = y1;
			y1 = -temp;
		} 
		else 
		{
			// quad 2
			base = M_PI;
			x1 = -x1;
			y1 = -y1;
		}
	} 
	else 
	{
		if (y1 < 0) 
		{
			// quad 3
			base = 3 * M_PI * 0.5;
			temp = x1;
			x1 = -y1;
			y1 = temp;
		}
	}

	if (y1 > x1) 
	{
		base += M_PI*0.5;
		temp = x1;
		x1 = y1;
		y1 = temp;
		dir = -1;
	} 
	else 
		dir = 1;

	// calcualte angle in octant 0
	if (x1 == 0) {
		return base;
	}

	y1 /= x1;

	for (i = 0; i < 512; i++)
	{
		test = sintable[i] / sintable[1023-i];
		if (test > y1)
			break;
	}

	return base + dir * i * (M_PI / 2048.0f);
}

double
Q_atan(double x)
{
	float test, y = Q_fabs(x), dir = (x < 0) ? -1 : 1;
	int i;

	if (!x)
		return 0;

	for (i = 0; i < 1024; i++)
	{
		test = sintable[i] / sintable[1023-i];
		if (test > y)
			break;
	}

	return dir * i * (M_PI / 2048.0f);
}

double 
Q_tan(double x)
{
	int	index = (int)(1024 * x / (M_PI * 0.5));
	int	quad = index >> 10;

	index &= 1023;
	quad &= 3;

	switch (quad) 
	{
		case 0:
		case 2:
			return sintable[index] / sintable[1023-index];
		case 1:
		case 3:
			return -sintable[1023-index] / sintable[index];
	}

	return 0;
}

double 
Q_floor(double x)
{
	return floor(x);
}

double 
Q_ceil(double x)
{
	return ceil(x);
}

float 
Q_fabs( float f ) 
{
	float tmp = f;

	return (tmp < 0) ? -tmp : tmp;
}

int 
Q_abs(int x) 
{
	int tmp = x;

	return (tmp < 0) ? -tmp : tmp;
}

static int q_randSeed = 0;

void 
Q_srand(unsigned seed)
{
	q_randSeed = seed;
}

int	
Q_rand(void)
{
	q_randSeed = (69069 * q_randSeed + 1);
	return q_randSeed & 0x7fff;
}

////////////////////////////////////////////////////////////////////////
// Square root with lookup table (http://www.nvidia.com/developer)
////////////////////////////////////////////////////////////////////////

#define FP_BITS(fp) (* (Uint32 *) &(fp))

typedef union FastSqrtUnion
{
	float f;
	unsigned int i;
} FastSqrtUnion;

static unsigned int iFastSqrtTable[0x10000];

// Build the square root table
static void 
Math_BuildSqrtTable(void)
{
	unsigned int i;
	FastSqrtUnion s;

	// Build the fast square root table
	for (i = 0; i <= 0x7FFF; i++)
	{
		// Build a float with the bit pattern i as mantissa
		// and an exponent of 0, stored as 127
		s.i = (i << 8) | (0x7F << 23);
		s.f = (float) sqrt(s.f);
    
		// Take the square root then strip the first 7 bits of
		// the mantissa into the table
		iFastSqrtTable[i + 0x8000] = (s.i & 0x7FFFFF);
    
		// Repeat the process, this time with an exponent of 1, 
		// stored as 128
		s.i = (i << 8) | (0x80 << 23);
		s.f = (float) sqrt(s.f);
    
		iFastSqrtTable[i] = (s.i & 0x7FFFFF);
	}
}

float 
Q_sqrt(float n)
{
	// Check for square root of 0
	if (FP_BITS(n) == 0)
		return 0.0;                 
  
	FP_BITS(n) = iFastSqrtTable[(FP_BITS(n) >> 8) & 0xFFFF] | 
		((((FP_BITS(n) - 0x3F800000) >> 1) +
		0x3F800000) & 0x7F800000);
  
	return n;
}

#define THREEHALFS	1.5f

float 
Q_RSqrt(float number)
{
	long i;
	float x2, y;

	x2 = number * 0.5f;
	y = number;
	i = * (long *) &y;						// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);             // what the fuck?
	y = * (float *) &i;
	y = y * (THREEHALFS - (x2 * y * y));   // 1st iteration

	return y;
}

double
Q_pow (double x, double y)
{
	return pow (x, y);
}

void 
Math_Init (void)
{
	Math_BuildSqrtTable();
	Math_BuildSinTable();

	Q_srand (time(NULL));
}

/*-----------------------------------------------------------------*/

#define DEG2RAD( a ) ( a * M_PI ) / 180.0F

void
ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal)
{
	float       d;
	vec3_t      n;
	float       inv_denom;

	inv_denom = 1.0F / DotProduct (normal, normal);

	d = DotProduct (normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void
PerpendicularVector (vec3_t dst, const vec3_t src)
{
	int         pos;
	int         i;
	float       minelem = 1.0F;
	vec3_t      tempvec;

	/* 
	   ** find the smallest magnitude axially aligned vector */
	for (pos = 0, i = 0; i < 3; i++) {
		if (Q_fabs (src[i]) < minelem) {
			pos = i;
			minelem = Q_fabs (src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/* 
	   ** project the point onto the plane defined by src */
	ProjectPointOnPlane (dst, tempvec, src);

	/* 
	   ** normalize the result */
	VectorNormalizeFast (dst);
}

#if defined(_WIN32) && _MSC_VER >= 800	/* MSVC 4.0 */
#pragma optimize( "", off )
#endif


void
RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point,
						 float degrees)
{
	float       m[3][3];
	float       im[3][3];
	float       zrot[3][3];
	float       tmpmat[3][3];
	float       rot[3][3];
	int         i;
	vec3_t      vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector (vr, dir);
	CrossProduct (vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy (im, m, sizeof (im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset (zrot, 0, sizeof (zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = Q_cos (DEG2RAD (degrees));
	zrot[0][1] = Q_sin (DEG2RAD (degrees));
	zrot[1][0] = -zrot[0][1];
	zrot[1][1] = zrot[0][0];

	R_ConcatRotations (m, zrot, tmpmat);
	R_ConcatRotations (tmpmat, im, rot);

	for (i = 0; i < 3; i++) {
		dst[i] =
			rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

#if defined(_WIN32) && _MSC_VER >= 800	/* MSVC 4.0 */
#pragma optimize( "", on )
#endif

/*-----------------------------------------------------------------*/

float
anglemod (float a)
{
#if 0
	if (a >= 0)
		a -= 360 * (int) (a / 360);
	else
		a += 360 * (1 + (int) (-a / 360));
#endif
	a = (360.0 / 65536) * ((int) (a * (65536 / 360.0)) & 65535);
	return a;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int
BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, mplane_t *p)
{
	float       dist1, dist2;
	int         sides;

// general case
	switch (p->signbits) {
		case 0:
			dist1 =
				p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			dist2 =
				p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			break;
		case 1:
			dist1 =
				p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			dist2 =
				p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			break;
		case 2:
			dist1 =
				p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			dist2 =
				p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			break;
		case 3:
			dist1 =
				p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			dist2 =
				p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			break;
		case 4:
			dist1 =
				p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			dist2 =
				p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			break;
		case 5:
			dist1 =
				p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emins[2];
			dist2 =
				p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emaxs[2];
			break;
		case 6:
			dist1 =
				p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			dist2 =
				p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			break;
		case 7:
			dist1 =
				p->normal[0] * emins[0] + p->normal[1] * emins[1] +
				p->normal[2] * emins[2];
			dist2 =
				p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
				p->normal[2] * emaxs[2];
			break;
		default:
			dist1 = dist2 = 0;			// shut up compiler
			Sys_Error ("BoxOnPlaneSide:  Bad signbits");
			break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
	if (sides == 0)
		Sys_Error ("BoxOnPlaneSide: sides==0");
#endif

	return sides;
}


void
AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float       angle;
	float       sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI * 2 / 360);
	sy = Q_sin (angle);
	cy = Q_cos (angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp = Q_sin (angle);
	cp = Q_cos (angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr = Q_sin (angle);
	cr = Q_cos (angle);

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
	right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
	right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
	right[2] = -1 * sr * cp;
	up[0] = (cr * sp * cy + -sr * -sy);
	up[1] = (cr * sp * sy + -sr * cy);
	up[2] = cr * cp;
}

int
_VectorCompare (vec3_t v1, vec3_t v2)
{
	int         i;

	for (i = 0; i < 3; i++)
		if (v1[i] != v2[i])
			return 0;

	return 1;
}

void
_VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}


vec_t
_DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void
_VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void
_VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void
_VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void
_CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t 
VectorLength (vec3_t v)
{
	float length = v[0]*v[0]+v[1]*v[1]+v[2]*v[2];

	return length ? Q_sqrt(length) : 0;
}

vec_t 
VectorNormalize (vec3_t v)
{
	float length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if (length) {
		length = Q_sqrt(length);
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

void 
VectorNormalizeFast (vec3_t v)
{
	float length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

	if (length) {
		length = Q_RSqrt(length);
		v[0] *= length;
		v[1] *= length;
		v[2] *= length;
	}
}

void
_VectorInverse (vec3_t v, vec3_t t)
{
	t[0] = -v[0];
	t[1] = -v[1];
	t[2] = -v[2];
}

void
_VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}


int
Q_log2 (int val)
{
	int         answer = 0;

	while ((val >>= 1) != 0)
		answer++;
	return answer;
}


/*
================
R_ConcatRotations
================
*/
void
R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void
R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
		in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
		in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
		in1[2][2] * in2[2][3] + in1[2][3];
}


/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
====================
*/

void
FloorDivMod (double numer, double denom, int *quotient, int *rem)
{
	int         q, r;
	double      x;

#ifndef PARANOID
	if (denom <= 0.0)
		Sys_Error ("FloorDivMod: bad denominator %d\n", denom);

//  if ((Q_floor(numer) != numer) || (Q_floor(denom) != denom))
//      Sys_Error ("FloorDivMod: non-integer numer or denom %f %f\n",
//              numer, denom);
#endif

	if (numer >= 0.0) {

		x = Q_floor (numer / denom);
		q = (int) x;
		r = (int) Q_floor (numer - (x * denom));
	} else {
		// 
		// perform operations with positive values, and fix mod to make
		// floor-based
		// 
		x = Q_floor (-numer / denom);
		q = -(int) x;
		r = (int) Q_floor (-numer - (x * denom));
		if (r != 0) {
			q--;
			r = (int) denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}


/*
===================
GreatestCommonDivisor
====================
*/
int
GreatestCommonDivisor (int i1, int i2)
{
	if (i1 > i2) {
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor (i2, i1 % i2);
	} else {
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor (i1, i2 % i1);
	}
}


/*
===================
Invert24To16

Inverts an 8.24 value to a 16.16 value
====================
*/

fixed16_t
Invert24To16 (fixed16_t val)
{
	if (val < 256)
		return (0xFFFFFFFF);

	return (fixed16_t)
		(((double) 0x10000 * (double) 0x1000000 / (double) val) + 0.5);
}

