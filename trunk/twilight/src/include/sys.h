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
// sys.h -- non-portable functions

#ifndef __SYS_H
#define __SYS_H

extern int sys_memsize;
extern void *sys_membase;


int         Sys_FileTime (char *path);
void        Sys_mkdir (char *path);

// FIXME: qw-server does not currently have Sys_DebugLog
void        Sys_DebugLog (char *file, char *fmt, ...);
void        Sys_Error (char *error, ...);

// an error will cause the entire program to exit

void        Sys_Printf (char *fmt, ...);

// send text to the console

void        Sys_Quit (void);
double      Sys_DoubleTime (void);
char       *Sys_ConsoleInput (void);
void        Sys_Init (void);

char       *Sys_ExpandPath (char *str);
// returns true if a clipboard paste occurred
int         Sys_CheckClipboardPaste(int key);

// FIXME: not in server
void        Sys_SendKeyEvents (void);

#endif // __SYS_H

