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
#include <pwd.h>
#endif
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include "conproc.h"
#endif

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "compat.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"


int         nostdout = 0;

qboolean    isDedicated;
int			sys_memsize = 0;
void	   *sys_membase = NULL;


#ifdef _WIN32
HANDLE				hinput, houtput;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;
#endif

char		*qdate = __DATE__;

// =======================================================================
// General routines
// =======================================================================


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

	if (nostdout)
		return;

#ifdef _WIN32
	if (isDedicated)
	{
		DWORD		dummy;

		WriteFile(houtput, text, strlen (text), &dummy, NULL);
		FlushFileBuffers(houtput);
		return;
	}
#endif

	for (p = (unsigned char *) text; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf ("[%02x]", *p);
		else
			putc (*p, stdout);

	fflush(stdout);
}

void
Sys_Quit (void)
{
	Host_Shutdown ();
#ifdef HAVE_FCNTL
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

#ifdef _WIN32
	if (isDedicated)
		FreeConsole ();
#endif

	exit (0);
}

#ifdef _WIN32
HANDLE		qwclsemaphore;
#endif

void
Sys_Init (void)
{
#ifdef _WIN32
	// Win32 clients need to make front-end programs happy

	// will fail if semaphore already exists
	qwclsemaphore = CreateMutex (NULL, 0, "qwcl");
	if (!qwclsemaphore)
		Sys_Error ("Project Twilight QW is already running");

	qwclsemaphore = CreateSemaphore (NULL, 0, 1, "qwcl");
#endif

	Math_Init ();
}

void
Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        text[1024];

	Host_Shutdown ();

#ifdef HAVE_FCNTL
// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

#ifdef _WIN32
	// Win32 gets a GUI message box
	MessageBox (NULL, text, "Error", 0);
#else
	fprintf (stderr, "Error: %s\n", text);
#endif

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


void
Sys_DebugLog (char *file, char *fmt, ...)
{
	va_list     argptr;
	static char data[1024];
	int         fd;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof (data), fmt, argptr);
	va_end (argptr);

	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write (fd, data, strlen (data));
	close (fd);
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

char *
Sys_ConsoleInput (void)
{
#ifdef _WIN32
	static char	text[256];
	static int		len;
	INPUT_RECORD	recs[1024];
	CHAR		ch;
	DWORD		numevents, numread, dummy;

	if (!isDedicated)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}

						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						if (len)
						{
							len--;
						}
						break;

					default:
						if (ch >= ' ')
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);	
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}

	return NULL;
#else
	return NULL;
#endif
}

#ifdef _WIN32
char *
Sys_ExpandPath (char *str)
{
	static char buf[MAX_PATH] = "";
	char *s = str, *p;

	if (*s == '~')
	{
		s++;
		if (*s == '/' || *s == '\0')
		{
			/* Current user's home directory */
			if ((p = getenv("TWILIGHT")))
				strlcpy(buf, p, MAX_PATH);
			else if ((p = getenv("HOME")))
				strlcpy(buf, p, MAX_PATH);
			else if ((p = getenv("WINDIR")))
				strlcpy(buf, p, MAX_PATH);
			else
				/* should never happen */
				strlcpy(buf, ".", MAX_PATH);
			strlcat (buf, s, MAX_PATH);
		} else {
			/* ~user expansion in win32 always fails */
			strcpy(buf, "");
		}
	} else
		strlcpy (buf, str, MAX_PATH);

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

int
main (int c, char **v)
{
	double      time, oldtime, newtime;
	int         j;

	COM_InitArgv (c, v);

	sys_memsize = 16 * 1024 * 1024;

	j = COM_CheckParm ("-mem");
	if (j)
		sys_memsize = (int) (Q_atof (com_argv[j + 1]) * 1024 * 1024);
	else
		if (COM_CheckParm ("-minmemory"))
			sys_memsize = MINIMUM_MEMORY;
	if (sys_memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game",
				sys_memsize / (float) 0x100000);

	if (!(sys_membase = malloc (sys_memsize)))
			Sys_Error ("Can't allocate %ld\n", sys_memsize);

#ifdef HAVE_FCNTL
	if (!COM_CheckParm ("-noconinput"))
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	if (COM_CheckParm ("-nostdout"))
		nostdout = 1;

	Sys_Init ();

	Host_Init ();

#ifdef _WIN32
	if (isDedicated)
	{
		int t;

		if (!AllocConsole ())
		{
			Sys_Error ("Couldn't create dedicated server console");
		}

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)Q_atoi (com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm ("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)Q_atoi (com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm ("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)Q_atoi (com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}
#endif

	oldtime = Sys_DoubleTime ();
	while (1) {
// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);
		oldtime = newtime;
	}

	return 0;
}

