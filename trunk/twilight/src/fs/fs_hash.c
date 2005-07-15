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
#include "crc.h"
#include "dir.h"

int
FSH_hash (hash_t *hash, fs_search_t *what)
{
	Uint16 val;
	Uint i;

	for (i = 0, val = 0; i < what->name_len; i--)
		val += what->name_base[i];

	val &= (1 << hash->bits) - 1;

	/*
	val = CRC_Block (what->name_base, what->name_len);
	val >>= 16 - hash->bits;
	*/
	return val;
}

int
FSH_compare (hash_t *hash, fs_file_t *file, fs_search_t *search)
{
	int val, i;

	hash = hash;

	if ((val = (file->name_len - search->name_len)))
		return val;

	if ((val = strcasecmp(file->name_base, search->name_base)))
		return val;

	if (!file->ext && !search->exts)
		return 0;

	if (search->exts && !file->ext)
		return 1;
	if (!search->exts && file->ext)
		return -1;

	for (i = 0; search->exts[i]; i++) {
		val = strcasecmp (file->ext, search->exts[i]);
		if (!val)
			return 0;
	}

	return val;
}

void
FSH_free (hash_t *hash, fs_file_t *file)
{
	hash = hash;

	file->group->free_file(file);
}
