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
#include "bsp.h"
#include "sys.h"
#include "rw_ops.h"
#include "mod_brush_disk.h"
#include "renderer/mod_alias_disk.h"
#include "renderer/mod_sprite_disk.h"
	
typedef struct fsb_group_s {
	fs_file_t	*bsp;
} fsb_group_t;

typedef struct fsb_file_s {
	size_t	ofs;
	Uint32	width, height;
} fsb_file_t;

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
FSB_Free (fs_group_t *group)
{
	Zone_Free (group->fs_data);
}

static void
FSB_Free_File (fs_file_t *file)
{
	Zone_Free (file->name_base);
	Zone_Free (file->fs_data.data);
}

static int
FSB_Close_File (SDL_RWops *rw, void *data)
{
	data = data;
	Zone_Free (rw->hidden.mem.base);
	return SDL_RWclose (rw);
}

static SDL_RWops *
FSB_Open_LMP_File (fs_file_t *file, Uint32 flags)
{
	fsb_file_t	*b_file = file->fs_data.data;
	fsb_group_t	*bsp = file->group->fs_data;
	SDL_RWops	*rw;
	Uint32		*buf;

	if (flags & FSF_WRITE)
		return NULL;

	rw = bsp->bsp->open (bsp->bsp, 0);
	buf = Zone_Alloc (tempzone, file->len);
	buf[0] = LittleLong (b_file->width);
	buf[1] = LittleLong (b_file->height);
	SDL_RWseek (rw, b_file->ofs, SEEK_SET);
	SDL_RWread (rw, &buf[2], file->len - 8, 1);
	SDL_RWclose (rw);

	rw = SDL_RWFromMem (buf, file->len);
	return WrapRW (rw, NULL, FSB_Close_File);
}

static SDL_RWops *
FSB_Open_RAW_File (fs_file_t *file, Uint32 flags)
{
	fsb_file_t	*b_file = file->fs_data.data;
	fsb_group_t	*bsp = file->group->fs_data;
	SDL_RWops	*rw;

	if (flags & FSF_WRITE)
		return NULL;

	rw = bsp->bsp->open (bsp->bsp, 0);
	SDL_RWseek (rw, b_file->ofs, SEEK_SET);
	rw = LimitFromRW (rw, b_file->ofs, b_file->ofs + file->len);
	return rw;
}

static qboolean
FSB_Add_BSP (fs_group_t *group, fsb_group_t *bsp, fs_file_t *file)
{
	dheader_t		header;
	fsb_file_t		*fsb_file;
	SDL_RWops		*rw;
	Uint32			i, nfiles, size;
	Uint32			*offsets;
	dmiptex_t		miptex;
	char			*base_name;
	fs_file_data_t	file_data;

	rw = file->open(file, 0);

	if ((base_name = strrchr (file->name_base, '/')))
		base_name++;
	else
		base_name = file->name_base;

	bsp->bsp = file;

	SDL_RWread(rw, &header, sizeof(header), 1);
	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) &header)[i] = LittleLong (((int *) &header)[i]);
	if (header.version != BSPVERSION) {
		if ((LittleLong(header.version) == IDPOLYHEADER) ||
				LittleLong(header.version) == IDSPRITEHEADER) {
			Com_DPrintf ("Very funny Tomaz, very funny.\n");
		} else {
			Com_Printf("WARNING: %s", file->name_base);
			if (file->ext)
				Com_Printf(".%s", file->ext);
			Com_Printf(" is NOT a bsp file.  Skipping.\n");
		}
		SDL_RWclose (rw);
		return false;
	}

	SDL_RWseek (rw, header.lumps[LUMP_TEXTURES].fileofs, SEEK_SET);
	SDL_RWread (rw, &nfiles, sizeof (Uint32), 1);
	nfiles = LittleLong (nfiles);
	offsets = Zone_Alloc (tempzone, sizeof (Uint32) * nfiles);
	SDL_RWread (rw, offsets, sizeof (Uint32), nfiles);

	for (i = 0; i < nfiles; i++) {
		offsets[i] = LittleLong (offsets[i]);
		SDL_RWseek (rw, offsets[i] + header.lumps[LUMP_TEXTURES].fileofs, SEEK_SET);
		SDL_RWread (rw, &miptex, sizeof (miptex), 1);

		fsb_file = Zone_Alloc (fs_zone, sizeof(fsb_file_t));
		fsb_file->ofs = LittleLong (miptex.offsets[0]) + offsets[i] + header.lumps[LUMP_TEXTURES].fileofs;
		fsb_file->width = LittleLong (miptex.width);
		fsb_file->height = LittleLong (miptex.height);

		size = (fsb_file->width * fsb_file->height) + (sizeof (Uint32) * 2);
		file_data.data = fsb_file;
		FS_Add_File (group, va("%s/%s.lmp", base_name, miptex.name), size, FSB_Open_LMP_File, file_data);
	}
	Zone_Free (offsets);

	if (header.lumps[LUMP_ENTITIES].fileofs &&
			header.lumps[LUMP_ENTITIES].filelen) {
		fsb_file = Zone_Alloc (fs_zone, sizeof(fsb_file_t));
		fsb_file->ofs = header.lumps[LUMP_ENTITIES].fileofs;

		size = header.lumps[LUMP_ENTITIES].filelen;
		file_data.data = fsb_file;
		FS_Add_File (group, va("%s.ent", base_name), size, FSB_Open_RAW_File, file_data);
	}

	if (header.lumps[LUMP_VISIBILITY].fileofs &&
			header.lumps[LUMP_VISIBILITY].filelen) {
		fsb_file = Zone_Alloc (fs_zone, sizeof(fsb_file_t));
		fsb_file->ofs = header.lumps[LUMP_VISIBILITY].fileofs;

		size = header.lumps[LUMP_VISIBILITY].filelen;
		file_data.data = fsb_file;
		FS_Add_File (group, va("%s.vis", base_name), size, FSB_Open_RAW_File, file_data);
	}

	if (header.lumps[LUMP_LEAFS].fileofs &&
			header.lumps[LUMP_LEAFS].filelen) {
		fsb_file = Zone_Alloc (fs_zone, sizeof(fsb_file_t));
		fsb_file->ofs = header.lumps[LUMP_LEAFS].fileofs;

		size = header.lumps[LUMP_LEAFS].filelen;
		file_data.data = fsb_file;
		FS_Add_File (group, va("%s.leaf", base_name), size, FSB_Open_RAW_File, file_data);
	}
	SDL_RWclose(rw);
	return true;
}

fs_group_t *
FSB_New_Group (fs_file_t *in, fs_group_t *parent, const char *id)
{
	fs_group_t	*group;
	fsb_group_t	*bsp;

	group = FS_Alloc_Group (parent, id);

	bsp = Zone_Alloc (fs_zone, sizeof (fsb_group_t));
	group->fs_data = bsp;
	group->free = FSB_Free;
	group->free_file = FSB_Free_File;
	if (parent)
		group->flags |= parent->flags;
	group->flags |= FS_NO_UPLOAD;

	if (FSB_Add_BSP (group, bsp, in))
		return group;
	else {
		FS_Free_Group (group);
		return NULL;
	}
}
