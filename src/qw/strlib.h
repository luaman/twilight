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
// strlib.h

#ifndef __STRLIB_H
#define __STRLIB_H

#include <string.h>


#define Q_memset(d, f, c) memset((d), (f), (c))
#define Q_memcpy(d, s, c) memcpy((d), (s), (c))
#define Q_memcmp(m1, m2, c) memcmp((m1), (m2), (c))
#define Q_strcpy(d, s) strcpy((d), (s))
#define Q_strncpy(d, s, n) strncpy((d), (s), (n))
#define Q_strlen(s) ((int)strlen(s))
#define Q_strchr(s, c) strchr((s), (c))
#define Q_strrchr(s, c) strrchr((s), (c))
#define Q_strcat(d, s) strcat((d), (s))
#define Q_strncat(d, s, c) strncat((d), (s), (c))
#define Q_strcmp(s1, s2) strcmp((s1), (s2))
#define Q_strncmp(s1, s2, n) strncmp((s1), (s2), (n))
#define Q_strstr(d, s) strstr((d), (s))
#define Q_strtok(d, s) strtok((d), (s))
#define Q_strdup(s) strdup ((s))

#ifdef _WIN32

#define Q_strcasecmp(s1, s2) _stricmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))

#else

#define Q_strcasecmp(s1, s2) strcasecmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))

#endif

// FIXME: do this with auto*
#ifdef _WIN32
#define Q_snprintf _snprintf
#else
#define Q_snprintf snprintf
#endif


// 'strlcat' and 'strlcpy' from OpenBSD
// smarter replacements for 'strncpy' and 'strncat'; descriptions in strlib.c
#ifdef HAVE_STRLCAT
# define Q_strlcat(d, s, n) strlcat((d), (s), (n))
#else
size_t Q_strlcat (char* dst, const char* src, size_t siz);
#endif

#ifdef HAVE_STRLCPY
# define Q_strlcpy(d, s, n) strlcpy((d), (s), (n))
#else
size_t Q_strlcpy (char* dst, const char* src, size_t siz);
#endif


#endif // __STRLIB_H
