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


#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cl_console.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "image.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "sbar.h"
#include "screen.h"
#include "sound.h"
#include "strlib.h"
#include "sys.h"
#include "tga.h"
#include "wad.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Com_Printf ();

net
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/

float		scr_con_current;
float		scr_conlines;				/* lines of console to display */

float		oldscreensize, oldfov;
cvar_t	   *scr_viewsize;
cvar_t	   *scr_fov;
cvar_t	   *scr_conspeed;
cvar_t	   *scr_centertime;
cvar_t	   *scr_showram;
cvar_t	   *scr_showturtle;
cvar_t	   *scr_showpause;
cvar_t	   *scr_printspeed;
cvar_t	   *scr_logcprint;
cvar_t	   *r_brightness;
cvar_t	   *r_contrast;
cvar_t	   *cl_avidemo;

extern cvar_t *crosshair;

qboolean	scr_initialized;			/* ready to draw */

qpic_t	   *scr_ram;
qpic_t	   *scr_net;
qpic_t	   *scr_turtle;

int			clearconsole;
int			clearnotify;

viddef_t	vid;						/* global video state */

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

void		SCR_ScreenShot_f (void);

Uint8	   *avibuffer;
Uint32		aviframeno;

void
GL_BrightenScreen(void)
{
	float		f;

	if (r_brightness->fvalue < 0.1f)
		Cvar_Set (r_brightness, va("%f",0.1f));
	if (r_brightness->fvalue > 5.0f)
		Cvar_Set (r_brightness, va("%f",5.0f));

	if (r_contrast->fvalue < 0.2f)
		Cvar_Set (r_contrast, va("%f",0.2f));
	if (r_contrast->fvalue > 1.0f)
		Cvar_Set (r_contrast, va("%f",1.0f));

	if (r_brightness->fvalue < 1.01f && r_contrast->fvalue > 0.99f)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	f = r_brightness->fvalue;
	if (f >= 1.01f)
	{
		qglBlendFunc (GL_DST_COLOR, GL_ONE);
		qglBegin (GL_TRIANGLES);
		while (f >= 1.01f)
		{
			if (f >= 2)
				qglColor4fv (whitev);
			else
				qglColor4f (f - 1.0f, f - 1.0f, f - 1.0f, 1.0f);
			qglVertex2f (-5000, -5000);
			qglVertex2f (10000, -5000);
			qglVertex2f (-5000, 10000);
			f *= 0.5;
		}
		qglEnd ();
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	if (r_contrast->fvalue <= 0.99f)
	{
		qglColor4f (1.0f, 1.0f, 1.0f, 1.0f - r_contrast->fvalue);
		qglBegin (GL_TRIANGLES);
		qglVertex2f (-5000, -5000);
		qglVertex2f (10000, -5000);
		qglVertex2f (-5000, 10000);
		qglEnd ();
	}

	qglColor4fv (whitev);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;		/* for slow victory printing */
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void
SCR_CenterPrint (char *str)
{
	char	   *s;
	char		line[64];
	int			i, j, l;

	strlcpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->fvalue;
	scr_centertime_start = cl.time;

	/* count the number of lines for centering */
	scr_center_lines = 1;
	s = str;
	while (*s) {
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	if (!scr_logcprint->ivalue)
		return;

	// echo it to the console
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = str;
	do
	{
		// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (s[l] == '\n' || !s[l])
				break;
		for (i=0 ; i<(40-l)/2 ; i++)
			line[i] = ' ';

		for (j=0 ; j<l ; j++)
			line[i++] = s[j];

		line[i] = '\n';
		line[i+1] = 0;
		Com_Printf ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;        // skip the \n
	} while (1);
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
}


void
SCR_DrawCenterString (void)
{
	char	   *start;
	int			x, y, l;
	int			remaining;

	/* the finale prints the characters one at a time */
	if (cl.intermission)
		remaining = scr_printspeed->ivalue * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height_2d * 0.35;
	else
		y = 48;

	while (1) {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width_2d - l * (con->tsize)) / 2;
		Draw_String_Len (x, y, start, min(l, remaining), con->tsize);
		remaining -= l;
		if (remaining <= 0)
			return;

		y += con->tsize;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						/* skip the \n */
	}
}

void
SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

/* ========================================================================= */

/*
====================
CalcFov
====================
*/
static float
CalcFov (float fov_x, float width, float height)
{
	return Q_atan (height / (width / Q_tan (fov_x/360*M_PI))) * 360 / M_PI;
}

/*
=================
SCR_CalcRefdef

=================
*/
static void
SCR_CalcRefdef (void)
{
	int			contents;

	/* intermission is always full screen */
	if (scr_viewsize->ivalue >= 120 || cl.intermission)
		sb_lines = 0;
	else if (scr_viewsize->ivalue >= 110)
		sb_lines = 24;
	else
		sb_lines = 24 + 16 + 8;

	r_refdef.fov_x = bound (10, scr_fov->fvalue * cl.viewzoom, 170);
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, vid.width, vid.height);

	if (cl.worldmodel)
	{
		contents = Mod_PointInLeaf (r_refdef.vieworg, cl.worldmodel)->contents;
		if (contents != CONTENTS_EMPTY && contents != CONTENTS_SOLID)
		{
			r_refdef.fov_x *= (Q_sin(cl.time * 4.7) * 0.015 + 0.985);
			r_refdef.fov_y *= (Q_sin(cl.time * 3.0) * 0.015 + 0.985);
		}
	}
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void
SCR_SizeUp_f (void)
{
	Cvar_Slide (scr_viewsize, 10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void
SCR_SizeDown_f (void)
{
	Cvar_Slide (scr_viewsize, -10);
}

/* ========================================================================= */
static void
SCR_viewsize_CB (cvar_t *cvar)
{
	/* bound viewsize */
	if (cvar->ivalue < 30)
		Cvar_Set (cvar, "30");
	else if (cvar->ivalue > 120)
		Cvar_Set (cvar, "120");
}

static void
SCR_fov_CB (cvar_t *cvar)
{
	/* bound field of view */
	if (cvar->fvalue < 1)
		Cvar_Set (cvar, "1");
	else if (cvar->fvalue > 170)
		Cvar_Set (cvar, "170");
}

void
SCR_Init_Cvars (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE, SCR_viewsize_CB);
	scr_fov = Cvar_Get ("fov", "90", CVAR_NONE, SCR_fov_CB);	/* 10-170 */
	scr_conspeed = Cvar_Get ("scr_conspeed", "300", CVAR_NONE, NULL);
	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, NULL);
	scr_showram = Cvar_Get ("showram", "1", CVAR_NONE, NULL);
	scr_showturtle = Cvar_Get ("showturtle", "0", CVAR_NONE, NULL);
	scr_showpause = Cvar_Get ("showpause", "1", CVAR_NONE, NULL);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, NULL);
	scr_logcprint = Cvar_Get ("scr_logcprint", "0", CVAR_ARCHIVE, NULL);
	r_brightness = Cvar_Get ("r_brightness", "1", CVAR_ARCHIVE, NULL);
	r_contrast = Cvar_Get ("r_contrast", "1", CVAR_ARCHIVE, NULL);
	cl_avidemo = Cvar_Get ("cl_avidemo", "0", CVAR_NONE, &AvidemoChanged);
}


/*
==================
SCR_Init
==================
*/
void
SCR_Init (void)
{

	/*
	 * register our commands
	 */
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}



/*
==============
SCR_DrawTurtle
==============
*/
void
SCR_DrawTurtle (void)
{
	static int		count;

	if (!scr_showturtle->ivalue)
		return;

	if (host_frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (0, 0, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void
SCR_DrawNet (void)
{
	if (host_realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (64, 0, scr_net);
}

void
SCR_DrawFPS (void)
{
	extern cvar_t	   *show_fps;
	static double		lastframetime;
	double				t;
	extern int			fps_count;
	static int			lastfps;
	int					x, y, st_len;
	char				st[80];

	if (!show_fps->ivalue)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}

	snprintf (st, sizeof (st), "%3d FPS", lastfps);
	st_len = strlen (st);

	x = vid.width_2d - st_len * con->tsize - con->tsize;
	y = vid.height_2d - sb_lines - con->tsize;
	Draw_String_Len (x, y, va("%3d FPS", lastfps), st_len, con->tsize);
}


/*
==============
DrawPause
==============
*/
void
SCR_DrawPause (void)
{
	qpic_t	   *pic;

	if (!scr_showpause->ivalue)			/* turn off for screenshots */
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ((vid.width_2d - pic->width) / 2,
			  (vid.height_2d - 48 - pic->height) / 2, pic);
}


/*
==============
SCR_DrawLoading
==============
*/
void
SCR_DrawLoading (void)
{
	qpic_t	   *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ((vid.width_2d - pic->width) / 2,
			  (vid.height_2d - 48 - pic->height) / 2, pic);
}



/* ========================================================================= */


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void
SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (scr_drawloading)
		/* never a console with loading plaque */
		return;

	/* decide on the height of the console */
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;
	if (con_forcedup) {
		scr_conlines = vid.height_2d;		/* full screen */
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.height_2d / 2;	/* half screen */
	else
		scr_conlines = 0;					/* none visible */

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->fvalue * host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->fvalue * host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}

/*
==================
SCR_DrawConsole
==================
*/
void
SCR_DrawConsole (void)
{
	if (scr_con_current) {
		Con_DrawConsole (scr_con_current);
		clearconsole = 0;
	} else {
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();			/* only draw notify in game */
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_ScreenShot_f
==================
*/
void
SCR_ScreenShot_f (void)
{
	Uint8		*buffer;
	char		name[MAX_OSPATH];
	static int	i = 0;

	/*
	 * find a file name to save it to
	 */
	for (; i < 10000; i++)
	{
		snprintf (name, sizeof (name), "%s/tw%04i.tga", com_gamedir, i);
		if (Sys_FileTime (name) == -1)
			break;	/* Doesn't exist */
	}
	if (i == 10000)
	{
		Com_Printf ("SCR_ScreenShot_f: Unable to create file\n");
		return;
	}

	buffer = Zone_Alloc (tempzone, vid.width * vid.height * 3);

	qglReadPixels (0, 0, vid.width, vid.height, GL_BGR, GL_UNSIGNED_BYTE,
				  buffer);

	if (TGA_Write (name, vid.width, vid.height, 3, buffer))
		Com_Printf ("Wrote %s\n", name);

	Zone_Free (buffer);
}

void
SCR_CaptureAviDemo (void)
{
	double			t;
	static double	lastframetime;
	char			filename[MAX_OSPATH];

	/* check frame time */
	t = Sys_DoubleTime ();
	if ((t - lastframetime) < 1.0 / (double)cl_avidemo->ivalue)
		return;

	lastframetime = t;
	
	snprintf (filename, sizeof (filename), "%s/twavi%06d.tga", com_gamedir, aviframeno);
	aviframeno++;

	qglReadPixels (0, 0, vid.width, vid.height, GL_BGR, GL_UNSIGNED_BYTE,
				  avibuffer);

	if (!TGA_Write (filename, vid.width, vid.height, 3, avibuffer)) {
		Com_Printf ("Screenshot write failed, stopping AVI demo.\n");
		Cvar_Set(cl_avidemo, "0");
	}
}

void AvidemoChanged(cvar_t *cvar)
{
	if (cvar->ivalue)
		avibuffer = Zone_Alloc(tempzone, vid.width * vid.height * 3);
	else {
		if (avibuffer)
			Zone_Free(avibuffer);
		aviframeno = 0;
	}
}

/*
===============
SCR_BeginLoadingPlaque

================
*/
void
SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

	/* redraw with no console and the loading plaque */
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = host_realtime;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void
SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify ();
}

/* ========================================================================= */

char       *scr_notifystring;
qboolean    scr_drawdialog;

void
SCR_DrawNotifyString (void)
{
	char	   *start;
	int			x, y, l;

	start = scr_notifystring;

	y = vid.height_2d * 0.35;

	do {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width_2d - l * con->tsize) / 2;
		Draw_String_Len (x, y, start, l, con->tsize);

		y += con->tsize;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						/* skip the \n */
	} while (1);
}

/* ========================================================================= */

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void
SCR_UpdateScreen (void)
{
	if (scr_disabled_for_loading) {
		if (host_realtime - scr_disabled_time > 60) {
			scr_disabled_for_loading = false;
			Com_Printf ("load failed.\n");
		} else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;							/* not initialized yet */

	// LordHavoc: do finish and display at the beginning of the frame,
	// to get CPU/GPU overlap benefits (CPU thinks while GPU draws)
	GL_EndRendering ();

	if (cl_avidemo->ivalue)
		SCR_CaptureAviDemo ();

	qglEnable (GL_DEPTH_TEST);

	SCR_CalcRefdef ();

	/*
	 * do 3D refresh drawing, and then update the screen
	 */
	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

	/*
	 * draw any areas not covered by the refresh
	 */

	if (scr_drawdialog) {
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	} else if (scr_drawloading) {
		SCR_DrawLoading ();
		Sbar_Draw ();
	} else if (cl.intermission == 1 && key_dest == key_game) {
		Sbar_IntermissionOverlay ();
	} else if (cl.intermission == 2 && key_dest == key_game) {
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	} else {
		if (crosshair->ivalue)
			Draw_Crosshair ();

		SCR_DrawNet ();
		SCR_DrawFPS ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		Sbar_Draw ();
		SCR_DrawConsole ();
		M_Draw ();
	}

	V_UpdatePalette ();
	GL_BrightenScreen ();

	// LordHavoc: flush command queue to card (most drivers buffer commands for burst transfer)
	// (note: this does not wait for anything to finish, it just empties the buffer)
	qglFlush();
}

