/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
rights reserved. */

static const char rcsid[] =
	"$Id$";

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <SDL.h>

#include "quakedef.h"

static qboolean cdValid = false;
static qboolean playing = false;
static qboolean wasPlaying = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static float cdvolume;
static byte remap[100];
static byte playTrack;
static byte maxTrack;

SDL_CD     *cd_handle;
static int  cd_dev = 0;					/* Default to first CD-ROM drive */

static void
CDAudio_Eject (void)
{
	if (!cd_handle || !enabled)
		return;							// no cd init'd

	if (SDL_CDEject (cd_handle) < 0)
		Con_Printf ("Unable to eject CD-ROM: %s\n", SDL_GetError ());
}


static void
CDAudio_CloseDoor (void)
{
	if (!cd_handle || !enabled)
		return;							// no cd init'd

	/* 
	 * Will SDL attempt to do this for us if we close and reopen the
	 * device?  There is no SDL_CDCloseDoor.
	 */
}

static int
CDAudio_GetAudioDiskInfo (void)
{
	cdValid = false;

	if (!cd_handle)
		return -1;

	if (!CD_INDRIVE (SDL_CDStatus (cd_handle))) {
		return -1;
	}

	cdValid = true;
	maxTrack = cd_handle->numtracks;

	return 0;
}


void
CDAudio_Play (byte track, qboolean looping)
{
	if (!cd_handle || !enabled)
		return;

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack) {
		Con_Printf ("CDAudio_Play: Bad track number %d.\n", track);
		return;
	}

	if (cd_handle->track[track - 1].type == SDL_DATA_TRACK) {
		Con_Printf ("CDAudio_Play: track %d is not audio\n", track);
		return;
	}

	if (playing) {
		if (playTrack == track)
			return;
		CDAudio_Stop ();
	}
	if (SDL_CDPlay
		(cd_handle, cd_handle->track[track - 1].offset,
		 cd_handle->track[track - 1].length) < 0) {
		Con_Printf ("CDAudio_Play: Unable to play %d: %s\n", track,
					SDL_GetError ());
		return;
	}
	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void
CDAudio_Stop (void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDStop (cd_handle) < 0)
		Con_DPrintf ("CDAudio_Stop: Unable to stop CD-ROM (%s)\n",
					 SDL_GetError ());

	wasPlaying = false;
	playing = false;
}

void
CDAudio_Pause (void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDPause (cd_handle) < 0)
		Con_DPrintf ("Unable to pause CD-ROM: %s\n", SDL_GetError ());

	wasPlaying = playing;
	playing = false;
}


void
CDAudio_Resume (void)
{
	if (!cd_handle || !enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (SDL_CDResume (cd_handle) < 0)
		Con_Printf ("Unable to resume CD-ROM: %s\n", SDL_GetError ());
	playing = true;
}

static void
CD_f (void)
{
	char       *command;
	int         ret;
	int         n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (Q_strcasecmp (command, "on") == 0) {
		enabled = true;
		return;
	}

	if (Q_strcasecmp (command, "off") == 0) {
		if (playing)
			CDAudio_Stop ();
		enabled = false;
		return;
	}

	if (Q_strcasecmp (command, "reset") == 0) {
		enabled = true;
		if (playing)
			CDAudio_Stop ();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo ();
		return;
	}

	if (Q_strcasecmp (command, "remap") == 0) {
		ret = Cmd_Argc () - 2;
		if (ret <= 0) {
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf ("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi (Cmd_Argv (n + 1));
		return;
	}

	if (Q_strcasecmp (command, "close") == 0) {
		CDAudio_CloseDoor ();
		return;
	}

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid) {
			Con_Printf ("No CD in player.\n");
			return;
		}
	}

	if (Q_strcasecmp (command, "play") == 0) {
		CDAudio_Play ((byte) Q_atoi (Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp (command, "loop") == 0) {
		CDAudio_Play ((byte) Q_atoi (Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp (command, "stop") == 0) {
		CDAudio_Stop ();
		return;
	}

	if (Q_strcasecmp (command, "pause") == 0) {
		CDAudio_Pause ();
		return;
	}

	if (Q_strcasecmp (command, "resume") == 0) {
		CDAudio_Resume ();
		return;
	}

	if (Q_strcasecmp (command, "eject") == 0) {
		if (playing)
			CDAudio_Stop ();
		CDAudio_Eject ();
		cdValid = false;
		return;
	}

	if (Q_strcasecmp (command, "info") == 0) {
		int         current_min, current_sec, current_frame;
		int         length_min, length_sec, length_frame;

		Con_Printf ("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf ("Currently %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf ("Paused %s track %u\n",
						playLooping ? "looping" : "playing", playTrack);
		if (playing || wasPlaying) {
			SDL_CDStatus (cd_handle);
			FRAMES_TO_MSF (cd_handle->cur_frame, &current_min, &current_sec,
						   &current_frame);
			FRAMES_TO_MSF (cd_handle->track[playTrack - 1].length, &length_min,
						   &length_sec, &length_frame);

			Con_Printf ("Current position: %d:%02d.%02d (of %d:%02d.%02d)\n",
						current_min, current_sec, current_frame * 60 / CD_FPS,
						length_min, length_sec, length_frame * 60 / CD_FPS);
		}
		Con_Printf ("Volume is %f\n", cdvolume);

		return;
	}
}

void
CDAudio_Update (void)
{
	CDstatus    curstat;
	static time_t lastchk;

	if (!enabled)
		return;

	if (bgmvolume.value != cdvolume) {
		if (cdvolume) {
			Cvar_SetValue ("bgmvolume", 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		} else {
			Cvar_SetValue ("bgmvolume", 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}

	if (playing && lastchk < time (NULL)) {
		lastchk = time (NULL) + 2;		// two seconds between chks
		curstat = SDL_CDStatus (cd_handle);
		if (curstat != CD_PLAYING && curstat != CD_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play (playTrack, true);
		}
	}
}

int
CDAudio_Init (void)
{
	int         i, sdl_num_drives;

#if 0
	if (cls.state == ca_dedicated)
		return -1;
#endif

	if (COM_CheckParm ("-nocdaudio"))
		return -1;

	if (SDL_Init (SDL_INIT_CDROM) < 0) {
		Con_Printf ("Unable to initialize CD audio: %s\n", SDL_GetError ());
		return -1;
	}
	sdl_num_drives = SDL_CDNumDrives ();
	Con_Printf ("SDL detected %d CD-ROM drive%c\n", sdl_num_drives,
				sdl_num_drives == 1 ? ' ' : 's');

	if (!sdl_num_drives)
		return -1;

	if ((i = COM_CheckParm ("-cddev")) != 0 && i < com_argc - 1) {
		cd_dev = atoi (com_argv[i + 1]);
		if (cd_dev < 0 || cd_dev > sdl_num_drives)
			cd_dev = 0;
		Con_Printf ("Using CD-ROM device '%s'\n", SDL_CDName (cd_dev));
	}

	if (!(cd_handle = SDL_CDOpen (cd_dev))) {
		Con_Printf ("CDAudio_Init: Unable to open CD-ROM drive %d (%s)\n",
					cd_dev, SDL_GetError ());
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo ()) {
		Con_Printf ("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Con_Printf ("CD Audio Initialized\n");

	return 0;
}


void
CDAudio_Shutdown (void)
{
	if (!initialized)
		return;
	CDAudio_Stop ();
	SDL_CDClose (cd_handle);
	cd_handle = NULL;
}
