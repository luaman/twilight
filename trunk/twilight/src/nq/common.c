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
/* common.c -- misc functions used in client and server */
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef _WIN32
#  include <win32conf.h>
# endif
#endif

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
#include "strlib.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"

cvar_t *registered;
cvar_t *cmdline;
cvar_t *fs_shareconf;
cvar_t *fs_sharepath;
cvar_t *fs_userconf;
cvar_t *fs_userpath;
cvar_t *fs_gamename;

void        COM_InitFilesystem (void);
void        COM_Path_f (void);


qboolean    standard_quake = true, rogue, hipnotic;


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
	int         val;
	int         sign;
	int         c;

	if (*str == '-') {
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		while (1) {
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
//
// check for character
//
	if (str[0] == '\'') {
		return sign * str[1];
	}
//
// assume decimal
//
	while (1) {
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
	double      val;
	int         sign;
	int         c;
	int         decimal, total;

	if (*str == '-') {
		sign = -1;
		str++;
	} else
		sign = 1;

	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		while (1) {
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
//
// check for character
//
	if (str[0] == '\'') {
		return sign * str[1];
	}
//
// assume decimal
//
	decimal = -1;
	total = 0;
	while (1) {
		c = *str++;
		if (c == '.') {
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
	while (total > decimal) {
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

//
// writing functions
//

void
MSG_WriteChar (sizebuf_t *sb, int c)
{
	Uint8      *buf;

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
	Uint8      *buf;

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
	Uint8      *buf;

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
	Uint8      *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void
MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union {
		float       f;
		int         l;
	} dat;


	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
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
	MSG_WriteShort (sb, Q_rint(f * 8));
}

void
MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, Q_rint(f * (256.0f / 360.0f)) & 255);
}

//
// reading functions
//
int         msg_readcount;
qboolean    msg_badread;

void
MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int
MSG_ReadChar (void)
{
	signed char c;

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
	unsigned char c;

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
	short c;

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
	int         c;

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
	union {
		Uint8       b[4];
		float       f;
		int         l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char       *
MSG_ReadString (void)
{
	static char string[2048];
	Uint32		l;
	Sint32		c;

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

void Com_Printf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];
#ifndef TWILIGHT_QWSV
	extern char	logname[MAX_OSPATH];
#else
	extern FILE *sv_logfile;
#endif

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

	if (!con_initialized)
		return;

// write it to the scrollable buffer
	Con_Print (msg);
}

void Com_DPrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	if (!developer || !developer->value)
		return;							// don't confuse non-developers with
	// techie stuff...

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}

void
Com_SafePrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[1024];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrint (msg);
}

//===========================================================================

void 
SZ_Init (sizebuf_t *buf, Uint8 *data, int length)
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

void       *
SZ_GetSpace (sizebuf_t *buf, int length)
{
	void       *data;

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)",
					buf->maxsize);

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

		Sys_Printf ("SZ_GetSpace: overflow\n");
		SZ_Clear (buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void
SZ_Write (sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace (buf, length), data, length);
}

void
SZ_Print (sizebuf_t *buf, char *data)
{
	int         len;

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
char       *
COM_SkipPath (char *pathname)
{
	char       *last;

	last = pathname;
	while (*pathname) {
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

	while (*in) {
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
char       *
COM_FileExtension (char *in)
{
	static char exten[8];
	int         i;

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
============
COM_FileBase
============
*/
void
COM_FileBase (char *in, char *out)
{
	char *slash, *dot;
	char *s;

	slash = in;
	dot = NULL;
	s = in;
	while(*s)
	{
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;
	if (dot - slash < 2)
		strcpy (out,"?model?");
	else {
		while (slash < dot)
			*out++ = *slash++;
		*out++ = 0;
	}
}


/*
==================
COM_DefaultExtension
==================
*/
void
COM_DefaultExtension (char *path, char *extension)
{
	char       *src;

//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen (path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return;						// it has an extension
		src--;
	}

	strcat (path, extension);
}

//============================================================================

char        com_token[1024];


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char       *
COM_Parse (char *data)
{
	int         c;
	int         len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
  skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0)
			return NULL;				// end of file;
		data++;
	}

// skip // comments
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
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
	FILE *h;

	COM_FOpenFile (fname, &h, false);

	if (!h) {
		return false;
	}

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

	// fs_shareconf/userconf have to be done in Host_Init
	fs_sharepath = Cvar_Get ("fs_sharepath", SHAREPATH, CVAR_ROM, NULL);
	fs_userpath = Cvar_Get ("fs_userpath", USERPATH, CVAR_ROM, NULL);

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

/// just for debugging
int
memsearch (Uint8 *start, int count, int search)
{
	int         i;

	for (i = 0; i < count; i++)
		if (start[i] == search)
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int         com_filesize;


//
// in memory
//

typedef struct {
	char        name[MAX_QPATH];
	int         filepos, filelen;
} packfile_t;

typedef struct pack_s {
	char        filename[MAX_OSPATH];
	FILE       *handle;
	int         numfiles;
	packfile_t *files;
} pack_t;

//
// on disk
//
typedef struct {
	char        name[56];
	int         filepos, filelen;
} dpackfile_t;

typedef struct {
	char        id[4];
	int         dirofs;
	int         dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	2048

char        com_gamedir[MAX_OSPATH];

typedef struct searchpath_s {
	char        filename[MAX_OSPATH];
	pack_t     *pack;					// only one of filename / pack will be
	// used
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
	if (!f) {
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
	searchpath_t *s;

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
	FILE       *f;
	char        name[MAX_OSPATH];

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, filename);

	f = fopen (name, "wb");
	if (!f) {
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

Only used for CopyFile
============
*/
void
COM_CreatePath (char *path)
{
	char       *ofs;

	for (ofs = path + 1; *ofs; ofs++) {
		if (*ofs == '/') {				// create the directory
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
	FILE		*in, *out;
	Uint32		remaining, count;
	char		buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	// create directories up to the cache file
	COM_CreatePath (cachepath);
	out = fopen (cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining) {
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


int         file_from_pak;				// global indicating file came from

										// pack file ZOID

int
COM_FOpenFile (char *filename, FILE ** file, qboolean complain)
{
	searchpath_t *search;
	char        netpath[MAX_OSPATH];
	pack_t     *pak;
	int         i;
	int         findtime;

	file_from_pak = 0;

//
// search through the path, one element at a time
//
	for (search = com_searchpaths; search; search = search->next) {
		// is the element a pak file?
		if (search->pack) {
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (!strcmp (pak->files[i].name, filename)) {	// found it!
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
			if (filename[0] == '/' || filename[0] == '\\')
				continue;
			if (strstr (filename, ".."))
				continue;

			snprintf (netpath, sizeof (netpath), "%s/%s", search->filename,
					  filename);

			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;

			Com_DPrintf ("FindFile: %s\n", netpath);

			*file = fopen (netpath, "rb");
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
Allways appends a 0 byte to the loaded data.
============
*/
cache_user_t *loadcache;
Uint8        *loadbuf;
int           loadsize;
Uint8 *
COM_LoadFile (char *path, int usehunk, qboolean complain)
{
	FILE       *h;
	Uint8      *buf;
	char        base[32];
	int         len;

	buf = NULL;							// quiet compiler warning

// look for it in the filesystem or pack files
	len = com_filesize = COM_FOpenFile (path, &h, complain);
	if (!h)
		return NULL;

// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	switch (usehunk) {
		case 0:
			buf = Z_Malloc (len + 1);
			break;
		case 1:
			buf = Hunk_AllocName (len + 1, base);
			break;
		case 2:
			buf = Hunk_TempAlloc (len + 1);
			break;
		case 4:
			if (len + 1 > loadsize)
				buf = Hunk_TempAlloc (len + 1);
			else
				buf = loadbuf;
			break;
		case 5:
			buf = malloc (len + 1);
			break;
		default:
			Sys_Error ("COM_LoadFile: bad usehunk");
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
COM_LoadZoneFile (char *path, qboolean complain)
{
	return COM_LoadFile (path, 0, complain);
}

Uint8 *
COM_LoadHunkFile (char *path, qboolean complain)
{
	return COM_LoadFile (path, 1, complain);
}

Uint8 *
COM_LoadTempFile (char *path, qboolean complain)
{
	return COM_LoadFile (path, 2, complain);
}

// uses temp hunk if larger than bufsize
Uint8 *
COM_LoadStackFile (char *path, void *buffer, int bufsize, qboolean complain)
{
	Uint8      *buf;

	loadbuf = (Uint8 *) buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4, complain);

	return buf;
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
	dpackheader_t header;
	int         i;
	packfile_t *newfiles;
	int         numpackfiles;
	pack_t     *pack;
	FILE       *packhandle;
	dpackfile_t info[MAX_FILES_IN_PACK];

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
	for (i = 0; i < numpackfiles; i++) {
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
COM_AddDirectory (char *indir)
{
	int         i;
	searchpath_t *search;
	pack_t     *pak;
	char        pakfile[MAX_OSPATH];
	char	   *dir;

	dir = Sys_ExpandPath (indir);
	strcpy (com_gamedir, dir);
	Sys_mkdir (com_gamedir);

//
// add the directory to the search path
//
	search = Z_Malloc (sizeof (searchpath_t));
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

//
// add any pak files in the format pak0.pak pak1.pak, ...
//
	for (i = 0;; i++) {
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

	snprintf (buf, sizeof (buf), "%s/%s", fs_sharepath->string, dir);
	COM_AddDirectory (buf);

	if (strcmp (fs_userpath->string, fs_sharepath->string) != 0) {
		// only do this if the share path is not the same as the base path
		snprintf (buf, sizeof (buf), "%s/%s", fs_userpath->string, dir);
		Sys_mkdir (buf);
		COM_AddDirectory (buf);
	}
}


/*
================
COM_InitFilesystem
================
*/
void
COM_InitFilesystem (void)
{
	int         i;

//
// -basedir <path>
// Overrides the system supplied base directory
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc - 1)
		Cvar_Set (fs_userpath, com_argv[i + 1]);

	Sys_mkdir (fs_userpath->string);

// Make sure fs_sharepath is set to something useful
	if (!strlen (fs_sharepath->string))
		Cvar_Set (fs_sharepath, fs_userpath->string);

	COM_AddGameDirectory (fs_gamename->string);

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	if (COM_CheckParm ("-rogue"))
	{
		rogue = true;
		standard_quake = false;
		COM_AddGameDirectory ("rogue");
	}
	if (COM_CheckParm ("-hipnotic"))
	{
		hipnotic = true;
		standard_quake = false;
		COM_AddGameDirectory ("hipnotic");
	}

//
// -game <gamedir>
// Adds basedir/gamedir as an override game
//
	i = COM_CheckParm ("-game");
	if (i && i < com_argc - 1)
		COM_AddGameDirectory (com_argv[i + 1]);
}

