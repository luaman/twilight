/*
	$Id$

	System configuration for Win32 without autoconf
*/

#if _MSC_VER >= 800	/* MSVC 4.0 */
#pragma warning( disable : 4018 4115 4127 4200 4201 4211 4214 4244 4305 4514 4706)
#endif

#define VERSION			"0.2.2"

#define USERPATH		"."

#define SHAREPATH		"."

#define USERCONF		"~/twilight.rc"

#define SHARECONF		"~/twilight.conf"

#define GL_LIBRARY		"opengl32.dll"
#define SDL_IMAGE_LIBRARY	"SDL_image.dll"

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

#define HAVE_LIMITS_H 1

#define HAVE__MKDIR 1

#define DYNGLENTRY APIENTRY

#define inline __inline
