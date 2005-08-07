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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "quakedef.h"
#include "cclient.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "renderer/draw.h"
#include "keys.h"
#include "renderer/video.h"
#include "strlib.h"
#include "sys.h"
#include "host.h"

static memzone_t *con_zone;

int con_ormask;
console_t *con;

static int con_linewidth;						// characters across screen

static float con_cursorspeed = 4;

static cvar_t *con_notifytime;

#define NUM_CON_TIMES 4
static double con_cleartime;

static int con_vislines;

qboolean con_initialized;


void
Con_ToggleConsole_f (void)
{
	Key_ClearEditLine (edit_line);

	if (key_dest == key_console)
	{
		if (ccls.state == ca_active)
			key_dest = key_game;
	}
	else
		key_dest = key_console;

	Con_ClearNotify ();
}


static void
Con_Clear_f (void)
{
	int i;
	if (!con)
		return;

	for (i = 0; i < CON_LINES; i++)
		if (con->raw_lines[i].text)
			Zone_Free(con->raw_lines[i].text);

	memset (con->raw_lines, 0, sizeof(con->raw_lines));
}



void
Con_ClearNotify (void)
{
	con_cleartime = host.time;
}



static void
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
static void
Con_MessageMode2_f (void)
{
	chat_team = true;
	key_dest = key_message;
}


static void
Con_Resize (console_t *con)
{
	int			width;

	width = (vid.width_2d / con->tsize) - 2;

	if (width < 1) // video hasn't been initialised yet
		width = 38;

	con_linewidth = width;
}


/*
================
If the line width has changed, reformat the buffer.
================
*/
void
Con_CheckResize (void)
{
	if (con)
		Con_Resize (con);
}


void
Con_Init_Cvars (void)
{
	con_notifytime = Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL);
}


void
Con_Init (void)
{
	con_zone = Zone_AllocZone ("console");

	con = Zone_Alloc (con_zone, sizeof(console_t));

	// these must be initialised here
	con_linewidth = -1;
	con->current_raw = 0;
	con->x = 0;
	con->display = 1;

	Size_Changed2D (NULL);
	con_initialized = true;

	Com_Printf ("Console initialized.\n");
	Com_Printf ("Quake is Copyright 1996,1997  Id Software, Inc.\n");
	Com_Printf ("See CVS logs for list of Twilight copyright holders.\n");

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
}



static con_line_t *
Con_Linefeed (void)
{
	con_line_t	*line;

	line = &con->raw_lines[con->current_raw % CON_LINES];
	if (line->text) {
		Zone_Free(line->text);
		line->text = NULL;
		line->length = 0;
	}
	line->time = host.time;
	con->current_raw++;
	return line;
}

/*
================
Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void
Con_Print (const char *txt)
{
	int			mask;
	char		c;
	con_line_t	*line;

	if (!con_initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2)
	{
		// go to colored text
		mask = 128;
		txt++;
	}
	else
		mask = 0;

	while ((c = *txt++))
	{
		switch (c)
		{
			case '\r':
				con->x = 0;
				break;
			case '\n':
				con->tmp_line[con->x++] = '\0';
				line = Con_Linefeed();
				line->text = Zone_Alloc(con_zone, con->x);
				line->length = con->x;
				memcpy(line->text, con->tmp_line, con->x);
				con->x = 0;
				break;
			default:
				con->tmp_line[con->x++] = c | mask | con_ormask;
				con->tmp_line[con->x] = '\0';
				break;
		}
		if (con->x >= sizeof(con->tmp_line)) {
			line = Con_Linefeed();
			line->text = Zone_Alloc(con_zone, con->x);
			line->length = con->x;
			memcpy(line->text, con->tmp_line, con->x);
			con->x = 0;
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
The input line scrolls horizontally if typing goes beyond the right edge
================
*/
static void
Con_DrawInput (void)
{
	char		*text;

	if (key_dest != key_console && (ccls.state >= ca_active))
		// don't draw anything (always draw if not connected)
		return;

	text = key_lines[edit_line];

	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	Draw_String_Len(con->tsize, con_vislines - (con->tsize * 2.75), text,
			con_linewidth, con->tsize);

	if ((int) (host.time * con_cursorspeed) & 1)
		Draw_Character (min(1 + key_linepos, con_linewidth) * con->tsize,
				con_vislines - (con->tsize * 2.65), 11, con->tsize);
}

static void
Con_FindLine (console_t *con, int line_len, int line,
		int *r_line, int *r_line_pos)
{
	int		i, pos;

	pos = 0;
	for (i = con->current_raw - 1; (pos < line) && i >= 0; i--) {
		pos += (con->raw_lines[i % CON_LINES].length + line_len - 1) / line_len;
	}
	*r_line_pos = pos - line;
	*r_line = i + 1;
}


/*
================
Draws the last few lines of output transparently over the game top
================
*/
void
Con_DrawNotify (void)
{
	int			i, y, line, line_pos, line_pos_max;
	con_line_t	*l;
	char		*text;
	double		kill_time;
	char		*s;
	Uint		skip;

	kill_time = max(con_cleartime, host.time - con_notifytime->fvalue);

	Con_FindLine (con, con_linewidth, NUM_CON_TIMES, &line, &line_pos);
	line_pos_max = -1 + (con->raw_lines[line % CON_LINES].length + con_linewidth - 1) / con_linewidth;
	for (i = y = 0; i < NUM_CON_TIMES; i++, line_pos++)
	{
		if (line_pos > line_pos_max) {
			line++;
			line_pos_max = -1 + (con->raw_lines[line % CON_LINES].length + con_linewidth - 1) / con_linewidth;
			line_pos = 0;
		}
		if ((line_pos < 0) || ((con->current_raw - line) > CON_LINES))
			continue;
		l = &con->raw_lines[line % CON_LINES];
		if (!l->text || !l->length || (l->time <= kill_time))
			continue;
		text = &l->text[line_pos * con_linewidth];
		Draw_String_Len(con->tsize, y, text, con_linewidth, con->tsize);
		y += con->tsize;
	}

	if (key_dest == key_message)
	{
		if (chat_team)
		{
			Draw_String (con->tsize, y, "say_team:", con->tsize);
			skip = 11;
		}
		else
		{
			Draw_String (con->tsize, y, "say:", con->tsize);
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width_2d / con->tsize) - (skip + 1))
			s += chat_bufferlen -
				((int) (vid.width_2d / con->tsize) - (skip + 1));

		Draw_String (skip * con->tsize, y, s, con->tsize);

		Draw_Character ((strlen(s) + skip) * con->tsize, y,
				10 + ((int) (host.time * con_cursorspeed) & 1),
				con->tsize);
		y += con->tsize;
	}
}

/*
================
Draws the console with the solid background
================
*/
void
Con_DrawConsole (int lines)
{
	Uint		i, rows;
	int			x, y, line, line_pos;
	con_line_t	*l;
	char		*text;

	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground (lines);

	con_vislines = lines;

	// rows of text to draw
	rows = (lines - (con->tsize * 2.75)) / con->tsize;

	y = lines - (con->tsize * 3.75);

	// draw from the bottom up
	if (con->display != 1)
	{
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character ((x + 1) * con->tsize, y, '^', con->tsize);

		y -= con->tsize;
		rows--;
	}

	Con_FindLine (con, con_linewidth, con->display, &line, &line_pos);
	for (i = 0; i < rows; i++, y -= con->tsize, line_pos--)
	{
		if (line_pos < 0) {
			line--;
			line_pos = -1 + (con->raw_lines[line % CON_LINES].length + con_linewidth - 1) / con_linewidth;
		}
		if (line < 0)
			break;
		if ((line_pos < 0) || ((con->current_raw - line) > CON_LINES))
			continue;
		l = &con->raw_lines[line % CON_LINES];
		if (!l->text || !l->length)
			continue;
		text = &l->text[line_pos * con_linewidth];
		Draw_String_Len(con->tsize, y, text, con_linewidth, con->tsize);
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
static void
Con_DisplayList(const char **list)
{
	int			i = 0;
	int			pos = 0;
	int			len = 0;
	int			maxlen = 0;
	int			width = (con_linewidth - 4);
	const char	**walk = list;

	while (*walk)
	{
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list)
	{
		len = strlen(*list);
		if (pos + maxlen >= width)
		{
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
	const char		*cmd = "";
	char		*s;
	int			c, v, a, i;
	int			cmd_len;
	const char	**list[3] = {0, 0, 0};

	s = key_lines[edit_line] + 1;
	if (*s == '\\' || *s == '/')
		s++;
	if (!*s)
		return;

	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);
	
	if (!(c + v + a))
	{
		// No possible matches, let the user know they're insane
		Com_Printf("\n\nNo matching aliases, commands, or cvars were found.\n\n");
		return;
	}
	
	if (c + v + a == 1)
	{
		if (c)
			list[0] = Cmd_CompleteBuildList(s);
		else if (v)
			list[0] = Cvar_CompleteBuildList(s);
		else
			list[0] = Cmd_CompleteAliasBuildList(s);
		cmd = *list[0];
		cmd_len = strlen (cmd);
	}
	else
	{
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList(s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList(s));
		if (a)
			cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));

		cmd_len = strlen (s);
		do
		{
			for (i = 0; i < 3; i++)
			{
				char ch = cmd[cmd_len];
				const char **l = list[i];
				if (l)
				{
					while (*l && (*l)[cmd_len] == ch)
						l++;
					if (*l)
						break;
				}
			}
			if (i == 3)
				cmd_len++;
		}
		while (i == 3);
		// 'quakebar'
		Com_Printf("\n\35");
		for (i = 0; i < con_linewidth - 4; i++)
			Com_Printf("\36");
		Com_Printf("\37\n");

		// Print Possible Commands
		if (c)
		{
			Com_Printf("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList(list[0]);
		}
		
		if (v)
		{
			Com_Printf("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList(list[1]);
		}
		
		if (a)
		{
			Com_Printf("%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
			Con_DisplayList(list[2]);
		}
	}
	
	if (cmd)
	{
		strlcpy(key_lines[edit_line] + 1, cmd, cmd_len + 1);
		key_linepos = cmd_len + 1;
		if (c + v + a == 1)
		{
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
		}
		key_lines[edit_line][key_linepos] = 0;
	}
	for (i = 0; i < 3; i++)
		if (list[i])
			free ((void *) list[i]);
}

