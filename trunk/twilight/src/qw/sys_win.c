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
// sys_win.h
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include <io.h>
#include <direct.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

#include "quakedef.h"
#include "winquake.h"

#define MINIMUM_WIN_MEMORY	0x0c00000
#define MAXIMUM_WIN_MEMORY	0x1000000

#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

int         starttime;
qboolean    ActiveApp = 1, Minimized = 0;	// LordHavoc: FIXME: set these

											// based on the real situation

static HANDLE hinput, houtput;

HANDLE      qwclsemaphore;

static HANDLE tevent;

void        Sys_InitFloatTime (void);

void        Sys_PopFPCW (void);
void        Sys_PushFPCW_SetHigh (void);

int			sys_memsize = 0;
void	   *sys_membase = NULL;

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
	write (fd, data, Q_strlen (data));
	close (fd);
};

/*
===============================================================================

FILE IO

===============================================================================
*/

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
Sys_FileTime (char *path)
{
	FILE       *f;
	int         retval;

	f = fopen (path, "rb");

	if (f) {
		fclose (f);
		retval = 1;
	} else {
		retval = -1;
	}

	return retval;
}

void
Sys_mkdir (char *path)
{
	_mkdir (path);
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
#if 0
	LARGE_INTEGER PerformanceFreq;
	unsigned int lowpart, highpart;
#endif

#ifndef SERVERONLY
	// allocate a named semaphore on the client so the
	// front end can tell if it is alive

	// mutex will fail if semephore already exists
	qwclsemaphore = CreateMutex (NULL,	/* Security attributes */
								 0,		/* owner */
								 "qwcl");	/* Semaphore name */
	if (!qwclsemaphore)
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

	qwclsemaphore = CreateSemaphore (NULL,	/* Security attributes */
									 0,	/* Initial count */
									 1,	/* Maximum count */
									 "qwcl");	/* Semaphore name */
#endif

#if 0
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
#endif

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod (1);

	Math_Init();
}


void
Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        text[1024];

	Host_Shutdown ();

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	MessageBox (NULL, text, "Error", 0 /* MB_OK */ );

#ifndef SERVERONLY
	CloseHandle (qwclsemaphore);
#endif

	exit (1);
}

void
Sys_Printf (char *fmt, ...)
{
	va_list     argptr;

	va_start (argptr, fmt);
	vprintf (fmt, argptr);
	va_end (argptr);
}

void
Sys_Quit (void)
{
	Host_Shutdown ();
#ifndef SERVERONLY
	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);
#endif

	exit (0);
}


#if 0
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

	Sys_PushFPCW_SetHigh ();

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

	Sys_PopFPCW ();

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

#endif

double
Sys_DoubleTime (void)
{
	static DWORD starttime;
	static qboolean first = true;
	DWORD       now;

	now = timeGetTime ();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime)				// wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}

char       *
Sys_ConsoleInput (void)
{
	static char text[256];
	static int  len;
	INPUT_RECORD recs[1024];
	DWORD         dummy, ch, numread, numevents;

#if 0
	int         i;
	HANDLE      th;
	char       *clipText, *textCopied;
#endif

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
						}
						break;

					case '\b':
						WriteFile (houtput, "\b \b", 3, &dummy, NULL);
						if (len) {
							len--;
							putchar ('\b');
						}
						break;

					default:
						Con_Printf ("Stupid: %d\n",
									recs[0].Event.KeyEvent.dwControlKeyState);
// LordHavoc: clipboard stuff in an SDL app?  no thanks
#if 0
						if (((ch == 'V' || ch == 'v')
							 && (recs[0].Event.KeyEvent.
								 dwControlKeyState & (LEFT_CTRL_PRESSED |
													  RIGHT_CTRL_PRESSED)))
							||
							((recs[0].Event.KeyEvent.
							  dwControlKeyState & SHIFT_PRESSED)
							 && (recs[0].Event.KeyEvent.wVirtualKeyCode ==
								 VK_INSERT))) {
							if (OpenClipboard (NULL)) {
								th = GetClipboardData (CF_TEXT);
								if (th) {
									clipText = GlobalLock (th);
									if (clipText) {
										textCopied =
											malloc (GlobalSize (th) + 1);
										Q_strcpy (textCopied, clipText);
/* Substitutes a NULL for every token */
										Q_strtok (textCopied, "\n\r\b");
										i = Q_strlen (textCopied);
										if (i + len >= 256)
											i = 256 - len;
										if (i > 0) {
											textCopied[i] = 0;
											text[len] = 0;
											Q_strcat (text, textCopied);
											len += dummy;
											WriteFile (houtput, textCopied, i,
													   &dummy, NULL);
										}
										free (textCopied);
									}
									GlobalUnlock (th);
								}
								CloseClipboard ();
							}
						} else
#endif
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
HINSTANCE   global_hInstance;
int         global_nCmdShow;
char       *argv[MAX_NUM_ARGVS];
static char *empty_string = "";

//HWND      hwnd_dialog;


int         WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		 int nCmdShow)
{
	int			pargc;
	char		**pargv;
	double      time, oldtime, newtime;
	MEMORYSTATUS lpBuffer;
	int         t;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof (MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	pargc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (pargc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[pargc] = lpCmdLine;
			pargc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}

		}
	}

	pargv = argv;

	COM_InitArgv (pargc, pargv);

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
		Sys_Error ("Couldn't create event\n");

	Sys_Init ();

// because sound is off until we become active
	S_BlockSound ();

	Sys_Printf ("Host_Init\n");
	Host_Init ();

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1) {
		// yield the CPU for a little while when paused, minimized, or not the
		// focus
		if ((cl.paused && !ActiveApp) || Minimized) {
			SleepUntilInput (PAUSE_SLEEP);
		} else if (!ActiveApp)
			SleepUntilInput (NOT_FOCUS_SLEEP);

		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;
	}

	/* return success of application */
	return TRUE;
}
