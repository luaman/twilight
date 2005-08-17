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
#include <stdlib.h>
#include <stdarg.h>

#include "quakedef.h"
#include "cmd.h"
#include "common.h"
#include "console.h"
#include "crc.h"
#include "cvar.h"
#include "renderer/draw.h"
#include "net.h"
#include "protocol.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"
#include "fs/fs.h"

usercmd_t nullcmd;					// guarenteed to be zero

cvar_t *fs_shareconf;
static cvar_t *fs_sharepath;
cvar_t *fs_userconf;
static cvar_t *fs_userpath;
static cvar_t *fs_gamename;
static cvar_t *game_name;
static cvar_t *registered;

static void COM_InitFilesystem (void);
//static void COM_Path_f (void);
static void Com_PrintHex (Uint8 *str, int len);
static void *SZ_GetSpace (sizebuf_t *buf, size_t length);


char gamedirfile[MAX_OSPATH];

/*


All of Quake's data access is through a hierchal file system, but the contents
of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and
all game directories.  The sys_* files pass this to host_init in
quakeparms_t->basedir.  This can be overridden with the "-basedir" command
line parm to allow code debugging in a different directory.  The base
directory is only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that
all generated files (savegames, screenshots, demos, config files) will be
saved to.  This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.  This is a
precacution against having a malicious server instruct clients to write files
over areas they shouldn't.

The "cache directory" is only used during development to save network
bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.
*/

//============================================================================


// ClearLink is used for new headnodes
void
ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void
RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void
InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void
InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int
Q_atoi (const char *str)
{
	int			val;
	int			sign;
	int			c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

	/*
	 * check for hex
	 */
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

	/*
	 * check for character
	 */
	if (str[0] == '\'')
		return sign * str[1];

	/*
	 * assume decimal
	 */
	while (1)
	{
		c = *str++;
		if (c < '0' || c > '9')
			return val * sign;
		val = val * 10 + c - '0';
	}

	return 0;
}


float
Q_atof (const char *str)
{
	double		val;
	int			sign;
	int			c;
	int			decimal, total;

	if (*str == '-')
	{
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

	/*
	 * check for hex
	 */
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

	/*
	 * check for character
	 */
	if (str[0] == '\'')
		return sign * str[1];

	/*
	 * assume decimal
	 */
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c < '0' || c > '9')
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val * sign;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

/*
 * writing functions
 */

void
MSG_WriteChar (sizebuf_t *sb, int c)
{
	Uint8	   *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void
MSG_WriteByte (sizebuf_t *sb, int c)
{
	Uint8	   *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void
MSG_WriteShort (sizebuf_t *sb, int c)
{
	Uint8	   *buf;

#ifdef PARANOID
	if (c < ((short) 0x8000) || c > (short) 0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void
MSG_WriteLong (sizebuf_t *sb, int c)
{
	Uint8	   *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void
MSG_WriteFloat (sizebuf_t *sb, float f)
{
	float_int_t dat;

	dat.f = f;
	dat.i = LittleLong (dat.i);

	SZ_Write (sb, &dat.i, 4);
}

void
MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen (s) + 1);
}

void
MSG_WriteCoord (sizebuf_t *sb, float f)
{
	// Vic: FIXME, it should be Q_rint
	// leave it for now because of old prediction
	MSG_WriteShort (sb, (int) (f * 8));
}

void
MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, Q_rint (f * (256.0f / 360.0f)) & 255);
}

void
MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f * (65536.0f / 360.0f)) & 65535);
}

void
MSG_WriteDeltaUsercmd (sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd)
{
	int			bits;

	/*
	 * send the movement message
	 */
	bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

	MSG_WriteByte (buf, bits);

	if (bits & CM_ANGLE1)
		MSG_WriteAngle16 (buf, cmd->angles[0]);
	if (bits & CM_ANGLE2)
		MSG_WriteAngle16 (buf, cmd->angles[1]);
	if (bits & CM_ANGLE3)
		MSG_WriteAngle16 (buf, cmd->angles[2]);

	if (bits & CM_FORWARD)
		MSG_WriteShort (buf, cmd->forwardmove);
	if (bits & CM_SIDE)
		MSG_WriteShort (buf, cmd->sidemove);
	if (bits & CM_UP)
		MSG_WriteShort (buf, cmd->upmove);

	if (bits & CM_BUTTONS)
		MSG_WriteByte (buf, cmd->buttons);
	if (bits & CM_IMPULSE)
		MSG_WriteByte (buf, cmd->impulse);
	MSG_WriteByte (buf, cmd->msec);
}


/*
 * reading functions
 */
size_t msg_readcount;
qboolean msg_badread;

void
MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int
MSG_GetReadCount (void)
{
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int
MSG_ReadChar (void)
{
	signed char		c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (signed char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int
MSG_ReadByte (void)
{
	unsigned char	c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (unsigned char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int
MSG_ReadShort (void)
{
	short		c;

	if (msg_readcount + 2 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (short) (net_message.data[msg_readcount]
				 + (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int
MSG_ReadLong (void)
{
	int			c;

	if (msg_readcount + 4 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
		+ (net_message.data[msg_readcount + 1] << 8)
		+ (net_message.data[msg_readcount + 2] << 16)
		+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float
MSG_ReadFloat (void)
{
	float_int_t dat;

	if (msg_readcount + 4 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	dat.b[0] = net_message.data[msg_readcount + 0];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.i = LittleLong (dat.i);

	return dat.f;
}

char *
MSG_ReadString (void)
{
	static char		string[2048];
	int				c;
	unsigned		l;

	l = 0;
	do {
		c = MSG_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof (string) - 1);

	string[l] = 0;

	return string;
}

char       *
MSG_ReadStringLine (void)
{
	static char		string[2048];
	int				c;
	unsigned		l;

	l = 0;
	do {
		c = MSG_ReadChar ();
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof (string) - 1);

	string[l] = 0;

	return string;
}

float
MSG_ReadCoord (void)
{
	return MSG_ReadShort () * (1.0 / 8);
}

float
MSG_ReadAngle (void)
{
	return MSG_ReadChar () * (360.0 / 256);
}

float
MSG_ReadAngle16 (void)
{
	return MSG_ReadShort () * (360.0 / 65536);
}

void
MSG_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move)
{
	int			bits;

	memcpy (move, from, sizeof (*move));

	bits = MSG_ReadByte ();

	// read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadAngle16 ();
	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadAngle16 ();
	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadAngle16 ();

	// read movement
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort ();
	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort ();
	if (bits & CM_UP)
		move->upmove = MSG_ReadShort ();

	// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

	// read time to run command
	move->msec = MSG_ReadByte ();
}

void
MSG_PrintPacket ()
{
	Com_PrintHex(net_message.data, net_message.cursize);
}


//===========================================================================

#define	MAXPRINTMSG	4096

static void (*rd_print) (char *) = NULL;

void
Com_BeginRedirect (void (*RedirectedPrint) (char *))
{
	rd_print = RedirectedPrint;
}

void
Com_EndRedirect (void)
{
	rd_print = NULL;
}

static
void Com_PrintHex (Uint8 *str, int len)
{
	Uint8	c;
	int		i;

	for (i = 0; i < len; i++) {
		c = str[i];
		if ((c > 126) || (c < 32)) {
			c = '.';
		}
		Com_Printf("%c  ", c);
	}
	Com_Printf("\n");

	for (i = 0; i < len; i++) {
		Com_Printf("%02x ", str[i] & 0xFF);
	}
	Com_Printf("\n");
}

void
Com_Printf (const char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	if ( rd_print ) {
		rd_print ( msg );
		return;
	}

// also echo to debugging console
	Sys_Printf ("%s", msg);				// also echo to debugging console

// log all messages to file
	if (logname[0])
		Sys_DebugLog (logname, "%s", msg);

// write it to the scrollable buffer
	Con_Print (msg);
}

/*
 * This just wraps to Com_DFPrintf for now.
 * Should we eventually move everything to Com_DFPrintf?
 */
void
Com_DPrintf (const char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	/*
	 * I'd really rather do this with a macro, but I can't do that
	 * without breaking portability.  I don't know which compilers
	 * support ... in #defines, but I know that it's an extension. - rain
	 */

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Com_DFPrintf (DEBUG_DEFAULT, "%s", msg);
}

/* Like Com_DFPrintf, but takes a log level to sort things out a bit */
void
Com_DFPrintf (int level, const char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	/*
	 * don't confuse non-developers with
	 * techie stuff...
	 */

	if (!developer || !developer->ivalue || !(developer->ivalue & level))
		return;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}

//===========================================================================

void 
SZ_Init (sizebuf_t *buf, Uint8 *data, size_t length)
{
	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void
SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

static void *
SZ_GetSpace (sizebuf_t *buf, size_t length)
{
	void	   *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)",
					buf->maxsize);

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

		// because Com_Printf may be redirected
		Sys_Printf ("SZ_GetSpace: overflow\n");
		SZ_Clear (buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void
SZ_Write (sizebuf_t *buf, const void *data, size_t length)
{
	memcpy (SZ_GetSpace (buf, length), data, length);
}

void
SZ_Print (sizebuf_t *buf, const char *data)
{
	int			len;

	len = strlen (data) + 1;

	if (!buf->cursize || buf->data[buf->cursize - 1])
		// no trailing 0
		memcpy ((Uint8 *) SZ_GetSpace (buf, len), data, len);
	else
		// write over trailing 0
		memcpy ((Uint8 *) SZ_GetSpace (buf, len - 1) - 1, data, len);
}


//============================================================================


const char *
COM_SkipPath (const char *pathname)
{
	const char       *last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

void
COM_StripExtension (const char *in, char *out)
{
	char *last = NULL;

	while (*in)
	{
		if (*in == '.')
			last = out;
		if ((*in == '/') || (*in == '\\') || (*in == ':'))
			last = NULL;
		*out++ = *in++;
	}

	if (last)
		*last = '\0';
}



void
COM_DefaultExtension (char *path, const char *extension, size_t len)
{
	const char	   *src;

	/*
	 * if path doesn't have a .EXT, append extension
	 * (extension should include the .)
	 */
	src = path + strlen (path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			// it has an extension
			return;
		src--;
	}

	strlcat (path, extension, len);
}

//============================================================================

char com_token[1024];


/*
==============
Parse a token out of a string
==============
*/
const char *
COM_Parse (const char *data)
{
	int			c;
	int			len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
  skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			// end of file;
			return NULL;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}


/*
================
Looks for the pop.lmp file
Sets the "registered" cvar.
================
*/
static void
COM_CheckRegistered (void)
{
	if (!FS_FindFile ("gfx/pop.lmp"))
		return;

	Cvar_Set (registered, "1");
	Com_Printf ("Playing registered version.\n");
}


void
COM_Init_Cvars (void)
{
	registered = Cvar_Get ("registered", "0", CVAR_NONE, NULL);

	// fs_shareconf/userconf can't be done here
	fs_sharepath = Cvar_Get ("fs_sharepath", SHAREPATH, CVAR_ROM, ExpandPath);
	fs_userpath = Cvar_Get ("fs_userpath", USERPATH, CVAR_ROM, ExpandPath);

	fs_gamename = Cvar_Get ("fs_gamename", "id1", CVAR_ROM, NULL);

	game_name = Cvar_Get ("game_name", "qw", CVAR_ROM, NULL);
}

void
COM_Init (void)
{
//	Cmd_AddCommand ("path", COM_Path_f);

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}


/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int		com_filesize;
char	com_gamedir[MAX_OSPATH];


/*
============
Only used for CopyFile and download
============
*/
void
COM_CreatePath (const char *path)
{
	char	   *ofs, *tmp;

	tmp = Zstrdup (tempzone, path);
	for (ofs = tmp + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{
			// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
	Z_Free (tmp);
}

/*
============
Filename are reletive to the quake directory.
Always appends a 0 byte to the loaded data.
============
*/
static Uint8 *
COM_LoadFile (const char *path, qboolean complain, int type, memzone_t *zone)
{
	fs_file_t	*file;
	SDL_RWops	*rw;
	Uint8		*buf = NULL;
	int			len;

	// look for it in the filesystem or pack files
	file = FS_FindFile (path);
	if (!file) {
		if (complain)
			Sys_Printf ("LoadFile: can't find %s\n", path);
		return NULL;
	}
	rw = file->open(file, 0);
	if (!rw) {
		if (complain)
			Sys_Printf ("LoadFile: can't open %s\n", path);
		return NULL;
	}
	len = com_filesize = file->len;

	switch (type)
	{
		case 0:			// Zone passed to us.
			buf = Zone_Alloc (zone, len + 1);
			break;
		case 1:			// Temp zone.
			buf = Zone_Alloc (tempzone, len + 1);
			break;
		case 2:			// Named alloc.
			buf = Zone_AllocName (path, len + 1);
			break;
		default:
			Sys_Error ("Bad type in COM_LoadFile!");
			break;
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s (%i bytes)", path,
				len + 1);

	Draw_Disc ();
	SDL_RWread(rw, buf, len, 1);
	SDL_RWclose(rw);
	buf[len] = '\0';

	return buf;
}

Uint8 *
COM_LoadZoneFile (const char *path, qboolean complain, memzone_t *zone)
{
	return COM_LoadFile (path, complain, 0, zone);
}

Uint8 *
COM_LoadTempFile (const char *path, qboolean complain)
{
	return COM_LoadFile (path, complain, 1, NULL);
}

Uint8 *
COM_LoadNamedFile (const char *path, qboolean complain)
{
	return COM_LoadFile (path, complain, 2, NULL);
}

/*
================
Sets com_gamedir, and adds the directory to the head of the path.
================
*/
static void
COM_AddDirectory (const char *dir, Uint32 flags)
{
	strlcpy_s (com_gamedir, dir);
	Sys_UpdateLogpath ();

	FS_AddPath (dir, NULL, flags);
}

/*
================
Wrapper for COM_AddDirectory
================
*/
static void
COM_AddGameDirectory (const char *dir)
{
	char		buf[1024];

	snprintf (buf, sizeof (buf), "%s/%s", fs_sharepath->svalue, dir);

	if (strcmp (fs_userpath->svalue, fs_sharepath->svalue))
	{
		// only do this if the share path is not the same as the base path
		COM_AddDirectory (buf, FS_READ_ONLY);

		snprintf (buf, sizeof (buf), "%s/%s", fs_userpath->svalue, dir);
		Sys_mkdir (buf);
	}

	COM_AddDirectory (buf, 0);
}


/*
================
Sets the gamedir and path to a different directory.
================
*/
void
COM_Gamedir (const char *dir)
{
	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":"))
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp (gamedirfile, dir))
		// still the same
		return;

	FS_Delete_ID (gamedirfile);

	strlcpy_s (gamedirfile, dir);

	COM_AddGameDirectory (dir);
}

static void
COM_InitFilesystem (void)
{
	Uint	i;

	FS_Init ();

// -basedir <path>
// Overrides the system supplied base directory.
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc - 1) {
		Cvar_Set (fs_userpath, com_argv[i + 1]);
		Cvar_Set (fs_sharepath, com_argv[i + 1]);
	}

	Sys_mkdir (fs_userpath->svalue);

	// Make sure fs_sharepath is set to something useful
	if (!strlen (fs_sharepath->svalue))
		Cvar_Set (fs_sharepath, fs_userpath->svalue);

	COM_AddGameDirectory (fs_gamename->svalue);
	COM_AddGameDirectory ("qw");

	// any set gamedirs will be freed up to here
//	com_base_searchpaths = com_searchpaths;
}

void
ExpandPath(cvar_t *var)
{
	char *expanded;
	
	expanded = Sys_ExpandPath(var->svalue);
	if (strcmp(expanded, var->svalue))
		Cvar_Set(var, expanded);
}
