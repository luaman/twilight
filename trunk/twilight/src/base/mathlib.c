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
#include <stdio.h>

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

#if 0
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
#endif

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
	float test, y = fabs(x), dir = (x < 0) ? -1 : 1;
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

////////////////////////////////////////////////////////////////////////
// Square root with lookup table (http://www.nvidia.com/developer)
////////////////////////////////////////////////////////////////////////

static unsigned int iFastSqrtTable[0x10000];

// Build the square root table
static void 
Math_BuildSqrtTable(void)
{
	Uint32		i;
	float_int_t	s;

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
Q_sqrt(double n)
{
	float_int_t	num;
	// Check for square root of 0
	if (n <= 0)
		return 0.0;                 
  
	num.f = n;

	num.i = iFastSqrtTable[(num.i >> 8) & 0xFFFF] | 
		((((num.i - 0x3F800000) >> 1) + 0x3F800000) & 0x7F800000);
  
	return num.f;
}

/*
 * Q_RSqrt
 * This function calculates 1/sqrt(num), using some form of black magic.
 * This function WILL NOT WORK if the storage of floats is not IEEE.
 * (Note: The last line can be done twice for additional precision.)
 */
float 
Q_RSqrt(double num)
{
	float_int_t	evil;	// NOTE: evil.f and evil.i refer to the same memory.

	if (num <= 0.0)
		return 0.0;

	evil.f = num;
	evil.i = 0x5f3759df - (evil.i >> 1);				// The black magic!
	evil.f *= (1.5f - (num * 0.5f * evil.f * evil.f));

	return evil.f;
}

void 
Math_Init (void)
{
	Math_BuildSqrtTable();
	Math_BuildSinTable();

	srand ((Uint) time(NULL));
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
AngleVectorsFLU (const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
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

int
near_pow2_high (int x)
{
	int y = 0;

	if (ispow2(x))
		return x;
	for (y = 1; y < x; y <<= 1);

	return y;
}

int
near_pow2_low (int x)
{
	int y = 0;

	if (ispow2(x))
		return x;
	for (y = 1; y < x; y <<= 1);

	if (y > x)
		y >>= 1;

	return y;
}
