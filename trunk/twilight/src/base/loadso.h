/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.
	Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

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

#ifndef __LOADSO_H
#define __LOADSO_H

void *TWI_LoadObject (const char *sofile);
void *TWI_LoadFunction (void *handle, const char *name);
void TWI_UnloadObject (void *handle);

#endif // __LOADSO_H

