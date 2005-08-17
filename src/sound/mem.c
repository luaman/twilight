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

#include "twiconfig.h"

#include "qtypes.h"
#include "common.h"
#include "cvar.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"

static wavinfo_t GetWavinfo (char *name, Uint8 *wav, int wavlength);

int         cache_full_cycle;

Uint8      *S_Alloc (int size);


static void
ResampleSfx (sfx_t *sfx, int inrate, int inwidth, Uint8 *data)
{
	int         outcount;
	int         srcsample;
	float       stepscale;
	int         i;
	int         sample, samplefrac, fracstep;

	if (!sfx->loaded)
		return;

	stepscale = (float) inrate / shm->speed;	// this is usually 0.5, 1, or 2

	outcount = sfx->length / stepscale;
	sfx->length = outcount;
	if (sfx->loopstart != -1)
		sfx->loopstart = sfx->loopstart / stepscale;

	sfx->speed = shm->speed;
	if (loadas8bit->ivalue)
		sfx->width = 1;
	else
		sfx->width = inwidth;
	sfx->channels = 1;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sfx->width == 1) {
// fast special case
		for (i = 0; i < outcount; i++)
			((signed char *) sfx->data)[i]
				= (int) ((unsigned char) (data[i]) - 128);
	} else {
// general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		for (i = 0; i < outcount; i++) {
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort (((short *) data)[srcsample]);
			else
				sample = (int) ((unsigned char) (data[srcsample]) - 128) << 8;
			if (sfx->width == 2)
				((short *) sfx->data)[i] = sample;
			else
				((signed char *) sfx->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================


sfx_t *
S_LoadSound (sfx_t *s)
{
	char        namebuffer[256];
	Uint8      *data;
	wavinfo_t   info;
	int         len;
	float       stepscale;

// see if still in memory
	if (s->loaded)
		return s;

// load it in
	strlcpy_s (namebuffer, "sound/");
	strlcat_s (namebuffer, s->name);

	data = COM_LoadTempFile (namebuffer, true);

	if (!data) {
		Com_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, com_filesize);

	Com_DFPrintf (DEBUG_SOUND, "%s:\n", s->name);
	Com_DFPrintf (DEBUG_SOUND, "  rate: %d\n", info.rate);
	Com_DFPrintf (DEBUG_SOUND, "  width: %d\n", info.width);
	Com_DFPrintf (DEBUG_SOUND, "  channels: %d\n", info.channels);
	Com_DFPrintf (DEBUG_SOUND, "  loopstart: %d\n", info.loopstart);
	Com_DFPrintf (DEBUG_SOUND, "  samples: %d\n", info.samples);
	Com_DFPrintf (DEBUG_SOUND, "  dataofs: %d\n", info.dataofs);

	if (info.channels != 1) {
		if (info.channels > 1)
			Com_Printf ("%s is a stereo sample\n", s->name);
		else
			Com_DFPrintf (DEBUG_SOUND, "  %s is a bad sample\n", s->name);
		Zone_Free (data);
		return NULL;
	}

	stepscale = (float) info.rate / shm->speed;
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	if (!info.samples)
		len = 1;

	s->data = Zone_Alloc (snd_zone, len);
	if (!s->data) {
		Zone_Free (data);
		return NULL;
	}

	s->loaded = true;
	s->length = info.samples;
	s->loopstart = info.loopstart;
	s->speed = info.rate;
	s->width = info.width;
	s->channels = info.channels;

	ResampleSfx (s, s->speed, s->width, data + info.dataofs);

	Zone_Free (data);
	return s;
}



/*
===============================================================================

WAV loading

===============================================================================
*/


Uint8      *data_p;
Uint8      *iff_end;
Uint8      *last_chunk;
Uint8      *iff_data;
int         iff_chunk_len;
char		*iff_name;

static short
GetLittleShort (void)
{
	short       val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

static int
GetLittleLong (void)
{
	int         val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	val = val + (*(data_p + 2) << 16);
	val = val + (*(data_p + 3) << 24);
	data_p += 4;
	return val;
}

static void
FindNextChunk (char *name)
{
	while (1) {
		data_p = last_chunk;

		if (data_p >= iff_end) {		// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong ();
		if (iff_chunk_len < 0) {
			data_p = NULL;
			return;
		}
//      if (iff_chunk_len > 1024*1024)
//          Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		if (iff_end - data_p < 8 + iff_chunk_len) {
			Com_DPrintf("Corrupt sound file '%s'.\n", iff_name);
			data_p = NULL;
			return;
		}
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);
		if (!memcmp (data_p, name, 4))
			return;
	}
}

static void
FindChunk (char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}



static wavinfo_t
GetWavinfo (char *name, Uint8 *wav, int wavlength)
{
	wavinfo_t   info;
	int         i;
	int         format;
	int         samples;

	memset (&info, 0, sizeof (info));

	if (!wav || !wavlength)
		return info;

	iff_name = name;
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk ("RIFF");
	if (!(data_p && !memcmp (data_p + 8, "WAVE", 4))) {
		Com_Printf ("Missing RIFF/WAVE chunks\n");
		return info;
	}
// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk ("fmt ");
	if (!data_p) {
		Com_Printf ("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort ();
	if (format != 1) {
		Com_Printf ("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort ();
	info.rate = GetLittleLong ();
	data_p += 4 + 2;
	info.width = GetLittleShort () / 8;

// get cue chunk
	FindChunk ("cue ");
	if (data_p) {
		data_p += 32;
		info.loopstart = GetLittleLong ();

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p) {
			// this is not a proper parse, but it works with cooledit...
			if (!memcmp (data_p + 28, "mark", 4)) {
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	} else
		info.loopstart = -1;

// find data chunk
	FindChunk ("data");
	if (!data_p) {
		Com_Printf ("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples) {
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	} else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}
