/*
	$RCSfile$

	Copyright (C) #YEAR#  #AUTHOR#

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

#ifndef __HOST_H
#define __HOST_H

extern qboolean noclip_anglehack;

extern struct cvar_s *sys_ticrate;
extern struct cvar_s *sys_nostdout;

extern qboolean host_initialized;		// true if into command execution
extern double host_frametime;
extern Uint8 *host_basepal;
extern Uint8 *host_colormap;
extern int  host_framecount;			// incremented every frame, never reset
extern double realtime;					// not bounded in any way, changed at

										// start of every frame, never reset

void		Host_ClearMemory (void);
void		Host_ServerFrame (void);
void		Host_InitCommands (void);
void		Host_Init (void);
void		Host_Shutdown (void);
void		Host_Error (char *error, ...);
void		Host_EndGame (char *message, ...);
void		Host_Frame (float time);
void		Host_Quit_f (void);
void		Host_ClientCommands (char *fmt, ...);
void		Host_ShutdownServer (qboolean crash);

extern qboolean msg_suppress_1;			// suppresses resolution and cache size 
										// console output
										// an fullscreen DIB focus gain/loss
extern int	current_skill;				// skill level for currently loaded
										// level (in case
										// the user changes the cvar while the
										// level is
										// running, this reflects the level
										// actually in use)

extern qboolean isDedicated;

extern int	minimum_memory;

//
// chase
//
extern struct cvar_s *chase_active;

void		Chase_Init_Cvars (void);
void		Chase_Init (void);
void		Chase_Reset (void);
void		Chase_Update (void);

#endif // __HOST_H

