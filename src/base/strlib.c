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

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "qtypes.h"
#include "strlib.h"
#include "zone.h"

/*
 * strlcat and strlcpy functions taken from OpenBSD.  Intellectual property
 * rights to these functions resides with Todd C. Miller.  They are included
 * here under his license.  Please see doc/COPYING-strl.txt for details.
 */

/*	$OpenBSD: strlcat.c,v 1.1.6.1 2001/05/14 22:32:48 niklas Exp $	*/
#ifndef HAVE_STRLCAT
/*
==================
Appends src to string dst of size siz (unlike strncat, siz is the
full size of dst, not space left).  At most siz-1 characters
will be copied.  Always NUL terminates (unless siz == 0).
Returns strlen(initial dst) + strlen(src); if retval >= siz,
truncation occurred.
==================
*/
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif  // #ifndef HAVE_STRLCAT


/*	$OpenBSD: strlcpy.c,v 1.1.6.1 2001/05/14 22:32:48 niklas Exp $	*/
#ifndef HAVE_STRLCPY
/*
==================
Copy src to string dst of size siz.  At most siz-1 characters
will be copied.  Always NUL terminates (unless siz == 0).
Returns strlen(src); if retval >= siz, truncation occurred.
==================
*/
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif // ifndef HAVE_STRLCPY


/*
==================
Duplicate a string (memory allocated with given zone)
==================
*/
char*
Zstrdup (memzone_t *zone, const char *string)
{
	size_t length = strlen (string) + 1;
	char *out = Zone_Alloc (zone, length);
	memcpy (out, string, length);
	return out;
}


/*
============
does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.

FIXME: This function is a hack.  A convenient hack it is, certainly, but it
       should be phased out at some point because it is unsafe.
============
*/
char *
va (char *format, ...)
{
	va_list     argptr;
	static int  refcount = 0;
	static char string[8][4096];
	
	refcount = (refcount + 1) % 8;
	va_start (argptr, format);
	vsnprintf (string[refcount], sizeof (string[refcount]), format, argptr);
	va_end (argptr);
	
	return string[refcount];
}
