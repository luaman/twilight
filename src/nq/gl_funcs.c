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
// Run time GL linking.
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
#include <SDL/SDL.h>
#include "qtypes.h"
#include "TGL_types.h"
#include "TGL_funcs.h"
#include "cvar.h"
#include "sys.h"

extern cvar_t     *gl_driver;

// First we need all the function pointers.
#define TWIGL_NEED(ret, name, args) \
ret APIENTRY (* q##name) args = NULL;
#include "TGL_funcs_list.h"
#undef TWIGL_NEED

qboolean
GLF_Init (void)
{
	fprintf(stderr, "gl_driver->string: '%s'\n", gl_driver->string);
	if (SDL_GL_LoadLibrary(gl_driver->string) == -1) {
		Sys_Error("Can't load lib %s: %s\n", gl_driver->string, SDL_GetError());
		return false;
	}

#define TWIGL_NEED(ret, name, args)					\
	if (!(q##name = SDL_GL_GetProcAddress(#name))) {	\
		Sys_Error ("Can't load func %s: %s\n", #name, SDL_GetError()); \
		return false; \
	}

#include "TGL_funcs_list.h"
#undef TWIGL_NEED

	return true;
}
