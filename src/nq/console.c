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
// console.c
static const char rcsid[] =
    "$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#ifdef _WIN32
# include <io.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "quakedef.h"
#include "cmd.h"
#include "console.h"
#include "client.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "keys.h"
#include "screen.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"

Uint32			con_linewidth;

float			con_cursorspeed = 4;

#define	CON_TEXTSIZE	16384

qboolean		con_forcedup;				// because no entities to refresh

Uint32			con_totallines;				// total lines in console scrollback
Uint32			con_backscroll;				// lines up from bottom to display
Uint32			con_current;				// where next message will be printed
Uint32			con_x;						// offset in current line for next print
char			*con_text = 0;

cvar_t			*con_notifytime;

#define	NUM_CON_TIMES 4
float			con_times[NUM_CON_TIMES];	// realtime time the line was generated
											// for transparent notify lines
int				con_vislines;

qboolean		con_debuglog;

#define			MAXCMDLINE	256
extern char		key_lines[32][MAXCMDLINE];
extern Uint32	edit_line;
extern Uint32	key_linepos;


qboolean		con_initialized;

Uint32			con_notifylines;			// scan lines to clear for notify lines

extern void	M_Menu_Main_f (void);

/*
================
Con_ToggleConsole_f
================
*/
void
Con_ToggleConsole_f (void)
{
	if (key_dest == key_console) {
		if (cls.state == ca_connected) {
			key_dest = key_game;
			key_lines[edit_line][1] = 0;	// clear any typing
			key_linepos = 1;
		} else {
			M_Menu_Main_f ();
		}
	} else
		key_dest = key_console;

	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof (con_times));
}

/*
================
Con_Clear_f
================
*/
void
Con_Clear_f (void)
{
	if (con_text)
		memset (con_text, ' ', CON_TEXTSIZE);
}


/*
================
Con_ClearNotify
================
*/
void
Con_ClearNotify (void)
{
	int         i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
extern qboolean chat_team;

void
Con_MessageMode_f (void)
{
	key_dest = key_message;
	chat_team = false;
}


/*
================
Con_MessageMode2_f
================
*/
void
Con_MessageMode2_f (void)
{
	key_dest = key_message;
	chat_team = true;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void
Con_CheckResize (void)
{
	Uint32		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char		tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if ((Sint32)width < 1)						// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con_text, ' ', CON_TEXTSIZE);
	} else {
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con_text, CON_TEXTSIZE);
		memset (con_text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con_current - i + oldtotallines) %
							oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init_Cvars
================
*/
void
Con_Init_Cvars (void)
{
	// time in seconds
	con_notifytime = Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL);
}

/*
================
Con_Init
================
*/
void
Con_Init (void)
{
#define MAXGAMEDIRLEN	1000
	char        temp[MAXGAMEDIRLEN + 1];
	char       *t2 = "/qconsole.log";

	con_debuglog = COM_CheckParm ("-condebug");

	if (con_debuglog) {
		if (strlen (com_gamedir) < (MAXGAMEDIRLEN - strlen (t2))) {
			snprintf (temp, sizeof (temp), "%s%s", com_gamedir, t2);
			unlink (temp);
		}
	}

	con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = 0;
	Con_CheckResize ();

	Con_Printf ("Console initialized.\n");

	//
	// register our commands
	//
	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void
Con_Linefeed (void)
{
	con_x = 0;
	con_current++;

	memset (&con_text[(con_current % con_totallines) * con_linewidth], ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void
Con_Print (char *txt)
{
	Uint32		y, l, mask;
	Sint32		c;
	static int	cr;

	con_backscroll = 0;

	if (txt[0] == 1) {
		mask = 128;						// go to colored text
		S_LocalSound ("misc/talk.wav");
		// play talk wav
		txt++;
	} else if (txt[0] == 2) {
		mask = 128;						// go to colored text
		txt++;
	} else
		mask = 0;


	while ((c = *txt)) {
		// count word length
		for (l = 0; l < con_linewidth; l++)
			if (txt[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		txt++;

		if (cr) {
			con_current--;
			cr = false;
		}


		if (!con_x) {
			Con_Linefeed ();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c) {
			case '\n':
				con_x = 0;
				break;

			case '\r':
				con_x = 0;
				cr = 1;
				break;

			default:
				// display character and advance
				y = con_current % con_totallines;
				con_text[y * con_linewidth + con_x] = c | mask;
				con_x++;
				if (con_x >= con_linewidth)
					con_x = 0;
				break;
		}

	}
}


/*
================
Con_DebugLog
================
*/
void
Con_DebugLog (char *file, char *fmt, ...)
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


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	4096
void
Con_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

// also echo to debugging console
	Sys_Printf ("%s", msg);

// log all messages to file
	if (con_debuglog) {
		char        msg2[MAX_OSPATH + 32];

		// LordHavoc: this used to use va(), but that was too dangerous,
		// as Con_Printf and va() calls are often mixed.
		snprintf (msg2, sizeof (msg2), "%s/qconsole.log", com_gamedir);
		Sys_DebugLog (msg2, "%s", msg);
	}

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		// no graphics mode
		return;

// write it to the scrollable buffer
	Con_Print (msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void
Con_DPrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	if (!developer->value)
		return;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void
Con_SafePrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[1024];
	int         temp;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void
Con_DrawInput (void)
{
	Uint32	y, i;
	char	*text, temp[256];

	if (key_dest != key_console && !con_forcedup)
		return;	// don't draw anything
	
	text = strcpy(temp, key_lines[edit_line]);
	y = strlen(text);

	// fill out remainder with spaces
	for (i = y; i < 256; i++)
		text[i] = ' ';
	
	// add the cursor frame
	if ((int)(realtime * con_cursorspeed) & 1)// cursor is visible
		text[key_linepos] = ((int)(realtime * con_cursorspeed) & 1)*11;

	//  prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	y = con_vislines - 16;

	for (i = 0; i < con_linewidth; i++)
		Draw_Character ((i + 1) << 3, con_vislines - 16, text[i]);
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void
Con_DrawNotify (void)
{
	Uint32			i, v, x, skip;
	char			*text, *s;
	float			time;
	extern char		chat_buffer[];
	extern Uint32	chat_bufferlen;

	v = 0;
	for (i = con_current - NUM_CON_TIMES + 1; i <= con_current; i++) {
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime->value)
			continue;
		text = con_text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character ((x + 1) << 3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message) {
		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team) {
			Draw_String (8, v, "say_team:");
			skip = 11;
		} else {
			Draw_String (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;

		if (chat_bufferlen > (vid.width >> 3) - (skip + 1))
			s += chat_bufferlen - ((vid.width >> 3) - (skip + 1));

		Draw_String (skip << 3, v, s);

		Draw_Character ((strlen(s) + skip) << 3, v,
						10 + ((int) (realtime * con_cursorspeed) & 1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void
Con_DrawConsole (int lines, qboolean drawinput)
{
	Uint32		i, j, x, y, rows;
	char       *text;

	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground (lines);

	// draw the text
	con_vislines = lines;

	rows = (lines - 16) >> 3;			// rows of text to draw
	y = lines - 16 - (rows << 3);		// may start slightly negative

	for (i = con_current - rows + 1; i <= con_current; i++, y += 8) {
		j = i - con_backscroll;
		if (j < 0)
			j = 0;
		text = con_text + (j % con_totallines) * con_linewidth;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character ((x + 1) << 3, y, text[x]);
	}

	// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput ();
}


/*
	Con_DisplayList

	New function for tab-completion system
*/
void
Con_DisplayList(char **list)
{
	int	i = 0;
	int	pos = 0;
	int	len = 0;
	int	maxlen = 0;
	int	width = (con_linewidth - 4);
	char	**walk = list;

	while (*walk) {
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen(*list);
		if (pos + maxlen >= width) {
			Con_Printf("\n");
			pos = 0;
		}

		Con_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Con_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Con_Printf("\n\n");
}

/*
	Con_CompleteCommandLine
*/
void
Con_CompleteCommandLine (void)
{
	char	*cmd = "";
	char	*s;
	Uint32	c, v, a, i;
	int		cmd_len;
	char	**list[3] = {0, 0, 0};

	s = key_lines[edit_line] + 1;
	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);
	
	if (!(c + v + a)) {
		// No possible matches, let the user know they're insane
		S_LocalSound ("misc/talk.wav");
		Con_Printf("\n\nNo matches found\n\n");
		return;
	}

	if (c + v + a == 1) {
		if (c)
			list[0] = Cmd_CompleteBuildList(s);
		else if (v)
			list[0] = Cvar_CompleteBuildList(s);
		else
			list[0] = Cmd_CompleteAliasBuildList(s);
		cmd = *list[0];
		cmd_len = strlen (cmd);
	} else {
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList(s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList(s));
		if (a)
			cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));

		cmd_len = strlen (s);
		do {
			for (i = 0; i < 3; i++) {
				char ch = cmd[cmd_len];
				char **l = list[i];
				if (l) {
					while (*l && (*l)[cmd_len] == ch)
						l++;
					if (*l)
						break;
				}
			}
			if (i == 3)
				cmd_len++;
		} while (i == 3);
		// 'quakebar'
		Con_Printf("\n\35");
		for (i = 0; i < con_linewidth - 4; i++)
			Con_Printf("\36");
		Con_Printf("\37\n");

		// Print Possible Commands
		if (c) {
			Con_Printf("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList(list[0]);
		}
		
		if (v) {
			Con_Printf("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList(list[1]);
		}
		
		if (a) {
			Con_Printf("%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
			Con_DisplayList(list[2]);
		}
		return;
	}
	
	if (cmd) {
		strncpy(key_lines[edit_line] + 1, cmd, cmd_len);
		key_linepos = cmd_len + 1;
		if (c + v + a == 1) {
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
		}
		key_lines[edit_line][key_linepos] = 0;
	}
	for (i = 0; i < 3; i++)
		if (list[i])
			free (list[i]);
}

