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

#include <stdio.h>

#include "qtypes.h"
#include "common.h"
#include "cvar.h"
#include "sound.h"
#include "strlib.h"

#define	PAINTBUFFER_SIZE	512
static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
static int         snd_scaletable[32][256];
static int        *snd_p, snd_linear_count, snd_vol;
static short      *snd_out;

static void        Snd_WriteLinearBlastStereo16 (void);

static void
Snd_WriteLinearBlastStereo16 (void)
{
	int         i;
	int         val;

	for (i = 0; i < snd_linear_count; i += 2) {
		val = (snd_p[i] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short) 0x8000)
			snd_out[i] = (short) 0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i + 1] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < (short) 0x8000)
			snd_out[i + 1] = (short) 0x8000;
		else
			snd_out[i + 1] = val;
	}
}

static void
S_TransferStereo16 (int endtime)
{
	int         lpos;
	int         lpaintedtime;
	void	   *pbuf;

	snd_vol = volume->fvalue * 256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;
	pbuf = (Uint32 *) shm->buffer;

	while (lpaintedtime < endtime) {
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples >> 1) - 1);

		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (shm->samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		Snd_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}
}

static void
S_TransferPaintBuffer (int endtime)
{
	int         out_idx;
	int         count;
	int         out_mask;
	int        *p;
	int         step;
	int         val;
	int         snd_vol;
	void	   *pbuf;

	if (shm->samplebits == 16 && shm->channels == 2) {
		S_TransferStereo16 (endtime);
		return;
	}

	p = (int *) paintbuffer;
	count = (endtime - paintedtime) * shm->channels;
	out_mask = shm->samples - 1;
	out_idx = paintedtime * shm->channels & out_mask;
	step = 3 - shm->channels;
	snd_vol = volume->fvalue * 256;
	pbuf = (Uint32 *) shm->buffer;

	if (shm->samplebits == 16) {
		short      *out = (short *) pbuf;

		while (count--) {
			val = (*p * snd_vol) >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short) 0x8000)
				val = (short) 0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	} else if (shm->samplebits == 8) {
		unsigned char *out = (unsigned char *) pbuf;

		while (count--) {
			val = (*p * snd_vol) >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short) 0x8000)
				val = (short) 0x8000;
			out[out_idx] = (val >> 8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static void SND_PaintChannelFrom8 (channel_t *ch, sfx_t *sfx, int endtime);
static void SND_PaintChannelFrom16 (channel_t *ch, sfx_t *sfx, int endtime);

void
S_PaintChannels (int endtime)
{
	int			 i;
	int			 end;
	channel_t	*ch;
	sfx_t		*sfx;
	int			 ltime, count;

	while (paintedtime < endtime) {
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		memset (paintbuffer, 0,
				  (end - paintedtime) * sizeof (portable_samplepair_t));

		// paint in the channels.
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++) {
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sfx = S_LoadSound (ch->sfx);
			if (!sfx || !sfx->loaded)
				continue;

			ltime = paintedtime;

			while (ltime < end) {		// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0) {
					if (sfx->width == 1)
						SND_PaintChannelFrom8 (ch, sfx, count);
					else
						SND_PaintChannelFrom16 (ch, sfx, count);

					ltime += count;
				}
				// if at end of loop, restart
				if (ltime >= ch->end) {
					if (sfx->loopstart >= 0) {
						ch->pos = sfx->loopstart;
						ch->end = ltime + sfx->length - ch->pos;
					} else {			// channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}

		}

		// transfer out according to DMA format
		S_TransferPaintBuffer (end);
		paintedtime = end;
	}
}

void
SND_InitScaletable (void)
{
	int         i, j;

	for (i = 0; i < 32; i++)
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((signed char) j) * i * 8;
}


static void
SND_PaintChannelFrom8 (channel_t *ch, sfx_t *sfx, int count)
{
	int			 data, i;
	int			*lscale, *rscale;
	Uint8		*sdata;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sdata = sfx->data + ch->pos;

	for (i = 0; i < count; i++) {
		data = sdata[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}

	ch->pos += count;
}


static void
SND_PaintChannelFrom16 (channel_t *ch, sfx_t *sfx, int count)
{
	int			 data;
	int			 left, right;
	int			 leftvol, rightvol;
	Sint16		*sdata;
	int			 i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sdata = (signed short *) sfx->data + ch->pos;

	for (i = 0; i < count; i++) {
		data = sdata[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[i].left += left;
		paintbuffer[i].right += right;
	}

	ch->pos += count;
}
