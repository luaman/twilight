/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "common.h"
#include "cvar.h"
#include "info.h"
#include "strlib.h"
#include "server.h"

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *
Info_ValueForKey (char *s, const char *key)
{
	char			pkey[512];
	static char		value[4][512];		// extra buffers for compares
	static int		valueindex;
	char		   *o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void
Info_RemoveKey (char *s, const char *key)
{
	char	   *start;
	char		pkey[512];
	char		value[512];
	char	   *o;

	if (strstr (key, "\\"))
	{
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey))
		{
			memmove (start, s, strlen(s) + 1);		// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void
Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	   *s;
	char		pkey[512];
	char		value[512];
	char	   *o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}


void
Info_SetValueForStarKey (char *s, const char *key, const char *value,
		unsigned maxsize)
{
	char		new[1024], *v;
	int			c;

	if (strstr (key, "\\") || strstr (value, "\\"))
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\""))
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen (key) > 63 || strlen (value) > 63)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}

	v = Info_ValueForKey (s, key);
	if (v && *v)
	{
		// make sure we have enough room for new value
		if (strlen (value) - strlen (v) + strlen (s) > maxsize)
		{
			Com_Printf ("TW: Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen (value))
		return;

	snprintf (new, sizeof (new), "\\%s\\%s", key, value);

	if ((strlen (new) + strlen (s)) > maxsize)
	{
		Com_Printf ("TW: Info string length exceeded\n");
		return;
	}
	// only copy ascii values
	s += strlen (s);
	v = new;
	while (*v)
	{
		c = (unsigned char) *v++;
		// client allows highbits on name
		// client only allows highbits on name
		if (strcasecmp (key, "name") != 0 ||
			(sv_highchars && !sv_highchars->ivalue))
		{
			c &= 127;
			if (c < 32 || c > 127)
				continue;
			// auto lowercase team
			if (strcasecmp (key, "team") == 0)
				c = tolower (c);
		}
		if (c > 13)
			*s++ = c;
	}
	*s = 0;
}

void
Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize)
{
	if (key[0] == '*')
	{
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void
Info_Print (const char *s)
{
	char		key[512];
	char		value[512];
	char	   *o;
	int			l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

