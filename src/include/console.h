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

	$Id$
*/
//
// console
//

#ifndef __CONSOLE_H
#define __CONSOLE_H

#define		CON_LINES		200
typedef struct {
	char	*text;
	int		length;
	double	time;
} con_line_t;

typedef struct {
	con_line_t	raw_lines[CON_LINES];
	int			current_raw;		// line where next message will be printed
	char		tmp_line[2048];		// buffer for line being worked on.
	Uint		x;					// offset in current line for next print
	int			display;			// bottom of console displays this line
	float		tsize;				// Scale of the font * 8
} console_t;

extern console_t *con;				// point to either con_main or con_chat

extern int  con_ormask;

extern int  con_totallines;
extern qboolean con_initialized;
extern Uint8 *con_chars;
extern int  con_notifylines;		// scan lines to clear for notify lines

void	Con_DrawCharacter (int cx, int line, int num);

void	Con_CheckResize (void);
void	Con_Init_Cvars (void);
void	Con_Init (void);
void	Con_DrawConsole (int lines);
void	Con_Print (char *txt);
void	Con_DrawNotify (void);
void	Con_ClearNotify (void);
void	Con_ToggleConsole_f (void);

void	Con_NotifyBox (char *text);	// during startup for sound / cd warnings

// wrapper function to attempt to either complete the command line
// or to list possible matches grouped by type
// (i.e. will display possible variables, aliases, commands
// that match what they've typed so far)
void Con_CompleteCommandLine(void);

#endif // __CONSOLE_H

