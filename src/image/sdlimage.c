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

#include <stdlib.h>

#include "SDL.h"

#include "quakedef.h"
#include "common.h"
#include "image.h"
#include "sdlimage.h"
#include "strlib.h"
#include "sys.h"
#include "fs.h"

/* SDL interprets each pixel as a 32-bit number, so our masks must depend
*        on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define rmask		0xff000000
#define gmask		0x00ff0000
#define bmask		0x0000ff00
#define amask		0x000000ff
#else
#define rmask		0x000000ff
#define gmask		0x0000ff00
#define bmask		0x00ff0000
#define amask		0xff000000
#endif

extern DECLSPEC void *SDL_LoadObject(const char *sofile);
extern DECLSPEC void *SDL_LoadFunction(void *handle, const char *name);
extern DECLSPEC void SDL_UnloadObject(void *handle);


/*
 * Much evil sleeps here.
 */
static qboolean		loaded;

typedef DECLSPEC SDL_Surface * SDLCALL (IMG_SDL_Load) (SDL_RWops *);

static void			*sdl_handle;

typedef struct img_sdl_search_s {
	char			*ext;
	char			*load_name;
	IMG_SDL_Load	*load;
} img_sdl_search_t;

static img_sdl_search_t	search[] = {
	{"bmp", "IMG_LoadBMP_RW", NULL},
	{"jpg", "IMG_LoadJPG_RW", NULL},
	{"lbm", "IMG_LoadLBM_RW", NULL},
	{"pcx", "IMG_LoadPCX_RW", NULL},
	{"png", "IMG_LoadPNG_RW", NULL},
	{"pnm", "IMG_LoadPNM_RW", NULL},
	{"tga", "IMG_LoadTGA_RW", NULL},
	{"tif", "IMG_LoadTIF_RW", NULL},
	{"xpm", "IMG_LoadXPM_RW", NULL},
	{NULL, NULL, NULL},
};

int
Image_InitSDL ()
{
	img_search_t	*i_search;
	int i, cnt = 0;

	if (loaded)
		return 0;

	sdl_handle = SDL_LoadObject("libSDL_image.so");
	if (!sdl_handle) {
		Com_Printf("Error! %s\n", SDL_GetError ());
		return 0;
	}

	for (i = 0; search[i].load_name; i++) {
		search[i].load = SDL_LoadFunction(sdl_handle, search[i].load_name);
		if (search[i].load) {
			i_search = Zone_Alloc (img_zone, sizeof (img_search_t));
			i_search->ext = Zstrdup (img_zone, search[i].ext);
			i_search->load = Image_FromSDL;
			i_search->next = img_search;
			img_search = i_search;
			cnt++;
		} else
			Com_Printf("Unable to find %s (%s).\n",
					search[i].load_name, SDL_GetError ());
	}

	loaded = true;

	return cnt;
}

image_t *
Image_FromSDL (fs_file_t *file, SDL_RWops *rw)
{
	SDL_Surface	*surf, *rgb_surf;
	int			i;
	image_t		*image = NULL;

	if (!loaded)
		return NULL;

	for (i = 0; search[i].ext; i++) {
		if (!strcasecmp(file->ext, search[i].ext)) {
			if ((surf = search[i].load(rw))) {
				rgb_surf = SDL_CreateRGBSurface (SDL_SWSURFACE, surf->w,
						surf->h, 32, rmask, gmask, bmask, amask);
				SDL_BlitSurface (surf, &surf->clip_rect, rgb_surf,
						&surf->clip_rect);
				image = Zone_Alloc (img_zone, sizeof(image_t));
				image->width = rgb_surf->w;
				image->height = rgb_surf->h;
				image->type = IMG_RGBA;
				image->pixels = Zone_Alloc(img_zone, image->width * image->height * 4);

				memcpy (image->pixels, rgb_surf->pixels,
						4 * rgb_surf->w * rgb_surf->h);
				SDL_FreeSurface (rgb_surf);
				SDL_FreeSurface (surf);
				return image;
			} else
				Sys_Error ("Bad file!\n");
			break;
		}
	}
	SDL_RWclose (rw);

	return NULL;
}
