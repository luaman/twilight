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

#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>

#include "SDL_endian.h"
#include "qtypes.h"

#define MAX_QPATH       64              // max length of a quake game pathname
#define MAX_OSPATH      128             // max length of a filesystem pathname

// FIXME: now in src/base/parm.c - remove this at some point

extern size_t com_argc;
extern char **com_argv;

void Cmdline_Init (int argc, char *argv[]);
int COM_CheckParm (char *parm);


extern struct cvar_s *fs_userconf;
extern struct cvar_s *fs_userpath;
extern struct cvar_s *fs_shareconf;
extern struct cvar_s *fs_sharepath;
extern struct cvar_s *game_directory;
extern struct cvar_s *game_rogue;
extern struct cvar_s *game_hipnotic;
extern struct cvar_s *game_mission;

//============================================================================

typedef struct sizebuf_s {
	qboolean			allowoverflow;	// if false, do a Sys_Error
	qboolean			overflowed;		// true if the buffer was too small
	Uint8				*data;
	size_t				maxsize;
	size_t				cursize;
	struct sizebuf_s	*next;			// used for cmd_text chain
} sizebuf_t;

void SZ_Init (sizebuf_t *buf, Uint8 *data, size_t length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, size_t length);
void SZ_Write (sizebuf_t *buf, void *data, size_t length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s {
	struct link_s *prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

//============================================================================

#ifndef NULL
# define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT ((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT ((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================
// Byte order macros

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
# define LittleShort(x) (x)
# define LittleLong(x) (x)
# define LittleFloat(x) (x)
# define BigShort(x) (Sint16)SDL_SwapBE16(x)
# define BigLong(x) (Sint32)SDL_SwapBE32(x)
# define BigFloat(x) ((float_int_t)SDL_SwapBE32(((float_int_t)(x)).i)).fp
#else
# define LittleShort(x) (Sint16)SDL_SwapLE16(x)
# define LittleLong(x) (Sint32)SDL_SwapLE32(x)
# define LittleFloat(x) ((float_int_t)SDL_SwapLE32(((float_int_t)(x)).i)).fp
# define BigShort(x) (x)
# define BigLong(x) (x)
# define BigFloat(x) (x)
#endif

//============================================================================

struct usercmd_s;

extern struct usercmd_s nullcmd;

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
void MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from,
		struct usercmd_s *cmd);

extern size_t msg_readcount;
extern qboolean msg_badread;			// if a read goes beyond end of msg

void MSG_BeginReading (void);
int MSG_GetReadCount (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);
char *MSG_ReadStringLine (void);

float MSG_ReadCoord (void);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);
void MSG_ReadDeltaUsercmd (struct usercmd_s *from, struct usercmd_s *cmd);
void MSG_PrintPacket (void);

//============================================================================

int Q_atoi (char *str);
float Q_atof (char *str);



//============================================================================

extern char com_token[1024];
extern qboolean com_eof;

char *COM_Parse (char *data);


void COM_Init_Cvars (void);
void COM_Init (void);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);
qboolean COM_CheckFile (char *fname);
char *COM_FileExtension (char *in);

//============================================================================

extern int  com_filesize;
struct cache_user_s;

extern char com_gamedir[MAX_OSPATH];

void COM_WriteFile (char *filename, void *data, int len);
int COM_FOpenFile (char *filename, FILE ** file, qboolean complain);

int COM_filelength (FILE *f);
// FIXME: ALL FILES SHOULD BE LOADED WITH COM_LoadAllocFile!
Uint8 *COM_LoadStackFile (char *path, void *buffer, int bufsize,
		qboolean complain);
Uint8 *COM_LoadTempFile (char *path, qboolean complain);
Uint8 *COM_LoadHunkFile (char *path, qboolean complain);
Uint8 *COM_LoadAllocFile (char *path, qboolean complain);
void COM_CreatePath (char *path);
void COM_Gamedir (char *dir);

void Com_PrintHex (char *str, int len);
void Com_Printf (const char *fmt, ...);
void Com_DPrintf (const char *fmt, ...);
void Com_SafePrintf (char *fmt, ...);
void Com_BeginRedirect (void (*RedirectedPrint) (char *));
void Com_EndRedirect (void);

extern struct cvar_s *registered;

int	build_number (void);

#endif // __COMMON_H

