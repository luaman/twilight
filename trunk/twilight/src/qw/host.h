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
// host.h

#ifndef __HOST_H
#define __HOST_H

extern struct cvar_s *sys_ticrate;
extern struct cvar_s *sys_nostdout;

extern struct cvar_s *password;

extern double host_frametime;
extern byte *host_basepal;
extern byte *host_colormap;
extern int  host_framecount;			// incremented every frame, never reset
extern double realtime;					// not bounded in any way, changed at

										// start of every frame, never reset

void        Host_ServerFrame (void);
void        Host_InitCommands (void);
void        Host_Init (void);
void        Host_Shutdown (void);
void        Host_Error (char *error, ...);
void        Host_EndGame (char *message, ...);
void        Host_Frame (float time);
void        Host_Quit_f (void);
void        Host_ClientCommands (char *fmt, ...);
void        Host_ShutdownServer (qboolean crash);

//extern qboolean msg_suppress_1;			// suppresses resolution and cache size 
										// console output
										// an fullscreen DIB focus gain/loss

#endif // __HOST_H

