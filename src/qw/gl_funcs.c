/*
	gl_funcs.c

	Run time GL linking.

	Copyright (C) 2001		Zephaniah E. Hull.

	Please refer to doc/copyright/GPL for terms of license.

	$Id$
*/

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
ret (* q##name) args = NULL;
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
