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

static const char rcsid[] =
	"$Id$";

#include "twiconfig.h"

#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "crc.h"
#include "dir.h"
#include "pak.h"
#include "cmd.h"

static int
FS_Close_New (SDL_RWops *new)
{
	fs_new_t	*cur = new->hidden.unknown.data1;

	cur->group->close_new (cur->group, cur);
	Zone_Free (cur->wanted);
	Zone_Free (new);

	return 0;
}

static int
FS_Seek_New (SDL_RWops *rw, int offset, int whence)
{
	fs_new_t	*new = rw->hidden.unknown.data1;

	return SDL_RWseek (new->rw, offset, whence);
}

static int
FS_Read_New (SDL_RWops *rw, void *ptr, int size, int maxnum)
{
	fs_new_t	*new = rw->hidden.unknown.data1;

	return SDL_RWread (new->rw, ptr, size, maxnum);
}

static int
FS_Write_New (SDL_RWops *rw, const void *ptr, int size, int num)
{
	fs_new_t	*new = rw->hidden.unknown.data1;

	return SDL_RWwrite (new->rw, ptr, size, num);
}

SDL_RWops *
FS_Open_New (const char *file, Uint32 flags)
{
	SDL_RWops	*rw;
	fs_new_t	*new;
	fs_group_t	*cur;

	new = Zone_Alloc (fs_zone, sizeof (fs_new_t));
	new->wanted = Zstrdup (fs_zone, file);
	new->flags = flags;

	for (cur = fs_paths; cur; cur = cur->next) {
		if (cur->open_new && cur->open_new(cur, new)) {
			new->group = cur;

			rw = Zone_Alloc (fs_zone, sizeof (SDL_RWops));
			rw->hidden.unknown.data1 = new;
			rw->seek = FS_Seek_New;
			rw->read = FS_Read_New;
			rw->write = FS_Write_New;
			rw->close = FS_Close_New;
			
			return rw;
		}
	}

	return NULL;
}
