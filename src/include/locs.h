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

#ifndef __LOCS_H
#define __LOCS_H

#include "qtypes.h"

typedef struct location_s {
	vec3_t	where;
	char	name[128];
} location_t;

void loc_init ();
void loc_new (vec3_t where, const char *name);
void loc_clear ();
location_t *loc_search (vec3_t where);
void loc_load (const char *map);
void loc_newmap (const char *worldname);
void loc_delete (location_t *del);
void loc_write (const char *worldname);

#endif // __LOCS_H

