/*
	$RCSfile$

	Copyright (C) 2004-2005 Thad Ward

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

#ifndef __WINCONSOLE_H
#define __WINCONSOLE_H

void WinCon_Printf(const char *text);
void WinCon_Shutdown(void);
void WinCon_Init(void);
void WinCon_PumpMessages(void);

// functions so the info can be updated only when it changes.
void WinCon_SetMapname(const char *mapname);
void WinCon_SetMaxClients(Uint maxclients);
void WinCon_SetConnectedClients(Uint clients);
void WinCon_SetPort(Uint port);

#endif /* __WINCONSOLE_H */
