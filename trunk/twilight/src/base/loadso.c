/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.
	Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

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
/* crc.c */
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include "loadso.h"
#include "qtypes.h"
#include "zone.h"
#include "common.h"

#include <stdio.h>

#if defined(HAVE_DLFCN_H)
# include <dlfcn.h>
#endif
#if defined(WIN32)
# include <windows.h>
#endif
#if defined(__BEOS__)
# include <be/kernel/image.h>
#endif
#if defined(macintosh)
# include <string.h>
# include <Strings.h>
# include <CodeFragments.h>
# include <Errors.h>
#endif

#include "SDL_types.h"
#include "SDL_error.h"

#ifdef HAVE_SDL_LOADOBJ
extern DECLSPEC void *SDL_LoadObject(const char *sofile);
extern DECLSPEC void *SDL_LoadFunction(void *handle, const char *name);
extern DECLSPEC void SDL_UnloadObject(void *handle);
#endif


typedef enum {
#ifdef HAVE_SDL_LOADOBJECT
	SO_SDL,
#endif
#ifdef HAVE_DLOPEN
	SO_DLOPEN,
#endif
#ifdef WIN32
	SO_WIN32,
#endif
#ifdef __BEOS__
	SO_BEOS,
#endif
#ifdef macintosh
	SO_MAC,
#endif
} so_type_e;

typedef struct so_handle_s {
	so_type_e	type;
	void		*handle;
} so_handle_t;

void *
TWI_LoadObject (const char *sofile)
{
	so_handle_t	*twi_handle;

	twi_handle = Zone_AllocName (sofile, sizeof (so_handle_t));

#ifdef HAVE_SDL_LOADOBJ
	if (!twi_handle->handle) {
		if ((twi_handle->handle = SDL_LoadObject (sofile)))
			twi_handle->type = SO_SDL;
		else
			Com_DPrintf ("TWI_LoadObject SDL: %s\n", SDL_GetError ());
	}
	
#endif
#ifdef HAVE_DLOPEN
	if (!twi_handle->handle) {
		if ((twi_handle->handle = dlopen (sofile, RTLD_NOW)))
			twi_handle->type = SO_DLOPEN;
		else
			Com_DPrintf ("TWI_LoadObject dlopen: %s\n", dlerror ());
	}
#endif
#ifdef WIN32
	if (!twi_handle->handle) {

		if ((twi_handle->handle = (void *)LoadLibrary (sofile)))
			twi_handle->type = SO_WIN32;
		else {
			char errbuf[512];

			FormatMessage((FORMAT_MESSAGE_IGNORE_INSERTS |
						FORMAT_MESSAGE_FROM_SYSTEM),
					NULL, GetLastError(),
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					errbuf, SDL_TABLESIZE(errbuf), NULL);
			Com_DPrintf ("TWI_LoadObject win32: %s\n", errbuf);
		}
	}
#endif
#ifdef __BEOS__
	if (!twi_handle->handle) {
		image_id library_id;

		library_id = load_add_on(sofile);
		if ( library_id == B_ERROR ) {
			Com_DPrintf ("TWI_LoadObject BEOS: BeOS error\n");
		} else {
			twi_handle->handle = (void *)(library_id);
			twi_handle->type = SO_BEOS;
		}
	}
#endif
#ifdef macintosh
	if (!twi_handle->handle) {
		CFragConnectionID library_id;
		Ptr mainAddr;
		Str255 errName;
		OSErr error;
		char psofile[512];

		strncpy(psofile, sofile, SDL_TABLESIZE(psofile));
		psofile[SDL_TABLESIZE(psofile)-1] = '\0';
		error = GetSharedLibrary(C2PStr(psofile), kCompiledCFragArch,
				kLoadCFrag, &library_id, &mainAddr, errName);
		switch (error) {
			case noErr:
				twi_handle->handle = (void *)(library_id);
				twi_handle->type = SO_MAC;
				break;
			case cfragNoLibraryErr:
				Com_DPrintf ("TWI_LoadObject MAC: Library not found\n");
				break;
			case cfragUnresolvedErr:
				Com_DPrintf ("TWI_LoadObject MAC: Unabled to resolve symbols\n");
				break;
			case cfragNoPrivateMemErr:
			case cfragNoClientMemErr:
				Com_DPrintf ("TWI_LoadObject MAC: Out of memory\n");
				break;
			default:
				Com_DPrintf ("TWI_LoadObject MAC: Unknown Code Fragment Manager error\n");
				break;
		}
	}
#endif

	if (twi_handle->handle)
		return twi_handle;
	else {
		Zone_Free (twi_handle);
		return NULL;
	}
}

void *
TWI_LoadFunction (void *handle, const char *name)
{
	so_handle_t	*twi_handle = handle;
	void		*symbol = NULL;
	const char	*loaderror = "TWI_LoadFunction: No cases.";
#ifdef WIN32
	char		errbuf[512];
#endif

	if (!handle || !name) {
		Com_Printf ("TWI_LoadFunction: Invalid name or handle.\n");
		return NULL;
	}

	switch (twi_handle->type) {
#ifdef HAVE_SDL_LOADOBJ
		case SO_SDL:
			symbol = SDL_LoadFunction (twi_handle->handle, name);
			loaderror = SDL_GetError ();
			break;
#endif
#ifdef HAVE_DLOPEN
		case SO_DLOPEN:
			if (!(symbol = dlsym (twi_handle->handle, name)))
				loaderror = dlerror ();
			break;
#endif
#ifdef WIN32
		case SO_WIN32:
			symbol = (void *)GetProcAddress((HMODULE)twi_handle->handle, name);
			if (!symbol) {
				FormatMessage((FORMAT_MESSAGE_IGNORE_INSERTS |
							FORMAT_MESSAGE_FROM_SYSTEM),
						NULL, GetLastError(),
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						errbuf, SDL_TABLESIZE(errbuf), NULL);
				loaderror = errbuf;
			}
			break;
#endif
#ifdef __BEOS__
		case SO_BEOS:
			if (get_image_symbol ((image_id) twi_handle->handle, name,
						B_SYMBOL_TYPE_TEXT, &symbol) != B_NO_ERROR) {
				loaderror = "Symbol not found";
			}
			break;
#endif
#ifdef macintosh
		case SO_MAC:
			{
				CFragSymbolClass class;
				CFragConnectionID library_id;
				char pname[512];

				library_id = (CFragConnectionID)twi_handle->handle;

				strncpy(pname, name, SDL_TABLESIZE(pname));
				pname[SDL_TABLESIZE(pname)-1] = '\0';
				if ( FindSymbol(library_id, C2PStr(pname),
							(char **)&symbol, &class) != noErr ) {
					loaderror = "Symbol not found";
				}
			}
			break;
#endif
	}

	if (!symbol)
		Com_DPrintf ("TWI_LoadFunction: %s\n", loaderror);

	return symbol;
}

void
TWI_UnloadObject (void *handle)
{
	so_handle_t	*twi_handle = handle;

	if (!handle) {
		Com_Printf ("TWI_LoadFunction: Invalid handle.\n");
		return;
	}

	switch (twi_handle->type) {
#ifdef HAVE_SDL_LOADOBJ
		case SO_SDL:
			SDL_UnloadObject (twi_handle->handle);
			break;
#endif
#ifdef HAVE_DLOPEN
		case SO_DLOPEN:
			dlclose (twi_handle->handle);
			break;
#endif
#ifdef WIN32
		case SO_WIN32:
			FreeLibrary ((HMODULE) twi_handle->handle);
			break;
#endif
#ifdef __BEOS__
		case SO_BEOS:
			unload_add_on((image_id) twi_handle->handle);
			break;
#endif
#ifdef macintosh
		case SO_MAC:
			CloseConnection((CFragConnectionID) twi_handle->handle);
			break;
#endif
	}
	Zone_Free (twi_handle);
}
