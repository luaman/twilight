/*
    gl_types.h

    GL types.h

    Copyright (C) 2001		Zephaniah E. Hull.

    Please refer to doc/copyright/GPL for terms of license.

    $Id$
*/

#ifndef __gl_types_h
#define __gl_types_h

#include <SDL_types.h>

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

#endif // __gl_types_h
