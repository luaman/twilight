/*
	$RCSfile$

	Copyright (C) 2001  Mathieu Olivier

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

#ifndef __STRLIB_H
#define __STRLIB_H

#include <string.h>
#include <ctype.h>

#include "twiconfig.h"
#include "zone.h"

#ifndef HAVE_STRCASECMP
# ifdef HAVE__STRICMP
#  define strcasecmp(s1, s2) _stricmp((s1), (s2))
# else
#  error "Don't have strcasecmp equivalent"
# endif
#endif

#ifndef HAVE_STRNCASECMP
# ifdef HAVE__STRNICMP
#  define strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))
# else
#  error "Don't have strncasecmp equivalent"
# endif
#endif


// 'strlcat' and 'strlcpy' from OpenBSD
// smarter replacements for 'strncpy' and 'strncat'; descriptions in strlib.c
#ifndef HAVE_STRLCAT
size_t strlcat (char* dst, const char* src, size_t siz);
#endif
#define strlcat_s(dst,src)		strlcat(dst, src, sizeof(dst))

#ifndef HAVE_STRLCPY
size_t strlcpy (char* dst, const char* src, size_t siz);
#endif
#define strlcpy_s(dst,src)		strlcpy(dst, src, sizeof(dst))


char *Zstrdup (memzone_t *zone, const char *string);

// does a varargs printf into a temp buffer
char *va (char *format, ...);

#endif // __STRLIB_H

