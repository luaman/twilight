/*
	$Id$

	System configuration
*/
#ifndef __WIN32CONF_H
#define __WIN32CONF_H

#if _MSC_VER >= 800	/* MSVC 4.0 */
#pragma warning( disable : 4244 4127 4201 4214 4514 4305 4115 4018)
#endif

#define VERSION			"0.0.0"

#define USERPATH		"."

#define SHAREPATH		"."

#define GL_LIBRARY		"opengl32.dll"

#define HAVE__SNPRINTF 1

#define HAVE__STRICMP 1

#define HAVE__STRNICMP 1

#define HAVE__VSNPRINTF 1

#define HAVE_FCNTL_H 1

#include "compat.h"

#endif // __WIN32CONF_H

