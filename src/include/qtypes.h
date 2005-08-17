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

#ifndef __QTYPES_H
#define __QTYPES_H

#include <stdio.h>

#include "twiconfig.h"
#include <stddef.h>

// KJB Undefined true and false defined in SciTech's DEBUG.H header
#undef true
#undef false

typedef enum { false, true } qboolean;

typedef unsigned int	Uint;
typedef signed int		Sint;

#ifndef DYNGL_TYPES
#define DYNGL_TYPES

typedef void			GLvoid;
typedef Uint8			GLboolean;
typedef Sint8			GLbyte;			/* 1-byte signed */
typedef Uint8			GLubyte;		/* 1-byte unsigned */
typedef Sint16			GLshort;		/* 2-byte signed */
typedef Uint16			GLushort;		/* 2-byte unsigned */
typedef Sint32			GLint;			/* 4-byte signed */
typedef Uint32			GLuint;			/* 4-byte unsigned */
typedef Uint32			GLsizei;		/* 4-byte signed */
typedef Uint32			GLenum;
typedef Uint32			GLbitfield;
typedef float			GLfloat;		/* single precision float */
typedef float			GLclampf;		/* single precision float in [0,1] */
typedef double			GLdouble;		/* double precision float */
typedef double			GLclampd;		/* double precision float in [0,1] */

/*
 * FIXME: These are from the GL_ARB_vertex_buffer_object extension.
 * I am 99% sure that nVidia has these types completely WRONG.
 * But this is what they have for x86, at least at the moment.
 */
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

#endif /* DYNGL_TYPES */

typedef GLfloat vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef GLdouble dvec_t;
typedef dvec_t dvec3_t[3];
typedef dvec_t dvec4_t[4];
typedef dvec_t dvec5_t[5];

typedef GLubyte bvec_t;
typedef bvec_t bvec3_t[3];
typedef bvec_t bvec4_t[4];

typedef struct {
    GLfloat v[2];
} texcoord_t;

typedef struct {
    vec3_t	v;
} vertex_t;

typedef struct {
    vec4_t	v;
} colorf_t;

typedef struct {
    bvec4_t	v;
} colorub_t;

// plane_t structure
typedef struct mplane_s {
	vec3_t      normal;
	float       dist;
	Uint8       type;					// for texture axis selection and fast
	// side tests
	Uint8       signbits;				// signx + signy<<1 + signz<<1
	Uint8       pad[2];
} mplane_t;

typedef union {
	float	f;
	Uint32	i;
	Uint8	b[4];
} float_int_t;

#endif // __QTYPES_H

