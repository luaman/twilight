/*
	$RCSfile$

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

static const char rcsid[] =
	"$Id$";

#include "twiconfig.h"

#include "common.h"
#include "image.h"
#include "strlib.h"
#include "pcx.h"
#include "qlmp.h"
#include "tga.h"
#include "sys.h"
#include "sdlimage.h"
#include "fs.h"
#include "gl_textures.h"

memzone_t			*img_zone;
img_search_t		*img_search;
static char			**exts_real;
static char			***exts;

void
Image_Init (void)
{
	img_search_t	*search;
	int				i, count = 0;

	img_zone = Zone_AllocZone ("Image");

	search = Zone_Alloc (img_zone, sizeof(img_search_t));
	search->ext = Zstrdup(img_zone, "tga");
	search->load = TGA_Load;
	search->next = img_search;
	count++;
	img_search = search;

	search = Zone_Alloc (img_zone, sizeof(img_search_t));
	search->ext = Zstrdup(img_zone, "pcx");
	search->load = PCX_Load;
	search->next = img_search;
	count++;
	img_search = search;

	search = Zone_Alloc (img_zone, sizeof(img_search_t));
	search->ext = Zstrdup(img_zone, "lmp");
	search->load = QLMP_Load;
	search->next = img_search;
	count++;
	img_search = search;

	count += Image_InitSDL ();

	exts = Zone_Alloc (img_zone, sizeof (char *) * (count + 1));
	exts_real = Zone_Alloc (img_zone, sizeof (char *) * (count + 1));

	for (i = 0, search = img_search;
			i < count && search;
			i++, search = search->next) {
		exts_real[i] = search->ext;
	}
	exts_real[i] = NULL;

	for (i = 0; i < count; i++)
		exts[i] = exts_real;
	exts[i] = NULL;

}

image_t *
Image_Load (char *name, int flags)
{
	const char	*names[2] = {name, NULL};

	return Image_Load_Multi (names, flags);
}

image_t *
Image_Load_Multi (const char **names, int flags)
{
	image_t			*img;
	img_search_t	*search;
	fs_file_t		*file;
	SDL_RWops		*rw;

	file = FS_FindFiles_Complex (names, exts);

	if (!file)
		return NULL;

	rw = file->open(file, 0);
	if (!rw)
		return NULL;

	for (search = img_search; search; search = search->next)
		if (!strcasecmp(file->ext, search->ext))
			if ((img = search->load(file, rw))) {
				if (flags & TEX_UPLOAD)
					GLT_Load_image (file->name_base, img, flags);
				if (!(flags & TEX_KEEPRAW)) {
					Zone_Free (img->pixels);
					img->pixels = NULL;
				}
				img->file = file;
				return img;
			}

	return NULL;
}
