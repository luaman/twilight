/*
	$RCSfile$

    Copyright (C) 2001  Zephaniah E. Hull.

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

#ifndef __TGL_types_h
#define __TGL_types_h

#include "SDL_types.h"

typedef void			GLvoid;
typedef	Uint8			GLboolean;
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

#endif // __TGL_types_h

