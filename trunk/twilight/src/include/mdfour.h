/*
	$RCSfile$

	Copyright (C) 1997-1998  Andrew Tridgell

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
//	an implementation of MD4 designed for use in the SMB authentication
//	protocol

#ifndef __MDFOUR_H
#define __MDFOUR_H

#ifdef HAVE_SDL_H
# include "SDL_types.h"
#else
# include "sdlstub.h"
#endif

struct mdfour {
	Uint32 A, B, C, D;
	Uint32 totalN;
};

Uint32 Com_BlockChecksum (void *buffer, int length);

#endif	// __MDFOUR_H

