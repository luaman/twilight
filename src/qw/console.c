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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "keys.h"
#include "screen.h"
#include "strlib.h"
#include "sys.h"

extern void Size_Changed2D (cvar_t *cvar);

int			con_ormask;
console_t  *con;

int			con_linewidth;				// characters across screen
int			con_totallines;				// total lines in console scrollback

float		con_cursorspeed = 4;


cvar_t	   *con_notifytime;
cvar_t	   *con_logname;

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime the line was generated

int			con_vislines;
int			con_notifylines;			// scan lines to clear for notify lines

extern char	key_lines[32][MAX_INPUTLINE];
extern int	edit_line;
extern int	key_linepos;


qboolean	con_initialized;

char	logname[MAX_OSPATH] = "";

void
Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;		// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void
Con_ToggleConsole_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console) {
		if (cls.state == ca_active) {
			key_dest = key_game;
		}
	} else {
		key_dest = key_console;
	}

	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void
Con_Clear_f (void)
{
	if (!con)
		return;

	memset (con->text, ' ', CON_TEXTSIZE);
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
void
Con_MessageMode_f (void)
{
	chat_team = false;
	key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void
Con_MessageMode2_f (void)
{
	chat_team = true;
	key_dest = key_message;
}

/*
================
Con_Resize

================
*/
void
Con_Resize (console_t *con)
{
	int         i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char        tbuf[CON_TEXTSIZE];

	width = (vid.width_2d / con->tsize) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)						// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con->text, ' ', CON_TEXTSIZE);
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

		memcpy (tbuf, con->text, CON_TEXTSIZE);
		memset (con->text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con->text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con->current - i + oldtotallines) %
						  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con->current = con_totallines - 1;
	con->display = con->current;
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
	if (con)
		Con_Resize (con);
}

static void
setlogname (cvar_t *con_logname)
{
	if (com_gamedir[0] && con_logname->svalue && con_logname->svalue[0])
		snprintf (logname, MAX_OSPATH, "%s/%s.log", com_gamedir,
				con_logname->svalue);
	else
		logname[0] = '\0';
}

/*
================
Con_Init_Cvars
================
*/
void
Con_Init_Cvars (void)
{
	con_notifytime = Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL);
	con_logname = Cvar_Get ("con_logname", "", CVAR_NONE, &setlogname);
}

/*
================
Con_Init
================
*/
void
Con_Init (void)
{
	if (COM_CheckParm ("-condebug"))
		Cvar_Set (con_logname, "qconsole");

	con = malloc (sizeof (console_t));
	con_linewidth = -1;
	con->current = 0;	// these *MUST*
	con->x = 0;			// be initialized
	con->display = 0;	// here

	Size_Changed2D (NULL);

	Com_Printf ("Console initialized.\n");

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
	con->x = 0;
	if (con->display == con->current)
		con->display++;
	con->current++;
	memset (&con->text[(con->current % con_totallines) * con_linewidth]
			  , ' ', con_linewidth);
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
	int			y, c, l, mask;
	static int	cr;

	if (txt[0] == 1 || txt[0] == 2) {
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
		if (l != con_linewidth && (con->x + l > con_linewidth))
			con->x = 0;

		txt++;

		if (cr) {
			con->current--;
			cr = false;
		}


		if (!con->x) {
			Con_Linefeed ();
			// mark time for transparent overlay
			if (con->current >= 0)
				con_times[con->current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c) {
			case '\n':
				con->x = 0;
				break;

			case '\r':
				con->x = 0;
				cr = 1;
				break;

			default:					// display character and advance
				y = con->current % con_totallines;
				con->text[y * con_linewidth + con->x] = c | mask | con_ormask;
				con->x++;
				if (con->x >= con_linewidth)
					con->x = 0;
				break;
		}

	}
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
	char       *text;

	if (key_dest != key_console && cls.state == ca_active)
		// don't draw anything (always draw if not active)
		return;

	text = key_lines[edit_line];

	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	Draw_String_Len(con->tsize, con_vislines - (con->tsize * 2.75), text,
			con_linewidth, con->tsize);

	if ((int) (cls.realtime * con_cursorspeed) & 1)
		Draw_Character ((1 + key_linepos) * con->tsize,
				con_vislines - (con->tsize * 2.65), 11, con->tsize);
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
	int			v;
	char	   *text;
	int			i;
	float		time;
	char	   *s;
	unsigned	skip;

	v = 0;
	for (i = con->current - NUM_CON_TIMES + 1; i <= con->current; i++) {
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->fvalue)
			continue;
		text = con->text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;

		Draw_String_Len(con->tsize, v, text, con_linewidth, con->tsize);

		v += con->tsize;
	}


	if (key_dest == key_message) {
		clearnotify = 0;

		if (chat_team) {
			Draw_String (con->tsize, v, "say_team:", con->tsize);
			skip = 11;
		} else {
			Draw_String (con->tsize, v, "say:", con->tsize);
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width_2d / con->tsize) - (skip + 1))
			s += chat_bufferlen -
				((int) (vid.width_2d / con->tsize) - (skip + 1));

		Draw_String (skip * con->tsize, v, s, con->tsize);

		Draw_Character ((strlen(s) + skip) * con->tsize, v,
						10 + ((int) (cls.realtime * con_cursorspeed) & 1),
						con->tsize);
		v += con->tsize;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void
Con_DrawConsole (int lines)
{
	unsigned	i;
	int			j, x, y, n;
	unsigned	rows;
	char	   *text;
	int			row;
	char		dlbar[1024];

	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

// draw the text
	con_vislines = lines;

// changed to line things up better
	rows = (lines - (con->tsize * 2.75)) / con->tsize;	// rows of text to draw

	y = lines - (con->tsize * 3.75);

// draw from the bottom up
	if (con->display != con->current) {
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character ((x + 1) * con->tsize, y, '^', con->tsize);

		y -= con->tsize;
		rows--;
	}

	row = con->display;
	for (i = 0; i < rows; i++, y -= con->tsize, row--) {
		if (row < 0)
			break;
		if (con->current - row >= con_totallines)
			break;						// past scrollback wrap point

		text = con->text + (row % con_totallines) * con_linewidth;

		Draw_String_Len(con->tsize, y, text, con_linewidth, con->tsize);
	}

	// draw the download bar
	// figure out width
	if (cls.download) {
		if ((text = strrchr (cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen (text) - con->tsize;
		i = con_linewidth / 3;
		if (strlen (text) > i) {
			y = x - i - 11;
			strncpy (dlbar, text, i);
			dlbar[i] = 0;
			strcat (dlbar, "...");
		} else
			strcpy (dlbar, text);
		strcat (dlbar, ": ");
		i = strlen (dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;

		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		snprintf (dlbar + strlen (dlbar), sizeof (dlbar) - strlen (dlbar),
				  " %02d%%", cls.downloadpercent);

		// draw it
		y = con_vislines - 22 + con->tsize;
		Draw_String(con->tsize, y, dlbar, con->tsize);
	}
// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

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
			Com_Printf("\n");
			pos = 0;
		}

		Com_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Com_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Com_Printf("\n\n");
}

/*
	Con_CompleteCommandLine

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
void
Con_CompleteCommandLine (void)
{
	char	*cmd = "";
	char	*s;
	int		c, v, a, i;
	int		cmd_len;
	char	**list[3] = {0, 0, 0};

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;
	if (!*s)
		return;

	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);
	
	if (!(c + v + a)) {	// No possible matches, let the user know they're insane
		Com_Printf("\n\nNo matching aliases, commands, or cvars were found.\n\n");
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
		Com_Printf("\n\35");
		for (i = 0; i < con_linewidth - 4; i++)
			Com_Printf("\36");
		Com_Printf("\37\n");

		// Print Possible Commands
		if (c) {
			Com_Printf("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList(list[0]);
		}
		
		if (v) {
			Com_Printf("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList(list[1]);
		}
		
		if (a) {
			Com_Printf("%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
			Con_DisplayList(list[2]);
		}
	}
	
	if (cmd) {
		strlcpy(key_lines[edit_line] + 1, cmd, cmd_len + 1);
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

