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

typedef DECLSPEC SDL_Surface * SDLCALL (IMG_Load) (SDL_RWops *);

static IMG_Load		*sdl_IMG_LoadBMP_RW;
static IMG_Load		*sdl_IMG_LoadPNM_RW;
static IMG_Load		*sdl_IMG_LoadXPM_RW;
static IMG_Load		*sdl_IMG_LoadPCX_RW;
static IMG_Load		*sdl_IMG_LoadJPG_RW;
static IMG_Load		*sdl_IMG_LoadTIF_RW;
static IMG_Load		*sdl_IMG_LoadPNG_RW;
static IMG_Load		*sdl_IMG_LoadTGA_RW;
static IMG_Load		*sdl_IMG_LoadLBM_RW;
static void			*sdl_handle;

void
Image_InitSDL ()
{
	if (loaded)
		return;

	sdl_handle = SDL_LoadObject("libSDL_image.so");
	if (!sdl_handle) {
		Com_Printf("Error! %s\n", SDL_GetError ());
		return;
	}
	sdl_IMG_LoadBMP_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadBMP_RW");
	sdl_IMG_LoadPNM_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadPNM_RW");
	sdl_IMG_LoadXPM_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadXPM_RW");
	sdl_IMG_LoadPCX_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadPCX_RW");
	sdl_IMG_LoadJPG_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadJPG_RW");
	sdl_IMG_LoadTIF_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadTIF_RW");
	sdl_IMG_LoadPNG_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadPNG_RW");
	sdl_IMG_LoadTGA_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadTGA_RW");
	sdl_IMG_LoadLBM_RW = SDL_LoadFunction(sdl_handle, "IMG_LoadLBM_RW");

	if (!(sdl_IMG_LoadBMP_RW && sdl_IMG_LoadPNM_RW && sdl_IMG_LoadXPM_RW &&
				sdl_IMG_LoadPCX_RW && sdl_IMG_LoadJPG_RW &&
				sdl_IMG_LoadTIF_RW && sdl_IMG_LoadPNG_RW &&
				sdl_IMG_LoadTGA_RW && sdl_IMG_LoadLBM_RW)) {
		Com_Printf("Error! %s\n", SDL_GetError ());
		return;
	}

	loaded = true;
}

image_t *
Image_FromSDL (char *name)
{
	SDL_Surface	*surf, *rgb_surf;
	SDL_RWops	*file;
	image_t		*image = NULL;
	Uint8		*buf;

	if (!loaded)
		return NULL;

	if ((buf = COM_LoadTempFile (va("%s.png", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadPNG_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.tga", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadTGA_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.jpg", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadJPG_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.tif", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadTIF_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.bmp", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadBMP_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.pnm", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadPNM_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.xpm", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadXPM_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.pcx", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadPCX_RW (file)))
			goto convert;
		goto cleanup;
	}

	if ((buf = COM_LoadTempFile (va("%s.lbm", name), false))) {
		file = SDL_RWFromMem (buf, com_filesize);
		if ((surf = sdl_IMG_LoadLBM_RW (file)))
			goto convert;
		goto cleanup;
	}

	return NULL;

convert:
	rgb_surf = SDL_CreateRGBSurface (SDL_SWSURFACE, surf->w, surf->h, 32,
			rmask, gmask, bmask, amask);
	SDL_BlitSurface (surf, &surf->clip_rect, rgb_surf, &surf->clip_rect);
	image = malloc (sizeof(image_t));
	image->width = rgb_surf->w;
	image->height = rgb_surf->h;
	image->type = IMG_RGBA;
	image->pixels = malloc(image->width * image->height * 4);

	memcpy (image->pixels, rgb_surf->pixels, 4 * rgb_surf->w * rgb_surf->h);
	SDL_FreeSurface (rgb_surf);

cleanup:
	SDL_FreeSurface (surf);
	SDL_FreeRW (file);
	Zone_Free (buf);

	return image;
}
