/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.

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

#ifndef __RW_OPS_H
#define __RW_OPS_H

#include "fs.h"

SDL_RWops *LimitFromRW (SDL_RWops *rw, int start, int end);
int RWprintf (SDL_RWops *rw, const char *format, ...);

#endif // __RW_OPS_H

