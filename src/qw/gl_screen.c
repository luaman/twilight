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
#include <time.h>

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "renderer/draw.h"
#include "host.h"
#include "image/image.h"
#include "keys.h"
#include "menu.h"
#include "image/pcx.h"
#include "hud.h"
#include "renderer/screen.h"
#include "sound/sound.h"
#include "strlib.h"
#include "sys.h"
#include "image/tga.h"
#include "renderer/textures.h"
#include "fs/fs.h"

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

float			scr_con_current;
static float	scr_conlines;				/* lines of console to display */

static cvar_t   *scr_viewsize;
static cvar_t   *scr_fov;
static cvar_t   *scr_conspeed;
static cvar_t   *scr_centertime;
static cvar_t   *scr_showram;
static cvar_t   *scr_showturtle;
static cvar_t   *scr_showpause;
static cvar_t   *scr_printspeed;
static cvar_t   *scr_logcprint;
static cvar_t   *scr_allowsnap;
static cvar_t   *r_brightness;
static cvar_t   *r_contrast;
static cvar_t   *cl_avidemo;

cvar_t	*show_fps;
int				fps_capped0, fps_capped1;
int				fps_count;

static qboolean	scr_initialized;			/* ready to draw */

static image_t   *scr_net;
static image_t   *scr_turtle;

qboolean		scr_disabled_for_loading;
static qboolean	scr_drawloading;
static float	scr_disabled_time;

static void		SCR_ScreenShot_f (void);
static void		SCR_RSShot_f (void);

static Uint8	*avibuffer;
static Uint32	 aviframeno;

static void
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

static char			scr_centerstring[1024];
static float		scr_centertime_start;		/* for slow victory printing */
float				scr_centertime_off;
static int			scr_center_lines;
static int			scr_erase_lines;
static int			scr_erase_center;

/*
==============
Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void
SCR_CenterPrint (const char *str)
{
	const char	*s;
	char		line[64];
	int			i, j, l;

	strlcpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->fvalue;
	scr_centertime_start = ccl.time;

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


static void
SCR_DrawCenterString (void)
{
	char	   *start;
	int			x, y, l;
	int			remaining;

	/* the finale prints the characters one at a time */
	if (ccl.intermission)
		remaining = scr_printspeed->ivalue * (ccl.time - scr_centertime_start);
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

static void
SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= ccl.frametime;

	if (scr_centertime_off <= 0 && !ccl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

/* ========================================================================= */

static float
CalcFov (float fov_x, float width, float height)
{
	return Q_atan (height / (width / Q_tan (fov_x/360*M_PI))) * 360 / M_PI;
}

static void
SCR_CalcRefdef (void)
{
	int			contents;

	/* intermission is always full screen */
	if (scr_viewsize->ivalue >= 120 || ccl.intermission)
		sb_lines = 0;
	else if (scr_viewsize->ivalue >= 110)
		sb_lines = 24;
	else
		sb_lines = 24 + 16 + 8;

	r.fov_x = bound (10, scr_fov->fvalue * ccl.viewzoom, 170);
	r.fov_y = CalcFov (r.fov_x, vid.width, vid.height);

	if (ccl.worldmodel)
	{
		contents = Mod_PointInLeaf (r.origin, ccl.worldmodel)->contents;
		if (contents != CONTENTS_EMPTY && contents != CONTENTS_SOLID)
		{
			r.fov_x *= (Q_sin(ccl.time * 4.7) * 0.015 + 0.985);
			r.fov_y *= (Q_sin(ccl.time * 3.0) * 0.015 + 0.985);
		}
	}
}


/*
=================
Keybinding command
=================
*/
static void
SCR_SizeUp_f (void)
{
	Cvar_Slide (scr_viewsize, 10);
}


/*
=================
Keybinding command
=================
*/
static void
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

static void
AvidemoChanged(cvar_t *cvar)
{
	if (cvar->ivalue)
		avibuffer = Zone_Alloc(tempzone, vid.width * vid.height * 3);
	else {
		if (avibuffer)
			Zone_Free(avibuffer);
		aviframeno = 0;
	}
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
	scr_allowsnap = Cvar_Get ("scr_allowsnap", "1", CVAR_NONE, NULL);
	r_brightness = Cvar_Get ("r_brightness", "1", CVAR_ARCHIVE, NULL);
	r_contrast = Cvar_Get ("r_contrast", "1", CVAR_ARCHIVE, NULL);
	cl_avidemo = Cvar_Get ("cl_avidemo", "0", CVAR_NONE, &AvidemoChanged);
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL);
}


void
SCR_Init (void)
{

	/*
	 * register our commands
	 */
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("snap", SCR_RSShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);

	scr_net = Image_Load ("gfx/net", TEX_UPLOAD | TEX_ALPHA);
	scr_turtle = Image_Load ("gfx/turtle", TEX_UPLOAD | TEX_ALPHA);

	scr_initialized = true;
}

void
SCR_Shutdown (void)
{
	/*
	Cmd_RemoveCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_RemoveCommand ("sizeup", SCR_SizeUp_f);
	Cmd_RemoveCommand ("sizedown", SCR_SizeDown_f);
	*/

	Image_Free (scr_net, true);
	Image_Free (scr_turtle, true);
	scr_net = NULL;
	scr_turtle = NULL;

	scr_initialized = false;
}




static void
SCR_DrawTurtle (void)
{
	static int		count;

	if (!scr_showturtle->ivalue)
		return;

	if (host.frametime < 0.1) {
		count = 0;
		return;
	}

	if (!scr_turtle)
		return;

	count++;
	if (count < 3)
		return;

	Draw_Img (0, 0, scr_turtle);
}

static void
SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged <
		UPDATE_BACKUP - 1)
		return;
	if (ccls.demoplayback)
		return;
	if (!scr_net)
		return;

	Draw_Img (64, 0, scr_net);
}

static void
SCR_DrawFPS (void)
{
	static double		lastframetime;
	static int			lastfps;
	int					x, y, st_len;
	char				st[80];

	if (!show_fps->ivalue)
		return;

	if ((host.time - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = host.time;
	}

	if (show_fps->ivalue == 2)
		snprintf (st, sizeof (st), "(%d %d) %3d FPS", fps_capped0, fps_capped1, lastfps);
	else
		snprintf (st, sizeof (st), "%3d FPS", lastfps);
	st_len = strlen (st);

	x = vid.width_2d - st_len * con->tsize - con->tsize;
	y = vid.height_2d - sb_lines - con->tsize;
	Draw_String_Len (x, y, st, st_len, con->tsize);
}


static void
SCR_DrawPause (void)
{
	image_t	   *pic;

	if (!scr_showpause->ivalue)			/* turn off for screenshots */
		return;

	if (!ccl.paused)
		return;

	pic = Draw_CacheImg ("gfx/pause");
	Draw_Img ((vid.width_2d - pic->width) / 2,
			  (vid.height_2d - 48 - pic->height) / 2, pic);
}


static void
SCR_DrawLoading (void)
{
	image_t	   *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CacheImg ("gfx/loading");
	Draw_Img ((vid.width_2d - pic->width) / 2,
			  (vid.height_2d - 48 - pic->height) / 2, pic);
}



/* ========================================================================= */


static void
SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (scr_drawloading)
		/* never a console with loading plaque */
		return;

	/* decide on the height of the console */
	if (ccls.state != ca_active) {
		scr_conlines = vid.height_2d;		/* full screen */
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console)
		scr_conlines = vid.height_2d / 2;	/* half screen */
	else
		scr_conlines = 0;					/* none visible */

	if (scr_conlines < scr_con_current) {
		scr_con_current -= scr_conspeed->fvalue * host.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed->fvalue * host.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}

static void
SCR_DrawConsole (void)
{
	if (scr_con_current) {
		Con_DrawConsole (scr_con_current);
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

static void
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

static void
SCR_CaptureAviDemo (void)
{
	static double	lastframetime;
	char			filename[MAX_OSPATH];

	/* check frame time */
	if ((host.time - lastframetime) < 1.0 / (double)cl_avidemo->ivalue)
		return;

	lastframetime = host.time;

	snprintf (filename, sizeof (filename), "%s/twavi%06d.tga", com_gamedir, aviframeno);
	aviframeno++;

	qglReadPixels (0, 0, vid.width, vid.height, GL_BGR, GL_UNSIGNED_BYTE,
				  avibuffer);

	if (!TGA_Write (filename, vid.width, vid.height, 3, avibuffer)) {
		Com_Printf ("Screenshot write failed, stopping AVI demo.\n");
		Cvar_Set(cl_avidemo, "0");
	}
}

void
SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (ccls.state != ca_active)
		return;

	/* redraw with no console and the loading plaque */
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = host.time;
}

void
SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify ();
}


static void
WritePCXfile (char *filename, Uint8 *data, int width, int height,
			  int rowbytes, Uint8 *palette, qboolean upload)
{
	int			i, j, length;
	pcx_t	   *pcx;
	Uint8	   *pack;
	SDL_RWops	*file;

	rowbytes = rowbytes;

	// Worst case is twice the size of the image.
	pcx = Zone_Alloc(tempzone, sizeof(pcx_t) + ((width * height) * 2) + 769);
	if (pcx == NULL) {
		Com_Printf ("SCR_ScreenShot_f: not enough memory\n");
		return;
	}

	pcx->manufacturer = 0x0a;			/* PCX id */
	pcx->version = 5;					/* 256 color */
	pcx->encoding = 1;					/* uncompressed */
	pcx->bits_per_pixel = 8;			/* 256 color */
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort ((short) (width - 1));
	pcx->ymax = LittleShort ((short) (height - 1));
	pcx->hres = LittleShort ((short) width);
	pcx->vres = LittleShort ((short) height);
	pcx->color_planes = 1;				/* chunky image */
	pcx->bytes_per_line = LittleShort ((short) width);
	pcx->palette_type = LittleShort (2);	/* not a grey scale */

	/* pack the image */
	pack = pcx->data;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			if ((*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else {
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
	}

	/* write the palette */
	*pack++ = 0x0c;						/* palette ID byte */
	for (i = 0; i < 768; i++)
		*pack++ = *palette++;

	/* write output file */
	length = pack - (Uint8 *) pcx;

	if (upload)
		CL_StartUpload ((void *) pcx, length);
	else if ((file = FS_Open_New (filename, 0))) {
		SDL_RWwrite (file, pcx, length, 1);
		SDL_RWclose (file);
	}
	Zone_Free(pcx);
}



/*
 * Find closest color in the palette for named color
 */
int
MipColor (int r, int g, int b)
{
	int			i;
	float		dist;
	int			best = 0;
	float		bestdist;
	int			r1, g1, b1;
	static int	lr = -1, lg = -1, lb = -1;
	static int	lastbest;

	if (r == lr && g == lg && b == lb)
		return lastbest;

	bestdist = 256 * 256 * 3;

	for (i = 0; i < 256; i++) {
		r1 = host_basepal[i * 3] - r;
		g1 = host_basepal[i * 3 + 1] - g;
		b1 = host_basepal[i * 3 + 2] - b;
		dist = r1 * r1 + g1 * g1 + b1 * b1;
		if (dist < bestdist) {
			bestdist = dist;
			best = i;
		}
	}
	lr = r;
	lg = g;
	lb = b;
	lastbest = best;
	return best;
}


static void
SCR_RSShot_f (void)
{
	int			x, y;
	Uint8	   *src, *dest;
	char		pcxname[80];
	Uint8	   *buf, *tmpbuf;
	int			w, h;
	char		st[80];
	time_t		now;
	int			textsize;
	int			len;

	if (CL_IsUploading ())
		return;							/* already one pending */

	if (ccls.state < ca_onserver)
		return;							/* gotta be connected */

	Com_Printf ("Remote screen shot requested.\n");

	textsize = 8 * vid.width_2d / 320;

	time (&now);
	strlcpy (st, ctime (&now), sizeof (st));
	len = strlen (st);
	x = vid.width_2d - (len * textsize);
	y = 0;
	Draw_Fill(x, y, vid.width_2d - x, textsize, d_8tofloattable[98]);
	Draw_String_Len (x, y, st, len, textsize);

	strlcpy (st, cls.servername, sizeof (st));
	x = vid.width_2d - (len * textsize);
	y += textsize;
	Draw_Fill(x, y, vid.width_2d - x, textsize, d_8tofloattable[98]);
	Draw_String_Len (x, y, st, len, textsize);

	strlcpy (st, name->svalue, sizeof (st));
	x = vid.width_2d - (len * textsize);
	y += textsize;
	Draw_Fill(x, y, vid.width_2d - x, textsize, d_8tofloattable[98]);
	Draw_String_Len (x, y, st, len, textsize);
	qglFinish ();

	/*
	 * save the pcx file
	 */
	buf = Zone_Alloc(tempzone, vid.height * vid.width * 3);

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buf);

	if ((vid.width > RSSHOT_WIDTH) || (vid.height > RSSHOT_HEIGHT)) {
		w = RSSHOT_WIDTH;
		h = RSSHOT_HEIGHT;
		tmpbuf = Zone_Alloc(tempzone, w * h * 3);

		R_ResampleTexture(buf, vid.width, vid.height, tmpbuf, w, h);

		Zone_Free(buf);
		buf = tmpbuf;
	} else {
		w = vid.width;
		h = vid.height;
	}

	/* convert to eight bit */
	for (y = 0; y < h; y++) {
		src = buf + (w * 3 * y);
		dest = buf + (w * y);

		for (x = 0; x < w; x++) {
			*dest++ = MipColor (src[0], src[1], src[2]);
			src += 3;
		}
	}

	WritePCXfile (pcxname, buf, w, h, w, host_basepal, true);

	Zone_Free (buf);

	Com_Printf ("Wrote %s\n", pcxname);
}


/* ========================================================================= */

static char       *scr_notifystring;
static qboolean    scr_drawdialog;

static void
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
This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void
SCR_UpdateScreen (void)
{
	if (scr_disabled_for_loading) {
		if (host.time - scr_disabled_time > 60) {
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
		HUD_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	} else if (scr_drawloading) {
		SCR_DrawLoading ();
		HUD_Draw ();
	} else if (ccl.intermission == 1 && key_dest == key_game) {
		HUD_IntermissionOverlay ();
	} else if (ccl.intermission == 2 && key_dest == key_game) {
		HUD_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	} else {
		Draw_Crosshair ();

		SCR_DrawNet ();
		SCR_DrawFPS ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		HUD_Draw ();
		SCR_DrawConsole ();
		M_Draw ();
	}

	V_UpdatePalette ();
	GL_BrightenScreen ();

	// LordHavoc: flush command queue to card (most drivers buffer commands for burst transfer)
	// (note: this does not wait for anything to finish, it just empties the buffer)
	qglFlush();
}
