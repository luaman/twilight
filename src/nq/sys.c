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
// Don't need windows.h till we have win32 GUI console
# include <io.h>
# include <conio.h>
#endif
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "cclient.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"
#include "keys.h"
#include "fatal.h"

// LordHavoc: for win32 which does not have PATH_MAX defined without POSIX
// (and that disables lots of other useful stuff)
#ifndef PATH_MAX
# define PATH_MAX 256
#endif

#ifdef WIN32
#include "winconsole.h"
#endif

int nostdout = 0;

static Uint32 sys_sleep;

static cvar_t *sys_asciionly;
static cvar_t *sys_extrasleep;
static cvar_t *sys_logname;

static int sys_gametypes;

char logname[MAX_OSPATH] = "";

double curtime = 0;

qboolean do_stdin = true;

#ifdef _WIN32
HANDLE semaphore;
#endif


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
	va_list		argptr;
	char		text[2048];
	unsigned char		*p;

	if (nostdout)
		return;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof (text), fmt, argptr);
	va_end (argptr);

#ifdef WIN32
	// windows dedicated server console has its own text output routine
	if (ccls.state == ca_dedicated)
	{
		WinCon_Printf(text);
		return;
	}
#endif

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
Sys_Quit (int ret)
{
	Host_Shutdown ();
#ifdef HAVE_FCNTL
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
#ifdef WIN32
	if (ccls.state == ca_dedicated)
		WinCon_Shutdown();
#endif

	SDL_Quit ();
	exit (ret);
}

static void
setextrasleep (cvar_t *sys_extrasleep)
{
	if (sys_extrasleep->ivalue < 0)
		Cvar_Set (sys_extrasleep, "0");
	else if (sys_extrasleep->ivalue > 1000000)
		Cvar_Set (sys_extrasleep, "1000000");

	sys_sleep = (Uint32)((sys_extrasleep->ivalue) * (1.0f / 1000.0f));
}

void
Sys_UpdateLogpath (void)
{
	if (sys_logname && sys_logname->svalue && sys_logname->svalue[0]) {
		if (com_gamedir[0])
			snprintf (logname, MAX_OSPATH, "%s/%s.log", com_gamedir,
					sys_logname->svalue);
		else	// FIXME: HACK HACK HACK!
			snprintf (logname, MAX_OSPATH, "id1/%s.log", sys_logname->svalue);
	} else
		logname[0] = '\0';
}

static void
setlogname (cvar_t *sys_logname)
{
	sys_logname = sys_logname;
	Sys_UpdateLogpath();
}

void
Sys_Init (void)
{
	int			sdlflags;

#ifdef _WIN32
	// Win32 clients need to make front-end programs happy

	// will fail if semaphore already exists
	semaphore = CreateMutex (NULL, 0, "nq");
	if (!semaphore)
		Sys_Error ("Project: Twilight is already running");

	semaphore = CreateSemaphore (NULL, 0, 1, "nq");
#endif

	sys_extrasleep = Cvar_Get ("sys_extrasleep", "0", CVAR_NONE,
			setextrasleep);
	sys_asciionly = Cvar_Get ("sys_asciionly", "1", CVAR_ARCHIVE, NULL);
	sys_logname = Cvar_Get ("sys_logname", "", CVAR_NONE, setlogname);

	if (COM_CheckParm ("-condebug"))
		Cvar_Set (sys_logname, "qconsole");

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

		if (!SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS))
			Sys_Printf ("SetPriorityClass() failed\n");
		else
			Sys_Printf ("Process priority class set to HIGH\n");

		if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
			Cvar_Set (sys_extrasleep, "0");
	}
#endif

	sdlflags = SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE;
	SDL_Init (sdlflags);

	if (COM_CheckParm ("-noparachute"))
		Sys_Printf ("Flying without a parachute!\n");
	else
		Twi_InstallParachute ();
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

	Twi_BackTrace (2);
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
		perror ("Unable to open debug file for writing:");
		return;
	}
	write (fd, data, strlen (data));
	close (fd);
}


double
Sys_DoubleTime (void)
{
	static double   curtime = 0.0;
	static Uint32   last = 0;
	Uint32          now = SDL_GetTicks ();

	if (now < last)
		Com_Printf ("Time wrapped or skewed. %f, %d %d\n",
				curtime, last, now);
	else
		curtime += now - last;

	last = now;
	return (double) (curtime / 1000.0);
}

char *
Sys_ConsoleInput (void)
{
#ifdef _WIN32
	static char		text[256];
	static int		len;
	int				c;

	// on windows, the dedicated server console input is handled in the message pump.
	if (ccls.state == ca_dedicated)
		return NULL;


	// read a line out
	while (_kbhit ())
	{
		c = _getch ();
		putch (c);
		if (c == '\r')
		{
			text[len] = 0;
			putch ('\n');
			len = 0;
			return text;
		}
		if (c == 8)
		{
			if (len)
			{
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
#else
	static char		text[256];
	int				len;

	if (!do_stdin)
		return NULL;

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
#endif
}


// FIXME: Sys_CheckClipboardPaste is the wrong way to do this, fix it!

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
	double		newtime, base;

	sys_gametypes = GAME_NQ_CLIENT|GAME_NQ_SERVER;

	Cmdline_Init (argc, argv);

#ifdef HAVE_FCNTL
	if (!COM_CheckParm ("-noconinput"))
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

#ifdef WIN32
	// if we are in dedicated server mode on windows, start up the console ASAP
	if (COM_CheckParm ("-dedicated"))
		WinCon_Init();
#endif

	Host_Init ();

	if (ccls.state != ca_dedicated && COM_CheckParm ("-nostdout"))
		nostdout = 1;

	base = Sys_DoubleTime ();
	while (1)
	{
#ifdef WIN32
		if (ccls.state == ca_dedicated)
			WinCon_PumpMessages();
#endif
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		curtime = newtime - base;

		Host_Frame (curtime);

		if (ccls.state == ca_dedicated && sys_extrasleep->ivalue)
			SDL_Delay (sys_sleep);
	}

	return 0;
}

