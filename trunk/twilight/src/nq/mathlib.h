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
// mathlib.h

#ifndef __MATHLIB_H
#define __MATHLIB_H

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct mplane_s;

extern vec3_t vec3_origin;

#define NANMASK		255 << 23
#define	IS_NAN(x) (((*(int *)&x)&NANMASK)==NANMASK)

#define CrossProduct(v1,v2,cross) (cross[0]=v1[1]*v2[2]-v1[2]*v2[1],cross[1]=v1[2]*v2[0]-v1[0]*v2[2],cross[2]=v1[0]*v2[1]-v1[1]*v2[0])
#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])

#define VectorSubtract(a,b,c) (c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c) (c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b) (b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define VectorInverse(a,b) (b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorScale(a,b,c) (c[0]=a[0]*b,c[1]=a[1]*b,c[2]=a[2]*b)
#define VectorMA(a,b,c,d) (d[0]=a[0]+b*c[0],d[1]=a[1]+b*c[1],d[2]=a[2]+b*c[2])
#define VectorCompare(a,b) ((a[0]==b[0])&&(a[1]==b[1])&&(a[2]==b[2]))
#define VectorClear(a)		(a[0]=a[1]=a[2]=0)

vec_t       _DotProduct (vec3_t v1, vec3_t v2);
void        _CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
void        _VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void        _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void        _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void        _VectorCopy (vec3_t in, vec3_t out);
int         _VectorCompare (vec3_t v1, vec3_t v2);
void        _VectorInverse (vec3_t v);
void        _VectorScale (vec3_t in, vec_t scale, vec3_t out);

vec_t       VectorLength (vec3_t v);
vec_t       VectorNormalize (vec3_t v);	// returns vector length
void		VectorNormalizeFast (vec3_t v);
int         Q_log2 (int val);

void        R_ConcatRotations (float in1[3][3], float in2[3][3],
							   float out[3][3]);
void        R_ConcatTransforms (float in1[3][4], float in2[3][4],
								float out[3][4]);

void        FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t   Invert24To16 (fixed16_t val);
int         GreatestCommonDivisor (int i1, int i2);

void        AngleVectors (vec3_t angles, vec3_t forward, vec3_t right,
						  vec3_t up);
int         BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
float       anglemod (float a);



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


void Math_Init (void);

double Q_sin(double x);
double Q_cos(double x);
double Q_asin(double x);
double Q_atan(double x);
double Q_atan2(double y, double x);
double Q_tan(double x);
double Q_floor(double x);
double Q_ceil(double x);
float Q_fabs(float f);
int Q_abs(int x);
float Q_sqrt(float n);
void Q_srand(unsigned seed);
int	Q_rand(void);
float Q_RSqrt(float number);
double Q_pow(double x, double y);

#endif // __MATHLIB_H

