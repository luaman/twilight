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
#include <unistd.h>
#include <dirent.h>
	
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "pak.h"
#include "sys.h"
#include "rw_ops.h"

typedef struct fsp_group_s {
	fs_file_t	*pak;
	int			dirofs;
	int			nfiles;
} fsp_group_t;

typedef struct fsp_file_s {
	int	ofs;
} fsp_file_t;

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;


static void
FSP_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSP_Free_File (fs_file_t *file)
{
	Zone_Free (file->name_base);
	Zone_Free (file->fs_data);
}

static SDL_RWops *
FSP_Open_File (fs_file_t *file, qboolean write)
{
	fsp_file_t	*p_file;
	fsp_group_t	*pak;
	SDL_RWops	*rw;

	if (write)
		return NULL;

	p_file = file->fs_data;
	pak = file->group->fs_data;

	rw = LimitFromRW(pak->pak->open(pak->pak, false), p_file->ofs, p_file->ofs + file->len);
	return rw;
}

static qboolean
FSP_Add_Pak (fs_group_t *group, fsp_group_t *pak, fs_file_t *file)
{
	dpackfile_t		pfile;
	dpackheader_t	pheader;
	fsp_file_t		*fsp_file;
	SDL_RWops		*rw;
	int				i;

	rw = file->open(file, false);

	pak->pak = file;

	SDL_RWread(rw, &pheader, sizeof(pheader), 1);
	if (strncmp("PACK", pheader.id, 4)) {
		Com_Printf("WARNING: %s", file->name_base);
		if (file->ext)
			Com_Printf(".%s", file->name_base, file->ext);
		Com_Printf(" is NOT a pack file.  Skipping.\n");
		SDL_RWclose (rw);
		return false;
	}
	pheader.dirofs = LittleLong(pheader.dirofs);
	pheader.dirlen = LittleLong(pheader.dirlen);
	pak->nfiles = pheader.dirlen / sizeof(pfile);

	SDL_RWseek(rw, pheader.dirofs, SEEK_SET);
	for (i = 0; i < pak->nfiles; i++) {
		SDL_RWread(rw, &pfile, sizeof(pfile), 1);
		pfile.filepos = LittleLong (pfile.filepos);
		pfile.filelen = LittleLong (pfile.filelen);
		fsp_file = Zone_Alloc (fs_zone, sizeof(fsp_file_t));
		fsp_file->ofs = pfile.filepos;
		FS_Add_File (group, pfile.name, pfile.filelen, FSP_Open_File, fsp_file);
	}
	SDL_RWclose(rw);
	return true;
}

fs_group_t *
FSP_New_Group (fs_file_t *in, fs_group_t *parent, const char *id)
{
	fs_group_t	*group;
	fsp_group_t	*pak;

	group = FS_Alloc_Group (parent, id);

	pak = Zone_Alloc (fs_zone, sizeof (fsp_group_t));
	group->fs_data = pak;
	group->free = FSP_Free;
	group->free_file = FSP_Free_File;

	if (FSP_Add_Pak (group, pak, in))
		return group;
	else {
		FS_Free_Group (group);
		return NULL;
	}
}
