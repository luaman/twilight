/*
	$RCSfile$

	Copyright (C) 2002  Zephaniah E. Hull

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

#ifndef __TEAMPLAY_H
#define __TEAMPLAY_H

const char *Team_ParseSay (const char *s);
void Team_Dead (void);
void Team_NewMap (void);
void Team_Init_Cvars (void);
void Team_Init (void);

#endif // __TEAMPLAY_H

