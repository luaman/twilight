/*
	$Id$

	Copyright (C) 2001  Joseph Carter

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

#ifndef __CVAR_H
#define __CVAR_H

#define CVAR_NONE			0

#define CVAR_ARCHIVE		1
#define CVAR_USER			2
#define CVAR_TEMP			4

#define CVAR_USERINFO		256
#define CVAR_SERVERINFO		512

#define CVAR_ROM			4096


typedef struct cvar_s {
	char			   *name;
	char			   *string;
	vec5_t			   	value;
	int					flags;

	// EVIL!  cvar_callback	   *callback; won't work here
	void			  (*callback) (struct cvar_s *var);
} cvar_t;

typedef void (*cvar_callback) (struct cvar_s *var);

void Cvar_Init (void);
void Cvar_Shutdown (void);

cvar_t *Cvar_Get (const char *name, const char *value, const int flags,
				const cvar_callback callback);

void Cvar_Set (cvar_t *var, const char *value);
void Cvar_Set_f (void);

void Cvar_Slide (cvar_t *var, const float change);

void Cvar_Show (cvar_t *var);
qboolean Cvar_LegacyCmd (void);

cvar_t *Cvar_Find (const char *name);

char *Cvar_TabComplete (const char *partial);

void Cvar_Cleanup (void);

void Cvar_Archive (FILE *f);

struct cvar_foreach_s *Cvar_ForeachStart (void);
cvar_t *Cvar_ForeachNext (struct cvar_foreach_s *id);
void Cvar_ForeachEnd (struct cvar_foreach_s *id);

#endif // __CVAR_H

