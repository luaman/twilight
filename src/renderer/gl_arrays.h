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

#include "mathlib.h"
#include "dyngl.h"
#include "gl_info.h"
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

#ifndef SERVER_ONLY
extern GLuint *vindices;

extern GLuint	v_index, i_index;
extern qboolean	va_locked;
extern GLuint	MAX_VERTEX_ARRAYS, MAX_VERTEX_INDICES;
extern cvar_t	*gl_copy_arrays;
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

extern void *_varray, *_tc0array, *_tc1array, *_carray;

extern inline void
TWI_ChangeVDrawArrays_p (vertex_t *v, texcoord_t *tc0,
		texcoord_t *tc1, colorub_t *cub, colorf_t *cf)
{
	if (v && v != _varray) {
		qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v);
		_varray = v;
	} else if (!v && _varray) {
		qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
		_varray = NULL;
	}

	if (tc0 && tc0 != _tc0array) {
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0);
		_tc0array = tc0;
	} else if (!tc0 && _tc0array) {
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);
		_tc0array = NULL;
	}
	if (tc1 && (tc1 != _tc1array) && gl_mtex) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		_tc1array = tc1;
	} else if (!tc1 && _tc1array) {
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		_tc1array = NULL;
	}

	if (cub && cub != _carray) {
		qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub);
		_carray = cub;
	}
	if (cf && cf != _carray) {
		qglColorPointer (4, GL_FLOAT, sizeof(colorf_t), cf);
		_carray = cf;
	}
	if (!cub && !cf && _carray) {
		qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub_array_p);
		_carray = NULL;
	}
}

extern inline void
TWI_ChangeVDrawArrays_m (GLuint num, vertex_t *v, texcoord_t *tc0,
		texcoord_t *tc1, colorub_t *cub, colorf_t *cf)
{
	if (num >= MAX_VERTEX_ARRAYS)
		Sys_Error("ErrrrrK!");

	if (v && v != _varray) {
		memcpy (v_array_p, v, sizeof (vertex_t) * num);
		_varray = v;
	} else if (!v && _varray) {
		_varray = NULL;
	}

	if (tc0 && tc0 != _tc0array) {
		memcpy (tc0_array_p, tc0, sizeof (texcoord_t) * num);
		_tc0array = tc0;
	} else if (!tc0 && _tc0array) {
		_tc0array = NULL;
	}
	if (tc1 && (tc1 != _tc1array) && gl_mtex) {
		memcpy (tc1_array_p, tc1, sizeof (texcoord_t) * num);
		_tc1array = tc1;
	} else if (!tc1 && _tc1array) {
		_tc1array = NULL;
	}

	if (cub && cub != _carray) {
		memcpy (cub_array_p, cub, sizeof (colorub_t) * num);
		_carray = cub;
	}
	if (cf && cf != _carray) {
		TWI_FtoUB(cf->v, cub_array_p->v, num * 4);
		_carray = cf;
	}
	if (!cub && !cf && _carray) {
		_carray = NULL;
	}
}

extern inline void
TWI_ChangeVDrawArrays (GLuint num, qboolean cva, vertex_t *v, texcoord_t *tc0,
		texcoord_t *tc1, colorub_t *cub, colorf_t *cf)
{
	if (gl_cva && va_locked) {
		qglUnlockArraysEXT ();
		va_locked = false;
	}

	if (!num)
		return;

	if (gl_copy_arrays->ivalue)
		TWI_ChangeVDrawArrays_m (num, v, tc0, tc1, cub, cf);
	else
		TWI_ChangeVDrawArrays_p (v, tc0, tc1, cub, cf);

	if (gl_cva && cva) {
		qglLockArraysEXT (0, num);
		va_locked = 1;
	}
}

extern inline void
TWI_ChangeVDrawArraysVBO (GLuint num, qboolean cva, GLuint v, GLuint tc0,
		GLuint tc1, GLuint cub, GLuint cf)
{
	if (gl_cva && va_locked) {
		qglUnlockArraysEXT ();
		va_locked = false;
	}

	if (!num)
		return;

	if (v && v != (GLuint) _varray) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, v);
		qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), 0);
		_varray = (void *) v;
	} else if (!v && _varray) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglVertexPointer (3, GL_FLOAT, sizeof(vertex_t), v_array_p);
		_varray = NULL;
	}

	if (tc0 && tc0 != (GLuint) _tc0array) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, tc0);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), 0);
		_tc0array = (void *) tc0;
	} else if (!tc0 && _tc0array) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc0_array_p);
		_tc0array = NULL;
	}
	if (tc1 && (tc1 != (GLuint) _tc1array) && gl_mtex) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, tc1);
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), 0);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		_tc1array = (void *) tc1;
	} else if (!tc1 && _tc1array) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglClientActiveTextureARB(GL_TEXTURE1_ARB);
		qglTexCoordPointer (2, GL_FLOAT, sizeof(texcoord_t), tc1_array_p);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB);
		_tc1array = NULL;
	}

	if (cub && cub != (GLuint) _carray) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, cub);
		qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), 0);
		_carray = (void *) cub;
	}
	if (cf && cf != (GLuint) _carray) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, cf);
		qglColorPointer (4, GL_FLOAT, sizeof(colorf_t), 0);
		_carray = (void *) cub;
	}
	if (!cub && !cf && _carray) {
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglColorPointer (4, GL_UNSIGNED_BYTE, sizeof(colorub_t), cub_array_p);
		_carray = NULL;
	}

	if (gl_cva && cva) {
		qglLockArraysEXT (0, num);
		va_locked = 1;
	}
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
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

#endif
#endif // __GL_ARRAYS_H
