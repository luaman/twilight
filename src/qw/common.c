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
#include "draw.h"
#include "net.h"
#include "protocol.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"

usercmd_t nullcmd;					// guarenteed to be zero

cvar_t *fs_shareconf;
cvar_t *fs_sharepath;
cvar_t *fs_userconf;
cvar_t *fs_userpath;
cvar_t *fs_gamename;
cvar_t *registered;

void COM_InitFilesystem (void);
void COM_Path_f (void);


qboolean standard_quake = true, rogue, hipnotic;

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
Q_atoi (char *str)
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
Q_atof (char *str)
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
MSG_WriteString (sizebuf_t *sb, char *s)
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

	dat.i = net_message.data[msg_readcount] +
		(net_message.data[msg_readcount + 1] << 8) +
		(net_message.data[msg_readcount + 2] << 16) +
		(net_message.data[msg_readcount + 3] << 24);
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

void (*rd_print) (char *) = NULL;

void Com_BeginRedirect (void (*RedirectedPrint) (char *))
{
	rd_print = RedirectedPrint;
}

void Com_EndRedirect (void)
{
	rd_print = NULL;
}

void Com_PrintHex (char *str, int len)
{
	char	c;
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

void Com_Printf (const char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];
	extern char	logname[MAX_OSPATH];

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
void Com_DPrintf (const char *fmt, ...)
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
void Com_DFPrintf (int level, const char *fmt, ...)
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

void *
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
SZ_Write (sizebuf_t *buf, void *data, size_t length)
{
	memcpy (SZ_GetSpace (buf, length), data, length);
}

void
SZ_Print (sizebuf_t *buf, char *data)
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


/*
============
COM_SkipPath
============
*/
char *
COM_SkipPath (char *pathname)
{
	char       *last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void
COM_StripExtension (char *in, char *out)
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


/*
============
COM_FileExtension
============
*/
char *
COM_FileExtension (char *in)
{
	static char		exten[8];
	int				i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}


/*
==================
COM_DefaultExtension
==================
*/
void
COM_DefaultExtension (char *path, char *extension)
{
	char	   *src;

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

	strcat (path, extension);
}

//============================================================================

char com_token[1024];


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *
COM_Parse (char *data)
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
COM_CheckFile

================
*/
qboolean 
COM_CheckFile (char *fname)
{
	FILE	   *h;

	COM_FOpenFile (fname, &h, false);

	if (!h)
		return false;

	fclose (h);
	return true;
}

/*
================
COM_CheckRegistered

Looks for the pop.lmp file
Sets the "registered" cvar.
================
*/
void
COM_CheckRegistered (void)
{
	if (!COM_CheckFile ("gfx/pop.lmp"))
		return;

	Cvar_Set (registered, "1");
	Com_Printf ("Playing registered version.\n");
}


/*
================
COM_Init_Cvars
================
*/
void
COM_Init_Cvars (void)
{
	registered = Cvar_Get ("registered", "0", CVAR_NONE, NULL);

	// fs_shareconf/userconf can't be done here
	fs_sharepath = Cvar_Get ("fs_sharepath", SHAREPATH, CVAR_ROM, ExpandPath);
	fs_userpath = Cvar_Get ("fs_userpath", USERPATH, CVAR_ROM, ExpandPath);

	fs_gamename = Cvar_Get ("fs_gamename", "id1", CVAR_ROM, NULL);
}

/*
================
COM_Init
================
*/
void
COM_Init (void)
{
	Cmd_AddCommand ("path", COM_Path_f);

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}


/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int com_filesize;


/*
 * in memory
 */

typedef struct
{
	char        name[MAX_QPATH];
	int         filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char        filename[MAX_OSPATH];
	FILE       *handle;
	int         numfiles;
	packfile_t *files;
} pack_t;

/*
 * on disk
 */
typedef struct
{
	char        name[56];
	int         filepos, filelen;
} dpackfile_t;

typedef struct
{
	char        id[4];
	int         dirofs;
	int         dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	2048

char com_gamedir[MAX_OSPATH];

typedef struct searchpath_s
{
	char		filename[MAX_OSPATH];
	pack_t	   *pack;			// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t *com_searchpaths;
searchpath_t *com_base_searchpaths;		// without gamedirs

/*
================
COM_filelength
================
*/
int
COM_filelength (FILE * f)
{
	int			pos;
	int			end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int
COM_FileOpenRead (char *path, FILE ** hndl)
{
	FILE	   *f;

	f = fopen (path, "rb");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_filelength (f);
}

/*
============
COM_Path_f

============
*/
void
COM_Path_f (void)
{
	searchpath_t	   *s;

	Com_Printf ("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next) {
		if (s == com_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename,
						s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void
COM_WriteFile (char *filename, void *data, int len)
{
	FILE	   *f;
	char		name[MAX_OSPATH];

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, filename);

	f = fopen (name, "wb");
	if (!f)
	{
		Sys_mkdir (com_gamedir);
		f = fopen (name, "wb");
		if (!f)
			Sys_Error ("Error opening %s", filename);
	}

	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);
}


/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
void
COM_CreatePath (char *path)
{
	char	   *ofs;

	for (ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{
			// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void
COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	   *in, *out;
	unsigned	remaining, count;
	char		buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	// create directories up to the cache file
	COM_CreatePath (cachepath);
	out = fopen (cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof (buf))
			count = remaining;
		else
			count = sizeof (buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}


int file_from_pak;						// last file came from pack

int
COM_FOpenFile (char *filename, FILE ** file, qboolean complain)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				i;
	char			*s;

	file_from_pak = 0;

	if ((s = strchr (filename, '\\')))
	{
		Com_Printf ("COM_FOpenFile: %s should use / to seperate paths, fixing it\n",
				filename);
		do
			*s = '/';
		while ((s = strchr (filename, '\\')));
	}
	if (filename[0] == '/')
	{
		Com_Printf ("COM_FOpenFile: %s should not begin with /, correcting it\n",
				filename);
		do
			filename++;
		while (filename[0] == '/');
	}

	/*
	 * search through the path, one element at a time
	 */
	for (search = com_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (!strcmp (pak->files[i].name, filename))
				{
					// found it!
					Com_DPrintf ("PackFile: %s : %s\n", pak->filename,
							filename);
					// open a new file on the pakfile
					*file = fopen (pak->filename, "rb");
					if (!*file)
						Sys_Error ("Couldn't reopen %s", pak->filename);
					fseek (*file, pak->files[i].filepos, SEEK_SET);
					com_filesize = pak->files[i].filelen;
					file_from_pak = 1;
					return com_filesize;
				}
		} else {
			// check a file in the directory tree
			if (strstr (filename, ".."))
				continue;

			snprintf (netpath, sizeof (netpath), "%s/%s", search->filename,
					  filename);

			*file = fopen (netpath, "rb");

			if (!*file)
				continue;

			Com_DPrintf ("FindFile: %s\n", netpath);

			return COM_filelength (*file);
		}

	}

	if (complain)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	*file = NULL;
	com_filesize = -1;
	return -1;
}

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 byte to the loaded data.
============
*/
static Uint8 *
COM_LoadFile (char *path, qboolean complain, int type, memzone_t *zone)
{
	FILE		*h;
	Uint8		*buf = NULL;
	int			len;

	// look for it in the filesystem or pack files
	len = com_filesize = COM_FOpenFile (path, &h, complain);
	if (!h)
		return NULL;

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

	((Uint8 *) buf)[len] = 0;
	Draw_Disc ();
	fread (buf, 1, len, h);
	fclose (h);

	return buf;
}

Uint8 *
COM_LoadZoneFile (char *path, qboolean complain, memzone_t *zone)
{
	return COM_LoadFile (path, complain, 0, zone);
}

Uint8 *
COM_LoadTempFile (char *path, qboolean complain)
{
	return COM_LoadFile (path, complain, 1, NULL);
}

Uint8 *
COM_LoadNamedFile (char *path, qboolean complain)
{
	return COM_LoadFile (path, complain, 2, NULL);
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t     *
COM_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t	   *newfiles;
	int				numpackfiles;
	pack_t		   *pack;
	FILE		   *packhandle;
	dpackfile_t		info[MAX_FILES_IN_PACK];

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof (header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A'
		|| header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof (dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	newfiles = Z_Malloc (numpackfiles * sizeof (packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (&info, 1, header.dirlen, packhandle);

	// parse the directory
	for (i = 0; i < numpackfiles; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong (info[i].filepos);
		newfiles[i].filelen = LittleLong (info[i].filelen);
	}

	pack = Z_Malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
COM_AddDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void
COM_AddDirectory (char *dir)
{
	int				i;
	searchpath_t   *search;
	pack_t		   *pak;
	char			pakfile[MAX_OSPATH];
	char		   *p;

	if ((p = strrchr (dir, '/')) != NULL)
		strcpy (gamedirfile, ++p);
	else
		strcpy (gamedirfile, p);
	strcpy (com_gamedir, dir);
	Sys_mkdir (com_gamedir);

	/*
	 * add the directory to the search path
	 */
	search = Z_Malloc (sizeof (searchpath_t));
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

	/*
	 * add any pak files in the format pak0.pak pak1.pak, ...
	 */
	for (i = 0;; i++)
	{
		snprintf (pakfile, sizeof (pakfile), "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		search = Z_Malloc (sizeof (searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

/*
================
COM_AddGameDirectory

Wrapper for COM_AddDirectory
================
*/
void
COM_AddGameDirectory (char *dir)
{
	char		buf[1024];

	snprintf (buf, sizeof (buf), "%s/%s", fs_sharepath->svalue, dir);
	COM_AddDirectory (buf);

	if (strcmp (fs_userpath->svalue, fs_sharepath->svalue) != 0)
	{
		// only do this if the share path is not the same as the base path
		snprintf (buf, sizeof (buf), "%s/%s", fs_userpath->svalue, dir);
		Sys_mkdir (buf);
		COM_AddDirectory (buf);
	}
}


/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void
COM_Gamedir (char *dir)
{
	searchpath_t *next;

	if (strstr (dir, "..") || strstr (dir, "/")
		|| strstr (dir, "\\") || strstr (dir, ":"))
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp (gamedirfile, dir))
		// still the same
		return;

	strcpy (gamedirfile, dir);

	/*
	 * free up any current game dir info
	 */
	while (com_searchpaths != com_base_searchpaths)
	{
		if (com_searchpaths == NULL)
			Sys_Error ("Com_Gamedir: tried to free to base searchpath and "
					"hit NULL\n");

		if (com_searchpaths->pack)
		{
			fclose (com_searchpaths->pack->handle);
			Z_Free (com_searchpaths->pack->files);
			Z_Free (com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	if (!strcmp (dir, fs_gamename->svalue) || !strcmp (dir, "qw"))
		return;

	COM_AddGameDirectory (dir);
}

/*
================
COM_InitFilesystem
================
*/
void
COM_InitFilesystem (void)
{
	Uint			i;

	// -basedir <path>, now the same as fs_userpath
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc - 1)
		Cvar_Set (fs_userpath, com_argv[i + 1]);

	Sys_mkdir (fs_userpath->svalue);

	// Make sure fs_sharepath is set to something useful
	if (!strlen (fs_sharepath->svalue))
		Cvar_Set (fs_sharepath, fs_userpath->svalue);

	COM_AddGameDirectory (fs_gamename->svalue);
	COM_AddGameDirectory ("qw");

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;
}

void
ExpandPath(cvar_t *var)
{
	char *expanded;
	
	expanded = Sys_ExpandPath(var->svalue);
	if (strcmp(expanded, var->svalue))
		Cvar_Set(var, expanded);
}
