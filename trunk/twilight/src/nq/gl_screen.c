/*
	$RCSfile$ -- master for refresh, status bar, console, chat, notify, etc

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
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "glquake.h"
#include "host.h"
#include "keys.h"
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
Con_Printf ();

net 
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/

int			glx, gly;

/* only the refresh window will be updated unless these variables are flagged  */
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;				/* lines of console to display */

cvar_t		*scr_viewsize;
cvar_t		*scr_fov;
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showram;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;
cvar_t		*gl_triplebuffer;
cvar_t		*r_brightness;
cvar_t		*r_contrast;
cvar_t		*r_waterwarp;

extern cvar_t *crosshair;

qboolean    scr_initialized;			/* ready to draw */

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

int         clearconsole;
int         clearnotify;

int         sb_lines;

viddef_t    vid;						/* global video state */

vrect_t     scr_vrect;

qboolean    scr_disabled_for_loading;
qboolean    scr_drawloading;
float       scr_disabled_time;

void        SCR_ScreenShot_f (void);

void
GL_BrightenScreen(void)
{
	float f;

	if (r_brightness->value < 0.1f)
		Cvar_Set (r_brightness, va("%f",0.1f));
	if (r_brightness->value > 5.0f)
		Cvar_Set (r_brightness, va("%f",5.0f));

	if (r_contrast->value < 0.2f)
		Cvar_Set (r_contrast, va("%f",0.2f));
	if (r_contrast->value > 1.0f)
		Cvar_Set (r_contrast, va("%f",1.0f));

	if (r_brightness->value < 1.01f && r_contrast->value > 0.99f)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
	f = r_brightness->value;
	if (f >= 1.01f)
	{
		qglBlendFunc (GL_DST_COLOR, GL_ONE);
		qglBegin (GL_TRIANGLES);
		while (f >= 1.01f)
		{
			if (f >= 2)
				qglColor3f (1, 1, 1);
			else
				qglColor3f (f-1, f-1, f-1);
			qglVertex2f (-5000, -5000);
			qglVertex2f (10000, -5000);
			qglVertex2f (-5000, 10000);
			f *= 0.5;
		}
		qglEnd ();
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	if (r_contrast->value <= 0.99f)
	{
		qglColor4f (1, 1, 1, 1 - r_contrast->value);
		qglBegin (GL_TRIANGLES);
		qglVertex2f (-5000, -5000);
		qglVertex2f (10000, -5000);
		qglVertex2f (-5000, 10000);
		qglEnd ();
	}

	qglColor3f (1, 1, 1);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char        scr_centerstring[1024];
float       scr_centertime_start;		/* for slow victory printing */
float       scr_centertime_off;
int         scr_center_lines;
int         scr_erase_lines;
int         scr_erase_center;

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
	strlcpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	/* count the number of lines for centering */
	scr_center_lines = 1;
	while (*str) {
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void
SCR_DrawCenterString (void)
{
	char       *start;
	int         l;
	int         j;
	int         x, y;
	int         remaining;

	/* the finale prints the characters one at a time */
	if (cl.intermission)
		remaining = scr_printspeed->value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.conheight * 0.35;
	else
		y = 48;

	do {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			Draw_Character (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						/* skip the \n */
	} while (1);
}

void
SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

/* ============================================================================= */

/*
====================
CalcFov
====================
*/
float
CalcFov (float fov_x, float width, float height)
{
	float x;
	float a;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	x = width / Q_tan (fov_x * (M_PI / 360));
	a = Q_atan (height / x) * (360 / M_PI);

	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void
SCR_CalcRefdef (void)
{
	vid.recalc_refdef = false;

	/* intermission is always full screen */
	if (scr_viewsize->value >= 120 || cl.intermission) {
		sb_lines = 0;
	} else if (scr_viewsize->value >= 110) {
		sb_lines = 24;
	} else {
		sb_lines = 24 + 16 + 8;
	}

	if (cl_sbar->value)
		r_refdef.vrect.height = vid.height - sb_lines;
	else
		r_refdef.vrect.height = vid.height;

	r_refdef.vrect.width = vid.width;
	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;

	r_refdef.fov_x = scr_fov->value;
	r_refdef.fov_y =
		CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;
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

/* ============================================================================ */
static void
SCR_viewsize_CB (cvar_t *cvar)
{
	/* bound viewsize */
	if (cvar->value < 30) {
		Cvar_Set (cvar, "30");
	} else if (cvar->value > 120) {
		Cvar_Set (cvar, "120");
	} else {
		vid.recalc_refdef = true;
	}
}

static void
SCR_fov_CB (cvar_t *cvar)
{
	/* bound field of view */
	if (cvar->value < 1) {
		Cvar_Set (cvar, "1");
	} else if (cvar->value > 170) {
		Cvar_Set (cvar, "170");
	} else {
		vid.recalc_refdef = true;
	}
}

void
SCR_Init_Cvars (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE, SCR_viewsize_CB);
	scr_fov = Cvar_Get ("fov", "90", CVAR_NONE, SCR_fov_CB);	/* 10 - 170 */
	scr_conspeed = Cvar_Get ("scr_conspeed", "300", CVAR_NONE, NULL);
	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, NULL);
	scr_showram = Cvar_Get ("showram", "1", CVAR_NONE, NULL);
	scr_showturtle = Cvar_Get ("showturtle", "0", CVAR_NONE, NULL);
	scr_showpause = Cvar_Get ("showpause", "1", CVAR_NONE, NULL);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, NULL);
	gl_triplebuffer = Cvar_Get ("gl_triplebuffer", "1", CVAR_ARCHIVE, NULL);
	r_brightness = Cvar_Get ("r_brightness", "1", CVAR_ARCHIVE, NULL);
	r_contrast = Cvar_Get ("r_contrast", "1", CVAR_ARCHIVE, NULL);
	r_waterwarp = Cvar_Get ("r_waterwarp", "0", CVAR_ARCHIVE, NULL);
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
	register our commands
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
	static int  count;

	if (!scr_showturtle->value)
		return;

	if (host_frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void
SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x + 64, scr_vrect.y, scr_net);
}

void
SCR_DrawFPS (void)
{
	extern cvar_t *show_fps;
	static double lastframetime;
	double      t;
	extern int  fps_count;
	static int  lastfps;
	int         x, y;
	char        st[80];

	if (!show_fps->value)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;
	}

	snprintf (st, sizeof (st), "%3d FPS", lastfps);
	x = vid.conwidth - strlen (st) * 8 - 8;
	y = vid.conheight - sb_lines - 8;
/*  Draw_TileClear(x, y, strlen(st) * 8, 8); */
	Draw_String (x, y, st);
}


/*
==============
DrawPause
==============
*/
void
SCR_DrawPause (void)
{
	qpic_t		*pic;

	if (!scr_showpause->value)			/* turn off for screenshots */
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ((vid.conwidth - pic->width) / 2,
			  (vid.conheight - 48 - pic->height) / 2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void
SCR_DrawLoading (void)
{
	qpic_t		*pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ((vid.conwidth - pic->width) / 2,
			  (vid.conheight - 48 - pic->height) / 2, pic);
}



/* ============================================================================= */


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
		return;								/* never a console with loading plaque */

	/* decide on the height of the console */
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;
	if (con_forcedup) {
		scr_conlines = vid.conheight;		/* full screen */
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.conheight / 2;	/* half screen */
	else
		scr_conlines = 0;					/* none visible */

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->value * host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->value * host_frametime;
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
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current, true);
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
	Uint8       *buffer;
	char        pcxname[80];
	char        checkname[MAX_OSPATH];
	int         i, c, temp;

/* 
	find a file name to save it to 
*/
	strcpy (pcxname, "quake00.tga");

	for (i = 0; i <= 99; i++) {
		pcxname[5] = i / 10 + '0';
		pcxname[6] = i % 10 + '0';
		snprintf (checkname, sizeof (checkname), "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime (checkname) == -1)
			break;						/* file doesn't exist */
	}
	if (i == 100) {
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}


	buffer = malloc (vid.width * vid.height * 3);

	qglReadPixels (glx, gly, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE,
				  buffer);

	/* swap rgb to bgr */
	c = vid.width * vid.height * 3;
	for (i = 0; i < c; i += 3) {
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

	if (TGA_Write (pcxname, vid.width, vid.height, 3, buffer))
		Con_Printf ("Wrote %s\n", pcxname);

	free (buffer);
}


/* ============================================================================= */


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
	scr_disabled_time = realtime;
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

/* ============================================================================= */

char       *scr_notifystring;
qboolean    scr_drawdialog;

void
SCR_DrawNotifyString (void)
{
	char       *start;
	int         l;
	int         j;
	int         x, y;

	start = scr_notifystring;

	y = vid.conheight * 0.35;

	do {
		/* scan the width of the line */
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;						/* skip the \n */
	} while (1);
}

/* ============================================================================= */


void
SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		/* left */
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		/* right */
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		/* top */
		Draw_TileClear (r_refdef.vrect.x, 0,
						r_refdef.vrect.x + r_refdef.vrect.width,
						r_refdef.vrect.y);
		/* bottom */
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						vid.height - sb_lines -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

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
	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading) {
		if (realtime - scr_disabled_time > 60) {
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		} else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;							/* not initialized yet */

	qglEnable (GL_DEPTH_TEST);

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	/*
		do 3D refresh drawing, and then update the screen
	*/
	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

	/*
		draw any areas not covered by the refresh
	*/
	SCR_TileClear ();

	if (scr_drawdialog) {
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	} else if (scr_drawloading) {
		SCR_DrawLoading ();
		Sbar_Draw ();
	} else if (cl.intermission == 1 && key_dest == key_game) {
		Sbar_IntermissionOverlay ();
	} else if (cl.intermission == 2 && key_dest == key_game) {
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	} else {
		if (crosshair->value)
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

	GL_EndRendering ();
}

