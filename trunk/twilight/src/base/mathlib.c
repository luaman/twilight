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
#include <time.h>
#include <strlib.h>

#include "mathlib.h"

void        Sys_Error (char *error, ...);

vec3_t      vec3_origin = { 0, 0, 0 };

/*-----------------------------------------------------------------*/

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
	int i;
	double degree;
	qboolean inv;

	if (x < 0)
	{
		inv = true;
		x = -x;
	}
	else
	{
		inv = false;
	}

	for (i = 0; i < 1024; i++)
	{
		if (sintable[i] >= x)
			break;
	}

	degree = i * M_PI / 2048.0;

	return inv ? degree : -degree;
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

//static int q_randSeed = 0;

void 
Q_srand(unsigned seed)
{
//	q_randSeed = seed;
	srand(seed);
}

int	
Q_rand(void)
{
//	q_randSeed = (69069 * q_randSeed + 1);
//	return q_randSeed & 0x7fff;
	return rand();
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

float 
Q_RSqrt(float number)
{
	long i;
	float x2, y;

	if (number == 0.0)
		return 0.0;

	x2 = number * 0.5f;
	y = number;
	i = * (long *) &y;				// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);		// what the fuck?
	y = * (float *) &i;
	y = y * (1.5f - (x2 * y * y));	// this can be done a second time

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

/*
 * Written by LordHavoc.
 * Like AngleVectors, but taking a forward vector instead of angles.
 * Useful!
 */
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	right[0] -= d * forward[0];
	right[1] -= d * forward[1];
	right[2] -= d * forward[2];
	VectorNormalizeFast(right);
	CrossProduct(right, forward, up);
}

void
RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point,
		float degrees)
{
	float		t0, t1;
	float		angle, c, s;
	vec3_t		vr, vu, vf;

	angle = DEG2RAD (degrees);

	c = cos (angle);
	s = sin (angle);

	VectorCopy (dir, vf);
	VectorVectors (vf, vr, vu);

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
		+ (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
		+ (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
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
	float       sr = 0.0f, sp, sy, cr = 0.0f, cp, cy;

	angle = angles[0] * (M_PI * 2 / 360);
	sp = sin (angle);
	cp = cos (angle);
	angle = angles[1] * (M_PI * 2 / 360);
	sy = sin (angle);
	cy = cos (angle);

	if (right || up) {
		angle = angles[2] * (M_PI * 2 / 360);
		sr = sin (angle);
		cr = cos (angle);
	}

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;

	if (right) {
		right[0] = -1 * (sr * sp * cy + cr * -sy);
		right[1] = -1 * (sr * sp * sy + cr * cy);
		right[2] = -1 * (sr * cp);
	}

	if (up) {
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
}

void
AngleVectorsFLU (vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
{
	float       angle;
	float       sr = 0.0f, sp, sy, cr = 0.0f, cp, cy;

	angle = angles[0] * (M_PI * 2 / 360);
	sp = sin (angle);
	cp = cos (angle);
	angle = angles[1] * (M_PI * 2 / 360);
	sy = sin (angle);
	cy = cos (angle);

	if (left || up) {
		angle = angles[2] * (M_PI * 2 / 360);
		sr = sin (angle);
		cr = cos (angle);
	}

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;

	if (left) {
		left[0] = (sr * sp * cy + cr * -sy);
		left[1] = (sr * sp * sy + cr * cy);
		left[2] = (sr * cp);
	}

	if (up) {
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
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
_VectorLength (vec3_t v)
{
	float length = DotProduct(v,v);

	return length ? Q_sqrt(length) : 0;
}

vec_t 
_VectorLength2 (vec3_t v)
{
	float length = DotProduct2(v,v);

	return length ? Q_sqrt(length) : 0;
}

vec_t 
VectorNormalize (vec3_t v)
{
	float length = DotProduct(v,v);

	if (length) {
		float ilength;

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
	float ilength = Q_RSqrt (DotProduct(v,v));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
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
=================
RadiusFromBounds
=================
*/
float
RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int         i;
	vec3_t      corner;

	for (i = 0; i < 3; i++) {
		corner[i] =
			fabs (mins[i]) > fabs (maxs[i]) ? fabs (mins[i]) : fabs (maxs[i]);
	}

	return VectorLength (corner);
}

void 
Lerp_Vectors (vec3_t v1, float frac, vec3_t v2, vec3_t v)
{
	if (frac < 0.01) {
		VectorCopy (v1, v);
		return;
	} else if (frac > 0.99f) {
		VectorCopy (v2, v);
		return;
	}

	v[0] = v1[0] + (vec_t)frac * (v2[0] - v1[0]);
	v[1] = v1[1] + (vec_t)frac * (v2[1] - v1[1]);
	v[2] = v1[2] + (vec_t)frac * (v2[2] - v1[2]);
}

void 
Lerp_Angles (vec3_t v1, vec_t frac, vec3_t v2, vec3_t v)
{
	vec3_t	d;
	int i;

	if (frac < (1.0/255.0)) 
	{
		VectorCopy (v1, v);
		return;
	} 
	else if (frac > (1-(1.0/255.0))) 
	{
		VectorCopy (v2, v);
		return;
	}

	for (i = 0; i < 3; i++)
	{
		d[i] = v2[i] - v1[i];

		if (d[i] > 180)
			d[i] -= 360;
		else if (d[i] < -180)
			d[i] += 360;
	}

	v[0] = v1[0] + frac * d[0];
	v[1] = v1[1] + frac * d[1];
	v[2] = v1[2] + frac * d[2];
}

void 
Vector2Angles (vec3_t in, vec3_t out)
{
	vec_t yaw, pitch;

	if (!in[0] && !in[1])
	{
		yaw = 0;
		pitch = (in[2] > 0) ? 90 : 270;
	}
	else
	{
		yaw = Q_atan2 (in[1], in[0]) * 180 / M_PI;
		if (yaw < 0)
			yaw += 360;

		pitch = Q_atan2 (in[2], VectorLength2 (in)) * 180 / M_PI;
		if (pitch < 0)
			pitch += 360;
	}

	out[0] = pitch;
	out[1] = yaw;
	out[2] = 0;
}

