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

#include <string.h>

#include "dyngl.h"
#include "gl_info.h"
#include "mathlib.h"
#include "sys.h"

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

extern GLuint *vindices;

extern GLuint	v_index, i_index;
extern qboolean	va_locked;
extern GLuint	MAX_VERTEX_ARRAYS, MAX_VERTEX_INDICES;
extern cvar_t	*gl_copy_arrays;
extern cvar_t	*gl_vbo_v, *gl_vbo_tc, *gl_vbo_c;

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

typedef enum { ARRAY_DEFAULT, ARRAY_POINTER, ARRAY_COPY, ARRAY_VBO } varray_type_t;
typedef union { void *pointer; vbo_t *vbo; void *v; } varray_t;
extern varray_type_t gva_vtype, gva_tc0type, gva_tc1type, gva_ctype;
extern varray_t gva_varray, gva_tc0array, gva_tc1array, gva_carray;


extern inline void
TWI_ChangeVDrawArraysALL (GLuint num, qboolean cva, vertex_t *pv, vbo_t *vv,
		texcoord_t *ptc0, vbo_t *vtc0, texcoord_t *ptc1, vbo_t *vtc1)
{
	if (va_locked) {
		qglUnlockArraysEXT ();
		va_locked = false;
	}

	if (!num)
		return;

	if (!gl_vbo)
		vv = vtc0 = vtc1 = NULL;
	else {
		if (!gl_vbo_v->ivalue)
			vv = NULL;
		if (!gl_vbo_tc->ivalue)
			vtc0 = vtc1 = NULL;
	}

	if (vv) {
		if ((gva_vtype != ARRAY_VBO) || (gva_varray.vbo != vv)) {
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vv->buffer);
			qglVertexPointer (vv->elements, vv->type, vv->stride, vv->ptr);
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			gva_vtype = ARRAY_VBO;
			gva_varray.vbo = vv;
		}
	} else if (pv) {
		if (gl_copy_arrays->ivalue) {
			if ((gva_vtype != ARRAY_DEFAULT) && (gva_vtype != ARRAY_COPY))
				qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
			if ((gva_vtype != ARRAY_COPY) || (gva_varray.pointer != pv)) {
				memcpy (v_array_p, pv, sizeof (vertex_t) * num);
				gva_vtype = ARRAY_COPY;
			}
		} else {
			if ((gva_vtype != ARRAY_POINTER) || (gva_varray.pointer != pv))
				qglVertexPointer (3, GL_FLOAT, sizeof (vertex_t), pv);
			gva_vtype = ARRAY_POINTER;
		}
		gva_varray.pointer = pv;
	} else if (gva_vtype != ARRAY_DEFAULT) {
		qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
		gva_vtype = ARRAY_DEFAULT;
	}

	if (vtc0) {
		if ((gva_tc0type != ARRAY_VBO) || (gva_tc0array.vbo != vtc0)) {
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vtc0->buffer);
			qglTexCoordPointer (vtc0->elements, vtc0->type, vtc0->stride, vtc0->ptr);
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			gva_tc0type = ARRAY_VBO;
			gva_tc0array.vbo = vtc0;
		}
	} else if (ptc0) {
		if (gl_copy_arrays->ivalue) {
			if ((gva_tc0type != ARRAY_DEFAULT) && (gva_tc0type != ARRAY_COPY))
				qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);
			if ((gva_tc0type != ARRAY_COPY) || (gva_tc0array.pointer != ptc0)) {
				memcpy (tc0_array_p, ptc0, sizeof (texcoord_t) * num);
				gva_tc0type = ARRAY_COPY;
			}
		} else {
			if ((gva_tc0type != ARRAY_POINTER) || (gva_tc0array.pointer != ptc0))
				qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), ptc0);
			gva_tc0type = ARRAY_POINTER;
		}
		gva_tc0array.pointer = ptc0;
	} else if (gva_tc0type != ARRAY_DEFAULT) {
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);
		gva_tc0type = ARRAY_DEFAULT;
	}

	if (gl_mtex) {
		if (vtc1) {
			if ((gva_tc1type != ARRAY_VBO) || (gva_tc1array.vbo != vtc1)) {
				qglClientActiveTextureARB(GL_TEXTURE1_ARB);
				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vtc1->buffer);
				qglTexCoordPointer (vtc1->elements, vtc1->type, vtc1->stride, vtc1->ptr);
				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				gva_tc1type = ARRAY_VBO;
				gva_tc1array.vbo = vtc1;
				qglClientActiveTextureARB(GL_TEXTURE0_ARB);
			}
		} else if (ptc1) {
			if (gl_copy_arrays->ivalue) {
				if ((gva_tc1type != ARRAY_DEFAULT) && (gva_tc1type != ARRAY_COPY)) {
					qglClientActiveTextureARB(GL_TEXTURE1_ARB);
					qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
					qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
				if ((gva_tc1type != ARRAY_COPY) || (gva_tc1array.pointer != ptc1)) {
					memcpy (tc1_array_p, ptc1, sizeof (texcoord_t) * num);
					gva_tc1type = ARRAY_COPY;
				}
			} else {
				if ((gva_tc1type != ARRAY_POINTER) || (gva_tc1array.pointer != ptc1)) {
					qglClientActiveTextureARB(GL_TEXTURE1_ARB);
					qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), ptc1);
					qglClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
				gva_tc1type = ARRAY_POINTER;
			}
			gva_tc1array.pointer = ptc1;
		} else if (gva_tc1type != ARRAY_DEFAULT) {
			qglClientActiveTextureARB(GL_TEXTURE1_ARB);
			qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
			qglClientActiveTextureARB(GL_TEXTURE0_ARB);
			gva_tc1type = ARRAY_DEFAULT;
		}
	}

	if (gl_cva && cva) {
		qglLockArraysEXT (0, num);
		va_locked = true;
	}
}

extern inline void
TWI_PreVDrawCVA (GLint min, GLint max)
{
	if (gl_cva && !va_locked) {
		qglLockArraysEXT (min, max);
		va_locked = true;
	}
}

extern inline void
TWI_PostVDrawCVA ()
{
	if (va_locked) {
		qglUnlockArraysEXT ();
		va_locked = false;
	}
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
extern void GLArrays_Shutdown (void);

extern inline void
GLArrays_Reset_Vertex (void)
{
	qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
}

extern inline void
GLArrays_Reset_Color (void)
{
	qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub_array_p);
	/*
	if (gl_secondary_color)
		qglSecondaryColorPointerEXT (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), scub_array_p);
		*/
}

extern inline void
GLArrays_Reset_TC (qboolean both)
{
	qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);

	if (gl_mtex && both) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
}

#endif // __GL_ARRAYS_H
