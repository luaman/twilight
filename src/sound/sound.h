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

	$Id$
*/
// sound.h -- client sound i/o functions

#ifndef __SOUND_H
#define __SOUND_H

#include "zone.h"

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

typedef struct {
	int         left;
	int         right;
} portable_samplepair_t;

typedef struct sfx_s {
	char		 name[MAX_QPATH];
	qboolean	 loaded;
	int			 length;
	int			 loopstart;
	int			 speed;
	int			 width;
	int			 channels;
	Uint8		*data;
} sfx_t;

typedef struct {
	int         channels;
	int         samples;				// mono samples in buffer
	int         samplepos;				// in mono samples
	int         samplebits;
	int         speed;
	unsigned char *buffer;
} dma_t;

typedef struct {
	sfx_t      *sfx;					// sfx number
	int         leftvol;				// 0-255 volume
	int         rightvol;				// 0-255 volume
	int         end;					// end time in global paintsamples
	int         pos;					// sample position in sfx
	int         looping;				// where to loop, -1 = no looping
	int         entnum;					// to allow overriding a specific sound
	int         entchannel;				// 
	vec3_t      origin;					// origin of sound effect
	vec_t       dist_mult;				// distance multiplier
										// (attenuation/clipK)
	int         master_vol;				// 0-255 master volume
} channel_t;

typedef struct {
	int         rate;
	int         width;
	int         channels;
	int         loopstart;
	int         samples;
	int         dataofs;				// chunk starts this many bytes from
										// file start
} wavinfo_t;

void        S_Init_Cvars (void);
void        S_Init (void);
void        S_Shutdown (void);
void        S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin,
						  float fvol, float attenuation);
void        S_StaticSound (sfx_t *sfx, vec3_t origin, float vol,
						   float attenuation);
void        S_StopSound (int entnum, int entchannel);
void        S_StopAllSounds (qboolean clear);
void        S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right,
					  vec3_t v_up);
void        S_ExtraUpdate (void);

sfx_t      *S_PrecacheSound (char *sample);
void        S_TouchSound (char *sample);
void        S_PaintChannels (int endtime);
void        S_InitPaintChannels (void);

// initializes cycling through a DMA buffer and returns information on it
qboolean    SNDDMA_Init (void);

// gets the current DMA position
int         SNDDMA_GetDMAPos (void);

// shutdown the DMA xfer.
void        SNDDMA_Shutdown (void);

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8


extern channel_t channels[MAX_CHANNELS];

// 0 to MAX_DYNAMIC_CHANNELS-1  = normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern int  total_channels;

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern memzone_t *snd_zone;
extern qboolean fakedma;
extern int  fakedma_updates;
extern int  paintedtime;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern vec_t sound_nominal_clip_dist;
extern dma_t *shm;

extern struct cvar_s *loadas8bit;
extern struct cvar_s *volume;

extern qboolean snd_initialized;

extern int  snd_blocked;

void		 S_LocalSound (char *s);
sfx_t		*S_LoadSound (sfx_t *s);

void        SND_InitScaletable (void);

extern int soundtime;

#endif // __SOUND_H
