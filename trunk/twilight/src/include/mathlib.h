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

	$Id$
*/

#ifndef __MATHLIB_H
#define __MATHLIB_H


#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#include <qtypes.h>
#include <math.h>

// LordHavoc: min and max are defined in stdlib in MSVC
#ifndef max
#include <stdlib.h>
#endif

#ifdef _WIN32
# include <windows.h>	// for min() and max()
#else

# ifndef max
#  define max(a,b) ((a) > (b) ? (a) : (b))
# endif

# ifndef min
#  define min(a,b) ((a) < (b) ? (a) : (b))
# endif

#endif

#ifndef bound
#define bound(a,b,c) (max(a, min(b, c)))
#endif

#ifndef boundsign
#define boundsign(a, b) ((b) > 0 ? max(0, (a)) : min ((a), 0))
#endif

#ifndef SLIDE
#define SLIDE(a, b, c) ((a) > (b)?(max((a) - (c), (b))):(min((a) + (c), (b))))
#endif

#ifndef BIT
#define BIT(bit)			(1Ul << (bit))
#endif

#ifndef bound_bits
#define bound_bits(n, bits)	bound(0, n, BIT(bits) - 1)
#endif

#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define invpow(base,number)	(log(number) / log(base))
#define ispow2(x)			(!(((x) - 1) & (x)))
int near_pow2_high (int x);
int near_pow2_low (int x);

struct mplane_s;

extern vec3_t vec3_origin;

void Math_Init (void);

double Q_sin(double x);
double Q_cos(double x);
double Q_atan(double x);
double Q_atan2(double y, double x);
double Q_tan(double x);
float Q_sqrt(double n);
float Q_RSqrt(double number);

#define NANMASK		255 << 23
#define	IS_NAN(x) (((*(int *)&x)&NANMASK)==NANMASK)
#define INF			(1.0f / 0.0f)

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

#define CrossProduct(v1,v2,cross) ((cross)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1],(cross)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2],(cross)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define DotProduct2(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1])	// this is for 2-dimensional vectors

#define VectorSet2(v,a,b)	  ((v)[0]=(a),(v)[1]=(b))
#define VectorSet(v,a,b,c)	  ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VectorSet3(v,a,b,c)	  ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VectorSet4(v,a,b,c,d)	  ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3]=(d))
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorMultiply(a,b,c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorCopy4(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define VectorInverse(a,b) ((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorScale(a,b,c) ((c)[0]=(a)[0]*(b),(c)[1]=(a)[1]*(b),(c)[2]=(a)[2]*(b))
#define VectorSlide(a,b,c) ((c)[0]=(a)[0]+(b),(c)[1]=(a)[1]+(b),(c)[2]=(a)[2]+(b))
#define VectorMA(a,b,c,d) ((d)[0]=(a)[0]+(b)*(c)[0],(d)[1]=(a)[1]+(b)*(c)[1],(d)[2]=(a)[2]+(b)*(c)[2])
#define VectorCompare(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2]))
#define VectorClear(a)		((a)[0]=(a)[1]=(a)[2]=0)
#define VectorLength(v)		(Q_sqrt(DotProduct(v,v)))
#define VectorLength2(v)	(Q_sqrt(DotProduct2(v,v)))

#define VectorNegate(a,b)	((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])

#define VectorTwiddleS(base, a, b, mod, to)			(\
		((to)[0]=(base)[0]+(((a)[0]+(b)[0])*mod)),	\
		((to)[1]=(base)[1]+(((a)[1]+(b)[1])*mod)),	\
		((to)[2]=(base)[2]+(((a)[2]+(b)[2])*mod)))

#define VectorTwiddle(base, a1, a2, b1, b2, c, to)					(\
		((to)[0]=(base)[0]+((((a1)[0]*(a2))+((b1)[0]*(b2)))*c)),	\
		((to)[1]=(base)[1]+((((a1)[1]*(a2))+((b1)[1]*(b2)))*c)),	\
		((to)[2]=(base)[2]+((((a1)[2]*(a2))+((b1)[2]*(b2)))*c)))

#define sq(x)		(x * x)
#define Q_rint(x)	((x) < 0 ? (int)((x)-0.5f) : (int)((x)+0.5f))

/*
 * VectorDistance, the distance between two points.
 * Yes, this is the same as sqrt(VectorSubtract then DotProduct),
 * however that way would involve more vars, this is cheaper.
 */
#define VectorDistance_fast(a, b)	((((a)[0] - (b)[0]) * ((a)[0] - (b)[0])) + \
									(((a)[1] - (b)[1]) * ((a)[1] - (b)[1])) + \
									(((a)[2] - (b)[2]) * ((a)[2] - (b)[2])))
#define VectorDistance(a, b)		Q_sqrt(VectorDistance_fast(a, b))

vec_t       _VectorLength (vec3_t v);
vec_t       _VectorLength2 (vec3_t v);
vec_t       _DotProduct (vec3_t v1, vec3_t v2);
void        _CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
void        _VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void        _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void        _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void        _VectorCopy (vec3_t in, vec3_t out);
int         _VectorCompare (vec3_t v1, vec3_t v2);
void        _VectorInverse (vec3_t v, vec3_t t);
void        _VectorScale (vec3_t in, vec_t scale, vec3_t out);

vec_t       VectorNormalize (vec3_t v);	// returns vector length
#define VectorNormalize2(v,dest) {float ilength = (float) sqrt(DotProduct(v,v));if (ilength) ilength = 1.0f / ilength;dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength;}

void		VectorNormalizeFast (vec3_t v);
void		Lerp_Vectors (vec3_t v1, float frac, vec3_t v2, vec3_t v);
void		Lerp_Angles (vec3_t v1, vec_t frac, vec3_t v2, vec3_t v);
void		Vector2Angles (vec3_t in, vec3_t out);

void        AngleVectors (vec3_t angles, vec3_t forward, vec3_t right,
						  vec3_t up);
void        AngleVectorsFLU (const vec3_t angles, vec3_t forward, vec3_t left,
						  vec3_t up);
int         BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);


#define DEG2RAD(a) ((a) * ((float) M_PI / 180.0f))
#define RAD2DEG(a) ((a) * (180.0f / (float) M_PI))
#define ANGLEMOD(a) (((int) ((a) * (65536.0f / 360.0f)) & 65535) * (360.0f / 65536.0f))

void        VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
void        RotatePointAroundVector (vec3_t dst, const vec3_t dir,
									const vec3_t point, float degrees);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

#endif // __MATHLIB_H

