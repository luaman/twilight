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

*/
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <stdio.h>

#include "SDL.h"

#include "strlib.h"
#include "opengl.h"

#include "qtypes.h"

/*
	This creates a function pointer for each function in oglfuncs of type
	OGL_NEED and OGL_EXT_WANT.  The function pointer for function glFoo is
	qglFoo.  qgl is used because Quake2 and Q3A use it.  Those two games
	are so popular that "qgl" will never be used for OS-specific OpenGL
	glue functions for fear of namespace conflicts.
 */
#define OGL_NEED(ret, name, args) ret (OGLDECL * q##name) args = NULL;
#define OGL_NEED_BUT(ret, name, args, fake)	ret (OGLDECL * q##name) args = NULL;
#define OGL_EXT_WANT(ret, name, args) ret (OGLDECL * q##name) args = NULL;
#include "oglfuncs.h"
#undef OGL_EXT_WANT
#undef OGL_NEED_BUT
#undef OGL_NEED


void OGLDECL
WRAP_glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count,
		GLenum type, const GLvoid *indices)
{
	qglDrawElements (mode, count, type, indices);
}

#define DYNGL_ERROR_SIZE 2048
static char dgl_error[DYNGL_ERROR_SIZE];
static qboolean dgl_loaded = false;
static const char *dgl_extensions;


/*
	DGL_GetError

	Returns the last error string in a somewhat abstract fashion.
*/
char *
DGL_GetError (void)
{
	return dgl_error;
}


/*
	DGL_LoadLibrary

	Opens your OpenGL library and makes sure we can get OpenGL functions
	from it.  This is normally platform-specific, but SDL handles it for
	us transparently.
*/
qboolean
DGL_LoadLibrary (char *name)
{
	if (SDL_GL_LoadLibrary(name) == -1)
	{
		snprintf (dgl_error, DYNGL_ERROR_SIZE,
				"DGL_LoadLibrary: Can't load %s", name);
		dgl_loaded = false;
		return false;
	}

	dgl_loaded = true;
	return true;
}


/*
	DGL_CloseLibrary

	Call to close the loaded library at shutdown or before loading another
	one.  Note that in SDL we don't actually close the lib - SDL will do
	that itself when you SDL_GL_LoadLibrary again.
*/
void
DGL_CloseLibrary (void)
{
	if (!dgl_loaded)
		return;

	dgl_loaded = false;
	dgl_extensions = NULL;
	return;
}


/*
	DGL_GetFuncs

	Assigns each qglFoo pointer from the library loaded.  OGL_NEED funcs are
	fatal if not found.  OGL_EXT_WANT functions will be assigned NULL if not
	found.  Again, SDL makes this easy, we have no need to figure out here
	if we want wglGetProcAddress, glXGetProcAddress, dlsym, or whatever.
*/
qboolean
DGL_GetFuncs (void)
{
	if (!dgl_loaded)
	{
		snprintf (dgl_error, DYNGL_ERROR_SIZE,
				"DGL_GetFuncs: no OpenGL library loaded");
		return false;
	}

#define OGL_NEED(ret, name, args)									\
	if (!(q##name = SDL_GL_GetProcAddress(#name))) {				\
		snprintf (dgl_error, DYNGL_ERROR_SIZE,						\
				"DGL_GetFuncs: can't find %s", #name);				\
		return false;												\
	}
#define OGL_NEED_BUT(ret, name, args, fake)							\
	if (!(q##name = SDL_GL_GetProcAddress(#name))) {				\
		q##name = &fake;											\
		snprintf (dgl_error, DYNGL_ERROR_SIZE,						\
				"DGL_GetFuncs: can't find %s, using fake.", #name);	\
	}
	
#define OGL_EXT_WANT(ret, name, args)								\
	q##name = SDL_GL_GetProcAddress(#name);

#include "oglfuncs.h"
#undef OGL_EXT_WANT
#undef OGL_NEED_BUT
#undef OGL_NEED

	return true;
}


/*
	DGL_HasExtension

	Checks to see that an extension exists - properly!  strcspn is a good
	function.  If you don't have it, I suggest implementing it rather than
	try and hack around not having it.  Even Win32 has it though.
*/
qboolean
DGL_HasExtension (char *ext)
{
	const char	   *p;
	const char	   *last;
	int				len;
	int				i;

	if (!dgl_loaded)
		return false;

	if (!dgl_extensions)
		dgl_extensions = qglGetString (GL_EXTENSIONS);

	p = dgl_extensions;
	len = strlen (ext);
	last = p + strlen (p);

	while (p < last)
	{
		i = strcspn (p, " ");
		if (len == i && strncmp(ext, p, i) == 0)
			return true;

		p += (i + 1);
	}

	return false;
}

