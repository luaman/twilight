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

#include <limits.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <winsock.h>
#include <conio.h>
#include <direct.h>

#include "qwsvdef.h"
#include "server.h"
#include "cvar.h"

cvar_t     *sys_nostdout;

/*
================
Sys_FileTime
================
*/
int
Sys_FileTime (char *path)
{
	FILE       *f;

	f = fopen (path, "rb");
	if (f) {
		fclose (f);
		return 1;
	}

	return -1;
}

/*
================
Sys_mkdir
================
*/
void
Sys_mkdir (char *path)
{
	_mkdir (path);
}


/*
================
Sys_Error
================
*/
void
Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        text[1024];

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

//    MessageBox(NULL, text, "Error", 0 /* MB_OK */ );
	printf ("ERROR: %s\n", text);

	exit (1);
}


/*
================
Sys_DoubleTime
================
*/
double
Sys_DoubleTime (void)
{
	static DWORD starttime;
	static qboolean first = true;
	DWORD now;

	now = timeGetTime();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}
	
	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}


/*
================
Sys_ConsoleInput
================
*/
char       *
Sys_ConsoleInput (void)
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


/*
================
Sys_Printf
================
*/
void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;

	if (sys_nostdout->value)
		return;

	va_start (argptr, fmt);
	vprintf (fmt, argptr);
	va_end (argptr);
}

/*
================
Sys_Quit
================
*/
void
Sys_Quit (void)
{
	exit (0);
}


/*
=============
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void
Sys_Init (void)
{
	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL);

	Math_Init();
}


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
				Q_strncpy(buf, p, MAX_PATH);
			else if ((p = getenv("HOME")))
				Q_strncpy(buf, p, MAX_PATH);
			else if ((p = getenv("WINDIR")))
				Q_strncpy(buf, p, MAX_PATH);
			else
				/* should never happen */
				Q_strncpy(buf, ".", MAX_PATH);
			Q_strncat (buf, s, MAX_PATH);
		} else {
			/* ~user expansion in win32 always fails */
			Q_strcpy(buf, "");
		}
	} else
		Q_strncpy (buf, str, MAX_PATH);

	return buf;
}


/*
==================
main

==================
*/
char       *newargv[256];

int
main (int argc, char **argv)
{
	quakeparms_t parms;
	double      newtime, time, oldtime;
	struct timeval timeout;
	fd_set      fdset;
	int         t;

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 16 * 1024 * 1024;

	if ((t = COM_CheckParm ("-heapsize")) != 0 && t + 1 < com_argc)
		parms.memsize = Q_atoi (com_argv[t + 1]) * 1024;

	if ((t = COM_CheckParm ("-mem")) != 0 && t + 1 < com_argc)
		parms.memsize = Q_atoi (com_argv[t + 1]) * 1024 * 1024;

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Insufficient memory.\n");

	SV_Init (&parms);

// run one frame immediately for first heartbeat
	SV_Frame (0.1);

//
// main loop
//
	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {
		// select on the net socket and stdin
		// the only reason we have a timeout at all is so that if the last
		// connected client times out, the message would not otherwise
		// be printed until the next event.
		FD_ZERO (&fdset);
		FD_SET (net_socket, &fdset);
		timeout.tv_sec = 0;
		timeout.tv_usec = 100;
		if (select (net_socket + 1, &fdset, NULL, NULL, &timeout) == -1)
			continue;

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		SV_Frame (time);
	}

	return true;
}
