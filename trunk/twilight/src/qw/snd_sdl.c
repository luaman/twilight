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

#include <SDL_audio.h>
#include <SDL_byteorder.h>

#include "strlib.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "sound.h"
#include "sys.h"
#include "zone.h"

cvar_t *snd_bits;
cvar_t *snd_rate;
cvar_t *snd_channels;
cvar_t *snd_samples;

static dma_t the_shm;
static qboolean snd_inited;

static void
paint_audio (void *unused, Uint8 *stream, int len)
{
	int streamsamples;
	int sampleposbytes;
	int samplesbytes;

	if (shm)
	{
		streamsamples = len / (shm->samplebits >> 3);
		sampleposbytes = shm->samplepos * (shm->samplebits >> 3);
		samplesbytes = shm->samples * (shm->samplebits >> 3);

		shm->samplepos += streamsamples;
		while (shm->samplepos >= shm->samples)
			shm->samplepos -= shm->samples;

		if (shm->samplepos + streamsamples <= shm->samples)
			memcpy (stream, shm->buffer + sampleposbytes, len);
		else {
			memcpy (stream, shm->buffer + sampleposbytes,
					samplesbytes - sampleposbytes);
			memcpy (stream + samplesbytes - sampleposbytes, shm->buffer,
					len - (samplesbytes - sampleposbytes));
		}
		soundtime += streamsamples;
	}
}

qboolean
SNDDMA_Init (void)
{
	int				i;
	SDL_AudioSpec	desired;
	SDL_AudioSpec	obtained;
	qboolean		supported;

	snd_inited = false;

	snd_bits = Cvar_Get ("snd_bits", "16", CVAR_ROM, NULL);
	snd_rate = Cvar_Get ("snd_rate", "11025", CVAR_ROM, NULL);
	snd_channels = Cvar_Get ("snd_channels", "2", CVAR_ROM, NULL);
	snd_samples = Cvar_Get ("snd_samples", "512", CVAR_ROM, NULL);

	// Backward compatibility with Quake
	i = COM_CheckParm ("-sndbits");
	if (i && i < com_argc - 1)
		Cvar_Set (snd_bits, com_argv[i + 1]);
	i = COM_CheckParm ("-sndspeed");
	if (i && i < com_argc - 1)
		Cvar_Set (snd_rate, com_argv[i + 1]);
	if (COM_CheckParm ("-sndmono"))
		Cvar_Set (snd_channels, "1");
	if (COM_CheckParm ("-sndstereo"))
		Cvar_Set (snd_channels, "2");

	if (snd_rate->value == 0)
		return false;

	// Here's what we want
	memset (&desired, 0, sizeof(desired));
	desired.freq = snd_rate->value;
	switch ((int)snd_bits->value)
	{
		case 16:
			desired.format = AUDIO_S16SYS;
			break;
		case 8:
			desired.format = AUDIO_U8;
			break;
		default:
			Con_Printf ("Unknown number of audio bits: %i\n",
					(int)snd_bits->value);
			return false;
	}
	desired.channels = snd_channels->value;
	desired.samples = snd_samples->value;
	desired.callback = paint_audio;
	desired.userdata = NULL;

	// See what we got
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
		return 0;
	}

	// Be sure it works
	supported = false;
	switch (obtained.format) {
		case AUDIO_U8:
			supported = true;
			break;
		case AUDIO_S16LSB:
			if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
				supported = true;
			break;
		case AUDIO_S16MSB:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				supported = true;
			break;
		default:
			break;
	}

	if (!supported)
	{
		SDL_CloseAudio ();
		// SDL will convert for us - I wouldn't count on it being fast.
		if (SDL_OpenAudio (&desired, NULL) < 0) {
			Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
			return false;
		}
		memcpy (&obtained, &desired, sizeof (desired));
	}

//	SDL_LockAudio();
	SDL_PauseAudio (0);

	/* Fill the audio DMA information block */
	shm = &the_shm;
	shm->splitbuffer = 0;
	shm->samplebits = (obtained.format & 0xFF);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = obtained.samples << 4;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = Z_Malloc (shm->samples * (shm->samplebits >> 3));
	if (!shm->buffer)
	{
		Sys_Error ("Failed to allocate buffer for sound!\n");
	}

	snd_inited = true;
	return true;
}

int
SNDDMA_GetDMAPos (void)
{
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_PauseAudio (1);
		SDL_UnlockAudio ();
		SDL_CloseAudio ();
		snd_inited = false;
		shm = NULL;
	}
}

