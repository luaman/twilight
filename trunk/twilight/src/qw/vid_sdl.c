/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef WIN32
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#ifndef WIN32
#include <dlfcn.h>
#endif

#include <SDL.h>
#include "quakedef.h"


#define WARP_WIDTH              640
#define WARP_HEIGHT             480


#ifdef WIN32
// LordHavoc: evil thing - DirectSound with SDL
HWND mainwindow;
#endif

unsigned short d_8to16table[256];
unsigned    d_8to24table[256];
unsigned char d_15to8table[65536];

cvar_t      vid_mode = { "vid_mode", "0", false };
cvar_t      m_filter = { "m_filter", "0" };
cvar_t      _windowed_mouse = { "_windowed_mouse", "1", true };
cvar_t      gl_ztrick = { "gl_ztrick", "1" };

static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;

static int  old_windowed_mouse;

static int  scr_width = 640, scr_height = 480;

/*-----------------------------------------------------------------------*/

//int       texture_mode = GL_NEAREST;
//int       texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int       texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int         texture_mode = GL_LINEAR;

//int       texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int       texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int         texture_extension_number = 1;

float       gldepthmin, gldepthmax;


const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

void        (*qglColorTableEXT) (int, int, int, int, int, const void *);
void        (*qgl3DfxSetPaletteEXT) (GLuint *);

static float vid_gamma = 1.0;

qboolean    isPermedia = false;
qboolean    gl_mtexable = false;

/*-----------------------------------------------------------------------*/
void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
}


void
VID_Shutdown (void)
{
	SDL_Quit ();
}

void
signal_handler (int sig)
{
	printf ("Received signal %d, exiting...\n", sig);
	Sys_Quit ();
	exit (0);
}

void
InitSig (void)
{
#ifndef WIN32
	signal (SIGHUP, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGILL, signal_handler);
	signal (SIGTRAP, signal_handler);
	signal (SIGIOT, signal_handler);
	signal (SIGBUS, signal_handler);
	signal (SIGFPE, signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
#endif
}

void
VID_ShiftPalette (unsigned char *p)
{
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal;
	unsigned    r, g, b;
	unsigned    v;
	int         r1, g1, b1;
	int         k;
	unsigned short i;
	unsigned   *table;
	FILE       *f;
	char        s[MAX_OSPATH];
	float       dist, bestdist;
	static qboolean palflag = false;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++) {
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff;		// 255 is transparent

	if (palflag)
		return;
	palflag = true;

	COM_FOpenFile ("glquake/15to8.pal", &f);
	if (f) {
		fread (d_15to8table, 1 << 15, 1, f);
		fclose (f);
	} else {
		for (i = 0; i < (1 << 15); i++) {
			/* Maps 000000000000000 000000000011111 = Red = 0x1F
			   000001111100000 = Blue = 0x03E0 111110000000000 = Grn = 0x7C00 */
			r = ((i & 0x1F) << 3) + 4;
			g = ((i & 0x03E0) >> 2) + 4;
			b = ((i & 0x7C00) >> 7) + 4;
			pal = (unsigned char *) d_8to24table;
			for (v = 0, k = 0, bestdist = 10000.0; v < 256; v++, pal += 4) {
				r1 = (int) r - (int) pal[0];
				g1 = (int) g - (int) pal[1];
				b1 = (int) b - (int) pal[2];
				dist = sqrt (((r1 * r1) + (g1 * g1) + (b1 * b1)));
				if (dist < bestdist) {
					k = v;
					bestdist = dist;
				}
			}
			d_15to8table[i] = k;
		}
		snprintf (s, sizeof(s), "%s/glquake", com_gamedir);
		Sys_mkdir (s);
		snprintf (s, sizeof(s), "%s/glquake/15to8.pal", com_gamedir);
		if ((f = fopen (s, "wb")) != NULL) {
			fwrite (d_15to8table, 1 << 15, 1, f);
			fclose (f);
		}
	}
}

/*
void
CheckMultiTextureExtensions (void)
{
	void       *prjobj;

	if (Q_strstr (gl_extensions, "GL_SGIS_multitexture ")
		&& !COM_CheckParm ("-nomtex")) {
		Con_Printf ("Found GL_SGIS_multitexture...\n");

		if ((prjobj = dlopen (NULL, RTLD_LAZY)) == NULL) {
			Con_Printf ("Unable to open symbol list for main program.\n");
			return;
		}

		qglMTexCoord2fSGIS = (void *) dlsym (prjobj, "glMTexCoord2fSGIS");
		qglSelectTextureSGIS = (void *) dlsym (prjobj, "glSelectTextureSGIS");

		if (qglMTexCoord2fSGIS && qglSelectTextureSGIS) {
			Con_Printf ("Multitexture extensions found.\n");
			gl_mtexable = true;
		} else
			Con_Printf ("Symbol not found, disabled.\n");

		dlclose (prjobj);
	}
}
*/

/*
	CheckMultiTextureExtensions

	Check for ARB or SGIS multitexture support
*/
GLenum gl_mtex_enum = 0;
#ifdef WIN32
void CheckMultiTextureExtensions(void)
{
	qglMTexCoord2f = NULL;
	qglSelectTexture = NULL;
	gl_mtexable = false;
	// Check to see if multitexture is disabled
	if (COM_CheckParm("-nomtex"))
	{
		Con_Printf("...multitexture disabled\n");
		return;
	}
	// Test for ARB_multitexture
	if (!COM_CheckParm("-SGISmtex") && strstr(gl_extensions, "GL_ARB_multitexture "))
	{
		Con_Printf("...using GL_ARB_multitexture\n");
		qglMTexCoord2f = (void *) wglGetProcAddress("glMultiTexCoord2fARB");
		qglSelectTexture = (void *) wglGetProcAddress("glActiveTextureARB");
		gl_mtexable = true;
		gl_mtex_enum = GL_TEXTURE0_ARB;
	}
	else if (strstr(gl_extensions, "GL_SGIS_multitexture ")) // Test for SGIS_multitexture (if ARB_multitexture not found)
	{
		Con_Printf("...using GL_SGIS_multitexture\n");
		qglMTexCoord2f = (void *) wglGetProcAddress("glMTexCoord2fSGIS");
		qglSelectTexture = (void *) wglGetProcAddress("glSelectTextureSGIS");
		gl_mtexable = true;
		gl_mtex_enum = TEXTURE0_SGIS;
	}
	else
		Con_Printf("...multitexture disabled (not detected)\n");
}
#else
void CheckMultiTextureExtensions(void)
{
	void 	   *dlhand;

	Con_Printf ("Checking for multitexture... ");
	if (COM_CheckParm ("-nomtex"))
	{
		Con_Printf ("disabled\n");
		return;
	}
	dlhand = dlopen (NULL, RTLD_LAZY);
	if (dlhand == NULL)
	{
		Con_Printf ("unable to check\n");
		return;
	}
	if (!COM_CheckParm("-SGISmtex") && strstr(gl_extensions, "GL_ARB_multitexture "))
	{
		Con_Printf ("GL_ARB_multitexture\n");
		qglMTexCoord2f = (void *)dlsym(dlhand, "glMultiTexCoord2fARB");
		qglSelectTexture = (void *)dlsym(dlhand, "glActiveTextureARB");
		gl_mtex_enum = GL_TEXTURE0_ARB;
		gl_mtexable = true;
	}
	else if (strstr(gl_extensions, "GL_SGIS_multitexture "))
	{
		Con_Printf ("GL_SGIS_multitexture\n");
		qglMTexCoord2f = (void *)dlsym(dlhand, "glMTexCoord2fSGIS");
		qglSelectTexture = (void *)dlsym(dlhand, "glSelectTextureSGIS");
		gl_mtex_enum = TEXTURE0_SGIS;
		gl_mtexable = true;
	}
	else
		Con_Printf ("none found\n");
	dlclose(dlhand);
	dlhand = NULL;
}
#endif


/*
===============
GL_Init
===============
*/
void
GL_Init (void)
{
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

//  Con_Printf ("%s %s\n", gl_renderer, gl_version);

	CheckMultiTextureExtensions ();

	glClearColor (1, 0, 0, 0);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
GL_BeginRendering

=================
*/
void
GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}


void
GL_EndRendering (void)
{
	SDL_GL_SwapBuffers ();
}


static void
Check_Gamma (unsigned char *pal)
{
	float       f, inf;
	unsigned char palette[768];
	int         i;

	if ((i = COM_CheckParm ("-gamma")) == 0) {
		if ((gl_renderer && Q_strstr (gl_renderer, "Voodoo")) ||
			(gl_vendor && Q_strstr (gl_vendor, "3Dfx")))
			vid_gamma = 1;
		else
			vid_gamma = 0.7;			// default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = Q_atof (com_argv[i + 1]);

	for (i = 0; i < 768; i++) {
		f = pow ((pal[i] + 1) / 256.0, vid_gamma);
		inf = f * 255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof (palette));
}


void
VID_Init (unsigned char *palette)
{
	int         i;
	char        gldir[MAX_OSPATH];
	int         flags = SDL_OPENGL;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&m_filter);
	Cvar_RegisterVariable (&gl_ztrick);
	Cvar_RegisterVariable (&_windowed_mouse);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));

// interpret command-line params

// set vid parameters
	if ((i = COM_CheckParm ("-window")) == 0)
		flags |= SDL_FULLSCREEN;

	if ((i = COM_CheckParm ("-width")) != 0)
		scr_width = atoi (com_argv[i + 1]);
	if (scr_width < 320)
		scr_width = 320;

	if ((i = COM_CheckParm ("-height")) != 0)
		scr_height = atoi (com_argv[i + 1]);
	if (scr_height < 200)
		scr_height = 200;

	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = Q_atoi (com_argv[i + 1]);
	else
		vid.conwidth = scr_width;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = Q_atoi (com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (vid.conheight > scr_height)
		vid.conheight = scr_height;
	if (vid.conwidth > scr_width)
		vid.conwidth = scr_width;

	if (SDL_Init (SDL_INIT_VIDEO) != 0) {
		fprintf (stderr, "Error: %s\n", SDL_GetError ());
		exit (1);
	}
	// We want at least 4444 (16 bit RGBA)
	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 4);
	SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 4);

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);

	if (SDL_SetVideoMode (scr_width, scr_height, 16, flags) == NULL) {
		fprintf (stderr, "Error: %s\n", SDL_GetError ());
		exit (1);
	}
	SDL_WM_SetCaption ("Twilight QWCL", "twilight");

	vid.height = scr_height;
	vid.width = scr_width;

	vid.aspect = ((float) vid.height / (float) vid.width) * (4.0 / 3.0);
	vid.numpages = 2;

	InitSig ();							// trap evil signals

	GL_Init ();

	Check_Gamma (palette);

	snprintf (gldir, sizeof(gldir), "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	VID_SetPalette (palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	SDL_ShowCursor (0);

#ifdef WIN32
	// LordHavoc: a dark incantation necessary for DirectSound with SDL
	// (an evil which Loki clearly did not intend)
	mainwindow = GetActiveWindow();
#endif
}

void
Sys_SendKeyEvents (void)
{
	SDL_Event   event;
	int         sym, state, but;
	int         modstate;

	while (SDL_PollEvent (&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				sym = event.key.keysym.sym;
				state = event.key.state;
				modstate = SDL_GetModState ();
				switch (sym) {
					case SDLK_DELETE:
						sym = K_DEL;
						break;
					case SDLK_BACKSPACE:
						sym = K_BACKSPACE;
						break;
					case SDLK_F1:
						sym = K_F1;
						break;
					case SDLK_F2:
						sym = K_F2;
						break;
					case SDLK_F3:
						sym = K_F3;
						break;
					case SDLK_F4:
						sym = K_F4;
						break;
					case SDLK_F5:
						sym = K_F5;
						break;
					case SDLK_F6:
						sym = K_F6;
						break;
					case SDLK_F7:
						sym = K_F7;
						break;
					case SDLK_F8:
						sym = K_F8;
						break;
					case SDLK_F9:
						sym = K_F9;
						break;
					case SDLK_F10:
						sym = K_F10;
						break;
					case SDLK_F11:
						sym = K_F11;
						break;
					case SDLK_F12:
						sym = K_F12;
						break;
					case SDLK_BREAK:
					case SDLK_PAUSE:
						sym = K_PAUSE;
						break;
					case SDLK_UP:
						sym = K_UPARROW;
						break;
					case SDLK_DOWN:
						sym = K_DOWNARROW;
						break;
					case SDLK_RIGHT:
						sym = K_RIGHTARROW;
						break;
					case SDLK_LEFT:
						sym = K_LEFTARROW;
						break;
					case SDLK_INSERT:
						sym = K_INS;
						break;
					case SDLK_HOME:
						sym = K_HOME;
						break;
					case SDLK_END:
						sym = K_END;
						break;
					case SDLK_PAGEUP:
						sym = K_PGUP;
						break;
					case SDLK_PAGEDOWN:
						sym = K_PGDN;
						break;
					case SDLK_RSHIFT:
					case SDLK_LSHIFT:
						sym = K_SHIFT;
						break;
					case SDLK_RCTRL:
					case SDLK_LCTRL:
						sym = K_CTRL;
						break;
					case SDLK_RALT:
					case SDLK_LALT:
						sym = K_ALT;
						break;
					case SDLK_KP0:
						if (modstate & KMOD_NUM)
							sym = K_INS;
						else
							sym = SDLK_0;
						break;
					case SDLK_KP1:
						if (modstate & KMOD_NUM)
							sym = K_END;
						else
							sym = SDLK_1;
						break;
					case SDLK_KP2:
						if (modstate & KMOD_NUM)
							sym = K_DOWNARROW;
						else
							sym = SDLK_2;
						break;
					case SDLK_KP3:
						if (modstate & KMOD_NUM)
							sym = K_PGDN;
						else
							sym = SDLK_3;
						break;
					case SDLK_KP4:
						if (modstate & KMOD_NUM)
							sym = K_LEFTARROW;
						else
							sym = SDLK_4;
						break;
					case SDLK_KP5:
						sym = SDLK_5;
						break;
					case SDLK_KP6:
						if (modstate & KMOD_NUM)
							sym = K_RIGHTARROW;
						else
							sym = SDLK_6;
						break;
					case SDLK_KP7:
						if (modstate & KMOD_NUM)
							sym = K_HOME;
						else
							sym = SDLK_7;
						break;
					case SDLK_KP8:
						if (modstate & KMOD_NUM)
							sym = K_UPARROW;
						else
							sym = SDLK_8;
						break;
					case SDLK_KP9:
						if (modstate & KMOD_NUM)
							sym = K_PGUP;
						else
							sym = SDLK_9;
						break;
					case SDLK_KP_PERIOD:
						if (modstate & KMOD_NUM)
							sym = K_DEL;
						else
							sym = SDLK_PERIOD;
						break;
					case SDLK_KP_DIVIDE:
						sym = SDLK_SLASH;
						break;
					case SDLK_KP_MULTIPLY:
						sym = SDLK_ASTERISK;
						break;
					case SDLK_KP_MINUS:
						sym = SDLK_MINUS;
						break;
					case SDLK_KP_PLUS:
						sym = SDLK_PLUS;
						break;
					case SDLK_KP_ENTER:
						sym = SDLK_RETURN;
						break;
					case SDLK_KP_EQUALS:
						sym = SDLK_EQUALS;
						break;
				}
				// If we're not directly handled and still above 255
				// just force it to 0
				if (sym > 255)
					sym = 0;
				Key_Event (sym, state);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				but = event.button.button;
				if (but == 2)
					but = 3;
				else if (but == 3)
					but = 2;
				switch (but) {
					case 1:
					case 2:
					case 3:
						Key_Event (K_MOUSE1 + but - 1, event.type
								   == SDL_MOUSEBUTTONDOWN);
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				if (_windowed_mouse.value) {
					if ((event.motion.x != (vid.width / 2))
						|| (event.motion.y != (vid.height / 2))) {
						mouse_x = event.motion.xrel * 5;
						mouse_y = event.motion.yrel * 5;
						if ((event.motion.x <
							 ((vid.width / 2) - (vid.width / 4)))
							|| (event.motion.x >
								((vid.width / 2) + (vid.width / 4)))
							|| (event.motion.y <
								((vid.height / 2) - (vid.height / 4)))
							|| (event.motion.y >
								((vid.height / 2) + (vid.height / 4)))) {
							SDL_WarpMouse ((Uint16) (vid.width / 2), (Uint16) (vid.height / 2));
						}
					}
				}
				break;

			case SDL_QUIT:
				CL_Disconnect ();
				// FIXME: Put this back when local server support is added
				// Host_ShutdownServer(false);
				Sys_Quit ();
				break;

			default:
				break;
		}
	}
}


void
Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void
IN_Init (void)
{
	mouse_x = mouse_y = 0.0;
}

void
IN_Shutdown (void)
{
}

/*
===========
IN_Commands
===========
*/
void
IN_Commands (void)
{
	// FIXME: Move this to a Cvar callback when they're implemented
	if (old_windowed_mouse != _windowed_mouse.value) {
		old_windowed_mouse = _windowed_mouse.value;
		if (!_windowed_mouse.value)
			SDL_WM_GrabInput (SDL_GRAB_OFF);
		else
			SDL_WM_GrabInput (SDL_GRAB_ON);
	}
}

/*
===========
IN_Move
===========
*/
void
IN_Move (usercmd_t *cmd)
{
	if (m_filter.value) {
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (in_mlook.state & 1)
		V_StopPitchDrift ();

	if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	} else {
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}

	mouse_x = mouse_y = 0.0;
}
