/*
	$Id$

	System configuration for Win32 without autoconf
*/
#ifndef __WIN32CONF_H
#define __WIN32CONF_H

#if _MSC_VER >= 800	/* MSVC 4.0 */
#pragma warning( disable : 4244 4127 4201 4214 4514 4305 4115 4018)
#endif

#define VERSION			"0.1.99"

#define USERPATH		"."

#define SHAREPATH		"."

#define USERCONF		"~/twilight.rc"

#define SHARECONF		"~/twilight.conf"

#define GL_LIBRARY		"opengl32.dll"

#undef HAVE_SNPRINTF

#define HAVE__SNPRINTF 1

#define HAVE__STRICMP 1

#define HAVE__STRNICMP 1

#define HAVE__VSNPRINTF 1

#define HAVE_SYS_TYPES_H 1

#define HAVE_SYS_STAT_H 1 

#define HAVE_DIRECT_H 1

#define HAVE_TCHAR_H 1

#define HAVE_FCNTL_H 1

#define HAVE_WINDEF_H 1

#define HAVE___INLINE 1

#define OGLDECL APIENTRY

#include "compat.h"

#endif // __WIN32CONF_H

