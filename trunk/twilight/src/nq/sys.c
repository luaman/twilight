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
#ifdef __WIN32
# include <windows.h>
# include <io.h>
# include "conproc.h"
#endif

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "cvar.h"
#include "host.h"
#include "mathlib.h"
#include "strlib.h"
#include "sys.h"

// LordHavoc: for win32 which does not have PATH_MAX defined without POSIX
// (and that disables lots of other useful stuff)
#ifndef PATH_MAX
#define PATH_MAX 256
#endif


int         nostdout = 0;

qboolean    isDedicated;


#ifdef _WIN32
HANDLE				hinput, houtput;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;
#endif

char	   *qdate = __DATE__;

cvar_t *sys_asciionly;

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
	va_list			argptr;
	unsigned char	text[2048];
	unsigned char	*p;

	if (nostdout)
		return;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof (text), fmt, argptr);
	va_end (argptr);

#ifdef _WIN32
	if (isDedicated)
	{
		DWORD		dummy;

		WriteFile(houtput, text, strlen (text), &dummy, NULL);
		FlushFileBuffers(houtput);
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

	SDL_Quit();
	exit (0);
}

#ifdef _WIN32
HANDLE		qwclsemaphore;
#endif

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

	Math_Init ();

	sdlflags = SDL_INIT_TIMER;
	if (COM_CheckParm ("-noparachute"))
	{
		sdlflags |= SDL_INIT_NOPARACHUTE;
		Sys_Printf ("Flying without a parachute!\n");
	}

	SDL_Init (sdlflags);
}

void
Sys_Error (char *error, ...)
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
	// Win32 gets a GUI message box
	MessageBox (NULL, text, "Error", 0);
#endif
	fprintf (stderr, "Error: %s\n", text);

	SDL_Quit ();
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
#endif
	return NULL;
}


// FIXME: Sys_CheckClipboardPaste is the wrong way to do this, fix it!

extern int key_linepos;
extern char key_lines[32][MAX_INPUTLINE];
extern int edit_line;

int
Sys_CheckClipboardPaste(int key)
{
#ifdef __WIN32
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
				strncpy(buf, p, PATH_MAX);
#ifdef __WIN32
			else if ((p = getenv("USERPROFILE")))
				strncpy(buf, p, PATH_MAX);
			else if ((p = getenv("WINDIR")))
				strncpy(buf, p, PATH_MAX);
#endif /* __WIN32 */
			else
				/* This should never happen */
				strncpy(buf, ".", PATH_MAX);

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
					strncpy (buf, entry->pw_dir, PATH_MAX);
					if (p)
					{
						*p = '/';
						strlcat (buf, p, PATH_MAX);
					}
				} else
#endif /* HAVE_GETPWNAM */
					/* Can't expand this, leave it alone */
					strncpy (buf, str, PATH_MAX);
			}
		}
	} else
		/* Does not begin with ~, leave it alone */
		strncpy (buf, str, PATH_MAX);

	return buf;
}

int
main (int c, char **v)
{
	double      time, oldtime, newtime, base;

	COM_InitArgv (c, v);

#ifdef HAVE_FCNTL
	if (!COM_CheckParm ("-noconinput"))
		fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	if (COM_CheckParm ("-nostdout"))
		nostdout = 1;

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

	base = oldtime = Sys_DoubleTime ();
	while (1) {
// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		curtime = newtime - base;

		Host_Frame (time);
		oldtime = newtime;
	}

	return 0;
}

