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
// sys_win.c -- Win32 system interface code
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "SDL.h"
#include "SDL_main.h"

#include <windows.h>

#include "quakedef.h"
#include "client.h"
#include "conproc.h"
#include <io.h>
#include <errno.h>
#include <direct.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "cvar.h"
#include "screen.h"
#include "sys.h"
#include "mathlib.h"
#include "host.h"
#include "strlib.h"

#define MINIMUM_WIN_MEMORY		0x0880000
#define MAXIMUM_WIN_MEMORY		0x1000000

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error
										// running
										// dedicated before exiting
#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

int         starttime;
qboolean    ActiveApp = 1, Minimized = 0;	// LordHavoc: FIXME: set these
											// based on the real situation

static double pfreq;
static double curtime = 0.0;
static double lastcurtime = 0.0;
static int  lowshift;
qboolean    isDedicated;
static qboolean sc_return_on_enter = false;
HANDLE      hinput, houtput;

/*
FIXME: Do I even want to know?
static char *tracking_tag = "Clams & Mooses";
*/

static HANDLE tevent;
static HANDLE hFile;
static HANDLE heventParent;
static HANDLE heventChild;

void        Sys_InitFloatTime (void);

int			sys_memsize = 0;
void	   *sys_membase = NULL;

volatile int sys_checksum;


/*
===============================================================================

FILE IO

===============================================================================
*/

#define	MAX_HANDLES		100
FILE       *sys_handles[MAX_HANDLES];

int
findhandle (void)
{
	int         i;

	for (i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
qfilelength
================
*/
int
qfilelength (FILE * f)
{
	int         pos;
	int         end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int
Sys_FileOpenRead (char *path, int *hndl)
{
	FILE       *f;
	int         i, retval;

	i = findhandle ();

	f = fopen (path, "rb");

	if (!f) {
		*hndl = -1;
		retval = -1;
	} else {
		sys_handles[i] = f;
		*hndl = i;
		retval = qfilelength (f);
	}

	return retval;
}

int
Sys_FileOpenWrite (char *path)
{
	FILE       *f;
	int         i;

	i = findhandle ();

	f = fopen (path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path, strerror (errno));
	sys_handles[i] = f;

	return i;
}

void
Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void
Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int
Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int
Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int
Sys_FileTime (char *path)
{
	FILE       *f;
	int         retval;

	f = fopen (path, "rb");

	if (f) {
		fclose (f);
		retval = 1;
	} else
		retval = -1;

	return retval;
}

void
Sys_mkdir (char *path)
{
	_mkdir (path);
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
//    fd = open(file, O_WRONLY | O_BINARY | O_CREAT | O_APPEND, 0666);
	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write (fd, data, strlen (data));
	close (fd);
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/


/*
================
Sys_Init
================
*/
void
Sys_Init (void)
{
	LARGE_INTEGER PerformanceFreq;
	unsigned int lowpart, highpart;

//	MaskExceptions ();
//	Sys_SetFPCW ();

	if (!QueryPerformanceFrequency (&PerformanceFreq))
		Sys_Error ("No hardware timer available");

// get 32 out of the 64 time bits such that we have around
// 1 microsecond resolution
	lowpart = (unsigned int) PerformanceFreq.LowPart;
	highpart = (unsigned int) PerformanceFreq.HighPart;
	lowshift = 0;

	while (highpart || (lowpart > 2000000.0)) {
		lowshift++;
		lowpart >>= 1;
		lowpart |= (highpart & 1) << 31;
		highpart >>= 1;
	}

	pfreq = 1.0 / (double) lowpart;

	Sys_InitFloatTime ();

	Math_Init();
}


void
Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        text[1024], text2[1024];
	char       *text3 = "Press Enter to exit\n";
	char       *text4 = "***********************************\n";
	char       *text5 = "\n";
	DWORD       dummy;
	double      starttime;
	static int  in_sys_error1 = 0;
	static int  in_sys_error2 = 0;
	static int  in_sys_error3 = 0;

	if (!in_sys_error3)
		in_sys_error3 = 1;

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	if (isDedicated) {
		va_start (argptr, error);
		vsnprintf (text, sizeof (text), error, argptr);
		va_end (argptr);

		snprintf (text2, sizeof (text2), "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen (text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen (text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen (text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);


		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;		// so Enter will get us out of here

		while (!Sys_ConsoleInput () &&
			   ((Sys_DoubleTime () - starttime) < CONSOLE_ERROR_TIMEOUT)) {
		}
	} else {
		// switch to windowed so the message box is visible, unless we already
		// tried that and failed
/*
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			VID_SetDefaultMode ();
*/
		MessageBox (NULL, text, "Quake Error",
					MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
/*
		}
		else
		{
			MessageBox(NULL, text, "Double Quake Error",
					   MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
*/
	}

	if (!in_sys_error1) {
		in_sys_error1 = 1;
		Host_Shutdown ();
	}
// shut down QHOST hooks if necessary
	if (!in_sys_error2) {
		in_sys_error2 = 1;
		DeinitConProc ();
	}

	exit (1);
}

void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        text[1024];
	DWORD       dummy;

	if (isDedicated) {
		va_start (argptr, fmt);
		vsnprintf (text, sizeof (text), fmt, argptr);
		va_end (argptr);

		WriteFile (houtput, text, strlen (text), &dummy, NULL);
	}
}

void
Sys_Quit (void)
{

	Host_Shutdown ();

	if (tevent)
		CloseHandle (tevent);

	if (isDedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}


/*
================
Sys_DoubleTime
================
*/
double
Sys_DoubleTime (void)
{
	static int  sametimecount;
	static unsigned int oldtime;
	static int  first = 1;
	LARGE_INTEGER PerformanceCount;
	unsigned int temp, t2;
	double      time;

//	Sys_PushFPCW_SetHigh ();

	QueryPerformanceCounter (&PerformanceCount);

	temp = ((unsigned int) PerformanceCount.LowPart >> lowshift) |
		((unsigned int) PerformanceCount.HighPart << (32 - lowshift));

	if (first) {
		oldtime = temp;
		first = 0;
	} else {
		// check for turnover or backward time
		if ((temp <= oldtime) && ((oldtime - temp) < 0x10000000)) {
			oldtime = temp;				// so we can't get stuck
		} else {
			t2 = temp - oldtime;

			time = (double) t2 *pfreq;

			oldtime = temp;

			curtime += time;

			if (curtime == lastcurtime) {
				sametimecount++;

				if (sametimecount > 100000) {
					curtime += 1.0;
					sametimecount = 0;
				}
			} else {
				sametimecount = 0;
			}

			lastcurtime = curtime;
		}
	}

//	Sys_PopFPCW ();

	return curtime;
}


/*
================
Sys_InitFloatTime
================
*/
void
Sys_InitFloatTime (void)
{
	int         j;

	Sys_DoubleTime ();

	j = COM_CheckParm ("-starttime");

	if (j) {
		curtime = (double) (Q_atof (com_argv[j + 1]));
	} else {
		curtime = 0.0;
	}

	lastcurtime = curtime;
}


char       *
Sys_ConsoleInput (void)
{
	static char text[256];
	static int  len;
	INPUT_RECORD recs[1024];
	DWORD	numevents, numread, ch, dummy;

	if (!isDedicated)
		return NULL;


	for (;;) {
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput (hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT) {
			if (!recs[0].Event.KeyEvent.bKeyDown) {
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch) {
					case '\r':
						WriteFile (houtput, "\r\n", 2, &dummy, NULL);

						if (len) {
							text[len] = 0;
							len = 0;
							return text;
						} else if (sc_return_on_enter) {
							// special case to allow exiting from the error
							// handler on Enter
							text[0] = '\r';
							len = 0;
							return text;
						}

						break;

					case '\b':
						WriteFile (houtput, "\b \b", 3, &dummy, NULL);
						if (len) {
							len--;
						}
						break;

					default:
						if (ch >= ' ') {
							WriteFile (houtput, &ch, 1, &dummy, NULL);
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}

	return NULL;
}

void
Sys_Sleep (void)
{
	Sleep (1);
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


/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/


/*
==================
WinMain
==================
*/
void
SleepUntilInput (int time)
{

	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}


/*
==================
WinMain
==================
*/
int         global_nCmdShow;
char       *argv[MAX_NUM_ARGVS];

//HWND      hwnd_dialog;

int main (int argc, char **argv)
{
	double      time, oldtime, newtime;
	MEMORYSTATUS lpBuffer;
	int         t;

	lpBuffer.dwLength = sizeof (MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	COM_InitArgv (argc, argv);

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

// take the greater of all the available memory or half the total memory,
// but at least 8 Mb and no more than 16 Mb, unless they explicitly
// request otherwise
	sys_memsize = lpBuffer.dwAvailPhys;

	if (sys_memsize < MINIMUM_WIN_MEMORY)
		sys_memsize = MINIMUM_WIN_MEMORY;

	if (sys_memsize < (lpBuffer.dwTotalPhys >> 1))
		sys_memsize = lpBuffer.dwTotalPhys >> 1;

	if (sys_memsize > MAXIMUM_WIN_MEMORY)
		sys_memsize = MAXIMUM_WIN_MEMORY;

	if (COM_CheckParm ("-heapsize")) {
		t = COM_CheckParm ("-heapsize") + 1;

		if (t < com_argc)
			sys_memsize = Q_atoi (com_argv[t]) * 1024;
	}

	sys_membase = malloc (sys_memsize);

	if (!sys_membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	if (isDedicated) {
		if (!AllocConsole ()) {
			Sys_Error ("Couldn't create dedicated server console");
		}

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

		// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0) {
			if (t < com_argc)
				hFile = (HANDLE) Q_atoi (com_argv[t + 1]);
		}

		if ((t = COM_CheckParm ("-HPARENT")) > 0) {
			if (t < com_argc)
				heventParent = (HANDLE) Q_atoi (com_argv[t + 1]);
		}

		if ((t = COM_CheckParm ("-HCHILD")) > 0) {
			if (t < com_argc)
				heventChild = (HANDLE) Q_atoi (com_argv[t + 1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

	Sys_Printf ("Host_Init\n");
	Host_Init ();

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1) {
		scr_skipupdate = 0;
		if (isDedicated) {
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate->value) {
				Sys_Sleep ();
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
		} else {
			// yield the CPU for a little while when paused, minimized, or not
			// the focus
			if ((cl.paused && !ActiveApp) || Minimized || block_drawing) {
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			} else if (!ActiveApp) {
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
		}

		Host_Frame (time);
		oldtime = newtime;
	}

	/* return success of application */
	return TRUE;
}

