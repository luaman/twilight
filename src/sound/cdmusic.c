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

#include <stdlib.h>

#include "SDL.h"
#include "qtypes.h"
#include "cdaudio.h"
#include "common.h"
#include "strlib.h"
#include "cvar.h"

typedef void	(*xcommand_t) (void);
void			Cmd_AddCommand (char *cmd_name, xcommand_t function);
int				Cmd_Argc (void);
char			*Cmd_Argv (int arg);

extern double	curtime;


static qboolean	cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	enabled = true;
static qboolean	playLooping = false;
static float	cdvolume;
static Uint8	remap[100];
static Uint8	playTrack;
static double	endOfTrack = -1.0, pausetime = -1.0;

SDL_CD			*cd_handle;
static int		cd_dev = 0;			/* Default to first CD-ROM drive */

static void
CDAudio_Eject (void)
{
	if (!cd_handle || !enabled)
		return;

	if (SDL_CDEject (cd_handle) < 0)
		Com_Printf ("Unable to eject CD-ROM: %s\n", SDL_GetError ());
}

static void
CDAudio_CloseDoor (void)
{
	if (!cd_handle || !enabled)
		return;
	/* This is currently a NOP as SDL doesn't allow us to do this. */
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

	return 0;
}

static void
CDAudio_Stop (void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDStop (cd_handle) < 0)
		Com_DPrintf ("CDAudio_Stop: Unable to stop CD-ROM (%s)\n",
					 SDL_GetError ());

	wasPlaying = false;
	playing = false;
	pausetime = -1.0;
	endOfTrack = -1.0;
}

void
CDAudio_Play (Uint8 track, qboolean looping)
{
	int	len_m, len_s, len_f;
	
	if (!cd_handle || !enabled)
		return;

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > cd_handle->numtracks) {
		Com_Printf ("CDAudio_Play: Bad track number %d.\n", track);
		return;
	}

	if (cd_handle->track[track - 1].type == SDL_DATA_TRACK) {
		Com_Printf ("CDAudio_Play: track %d is not audio\n", track);
		return;
	}

	if (playing) {
		if (playTrack == track)
			return;
		CDAudio_Stop ();
	}

	if (SDL_CDPlay (cd_handle, cd_handle->track[track - 1].offset, cd_handle->track[track - 1].length) < 0) {

		/* ok, check for status now */
		int cd_status = SDL_CDStatus (cd_handle);

		if (cd_status > 0)
			Com_Printf ("CDAudio_Play: Unable to play %d: %s\n", track, SDL_GetError ());	
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;
	FRAMES_TO_MSF(cd_handle->track[track - 1].length, &len_m, &len_s, &len_f);
	endOfTrack = curtime + ((double)len_m * 60.0) + (double)len_s + (double)len_f / (double)CD_FPS;

	/*
	 * Add the pregap for the next track.  This means that disc-at-once CDs
	 * won't loop smoothly, but they wouldn't anyway so it doesn't really
	 * matter.  SDL doesn't give us pregap information anyway, so you'll
	 * just have to live with it.
	 */

	endOfTrack += 2.0;
	pausetime = -1.0;
	
	if (cdvolume == 0.0)
		CDAudio_Pause ();
}

void
CDAudio_Pause (void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDPause (cd_handle) < 0)
		Com_DPrintf ("Unable to pause CD-ROM: %s\n", SDL_GetError ());

	wasPlaying = playing;
	playing = false;
	pausetime = curtime;
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
		Com_Printf ("Unable to resume CD-ROM: %s\n", SDL_GetError ());
	playing = true;
	endOfTrack += curtime - pausetime;
	pausetime = -1.0;
}

static void
CD_f (void)
{
	char	*command;
	int		ret, n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (strcasecmp (command, "on") == 0) {
		enabled = true;
		return;
	}

	if (strcasecmp (command, "off") == 0) {
		if (playing)
			CDAudio_Stop ();
		enabled = false;
		return;
	}

	if (strcasecmp (command, "reset") == 0) {
		enabled = true;
		if (playing)
			CDAudio_Stop ();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo ();
		return;
	}

	if (strcasecmp (command, "remap") == 0) {
		ret = Cmd_Argc () - 2;
		if (ret <= 0) {
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Com_Printf ("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi (Cmd_Argv (n + 1));
		return;
	}

	if (strcasecmp (command, "close") == 0) {
		CDAudio_CloseDoor ();
		return;
	}

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo ();
		if (!cdValid) {
			Com_Printf ("No CD in player.\n");
			return;
		}
	}

	if (strcasecmp (command, "play") == 0) {
		CDAudio_Play ((Uint8) Q_atoi (Cmd_Argv (2)), false);
		return;
	}

	if (strcasecmp (command, "loop") == 0) {
		CDAudio_Play ((Uint8) Q_atoi (Cmd_Argv (2)), true);
		return;
	}

	if (strcasecmp (command, "stop") == 0) {
		CDAudio_Stop ();
		return;
	}

	if (strcasecmp (command, "pause") == 0) {
		CDAudio_Pause ();
		return;
	}

	if (strcasecmp (command, "resume") == 0) {
		CDAudio_Resume ();
		return;
	}

	if (strcasecmp (command, "eject") == 0) {
		if (playing)
			CDAudio_Stop ();
		CDAudio_Eject ();
		cdValid = false;
		return;
	}

	if (strcasecmp (command, "info") == 0) {
		int	current_min, current_sec, current_frame;
		int	length_min, length_sec, length_frame;

		Com_Printf ("%u tracks\n", cd_handle->numtracks);

		if (playing)
			Com_Printf ("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Com_Printf ("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);

		if (playing || wasPlaying) {
			SDL_CDStatus (cd_handle);
			FRAMES_TO_MSF (cd_handle->cur_frame, &current_min, &current_sec, &current_frame);
			FRAMES_TO_MSF (cd_handle->track[playTrack - 1].length, &length_min, &length_sec, &length_frame);

			Com_Printf ("Current position: %d:%02d.%02d (of %d:%02d.%02d)\n",
						current_min, current_sec, current_frame * 60 / CD_FPS,
						length_min, length_sec, length_frame * 60 / CD_FPS);
		}
		Com_Printf ("Volume is %f\n", cdvolume);

		return;
	}
}

void
CDAudio_Update (void)
{
	CDstatus	curstat;

	if (!enabled)
		return;

	if (playing && curtime > endOfTrack) {
		curstat = cd_handle->status;
		if (curstat != CD_PLAYING && curstat != CD_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play (playTrack, true);
			else
				endOfTrack = -1.0;
		}
	}
}

void
CDAudio_Init_Cvars(void)
{
}

int
CDAudio_Init (void)
{
	Uint i;
	int	x, sdl_num_drives;

	if (COM_CheckParm ("-nocdaudio"))
		return -1;

	if (SDL_InitSubSystem (SDL_INIT_CDROM) < 0) {
		Com_Printf ("Unable to initialize CD audio: %s\n", SDL_GetError ());
		return -1;
	}
	sdl_num_drives = SDL_CDNumDrives ();
	Com_Printf ("SDL detected %d CD-ROM drive%c\n", sdl_num_drives,
				sdl_num_drives == 1 ? ' ' : 's');

	if (!sdl_num_drives)
		return -1;

	if ((i = COM_CheckParm ("-cddev")) != 0 && i < com_argc - 1) {
		cd_dev = atoi (com_argv[i + 1]);
		for (x = 0; x < sdl_num_drives; x++) {
			if (!strcasecmp (SDL_CDName (x), com_argv[i + 1])) {
				cd_dev = x;
				break;
			}
		}
		if (cd_dev < 0 || cd_dev > sdl_num_drives)
			cd_dev = 0;
		Com_Printf ("Using CD-ROM device '%s'\n", SDL_CDName (cd_dev));
	}

	if (!(cd_handle = SDL_CDOpen (cd_dev))) {
		Com_Printf ("CDAudio_Init: Unable to open CD-ROM drive %d (%s)\n",
					cd_dev, SDL_GetError ());
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;

	enabled = true;

	if (CDAudio_GetAudioDiskInfo ()) {
		Com_Printf ("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Com_Printf ("CD Audio Initialized\n");

	return 0;
}

void
CDAudio_Shutdown (void)
{
	if (!cd_handle)
		return;

	CDAudio_Stop ();
	SDL_CDClose (cd_handle);
	cd_handle = NULL;
}

