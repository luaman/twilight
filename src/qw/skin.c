/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "image.h"
#include "pcx.h"
#include "strlib.h"
#include "sys.h"
#include "glquake.h"
#include "draw.h"
#include "gl_textures.h"
#include "mathlib.h"
#include <errno.h>

typedef struct {
	char		name[128];
	skin_t		*skin;
} cached_skin_t;

skin_t *Skin_Load (char *skin_name);

cvar_t *baseskin;
cvar_t *noskins;

extern model_t	*player_model;

skin_t	*base_skin;

memzone_t *skin_zone;

char allskins[128];

#define	MAX_CACHED_SKINS		128
cached_skin_t skins[MAX_CACHED_SKINS];
int numskins;

/*
==========
Skin_Load

Returns a pointer to the skin struct, or NULL to use the default
==========
*/
skin_t *
Skin_Load (char *skin_name)
{
	char			name[MAX_OSPATH];
	Uint8			*raw;
	Uint8			*out, *pix, *tmp, *in, *final;
	pcx_t			*pcx;
	int				i;
	int				x, y;
	int				dataByte;
	int				runLength;
	cached_skin_t	*cached;
	skin_t			*skin;

	if (cls.downloadtype == dl_skin)
		// use base until downloaded
		return NULL;

	if (noskins->ivalue == 1)
		// JACK: So NOSKINS > 1 will show skins, but not download new ones.
		return NULL;

	snprintf (name, sizeof (name), "skins/%s.pcx", skin_name);
	for (i = 0; i < numskins; i++) {
		if (!strcmp (name, skins[i].name)) {
			if (skins[i].skin)
				return skins[i].skin;
			else
				return base_skin;
		}
	}

	cached = &skins[numskins++];
	memset (cached, 0, sizeof (*cached));
	strncpy (cached->name, name, sizeof (cached->name) - 1);

	/*
	 * load the pic from disk
	 */
	raw = COM_LoadTempFile (name, true);
	if (!raw) {
		Com_Printf ("Couldn't load skin %s\n", name);
		return NULL;
	}
	
	/*
	 * parse the PCX file
	 */
	pcx = (pcx_t *) raw;
	raw = pcx->data;

	pcx->xmax = LittleShort (pcx->xmax);
	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);
	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8 || pcx->xmax < 296 || pcx->ymax < 194) {
		Com_Printf ("Bad skin %s\n", name);
		return base_skin;
	}

	tmp = Zone_Alloc (skin_zone, (pcx->xmax + 1) * (pcx->ymax + 1));
	if (!tmp)
		Sys_Error ("Skin_Load: couldn't allocate");

	pix = tmp;

	for (y = 0; y < pcx->ymax; y++, pix += pcx->xmax) {
		for (x = 0; x <= pcx->xmax;) {
			if (raw - (Uint8 *) pcx > com_filesize) {
				Zone_Free (tmp);
				Com_Printf ("Skin %s was malformed.  You should delete it.\n",
							name);
				return base_skin;
			}
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				if (raw - (Uint8 *) pcx > com_filesize) {
					Zone_Free (tmp);
					Com_Printf
						("Skin %s was malformed.  You should delete it.\n",
						 name);
					return base_skin;
				}
				dataByte = *raw++;
			} else
				runLength = 1;

			// skin sanity check
			if (runLength + x > pcx->xmax + 2) {
				Zone_Free (tmp);
				Com_Printf ("Skin %s was malformed.  You should delete it.\n",
							name);
				return base_skin;
			}
			while (runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if (raw - (Uint8 *) pcx > com_filesize) {
		Zone_Free (tmp);
		Com_Printf ("Skin %s was malformed.  You should delete it.\n", name);
		return base_skin;
	}

	final = Zone_Alloc (skin_zone, 296 * 194);
	out = final;
	in = tmp;

	for (y = 0; y < 194; y++, in += pcx->xmax, out += 296)
		for (x = 0; x < 296; x++)
			out[x] = in[x];

	skin = Zone_Alloc(skin_zone, sizeof(skin_t));

	GLT_Skin_Parse(final, skin, Mod_Extradata(player_model), name, 296, 194, 1, 1);
	Zone_Free (tmp);
	Zone_Free (final);

	cached->skin = skin;

	return skin;
}


/*
=================
Skin_NextDownload
=================
*/
void
Skin_NextDownload (void)
{
	player_info_t	   *sc;
	int					i;

	if (cls.downloadnumber == 0)
		Com_Printf ("Checking skins...\n");
	cls.downloadtype = dl_skin;

	for (; cls.downloadnumber != MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0] || !sc->skin_name[0])
			continue;
		if (noskins->ivalue)
			continue;
		if (!CL_CheckOrDownloadFile (va ("skins/%s.pcx", sc->skin_name)))
			return;						// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name[0] || !sc->skin_name[0])
			continue;
		sc->skin = Skin_Load (sc->skin_name);
	}

	if (cls.state != ca_active) {		// get next signon phase
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va ("begin %i", cl.servercount));
	}
}

/*
==========
Skin_Skins_f

Refind all skins, downloading if needed.
==========
*/
void
Skin_Skins_f (void)
{
#if 0
	int			i;

	for (i = 0; i < numskins; i++) {
		/* FIXME: Free the GL textures here! */
	}
	numskins = 0;
#endif

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload ();
}


/*
==========
Skin_AllSkins_f

Sets all skins to one specific one
==========
*/
void
Skin_AllSkins_f (void)
{
	strcpy (allskins, Cmd_Argv (1));
	Skin_Skins_f ();
}

/*
=================
CL_InitSkins
=================
*/
void
CL_InitSkins (void)
{
	skin_zone = Zone_AllocZone("skins");

	base_skin = Skin_Load(baseskin->svalue);

	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);
}
