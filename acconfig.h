/*
	$Id$

	System configuration
*/
/* Sigh. */
#ifndef __CONFIG_H
#define __CONFIG_H
@TOP@

/* Location of read-only (shared) gamedata */
#undef SHAREPATH

/* Location of user-modifyable gamedata */
#undef USERPATH

/* Define to whatever your OS requires */
#undef APIENTRY

/* Define to your default OpenGL library */
#undef GL_LIBRARY

@BOTTOM@

#include "compat.h"
#endif // __CONFIG_H
