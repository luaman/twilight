/*
	$RCSfile$

	Copyright (C) 2002  Zephaniah E. Hull.
	Copyright (C) 2002  Forest Hale

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

#ifndef __GL_ARRAYS_H
#define __GL_ARRAYS_H

#include "mathlib.h"
#include "dyngl.h"
#include "gl_info.h"

// Vertex array stuff.

extern texcoord_t	*tc0_array_p;
extern texcoord_t	*tc1_array_p;
extern vertex_t		*v_array_p;
extern colorf_t		*cf_array_p;
extern colorub_t	*cub_array_p;
extern colorf_t		*scf_array_p;
extern colorub_t	*scub_array_p;

#define tc_array_v(x) tc0_array_p[x].v
#define tc_array(x,y) tc0_array_p[x].v[y]
#define tc0_array_v(x) tc0_array_p[x].v
#define tc0_array(x,y) tc0_array_p[x].v[y]
#define tc1_array_v(x) tc1_array_p[x].v
#define tc1_array(x,y) tc1_array_p[x].v[y]
#define v_array_v(x) v_array_p[x].v
#define v_array(x,y) v_array_p[x].v[y]
#define c_array_v(x) cub_array_p[x].v
#define c_array(x,y) cub_array_p[x].v[y]
#define cub_array_v(x) cub_array_p[x].v
#define cub_array(x,y) cub_array_p[x].v[y]
#define cf_array_v(x) cf_array_p[x].v
#define cf_array(x,y) cf_array_p[x].v[y]
#define scub_array_v(x) scub_array_p[x].v
#define scub_array(x,y) scub_array_p[x].v[y]
#define scf_array_v(x) scf_array_p[x].v
#define scf_array(x,y) scf_array_p[x].v[y]

#ifndef SERVER_ONLY
extern GLuint *vindices;

extern GLint	v_index, i_index;
extern qboolean	va_locked;
extern GLint	MAX_VERTEX_ARRAYS, MAX_VERTEX_INDICES;
extern memzone_t *vzone;

extern float_int_t *FtoUB_tmp;

extern inline void
TWI_FtoUBMod (GLfloat *in, GLubyte *out, vec4_t mod, int num)
{
	int		i;

	// shift float to have 8bit fraction at base of number
	for (i = 0; i < num; i += 4) {
		FtoUB_tmp[i    ].f = (in[i    ] * mod[0]) + 32768.0f;
		FtoUB_tmp[i + 1].f = (in[i + 1] * mod[1]) + 32768.0f;
		FtoUB_tmp[i + 2].f = (in[i + 2] * mod[2]) + 32768.0f;
		FtoUB_tmp[i + 3].f = (in[i + 3] * mod[3]) + 32768.0f;
	}

	// then read as integer and kill float bits...
	for (i = 0; i < num; i += 4) {
		out[i    ] = (Uint8) min(FtoUB_tmp[i    ].i & 0x7FFFFF, 255);
		out[i + 1] = (Uint8) min(FtoUB_tmp[i + 1].i & 0x7FFFFF, 255);
		out[i + 2] = (Uint8) min(FtoUB_tmp[i + 2].i & 0x7FFFFF, 255);
		out[i + 3] = (Uint8) min(FtoUB_tmp[i + 3].i & 0x7FFFFF, 255);
	}
}

extern inline void
TWI_FtoUB (GLfloat *in, GLubyte *out, int num)
{
	int		i;

	// shift float to have 8bit fraction at base of number
	for (i = 0; i < num; i += 4) {
		FtoUB_tmp[i    ].f = in[i    ] + 32768.0f;
		FtoUB_tmp[i + 1].f = in[i + 1] + 32768.0f;
		FtoUB_tmp[i + 2].f = in[i + 2] + 32768.0f;
		FtoUB_tmp[i + 3].f = in[i + 3] + 32768.0f;
	}

	// then read as integer and kill float bits...
	for (i = 0; i < num; i += 4) {
		out[i    ] = (Uint8) min(FtoUB_tmp[i    ].i & 0x7FFFFF, 255);
		out[i + 1] = (Uint8) min(FtoUB_tmp[i + 1].i & 0x7FFFFF, 255);
		out[i + 2] = (Uint8) min(FtoUB_tmp[i + 2].i & 0x7FFFFF, 255);
		out[i + 3] = (Uint8) min(FtoUB_tmp[i + 3].i & 0x7FFFFF, 255);
	}
}

extern inline void
TWI_PreVDrawCVA (GLint min, GLint max)
{
	if (gl_cva) {
		qglLockArraysEXT (min, max);
		va_locked = 1;
	}
}

extern inline void
TWI_PostVDrawCVA ()
{
	if (va_locked)
		qglUnlockArraysEXT ();
}

extern inline void
TWI_PreVDraw (GLint min, GLint max)
{
	min = min; max = max; // Kill the warning.
}

extern inline void
TWI_PostVDraw ()
{
}

extern void GLArrays_Init_Cvars (void);
extern void GLArrays_Init (void);

extern inline void
GLArrays_Reset_Vertex (void)
{
	qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
}

extern inline void
GLArrays_Reset_Color (void)
{
	qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub_array_p);
	if (gl_secondary_color)
		qglSecondaryColorPointerEXT (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), scub_array_p);
}

extern inline void
GLArrays_Reset_TC (void)
{
	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);

	if (gl_mtex) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
}

#endif
#endif // __GL_ARRAYS_H
