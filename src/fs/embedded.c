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

#include <sys/types.h>
#include <sys/stat.h>
	
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "embedded.h"
#include "sys.h"
#include "rw_ops.h"

#include "embedded_data.h"

static void
FSE_Free_File (fs_file_t *file)
{
	Zone_Free (file->name_base);
}

static SDL_RWops *
FSE_Open_File (fs_file_t *file, Uint32 flags)
{
	embeddedfile_t	*e_file = file->fs_data.data;

	if (flags & FSF_WRITE)
		return NULL;

	return SDL_RWFromMem (e_file->data, e_file->datasize);
}

static qboolean
FSE_Add (fs_group_t *group)
{
	embeddedfile_t	*e_file;
	fs_file_data_t	file_data;

	group->fs_data = embeddedfile;

	for (e_file = embeddedfile; e_file->name; e_file++) {
		file_data.data = e_file;
		FS_Add_File (group, e_file->name, e_file->datasize, FSE_Open_File, file_data);
	}
	return true;
}

fs_group_t *
FSE_New_Group (const char *id)
{
	fs_group_t	*group;

	group = FS_Alloc_Group (NULL, id);

	group->free_file = FSE_Free_File;
	group->flags |= FS_READ_ONLY;

	if (FSE_Add (group))
		return group;
	else {
		FS_Free_Group (group);
		return NULL;
	}
}
