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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif


#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_TCHAR_H
#include <tchar.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <errno.h>

#ifdef _WIN32
// Don't need windows.h till we have win32 GUI console
#include <io.h>
#include <conio.h>
#endif

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "cvar.h"
#include "compat.h"
#include "mathlib.h"
#include "strlib.h"
#include "server.h"
#include "sys.h"

// FIXME: put this somewhere else
void SV_Init (void);

Uint32		sys_sleep;

char       *qdate = __DATE__;

cvar_t	   *sys_asciionly;
cvar_t	   *sys_extrasleep;

double		curtime;

// =======================================================================
// General routines
// =======================================================================

static const char sys_charmap[256] = {
	' ', '#', '#', '#', '#', '.', '#', '#',
	'#', '\t', '\n', '#', ' ', '\n', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        text[2048];
	unsigned char *p;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof (text), fmt, argptr);
	va_end (argptr);

	if (strlen (text) > sizeof (text))
		Sys_Error ("memory overwrite in Sys_Printf");

	if (sys_asciionly && sys_asciionly->ivalue)
		for (p = (unsigned char *) text; *p; p++)
			putc (sys_charmap[*p], stdout);
	else
		for (p = (unsigned char *) text; *p; p++)
			if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
				printf ("[%02x]", *p);
			else
				putc (*p, stdout);
	fflush (stdout);
}

void
Sys_Quit (void)
{
	exit (0);
}

void
Sys_ESCallback (cvar_t *cvar)
{
	if (cvar->ivalue < 0)
		Cvar_Set (cvar, "0");
	else if (cvar->ivalue > 1000000)
		Cvar_Set (cvar, "1000000");

	sys_sleep = (Uint32)((cvar->ivalue) * (1.0f / 1000.0f));
}

void
Sys_Init (void)
{
	sys_extrasleep = Cvar_Get ("sys_extrasleep", "0", CVAR_NONE,
			&Sys_ESCallback);
	sys_asciionly = Cvar_Get ("sys_asciionly", "1", CVAR_ARCHIVE, NULL);

	Math_Init ();

#ifdef _WIN32
	if (COM_CheckParm ("-nopriority"))
	{
		Cvar_Set (sys_extrasleep, "0");
	}
	else
	{
		OSVERSIONINFO	vinfo;

		vinfo.dwOSVersionInfoSize = sizeof(vinfo);

		if (!GetVersionEx (&vinfo))
			Sys_Error ("Couldn't get OS info");

		if ((vinfo.dwMajorVersion < 4) ||
			(vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
		{
			Sys_Error ("QuakeWorld requires at least Win95 or NT 4.0");
		}

		if ( ! SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS))
			Sys_Printf ("SetPriorityClass() failed\n");
		else
			Sys_Printf ("Process priority class set to HIGH\n");

		// sys_extrasleep > 0 seems to cause packet loss on WinNT (why?)
		if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
			Cvar_Set (sys_extrasleep, "0");
	}
#endif
}

void
Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	fprintf (stderr, "Error: %s\n", text);

	exit (1);

}


/*
============
Sys_FileTime

returns -1 if not present
============
*/
int
Sys_FileTime (char *path)
{
	struct stat buf;

	if (stat (path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}


void
Sys_mkdir (char *path)
{
#ifdef _WIN32
	_mkdir (path);
#else
	mkdir (path, 0777);
#endif
}


double
Sys_DoubleTime (void)
{
	static double	epoch = 0.0;
	static Uint32	last;
	Uint32			now = SDL_GetTicks ();

	// happens every 47 days or so - hey it _could_ happen!
	if (now < last)
		epoch += 65536.0 * 65536.0; // max Uint32 + 1

	last = now;

	return epoch + (double)(now / 1000.0);
}

#ifndef _WIN32
qboolean		do_stdin = true;
qboolean		stdin_ready;
#endif

char *
Sys_ConsoleInput (void)
#ifndef _WIN32
{
	static char		text[256];
	int				len;

	if (!stdin_ready || !do_stdin)
		// the select didn't say it was ready
		return NULL;

	stdin_ready = false;

	len = read (0, text, sizeof (text));
	if (len == 0)
	{
		// end of file
		do_stdin = 0;
		return NULL;
	}
	if (len < 1)
		return NULL;

	// rip off the \n and terminate
	text[len - 1] = '\0';

	return text;
}
#else
{
	static char text[256];
	static int  len;
	int         c;

	// read a line out
	while (_kbhit ()) {
		c = _getch ();
		putch (c);
		if (c == '\r') {
			text[len] = 0;
			putch ('\n');
			len = 0;
			return text;
		}
		if (c == 8) {
			if (len) {
				putch (' ');
				putch (c);
				len--;
				text[len] = 0;
			}
			continue;
		}
		text[len] = c;
		len++;
		text[len] = 0;
		if (len == sizeof (text))
			len = 0;
	}

	return NULL;
}
#endif

#ifdef _WIN32
char *
Sys_ExpandPath (char *str)
{
	static char buf[_MAX_PATH] = "";
	char *s = str, *p;

	if (*s == '~')
	{
		s++;
		if (*s == '/' || *s == '\0')
		{
			/* Current user's home directory */
			if ((p = getenv("TWILIGHT")))
				strlcpy(buf, p, _MAX_PATH);
			else if ((p = getenv("HOME")))
				strlcpy(buf, p, _MAX_PATH);
			else if ((p = getenv("WINDIR")))
				strlcpy(buf, p, _MAX_PATH);
			else
				/* should never happen */
				strlcpy(buf, ".", _MAX_PATH);
			strlcat (buf, s, _MAX_PATH);
		} else {
			/* ~user expansion in win32 always fails */
			strcpy(buf, "");
		}
	} else
		strlcpy (buf, str, _MAX_PATH);

	return buf;
}
#else
char *
Sys_ExpandPath (char *str)
{
	static char buf[PATH_MAX] = "";
	char *s = str, *p;
	struct passwd *entry;

	if (*s == '~')
	{
		s++;
		if (*s == '/' || *s == '\0')
		{
			/* Current user's home directory */
			if ((p = getenv("HOME")))
				strlcpy(buf, p, PATH_MAX);
			else
				strlcpy(buf, ".", PATH_MAX);
			strlcat (buf, s, PATH_MAX);
		} else {
			/* Another user's home directory */
			if ((p = strchr(s, '/')) != NULL)
				*p = '\0';
			if ((entry = getpwnam(s)) != NULL)
			{
				strlcpy (buf, entry->pw_dir, PATH_MAX);
				if (p) {
					*p = '/';
					strlcat (buf, p, PATH_MAX);
				}
			} else
				/* ~user expansion failed, no such user */
				strcpy(buf, "");
		}
	} else
		strlcpy (buf, str, PATH_MAX);

	return buf;
}
#endif

#ifdef _CONSOLE 
#undef main
#endif

int
main (int c, char **v)
{
	double  	    time, oldtime, newtime, base;

	SDL_Init (SDL_INIT_TIMER);
	atexit (SDL_Quit);

	COM_InitArgv (c, v);

	SV_Init ();

	SV_Frame (0.1);

	base = oldtime = Sys_DoubleTime () - 0.1;
	while (1) {
		// the only reason we have a timeout at all is so that if the last
		// connected client times out, the message would not otherwise
		// be printed until the next event.
#ifndef _WIN32
		NET_Sleep (10);
#else
		NET_Sleep (1);
#endif

		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		curtime = newtime - base;

		SV_Frame (time);
		oldtime = newtime;

		if (sys_extrasleep->ivalue)
			SDL_Delay (sys_sleep);
	}

	return 0;
}

