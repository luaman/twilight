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

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_DIRECT_H
# include <direct.h>
#endif
#ifdef HAVE_TCHAR_H
# include <tchar.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_PWD_H
# include <pwd.h>
#endif
#include <errno.h>
#ifdef _WIN32
# include <windows.h>
# include <io.h>
#endif
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "keys.h"

// LordHavoc: for win32 which does not have PATH_MAX defined without POSIX
// (and that disables lots of other useful stuff)
#ifndef PATH_MAX
# define PATH_MAX 256
#endif


static int nostdout = 0;

static cvar_t *sys_asciionly;
static cvar_t *sys_logname;

int sys_gametypes;

char logname[MAX_OSPATH] = "";

double curtime = 0;

// =======================================================================
// General routines
// =======================================================================

static const char sys_charmap[256] =
{
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
Sys_Printf (const char *fmt, ...)
{
	va_list			argptr;
	unsigned char	text[2048];
	unsigned char	*p;

	if (nostdout)
		return;

	va_start (argptr, fmt);
	vsnprintf ((char *) text, sizeof (text), fmt, argptr);
	va_end (argptr);

	if (sys_asciionly && sys_asciionly->ivalue)
		for (p = text; *p; p++)
			putc (sys_charmap[*p], stdout);
	else
		for (p = text; *p; p++)
			if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
				printf ("[%02x]", *p);
			else
				putc (*p, stdout);
}

void
Sys_Quit (int ret)
{
	Host_Shutdown ();
#ifdef HAVE_FCNTL
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
	SDL_Quit ();
	exit (ret);
}

#ifdef _WIN32
HANDLE qwclsemaphore;
#endif

void
Sys_UpdateLogpath (void)
{
	if (sys_logname->svalue && sys_logname->svalue[0]) {
		if (com_gamedir[0])
			snprintf (logname, MAX_OSPATH, "%s/%s.log", com_gamedir,
					sys_logname->svalue);
		else    // FIXME: HACK HACK HACK!
			snprintf (logname, MAX_OSPATH, "id1/%s.log", sys_logname->svalue);
	} else
		logname[0] = '\0';
}

static void
setlogname (cvar_t *sys_logname)
{
	sys_logname = sys_logname;
	Sys_UpdateLogpath ();
}

void
Sys_Init (void)
{
	int			sdlflags;

#ifdef _WIN32
	// Win32 clients need to make front-end programs happy

	// will fail if semaphore already exists
	qwclsemaphore = CreateMutex (NULL, 0, "qwcl");
	if (!qwclsemaphore)
		Sys_Error ("Project Twilight QW is already running");

	qwclsemaphore = CreateSemaphore (NULL, 0, 1, "qwcl");
#endif

	sys_asciionly = Cvar_Get ("sys_asciionly", "1", CVAR_ARCHIVE, NULL);
	sys_logname = Cvar_Get ("sys_logname", "", CVAR_NONE, &setlogname);

	if (COM_CheckParm ("-condebug"))
		Cvar_Set (sys_logname, "qconsole");

	sdlflags = SDL_INIT_TIMER;
	if (COM_CheckParm ("-noparachute"))
	{
		sdlflags |= SDL_INIT_NOPARACHUTE;
		Sys_Printf ("Flying without a parachute!\n");
	}
	SDL_Init (sdlflags);
			
	Math_Init ();
}

static void
Sys_BackTrace (int fd)
{
#if HAVE_EXECINFO_H
	void		*array[256];
	int			size;

	memset (array, 0, sizeof(array));
	size = backtrace (array, sizeof(array) / sizeof(array[0]));
	backtrace_symbols_fd (array, size, fd);
#else
	fd = fd;	// Make it referenced.
#endif
}

void
Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	Host_Shutdown ();

#ifdef HAVE_FCNTL
// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

#ifdef _WIN32
	// Win32 gets a message box, but needs us to clear events first!
	do
	{
		MSG			msg;

		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}
	while (0);
	MessageBox (NULL, text, "Error", 0);
#endif
	fprintf (stderr, "Error: %s\n", text);

	Sys_BackTrace (2);
	SDL_Quit ();
	Sys_Quit (1);
}


/*
============
returns -1 if not present
============
*/
int
Sys_FileTime (const char *path)
{
	struct stat		buf;

	if (stat (path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}


void
Sys_mkdir (const char *path)
{
#if defined(HAVE__MKDIR)	/* FIXME: ordering hack to compile with mingw */
#define do_mkdir(x)	_mkdir(x)
#elif defined(HAVE_MKDIR)
#define do_mkdir(x)	mkdir(x, 0777)
#else
# error "Need either POSIX mkdir or Win32 _mkdir"
#endif

	int		ret;
	char	*dup, *c;

	ret = do_mkdir(path);
	if (!ret || (errno == EEXIST))
		return;

	dup = strdup(path);
	c = dup;
	if (*c == '/')
		c++;
	while (*c) {
		if (*c == '/') {
			*c = '\0';
			ret = do_mkdir(dup);
			if (ret && (errno != EEXIST))
				Sys_Error("ERROR: Can not make path %s %s (%s)\n",
						dup, path, strerror(errno));

			*c = '/';
		}
		c++;
	}

	ret = do_mkdir(dup);
	if (ret && (errno != EEXIST))
		Sys_Error("ERROR: Can not make path %s (%s)\n", dup, strerror(errno));
}


void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	va_list     argptr;
	static char data[1024];
	int         fd;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof (data), fmt, argptr);
	va_end (argptr);

	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) {
		fprintf(stderr, "-%s-\n", file);
		perror ("Unable to open debug file for writing:");
		return;
	}
	write (fd, data, strlen (data));
	close (fd);
}


double
Sys_DoubleTime (void)
{
	static double	curtime = 0.0;
	static Uint32	last = 0;
	Uint32			now = SDL_GetTicks ();

	if (now < last)
		Com_Printf ("Time wrapped or skewed. %f, %d %d\n",
				curtime, last, now);
	else
		curtime += now - last;

	last = now;
	return (double) (curtime / 1000.0);
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

char *
Sys_ConsoleInput (void)
{
	return NULL;
}

int
Sys_CheckClipboardPaste(int key)
{
#ifdef _WIN32
	int		i;
	HANDLE	th;
	char	*clipText, *textCopied;
	if ((key == 'V' || key == 'v') && GetKeyState(VK_CONTROL) < 0)
	{
		if (OpenClipboard(NULL))
		{
			th = GetClipboardData(CF_TEXT);
			if (th)
			{
				clipText = GlobalLock(th);
				if (clipText)
				{
					textCopied = malloc(GlobalSize(th) + 1);
					strcpy(textCopied, clipText);
					/* Substitutes a NULL for every token */
					strtok(textCopied, "\n\r\b");
					i = strlen(textCopied);
					if (i + key_linepos >= MAX_INPUTLINE - 1)
						i = MAX_INPUTLINE - 1 - key_linepos;
					if (i > 0)
					{
						textCopied[i]=0;
						strcat(key_lines[edit_line], textCopied);
						key_linepos+=i;;
					}
					free(textCopied);
				}
				GlobalUnlock(th);
			}
			CloseClipboard();
			return true;
		}
	}
#endif
	key = key;
	return false;
}


char *
Sys_ExpandPath (char *str)
{
	static char	buf[PATH_MAX] = "";
    char		*s, *p;

	s = str;
	if (*s == '~')
	{
		s++;
		if (*s == '/' || *s == '\0')
		{                           
			/* Current user's home directory */
			if ((p = getenv("HOME")))
				strlcpy(buf, p, PATH_MAX);
#ifdef _WIN32
			else if ((p = getenv("USERPROFILE")))
				strlcpy(buf, p, PATH_MAX);
			else if ((p = getenv("WINDIR")))
				strlcpy(buf, p, PATH_MAX);
#endif /* _WIN32 */
			else
				/* This should never happen */
				strlcpy(buf, ".", PATH_MAX);

			strlcat (buf, s, PATH_MAX);
		} else {
			/* Get named user's home directory */
			if ((p = strchr(s, '/')) != NULL)
			{
#ifdef HAVE_GETPWNAM
				struct passwd   *entry;
				*p = '\0';
				if ((entry = getpwnam(s)) != NULL)
				{
					strlcpy (buf, entry->pw_dir, PATH_MAX);
					*p = '/';
					strlcat (buf, p, PATH_MAX);
				} else
#endif /* HAVE_GETPWNAM */
					/* Can't expand this, leave it alone */
					strlcpy (buf, str, PATH_MAX);
			}
		}
	} else
		/* Does not begin with ~, leave it alone */
		strlcpy (buf, str, PATH_MAX);

	return buf;
}

int
main (int argc, char *argv[])
{
	sys_gametypes = GAME_QW_CLIENT;

	Cmdline_Init (argc, argv);

#ifdef HAVE_FCNTL
	if (!COM_CheckParm ("-noconinput"))
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	if (COM_CheckParm ("-nostdout"))
		nostdout = 1;

	Host_Init ();

	while (1)
	{
		// find time spent rendering last frame
		curtime = Sys_DoubleTime ();

		Host_Frame (curtime);
	}

	return 0;
}

