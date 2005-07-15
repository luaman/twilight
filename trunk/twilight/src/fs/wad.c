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
#include "wad.h"
#include "sys.h"
#include "rw_ops.h"


//===============
//   TYPES
//===============

#define	CMP_NONE		0
#define	CMP_LZSS		1

#define	TYP_NONE		0
#define	TYP_LABEL		1

#define	TYP_LUMPY		64				// 64 + grab command number
#define	TYP_PALETTE		64
#define	TYP_QTEX		65
#define	TYP_QPIC		66
#define	TYP_SOUND		67
#define	TYP_MIPTEX		68

typedef struct {
	char        id[4];		// should be WAD2
	int         nfiles;
	int         dirofs;
} dwadheader_t;

typedef struct {
	int         filepos;
	int         disksize;
	int         filelen;				// uncompressed
	char        type;
	char        compression;
	char        pad1, pad2;
	char        name[16];				// must be null terminated
} dwadfile_t;


typedef struct fsw_group_s {
	fs_file_t	*wad;
} fsw_group_t;


static void
FSW_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSW_Free_File (fs_file_t *file)
{
	Zone_Free (file->name_base);
}

static SDL_RWops *
FSW_Open_File (fs_file_t *file, Uint32 flags)
{
	fsw_group_t	*wad;
	SDL_RWops	*rw;

	if (flags & FSF_WRITE)
		return NULL;

	wad = file->group->fs_data;

	rw = LimitFromRW(wad->wad->open(wad->wad, 0), file->fs_data.ofs, file->fs_data.ofs + file->len);
	return rw;
}

static qboolean
FSW_Add_Wad (fs_group_t *group, fsw_group_t *wad, fs_file_t *file)
{
	dwadfile_t		pfile;
	dwadheader_t	pheader;
	fs_file_data_t	file_data;
	SDL_RWops		*rw;
	int				i;

	if (!(rw = file->open(file, 0)))
		return false;

	wad->wad = file;

	SDL_RWread(rw, &pheader, sizeof(pheader), 1);
	if (strncmp("WAD2", pheader.id, 4)) {
		Com_Printf("WARNING: %s", file->name_base);
		if (file->ext)
			Com_Printf(".%s", file->name_base, file->ext);
		Com_Printf(" is NOT a wad file.  Skipping.\n");
		SDL_RWclose (rw);
		return false;
	}
	pheader.dirofs = LittleLong(pheader.dirofs);
	pheader.nfiles = LittleLong(pheader.nfiles);

	SDL_RWseek(rw, pheader.dirofs, SEEK_SET);
	for (i = 0; i < pheader.nfiles; i++) {
		SDL_RWread(rw, &pfile, sizeof(pfile), 1);
		pfile.filepos = LittleLong (pfile.filepos);
		pfile.filelen = LittleLong (pfile.filelen);
		file_data.ofs = pfile.filepos;
		FS_Add_File (group, va("gfx/%s.lmp", pfile.name), pfile.filelen, FSW_Open_File, file_data);
	}
	SDL_RWclose(rw);
	return true;
}

fs_group_t *
FSW_New_Group (fs_file_t *in, fs_group_t *parent, const char *id)
{
	fs_group_t	*group;
	fsw_group_t	*wad;

	group = FS_Alloc_Group (parent, id);

	wad = Zone_Alloc (fs_zone, sizeof (fsw_group_t));
	group->fs_data = wad;
	group->free = FSW_Free;
	group->free_file = FSW_Free_File;
	if (parent)
		group->flags |= parent->flags;

	if (FSW_Add_Wad (group, wad, in))
		return group;
	else {
		FS_Free_Group (group);
		return NULL;
	}
}
